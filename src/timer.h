#pragma once

#include <Arduino.h>

class IntervalTimer {
 public:
  IntervalTimer() : forceTrigger_(false), isRunning_(false), isStrict_(false), periodMs_(0), triggerTime_(0) {}

  void start(uint32_t periodMs, bool isStrict = false) {
    periodMs_ = periodMs;
    isStrict_ = isStrict;
    isRunning_ = true;
    triggerTime_ = millis() + periodMs_;
  }

  void forceTrigger() { forceTrigger_ = true; }
  virtual void onTimer() = 0;

  void handle() {
    if (isRunning_) {
      uint32_t now = millis();
      if (forceTrigger_ || now >= triggerTime_) {
        forceTrigger_ = false;
        onTimer();
        if (isStrict_) {
          triggerTime_ += periodMs_;
        } else {
          triggerTime_ = now + periodMs_;
        }
      }
    }
  }

  void setPeriodMs(uint32_t periodMs) { periodMs_ = periodMs; }

 private:
  bool forceTrigger_;
  bool isRunning_;
  bool isStrict_;
  uint32_t periodMs_;
  uint32_t triggerTime_;
};

class Timer {
 public:
  Timer() : isRunning_(false), on_(false), durationMs_(0), startTime_(0), stopTime_(0) {}

  virtual void triggerFor(uint32_t durationMs) {
    durationMs_ = durationMs;
    isRunning_ = true;
    stopTime_ = millis() + durationMs_;
    on_ = true;
    onTimer(on_);
  }

  virtual void triggerAfterDelay(uint32_t delayMs, uint32_t durationMs) {
    durationMs_ = durationMs;
    delayMs_ = delayMs;
    isRunning_ = true;
    startTime_ = millis() + delayMs;
    stopTime_ = startTime_ + durationMs_;
  }

  virtual void onTimer(bool isOn) = 0;

  bool isRunning() { return isRunning_; }

  bool isOn() { return on_; }

  void handle() {
    if (isRunning_) {
      uint32_t now = millis();
      if (on_) {
        if (now >= stopTime_) {
          isRunning_ = false;
          on_ = false;
          onTimer(on_);
        }
      } else {
        if (now >= startTime_) {
          on_ = true;
          onTimer(on_);
        }
      }
    }
  }

 private:
  bool isRunning_;
  bool on_;
  uint32_t durationMs_;
  uint32_t delayMs_;
  uint32_t startTime_;
  uint32_t stopTime_;
};