#pragma once

#include <Arduino.h>
#include "types.h"  
#include "timer.h"

class CameraTimer : public Timer
{
public:
    CameraTimer() : Timer()
    {
    }

    void setup(uint8_t ledPin)
    {
        ledPin_ = ledPin;
        pinMode(ledPin_, OUTPUT);
    }

    virtual void triggerFor(uint32_t durationMs)
    {
        Timer::triggerFor(durationMs);
        digitalWrite(ledPin_, 1);
    }

    virtual void triggerAfterDelay(uint32_t delayMs, uint32_t durationMs)
    {
        Timer::triggerAfterDelay(delayMs, durationMs);
        digitalWrite(ledPin_, 0);
    }

    virtual void onTimer(bool isOn)
    {
        digitalWrite(ledPin_, isOn);
    }

private:
    uint8_t ledPin_;
};

class Camera
{
public:
    typedef union {
        struct
        {
            bool focus : 1;
            bool trigger : 1;
        } fields;
        zj_u8_t u8;
    } Status_t;

    bool setup(uint8_t focusPin, uint8_t triggerPin)
    {
        focusTimer_.setup(focusPin);
        triggerTimer_.setup(triggerPin);

        return true;
    }

    void handle()
    {
        focusTimer_.handle();
        triggerTimer_.handle();
    }

    void startFocus(zj_u32_t ms)
    {
        focusTimer_.triggerFor(ms.uint32);
    }

    bool isFocusing()
    {
        return focusTimer_.isOn();
    }

    void startTrigger(zj_u32_t ms)
    {
        triggerTimer_.triggerFor(ms.uint32);
    }

    void startShot(zj_u32_t focusMs, zj_u32_t triggerMs)
    {
        focusTimer_.triggerFor(focusMs.uint32 + triggerMs.uint32);
        triggerTimer_.triggerAfterDelay(focusMs.uint32, triggerMs.uint32);
    }

    bool isTriggering()
    {
        return triggerTimer_.isOn();
    }

    bool isActive()
    {
        return triggerTimer_.isRunning() || focusTimer_.isRunning();
    }

    Status_t getStatus()
    {
        Status_t result;
        result.u8.uint8 = 0;
        result.fields.focus = focusTimer_.isOn();
        result.fields.trigger = triggerTimer_.isOn();
        return result;
    }

private:
    CameraTimer focusTimer_;
    CameraTimer triggerTimer_;
};