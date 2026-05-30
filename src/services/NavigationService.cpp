#include "services/NavigationService.h"
#include <Arduino.h>

void NavigationService::registerScreen(IScreen* screen) {
  if (screenCount_ < MAX_SCREENS && screen != nullptr) {
    screens_[screenCount_++] = screen;
  }
}

void NavigationService::navigateTo(IScreen* screen) {
  for (uint8_t i = 0; i < screenCount_; i++) {
    if (screens_[i] == screen) {
      navigateTo(i);
      return;
    }
  }
}

void NavigationService::navigateTo(uint8_t index) {
  if (index >= screenCount_) {
    return;
  }

  if (screens_[activeScreenIndex_]) {
    screens_[activeScreenIndex_]->onExit();
  }

  activeScreenIndex_ = index;
  Serial.printf("[Navigation] Switched to screen index: %d\n", activeScreenIndex_);

  if (screens_[activeScreenIndex_]) {
    screens_[activeScreenIndex_]->onEnter();
  }
}

void NavigationService::injectEvent(InputEvent event) {
  Serial.printf("[Navigation] Received event code: %d\n", static_cast<int>(event));

  IScreen* active = getActiveScreen();
  if (active) {
    if (active->handleInput(event)) {
      Serial.println("[Navigation] Event consumed by active screen local handler.");
      return; // Event was consumed by the active screen
    }
  }

  // Global navigation fallback
  if (event == InputEvent::DPAD_RIGHT) {
    if (screenCount_ > 1) {
      uint8_t nextIndex = (activeScreenIndex_ + 1) % screenCount_;
      Serial.printf("[Navigation] Event not consumed. Global DPAD_RIGHT -> Navigate to screen index: %d\n", nextIndex);
      navigateTo(nextIndex);
    } else {
      Serial.println("[Navigation] Event not consumed. DPAD_RIGHT ignored: screenCount <= 1");
    }
  } else if (event == InputEvent::DPAD_LEFT) {
    if (screenCount_ > 1) {
      uint8_t prevIndex = (activeScreenIndex_ + screenCount_ - 1) % screenCount_;
      Serial.printf("[Navigation] Event not consumed. Global DPAD_LEFT -> Navigate to screen index: %d\n", prevIndex);
      navigateTo(prevIndex);
    } else {
      Serial.println("[Navigation] Event not consumed. DPAD_LEFT ignored: screenCount <= 1");
    }
  } else {
    Serial.println("[Navigation] Event not consumed and has no global action handler.");
  }
}

IScreen* NavigationService::getActiveScreen() const {
  if (screenCount_ == 0) {
    return nullptr;
  }
  return screens_[activeScreenIndex_];
}

void NavigationService::loop() {
  IScreen* active = getActiveScreen();
  if (active) {
    active->loop();
  }
}
