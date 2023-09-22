/** \file
    \note endpoint size = 64B = const
*/

/*
	Copyright 2009 Tomasz Ostrowski, http://tomeko.net. All rights reserved.

	Redistribution and use in source and binary forms, with or without modification, are
	permitted provided that the following conditions are met:

	   1. Redistributions of source code must retain the above copyright notice, this list of
		  conditions and the following disclaimer.

	   2. Redistributions in binary form must reproduce the above copyright notice, this list
		  of conditions and the following disclaimer in the documentation and/or other materials
		  provided with the distribution.

	THIS SOFTWARE IS PROVIDED 'AS IS' AND ANY EXPRESS OR IMPLIED
	WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER OR
	CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
	ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HidDevice.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
extern "C"
{
#include <ddk/hidsdi.h>
#include <setupapi.h>
}
#include <dbt.h>

#include <sstream>
#include <iostream>
#include <algorithm>
#include <assert.h>
#include <stdio.h>

#include "Log.h"
#include "ErrorString.h"

using namespace nsHidDevice;
using namespace std;

#define HID_USAGE_TELEPHONY_HOOK_SWITCH 0x20
#define HID_USAGE_TELEPHONY_FLASH       0x21
#define HID_USAGE_TELEPHONY_HOLD        0x23
#define HID_USAGE_TELEPHONY_REDIAL      0x24
#define HID_USAGE_TELEPHONY_MUTE        0x2F    // microphone mute
#define HID_USAGE_TELEPHONY_LINE_BUSY   0x97
#define HID_USAGE_TELEPHONY_LED_RINGER  0x9E    //?

HidDevice::SERROR HidDevice::tabErrorsName[E_ERR_LIMIT] =
{
    { E_ERR_INV_PARAM,                  "Invalid parameter" },
    { E_ERR_NOTFOUND,                   "Device not found" },
    { E_ERR_IO,                         "Error calling I/O function" },
    { E_ERR_TIMEOUT,                    "Timeout" },
    { 0,		                        "No error" }
};

/** \brief Get short error message
*/
std::string HidDevice::GetErrorDesc(int ErrorCode)
{
    int a=0;
    std::stringstream stream;
    //stream << (std::string)"B³¹d ";
    //stream << static_cast<int>(ErrorCode) + ". ";
    while (tabErrorsName[a].nCode > 0)
    {
        if (tabErrorsName[a].nCode == ErrorCode)
        {
            stream << tabErrorsName[a].lpName;
            return stream.str();
        }
        a++;
    }
    return stream.str();
}

void HidDevice::UnicodeToAscii(char *buffer)
{
    unsigned short  *unicode = (unsigned short*)buffer;
    char            *ascii = buffer;
    while (*unicode)
    {
        if (*unicode >= 256)
            *ascii++ = '?';
        else
            *ascii++ = *unicode++;
    }
    *ascii++ = 0;
}

HidDevice::HidDevice(void):
        handle(INVALID_HANDLE_VALUE),
        readHandle(INVALID_HANDLE_VALUE),
        writeHandle(INVALID_HANDLE_VALUE),
        hEventObject(NULL),
        VID(0),
        PID(0),
        usagePage(-1),
        preparsedData(NULL),
        reportInLength(0),
        reportOutLength(0),
        offHookSetTimer(0)
{
    pOverlapped = new OVERLAPPED;
    HidD_GetHidGuid(&hidGuid);

#if 0
    // crash test
    int *test = NULL;
    *test = 1;
    (void)test;
#endif
}

HidDevice::~HidDevice(void)
{
    OVERLAPPED *o = (OVERLAPPED*)pOverlapped;
    delete o;
    if (preparsedData)
        HidD_FreePreparsedData(preparsedData);
    Close();
}

void HidDevice::GetHidGuid(GUID *guid) const
{
    *guid = hidGuid;
}

/** \todo Automatic device selection based on usagePage + usage
*/

int HidDevice::Open(int VID, int PID, char *vendorName, char *productName, int usagePage)
{
    HDEVINFO                            deviceInfoList;
    SP_DEVICE_INTERFACE_DATA            deviceInfo;
    SP_DEVICE_INTERFACE_DETAIL_DATA     *deviceDetails = NULL;
    DWORD                               size;
    int                                 i, openFlag = 0;  /* may be FILE_FLAG_OVERLAPPED */
    int                                 errorCode = E_ERR_NOTFOUND;
    HIDD_ATTRIBUTES                     deviceAttributes;

    this->usagePage = usagePage;

    deviceInfoList = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
    deviceInfo.cbSize = sizeof(deviceInfo);
    for (i=0;;i++)
    {
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
        if (!SetupDiEnumDeviceInterfaces(deviceInfoList, 0, &hidGuid, i, &deviceInfo))
            break;
        // check actual size to allocate buffer
        SetupDiGetDeviceInterfaceDetail(deviceInfoList, &deviceInfo, NULL, 0, &size, NULL);
        if (deviceDetails != NULL)
            free(deviceDetails);
        deviceDetails = (SP_DEVICE_INTERFACE_DETAIL_DATA*)malloc(size);
        deviceDetails->cbSize = sizeof(*deviceDetails);
        // call using allocated buffer
        SetupDiGetDeviceInterfaceDetail(deviceInfoList, &deviceInfo, deviceDetails, size, &size, NULL);

        // note: writing/reading may be unsupported
        handle = CreateFile(deviceDetails->DevicePath, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, openFlag, NULL);
        if (handle == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        // check VID + PID
        deviceAttributes.Size = sizeof(deviceAttributes);
        HidD_GetAttributes(handle, &deviceAttributes);
        if (VID != 0 && deviceAttributes.VendorID != VID)
            continue;
        if (PID != 0 && deviceAttributes.ProductID != PID)
            continue;

        errorCode = E_ERR_NOTFOUND;
        if (vendorName != NULL)
        {
            char buffer[512];
            if (!HidD_GetManufacturerString(handle, buffer, sizeof(buffer)))
            {
                errorCode = E_ERR_IO;
                continue;
            }
            UnicodeToAscii(buffer);
            if (strcmp(vendorName, buffer) != 0)
                continue;
        }
        if (productName != NULL)
        {
            char buffer[512];
            if (!HidD_GetProductString(handle, buffer, sizeof(buffer)))
            {
                errorCode = E_ERR_IO;
                continue;
            }
            UnicodeToAscii(buffer);
            if (strcmp(productName, buffer) != 0)
                continue;
        }

        if (usagePage >= 0)
        {
            // returns a pointer to a buffer containing the information about the device's capabilities.
            if (HidD_GetPreparsedData(handle, &preparsedData) == FALSE) {
                errorCode = E_ERR_IO;
                continue;
            }

            HIDP_CAPS Capabilities;
            if (HidP_GetCaps(preparsedData, &Capabilities) != HIDP_STATUS_SUCCESS) {
                errorCode = E_ERR_IO;
                continue;
            }
            if (Capabilities.UsagePage != usagePage /*0x0b*/)
                continue;

            this->VID = deviceAttributes.VendorID;
            this->PID = deviceAttributes.ProductID;

            reportInLength = Capabilities.InputReportByteLength;
            reportOutLength = Capabilities.OutputReportByteLength;

            HIDP_REPORT_TYPE reportTypes[] = { HidP_Input, HidP_Output };
            for (unsigned int reportTypeId = 0; reportTypeId < sizeof(reportTypes)/sizeof(reportTypes[0]); reportTypeId++)
            {
                HIDP_BUTTON_CAPS buttonCaps[64];
                unsigned long buttonCapsCount = sizeof(buttonCaps)/sizeof(buttonCaps[0]);
                if (HidP_GetButtonCaps(reportTypes[reportTypeId], buttonCaps, &buttonCapsCount, preparsedData) == HIDP_STATUS_SUCCESS)
                {
                    for (unsigned int i=0; i<buttonCapsCount; i++)
                    {
                        const HIDP_BUTTON_CAPS &caps = buttonCaps[i];
                        if (caps.IsRange)
                            continue;
                        unsigned short usage = caps.NotRange.Usage;
                        if (caps.UsagePage == HID_USAGE_PAGE_TELEPHONY)
                        {
                            switch (usage)
                            {
                            case HID_USAGE_TELEPHONY_HOOK_SWITCH:
                                bcTelephonyHookSwitch.valid = true;
                                bcTelephonyHookSwitch.reportId = caps.ReportID;
                                bcTelephonyHookSwitch.absolute = caps.IsAbsolute;
                                LOG("Caps: USAGE_TELEPHONY_HOOK_SWITCH: reportId = %d, absolute = %d", caps.ReportID, caps.IsAbsolute);
                                break;
                            case HID_USAGE_TELEPHONY_MUTE:
                                bcTelephonyMute.valid = true;
                                bcTelephonyMute.reportId = caps.ReportID;
                                bcTelephonyMute.absolute = caps.IsAbsolute;
                                LOG("Caps: USAGE_TELEPHONY_MUTE: reportId = %d, absolute = %d", caps.ReportID, caps.IsAbsolute);
                                break;
                            case HID_USAGE_TELEPHONY_REDIAL:
                                bcTelephonyRedial.valid = true;
                                bcTelephonyRedial.reportId = caps.ReportID;
                                bcTelephonyRedial.absolute = caps.IsAbsolute;
                                LOG("Caps: USAGE_TELEPHONY_REDIAL: reportId = %d, absolute = %d", caps.ReportID, caps.IsAbsolute);
                                break;
                            default:
                                LOG("Caps: Unknown usage 0x%02X: reportId = %d, absolute = %d", usage, caps.ReportID, caps.IsAbsolute);
                                break;
                            }

                        }
                        else if (caps.UsagePage == HID_USAGE_PAGE_LED)
                        {
                            switch (usage)
                            {
                            case HID_USAGE_LED_ON_LINE:
                                bcLedOnline.valid = true;
                                bcLedOnline.reportId = caps.ReportID;
                                bcLedOnline.absolute = caps.IsAbsolute;
                                break;
                            case HID_USAGE_LED_OFF_HOOK:
                                bcLedOffHook.valid = true;
                                bcLedOffHook.reportId = caps.ReportID;
                                bcLedOffHook.absolute = caps.IsAbsolute;
                                break;
                            case HID_USAGE_LED_RING:
                                bcLedRing.valid = true;
                                bcLedRing.reportId = caps.ReportID;
                                bcLedRing.absolute = caps.IsAbsolute;
                                break;
                            case HID_USAGE_LED_MUTE:
                                bcLedMute.valid = true;
                                bcLedMute.reportId = caps.ReportID;
                                bcLedMute.absolute = caps.IsAbsolute;
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
                else
                {
                    LOG("HidP_GetButtonCaps failed!");
                }
            }

            HidD_FlushQueue(handle);

        }

        break;
    }

    if (handle != INVALID_HANDLE_VALUE)
    {
        path = deviceDetails->DevicePath;
        errorCode = CreateReadWriteHandles(deviceDetails->DevicePath);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoList);
    if (deviceDetails != NULL)
        free(deviceDetails);
    return errorCode;
}

int HidDevice::CreateReadWriteHandles(std::string path)
{
    writeHandle = CreateFile (path.c_str(), GENERIC_WRITE,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        (LPSECURITY_ATTRIBUTES)NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (writeHandle == INVALID_HANDLE_VALUE) {
        LOG("Failed to create write handle!");
        return E_ERR_IO;
    }
	readHandle = CreateFile	(path.c_str(), GENERIC_READ,
		FILE_SHARE_READ|FILE_SHARE_WRITE,
		(LPSECURITY_ATTRIBUTES)NULL,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		NULL);
    if (readHandle == INVALID_HANDLE_VALUE) {
        LOG("Failed to create read handle!");
        return E_ERR_IO;
    }

	if (hEventObject == 0)
	{
		hEventObject = CreateEvent
			(NULL,  // security
			TRUE,   // manual reset (call ResetEvent)
			TRUE,   // initial state = signaled
			"");    // name
        if (hEventObject == NULL) {
            LOG("Failed to create event handle!");
            return E_ERR_OTHER;
        }
        ((OVERLAPPED*)pOverlapped)->hEvent = hEventObject;
        ((OVERLAPPED*)pOverlapped)->Offset = 0;
        ((OVERLAPPED*)pOverlapped)->OffsetHigh = 0;
	}
	return 0;
}


int HidDevice::DumpCapabilities(std::string &dump)
{
    PHIDP_PREPARSED_DATA	PreparsedData;

    // returns a pointer to a buffer containing the information about the device's capabilities.
    if (HidD_GetPreparsedData(handle, &PreparsedData) == FALSE)
        return E_ERR_IO;

    HIDP_CAPS Capabilities;
    if (HidP_GetCaps(PreparsedData, &Capabilities) != HIDP_STATUS_SUCCESS) {
        LOG("HidP_GetCaps failed!");
        return E_ERR_IO;
    }

    std::stringstream stream;
    stream << "    Usage Page: 0x" << hex << Capabilities.UsagePage << endl;
    stream << "    Input Report Byte Length: " << Capabilities.InputReportByteLength << endl;
    stream << "    Output Report Byte Length: " << Capabilities.OutputReportByteLength << endl;
    stream << "    Feature Report Byte Length: " << Capabilities.FeatureReportByteLength << endl;
    stream << "    Number of Link Collection Nodes: " << Capabilities.NumberLinkCollectionNodes << endl;
    stream << "    Number of Input Button Caps: " << Capabilities.NumberInputButtonCaps << endl;
    stream << "    Number of InputValue Caps: " << Capabilities.NumberInputValueCaps << endl;
    stream << "    Number of InputData Indices: " << Capabilities.NumberInputDataIndices << endl;
    stream << "    Number of Output Button Caps: " << Capabilities.NumberOutputButtonCaps << endl;
    stream << "    Number of Output Value Caps: " << Capabilities.NumberOutputValueCaps << endl;
    stream << "    Number of Output Data Indices: " << Capabilities.NumberOutputDataIndices << endl;
    stream << "    Number of Feature Button Caps: " << Capabilities.NumberFeatureButtonCaps << endl;
    stream << "    Number of Feature Value Caps: " << Capabilities.NumberFeatureValueCaps << endl;
    stream << "    Number of Feature Data Indices: " << Capabilities.NumberFeatureDataIndices;

    dump = stream.str();
    HidD_FreePreparsedData(PreparsedData);
    return 0;
}

void HidDevice::Close(void)
{
    if (handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
    if (writeHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(writeHandle);
        writeHandle = INVALID_HANDLE_VALUE;
    }
    if (readHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(readHandle);
        readHandle = INVALID_HANDLE_VALUE;
    }
}

int HidDevice::WriteReport(enum E_REPORT_TYPE type, int id, unsigned char *buffer, int len)
{
    BOOL status = 0;
    DWORD   bytesWritten;
    unsigned char sendbuf[65];
    memset(sendbuf, 0, sizeof(sendbuf));
    sendbuf[0] = id;
    assert (len < (int)sizeof(sendbuf));
    memcpy(sendbuf+1, buffer, std::min((int)sizeof(sendbuf)-1, len));

    switch (type)
    {
    case E_REPORT_IN:
        return E_ERR_INV_PARAM;
    case E_REPORT_OUT:
        status = WriteFile(writeHandle, sendbuf, len+1, &bytesWritten, NULL);

        break;
    case E_REPORT_FEATURE:
        status = HidD_SetFeature(handle, sendbuf, sizeof(sendbuf));
        break;
    default:
        return E_ERR_INV_PARAM;
    }
    return status == 0 ? E_ERR_IO : 0;
}

int HidDevice::Write(unsigned char *buffer, int len)
{
    BOOL status = 0;
    DWORD   bytesWritten = 0;

    std::string str;
    for (int i=0; i<len; i++)
    {
        char tmp[16];
        sprintf(tmp, "0x%02X ", buffer[i]);
        str += tmp;
    }
    LOG("Writing: %s", str.c_str());

    status = WriteFile(writeHandle, buffer, len, &bytesWritten, NULL);
    if (status == 0)
    {
        int err = GetLastError();
        LOG("HID WriteFile failed, err = %d (%s)", err, ErrorString(err).c_str());
    }
    return status == 0 ? E_ERR_IO : 0;
}

int HidDevice::WriteUsage(USAGE usage, bool state)
{
    char report[64+1];
    memset(report, 0, sizeof(report));

    switch (usage)
    {
    case HID_USAGE_LED_ON_LINE:
        if (!bcLedOnline.valid)
            return 0;
        LOG("WriteUsage: ONLINE, %s", state?"true":"false");
        report[0] = bcLedOnline.reportId;
        break;
    case HID_USAGE_LED_RING:
        if (!bcLedRing.valid)
            return 0;
        LOG("WriteUsage: RING, %s", state?"true":"false");
        report[0] = bcLedRing.reportId;
        break;
    case HID_USAGE_LED_OFF_HOOK:
        if (!bcLedOffHook.valid)
            return 0;
        LOG("WriteUsage: OFFHOOK, %s", state?"true":"false");
        if (state)
            offHookSetTimer = 10;
        report[0] = bcLedOffHook.reportId;
        break;
    case HID_USAGE_LED_MUTE:
        if (!bcLedMute.valid)
            return 0;
        LOG("WriteUsage: MUTE, %s", state?"true":"false");
        report[0] = bcLedMute.reportId;
        break;
    default:
        LOG("Unhandled usage!");
        return -1;
    }

    NTSTATUS status;

    if (state)
    {
        USAGE usagePageForSetUsages = HID_USAGE_PAGE_LED;
        USAGE usageList[1];
        usageList[0] = usage;
        unsigned long usageCount = 1;

        status = HidP_SetUsages(HidP_Output, usagePageForSetUsages, 0, usageList, &usageCount, preparsedData, report, reportOutLength);
        if (status != HIDP_STATUS_SUCCESS)
        {
            if (status == HIDP_STATUS_USAGE_NOT_FOUND)
            {
                LOG("HidP_SetUsages failed, status = 0x%08X (HIDP_STATUS_USAGE_NOT_FOUND)", status);
            }
            else
            {
                LOG("HidP_SetUsages failed, status = 0x%08X", status);
            }
            return -1;
        }
    }

    status = Write((unsigned char*)report, reportOutLength);
    if (status != 0)
    {
        LOG("HID write error");
        return -1;
    }
    return status;
}


/* ------------------------------------------------------------------------ */

int HidDevice::ReadReport(enum E_REPORT_TYPE type, int id, unsigned char *buffer, int *len, int timeout)
{
    BOOL status = 0;
    DWORD bytesRead = 0;
    DWORD result;

    int outBufSize = *len;

    char rcvbuf[65];
    memset(rcvbuf, 0, sizeof(rcvbuf));
    rcvbuf[0] = id;
    assert (*len < (int)sizeof(rcvbuf));
    *len = sizeof(rcvbuf);

    switch (type)
    {
    case E_REPORT_IN:
        status = ReadFile(readHandle, rcvbuf, *len, &bytesRead, (LPOVERLAPPED)pOverlapped);
        if( !status )
        {
            if( GetLastError() == ERROR_IO_PENDING )
            {
                result = WaitForSingleObject(hEventObject, timeout);
                switch (result)
                {
                case WAIT_OBJECT_0:
                    //*len = bytesRead;
                    ResetEvent(hEventObject);
                    memcpy(buffer, rcvbuf+1, std::min<int>(*len, outBufSize));
                    return 0;
                case WAIT_TIMEOUT:
                    result = CancelIo(readHandle);
                    ResetEvent(hEventObject);
                    return E_ERR_TIMEOUT;
                default:
                    ResetEvent(hEventObject);
                    return E_ERR_IO;
                }
            } else {
                return E_ERR_IO;
            }
        }
        else
        {
            *len = bytesRead;
            memcpy(buffer, rcvbuf+1, std::min<int>(*len, outBufSize));
            return 0;
        }
        break;
    case E_REPORT_OUT:
        return E_ERR_INV_PARAM;
    case E_REPORT_FEATURE:
        // Capabilities.FeatureReportByteLength?
        status = HidD_GetFeature(handle, rcvbuf, *len);
        if (status)
            memcpy(buffer, rcvbuf+1, std::min<int>(*len, outBufSize));
        break;
    default:
        return E_ERR_INV_PARAM;
    }
    return status == 0 ? E_ERR_IO : 0;
}

int HidDevice::Read(unsigned char *buffer, int &len, int timeout)
{
    BOOL status = 0;
    DWORD bytesRead = 0;
    DWORD result;

    if (offHookSetTimer > 0)
        offHookSetTimer--;

    status = ReadFile(readHandle, buffer, len, &bytesRead, (LPOVERLAPPED)pOverlapped);
    if( !status )
    {
        if( GetLastError() == ERROR_IO_PENDING )
        {
            result = WaitForSingleObject(hEventObject, timeout);
            switch (result)
            {
            case WAIT_OBJECT_0:
                //*len = bytesRead;
                ResetEvent(hEventObject);
                return 0;
            case WAIT_TIMEOUT:
                result = CancelIo(readHandle);
                ResetEvent(hEventObject);
                return E_ERR_TIMEOUT;
            default:
                ResetEvent(hEventObject);
                return E_ERR_IO;
            }
        } else {
            return E_ERR_IO;
        }
    }
    else
    {
        len = bytesRead;
        return 0;
    }
}

int HidDevice::ParseReceivedReport(unsigned char* buffer, int len, bool &offHook, bool &mute, bool &redial, bool &lineBusy)
{
    offHook = false;
    mute = false;
    redial = false;
    lineBusy = false;

	USAGE usageList[16];
	ULONG usageCount = sizeof(usageList)/sizeof(usageList[0]);
    NTSTATUS status = HidP_GetUsages(HidP_Input, usagePage, 0, usageList, &usageCount, preparsedData, (PCHAR)buffer, len);
    if (status != HIDP_STATUS_SUCCESS) {
        LOG("HidP_GetUsages: error 0x%04x", status);
        return status;
    } else {
        LOG("Received %u usage(s)", usageCount);
    }

    for (unsigned int i=0; i<usageCount; i++)
    {
        switch (usageList[i])
        {
        case HID_USAGE_TELEPHONY_HOOK_SWITCH:
            offHook = true;
            break;
        case HID_USAGE_TELEPHONY_MUTE:
            mute = true;
            break;
        case HID_USAGE_TELEPHONY_REDIAL:
            redial = true;
            break;
        case HID_USAGE_TELEPHONY_LINE_BUSY:
            lineBusy = true;
            break;
        default:
            LOG("Received unhandled usage = %d", static_cast<int>(usageList[i]));
            break;
        }
    }

    return 0;
}


