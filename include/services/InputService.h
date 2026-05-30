#pragma once
#include <stdint.h>
#include <PCF8575.h>
#include "types/events.h"

// Forward declaration — no header include to avoid circular deps
class NavigationService;

class InputService {
 public:
  explicit InputService(NavigationService& nav);

  // Call after Wire.begin() has been called in main
  bool begin();

  // Call every loop iteration
  void tick();

 private:
  PCF8575 pcf_;
  NavigationService& nav_;

  // Previous D-pad button states for edge detection
  uint8_t prevUp_    = HIGH;
  uint8_t prevDown_  = HIGH;
  uint8_t prevLeft_  = HIGH;
  uint8_t prevRight_ = HIGH;

  // Rotary encoder button previous state
  bool prevRotarySw_ = HIGH;
  uint32_t lastRotaryBtnMs_ = 0;
};
