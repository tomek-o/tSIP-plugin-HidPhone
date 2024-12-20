#ifndef ControlQueueH
#define ControlQueueH

namespace nsHidDevice
{
    class HidDevice;
}

namespace ControlQueue
{
    void SetOnline(bool state);
    void SetRing(bool state);
    void SetCall(bool state);
    void SetMute(bool state);

    int Poll(nsHidDevice::HidDevice &dev);
    int SetInitialState(nsHidDevice::HidDevice &dev);

    bool GetRing(void);
    bool GetOnline(void);
    bool GetCall(void);
    bool GetMute(void);
    bool GetOffHook(void);
}

#endif // ControlQueueH
