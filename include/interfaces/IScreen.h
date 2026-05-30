#pragma once
#include "types/events.h"

class IScreen {
 public:
  virtual ~IScreen() = default;
  // called when the screen comes into focus
  virtual void onEnter() = 0;
  // called when the screen goes out of focus
  virtual void onExit() = 0;
  // core event handler 
  virtual bool handleInput(InputEvent event) = 0;

  // start the screen's main loop (if needed)
  virtual void loop() = 0;
};