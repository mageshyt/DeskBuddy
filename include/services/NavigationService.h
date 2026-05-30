#pragma once
#include <stdint.h>
#include "interfaces/IScreen.h"

class NavigationService {
 private:
  static constexpr uint8_t MAX_SCREENS = 8;
  IScreen* screens_[MAX_SCREENS]{nullptr};
  uint8_t screenCount_ = 0;
  uint8_t activeScreenIndex_ = 0;

 public:
  void registerScreen(IScreen* screen);
  void injectEvent(InputEvent event);
  void navigateTo(IScreen* screen);
  void navigateTo(uint8_t index);

  IScreen* getActiveScreen() const;
  void loop();
};