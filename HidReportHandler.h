#ifndef HidReportHandlerH
#define HidReportHandlerH

namespace nsHidDevice
{
    class HidDevice;
}

void ProcessReceivedReport(nsHidDevice::HidDevice &hidDevice, unsigned char* buffer, int size);

#endif // HidReportHandlerH
