#pragma once

#include <Adafruit_SH110X.h>

class OledService {
 public:
  explicit OledService(Adafruit_SH1106G& oled);

  void begin();
  void drawIdleImage();
  void displayMessage(const char* title, const char* msg);
  void handleImageEvent(const char* base64Data, bool persistent);

 private:
  Adafruit_SH1106G& oled_;
  bool initialized_ = false;
  uint8_t activeStateImageBuffer_[1024];
  bool hasActiveStateImage_ = false;

  // Base64 decoding helper
  size_t decodeBase64(const char* input, uint8_t* output, size_t maxLen);
};
