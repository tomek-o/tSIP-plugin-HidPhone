#include "HidReportHandler.h"
#include "HidDevice.h"
#include "CustomSettings.h"
#include "ControlQueue.h"
#include "Log.h"
#include <string.h>

static bool callReject = false;

int RunScriptAsync(const char* script);

void ProcessReceivedReport(nsHidDevice::HidDevice &hidDevice, unsigned char* buffer, int size)
{
    if (size <= 0)
        return;

    bool handled = false;

    for (unsigned int i=0; i<customSettings.reportActions.size(); i++)
    {
        const struct ReportAction &ra = customSettings.reportActions[i];
        if (size >= static_cast<int>(ra.report.size())) {
            if (memcmp(buffer, &ra.report[0], ra.report.size()) == 0) {
                LOG("Running script #%d:\n%s\n", i, ra.script.c_str());
                RunScriptAsync(ra.script.c_str());
                handled = true;
                break;
            }
        }
    }

    if (handled)
        return;

    bool offHook = false, mute = false, redial = false, lineBusy = false, flash = false;

    int status = hidDevice.ParseReceivedReport(buffer, size, offHook, mute, redial, lineBusy, flash);
    if (status == 0)
    {
        LOG("Parsed report: offHook %d, mute %d, redial %d, lineBusy %d, flash %d", offHook, mute, redial, lineBusy, flash);
        if (offHook)
        {
            if (hidDevice.GetBcTelephonyHookSwitch().absolute == false)
            {
                unsigned int timer = hidDevice.GetOffHookSetTimer();
                if (timer == 0)
                {
                    LOG("Running offHookToggle script");
                    RunScriptAsync(customSettings.scriptOffHookToggle.c_str());
                }
                else
                {
                    LOG("Ignoring offHook script, timer = %u", timer);
                }
            }
            else
            {
                LOG("Running offHook1 script");
                RunScriptAsync(customSettings.scriptOffHook1.c_str());
            }
            callReject = false;
        }
        else
        {
            if (hidDevice.GetBcTelephonyHookSwitch().absolute)
            {
                if (ControlQueue::GetRing() == false) {
                    LOG("Running offHook0 script");
                    RunScriptAsync(customSettings.scriptOffHook0.c_str());
                    callReject = false;
                } else {
                    if (callReject) {
                        LOG("Running offHook0 script for call reject");
                        RunScriptAsync(customSettings.scriptOffHook0.c_str());
                        callReject = false;
                    } else {
                        LOG("Delaying offHook0 script - setting call reject flag");
                        callReject = true;
                    }
                }
            }
        }

        if (mute)
        {
            if (hidDevice.GetBcTelephonyMute().absolute == false)
            {
                LOG("Running muteToggle script");
                RunScriptAsync(customSettings.scriptMuteToggle.c_str());
            }
            else
            {
                LOG("Running mute1 script");
                RunScriptAsync(customSettings.scriptMute1.c_str());
            }
        }
        else
        {
            if (hidDevice.GetBcTelephonyMute().absolute)
            {
                LOG("Running mute0 script");
                RunScriptAsync(customSettings.scriptMute0.c_str());
            }
        }
    }
    else
    {
        LOG("Failed to parse received report");
    }
}
