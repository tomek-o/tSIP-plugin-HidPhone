#include "HidReportHandler.h"
#include "HidDevice.h"
#include "CustomSettings.h"
#include "Log.h"
#include <string.h>

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

    bool offHook = false, mute = false, redial = false, lineBusy = false;

    int status = hidDevice.ParseReceivedReport(buffer, size, offHook, mute, redial, lineBusy);
    if (status == 0)
    {
        LOG("Parsed report: offHook %d, mute %d, redial %d, lineBusy %d", offHook, mute, redial, lineBusy);
        if (offHook)
        {
            unsigned int timer = hidDevice.GetOffHookSetTimer();
            if (timer == 0)
            {
                LOG("Running offHook script");
                RunScriptAsync(customSettings.scriptOffHook.c_str());
            }
            else
            {
                LOG("Ignoring offHook script, timer = %u", timer);
            }
        }
    }
    else
    {
        LOG("Failed to parse received report");
    }
}
