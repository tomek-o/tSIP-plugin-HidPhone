#include "ControlQueue.h"
#include "HidDevice.h"
#include "HidReportHandler.h"
#include "Log.h"
#include "ScopedLock.h"
#include "Mutex.h"
#include <deque>

namespace {

Mutex mutex;

bool onlineState = 0;
bool callState = 0;
bool ringState = 0;
bool muteState = 0;

struct Command
{
    bool updateOnlineState;
    bool onlineState;
    bool updateCallState;
    bool callState;
    bool updateRingState;
    bool ringState;
    bool updateMuteState;
    bool muteState;
    Command(void):
        updateOnlineState(false),
        onlineState(false),
        updateCallState(false),
        callState(false),
        updateRingState(false),
        ringState(false),
        updateMuteState(false),
        muteState(false)
    {
    }
};

std::deque<Command> commandQue;

}   // namespace


void ControlQueue::SetOnline(bool state)
{
    ScopedLock<Mutex> lock(mutex);
    Command cmd;
    cmd.updateOnlineState = true;
    cmd.onlineState = state;
    commandQue.push_back(cmd);
}

void ControlQueue::SetRing(bool state)
{
    ScopedLock<Mutex> lock(mutex);
    Command cmd;
    cmd.updateRingState = true;
    cmd.ringState = state;
    commandQue.push_back(cmd);
}

void ControlQueue::SetCall(bool state)
{
    ScopedLock<Mutex> lock(mutex);
    Command cmd;
    cmd.updateCallState = true;
    cmd.callState = state;
    commandQue.push_back(cmd);
}

void ControlQueue::SetMute(bool state)
{
    ScopedLock<Mutex> lock(mutex);
    Command cmd;
    cmd.updateMuteState = true;
    cmd.muteState = state;
    commandQue.push_back(cmd);
}

int ControlQueue::Poll(nsHidDevice::HidDevice &dev)
{
    Command cmd;

    {
        ScopedLock<Mutex> lock(mutex);
        if (commandQue.empty())
            return 0;
        cmd = commandQue.front();
        commandQue.pop_front();
    }

    int status = 0;

    if (cmd.updateOnlineState) {
        if (cmd.onlineState != onlineState) {
            onlineState = cmd.onlineState;
            status = dev.WriteUsage(HID_USAGE_LED_ON_LINE, onlineState);
            if (status)
                return status;
        }
    }

    if (cmd.updateRingState) {
        if (cmd.ringState != ringState) {
            ringState = cmd.ringState;
            if (!ringState  && callState) {
                status = dev.WriteUsage(HID_USAGE_LED_OFF_HOOK, callState);
                if (status)
                    return status;
            }
            else
            {
                status = dev.WriteUsage(HID_USAGE_LED_RING, ringState);
                if (status)
                    return status;
            }
        }
    }

    if (cmd.updateCallState) {
        if (cmd.callState != callState) {
            callState = cmd.callState;
            if (!ringState) {
                status = dev.WriteUsage(HID_USAGE_LED_OFF_HOOK, callState);
                if (status)
                    return status;
            }
        }
    }

    if (cmd.updateMuteState) {
        if (cmd.muteState != muteState) {
            muteState = cmd.muteState;
            status = dev.WriteUsage(HID_USAGE_LED_MUTE, muteState);
            if (status)
                return status;
        }
    }

    return status;
}

int ControlQueue::SetInitialState(nsHidDevice::HidDevice &dev)
{
    int status;
    status = dev.WriteUsage(HID_USAGE_LED_ON_LINE, onlineState);
    if (status)
        return status;

    status = dev.WriteUsage(HID_USAGE_LED_MUTE, muteState);
    if (status)
        return status;

    status = dev.WriteUsage(HID_USAGE_LED_OFF_HOOK, callState);
    if (status)
        return status;

    status = dev.WriteUsage(HID_USAGE_LED_RING, ringState);
    if (status)
        return status;

    return status;
}

bool ControlQueue::GetRing(void)
{
    return ringState;
}

bool ControlQueue::GetOnline(void)
{
    return onlineState;
}

bool ControlQueue::GetCall(void)
{
    return callState;
}

bool ControlQueue::GetMute(void)
{
    return muteState;
}

bool ControlQueue::GetOffHook(void)
{
    return callState && !ringState;
}

