#include "CommThread.h"
#include "..\tSIP\tSIP\phone\Phone.h"
#include "HidDevice.h"
#include "HidReportHandler.h"
#include "CustomSettings.h"
#include "ControlQueue.h"
#include "Log.h"
#include <windows.h>
#include <assert.h>
#include <time.h>
#include <stdio.h>

void Key(int keyCode, int state);

using namespace nsHidDevice;

namespace {

volatile bool connected = false;
volatile bool exited = false;

}

DWORD WINAPI CommThreadProc(LPVOID data) {
    //ThreadComm *thrRead = (ThreadComm*)data;
    HidDevice hidDevice;
    bool devConnected = false;
    unsigned int loopCnt = 0;

    unsigned char rcvbuf[17];
    LOG("Running comm thread");

    while (connected) {
        if (devConnected == false) {
            if (loopCnt % 100 == 0) {
                int status = hidDevice.Open(customSettings.usbVid, customSettings.usbPid, NULL, NULL, customSettings.usbUsagePage);
                if (status == 0) {
                    devConnected = true;
                    LOG("Device connected (USB VID 0x%04X, PID 0x%04X, usage page 0x%X)",
                        hidDevice.GetVid(), hidDevice.GetPid(), hidDevice.GetUsagePage());
                    //LOG("  devConnected: %d", (int)devConnected);
                    std::string dump;
                    hidDevice.DumpCapabilities(dump);
                    LOG("USB HID capabilities: %s\n", dump.c_str());
                    ControlQueue::SetInitialState(hidDevice);
                } else {
                    LOG("Error opening USB device (USB VID 0x%04X, PID 0x%04X, usage page 0x%X): %s",
                        static_cast<int>(customSettings.usbVid), static_cast<int>(customSettings.usbPid), static_cast<int>(customSettings.usbUsagePage),
                        HidDevice::GetErrorDesc(status).c_str());
                }
            }
        } else {
            if (ControlQueue::Poll(hidDevice) != 0) {
                LOG("Device: error updating");
                hidDevice.Close();
                devConnected = false;
                //lastRcvFilled = false;
            }
            else
            {
                int size = sizeof(rcvbuf);
                memset(rcvbuf, 0, sizeof(rcvbuf));
                //LOG("%03d  devConnected: %d, size = %d", __LINE__, (int)devConnected, size);
                int status = hidDevice.ReadReport(HidDevice::E_REPORT_IN, 0, rcvbuf, &size, 100);
                //LOG("%03d  devConnected: %d, size = %d", __LINE__, (int)devConnected, size);
                if (status == 0) {
                    if (customSettings.logReceivedHidReports) {
                        std::string hex;
                        for (int i=0; i<size; i++) {
                            char str[16];
                            snprintf(str, sizeof(str), "0x%02X ", rcvbuf[i]);
                            hex += str;
                        }
                        LOG("HID REPORT_IN received %d B: %s", size, hex.c_str());
                    }
                    ProcessReceivedReport(rcvbuf, size);
                } else if (status != HidDevice::E_ERR_TIMEOUT) {
                    LOG("USB device: error reading report");
                    hidDevice.Close();
                    devConnected = false;
                    //lastRcvFilled = false;
                }
            }
        }
        loopCnt++;
        Sleep(50);
    }

    if (devConnected)
    {
        // clear state
        ControlQueue::SetOnline(false);
        ControlQueue::SetRing(false);
        ControlQueue::SetCall(false);
        ControlQueue::SetMute(false);
        ControlQueue::Poll(hidDevice);
    }

    hidDevice.Close();
    exited = true;
    return 0;
}


int CommThreadStart(void) {
    DWORD dwtid;
    exited = false;
    connected = true;
    HANDLE CommThread = CreateThread(NULL, 0, CommThreadProc, /*this*/NULL, 0, &dwtid);
    if (CommThread == NULL) {
        connected = false;
        exited = true;
    }

    return 0;
}

int CommThreadStop(void) {
    connected = false;
    while (!exited) {
        Sleep(50);
    }
    return 0;
}

