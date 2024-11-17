#include "HidReportHandler.h"
#include "HidDevice.h"
#include "CustomSettings.h"
#include "ControlQueue.h"
#include "Log.h"
#include <string.h>

/**
Handling strange Gigaset ION behavior: when answering incoming call first report does not contain
offHook usage (or is just lineBusy in response to ring?), second one does. With this flag two reports without offHook are required.
This is not breaking Jabra Evolve 65 interoperability.
*/

static bool callReject = false;
static bool stateOffHook = false;
//static bool stateMute = false;


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
        if (offHook != stateOffHook)
        {
            if (offHook)
            {
                if (hidDevice.GetBcTelephonyHookSwitch().absolute == false)
                {
                    callReject = false;
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
                    if (stateOffHook == ControlQueue::GetOffHook())
                    {
                        LOG("Running offHook1 script");
                        RunScriptAsync(customSettings.scriptOffHook1.c_str());
                    }
                }
            }
            else
            {
                if (hidDevice.GetBcTelephonyHookSwitch().absolute)
                {
                    if (stateOffHook == ControlQueue::GetOffHook())
                    {
                        LOG("Running offHook0 script");
                        RunScriptAsync(customSettings.scriptOffHook0.c_str());
                    }
                }
            }
            stateOffHook = offHook;
        }
        else
        {
            //LOG("Value of offHook not changed");
            if (offHook == false && ControlQueue::GetRing())
            {
                if (callReject == false)
                {
                    callReject = true;
                }
                else
                {
                    LOG("Running offHook0 script");
                    RunScriptAsync(customSettings.scriptOffHook0.c_str());
                }
            }
            else
            {
                callReject = false;
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
