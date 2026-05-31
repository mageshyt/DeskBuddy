#include "services/OledService.h"
#include "assets/images.h"
#include <Arduino.h>

OledService::OledService(Adafruit_SH1106G& oled) : oled_(oled) {}

void OledService::begin() {
  initialized_ = true;
  drawIdleImage();
}

void OledService::drawIdleImage() {
  if (!initialized_) return;
  oled_.clearDisplay();
  if (hasActiveStateImage_) {
    // Draw the cached active persistent focus/break layout
    oled_.drawBitmap(0, 0, activeStateImageBuffer_, 128, 64, SH110X_WHITE);
  } else {
    // Draw the 128x64 robot idle bitmap at (0,0)
    oled_.drawBitmap(0, 0, kRobotIdleBitmap, 128, 64, SH110X_WHITE);
  }
  oled_.display();
}

void OledService::displayMessage(const char* title, const char* msg) {
  if (!initialized_) return;
  
  oled_.clearDisplay();
  oled_.setTextSize(1);
  oled_.setTextColor(SH110X_WHITE);
  oled_.setTextWrap(true);
  
  // Draw title
  oled_.setCursor(0, 0);
  oled_.println(title);
  oled_.println("---------------------");
  
  // Draw content
  oled_.println(msg);
  
  oled_.display();
}

void OledService::handleImageEvent(const char* base64Data, bool persistent) {
  if (!initialized_ || base64Data == nullptr || base64Data[0] == '\0') {
    return;
  }
  
  uint8_t buffer[1024];
  size_t decodedLen = decodeBase64(base64Data, buffer, sizeof(buffer));
  if (decodedLen != 1024) {
    Serial.printf("[OledService] Base64 decode failed or size is incorrect. Decoded %d bytes (expected 1024)\n", decodedLen);
    return;
  }
  
  if (persistent) {
    memcpy(activeStateImageBuffer_, buffer, 1024);
    hasActiveStateImage_ = true;
  }
  
  oled_.clearDisplay();
  oled_.drawBitmap(0, 0, buffer, 128, 64, SH110X_WHITE);
  oled_.display();
  Serial.printf("[OledService] Decoded and rendered base64 image event (persistent=%s).\n", persistent ? "true" : "false");
}

// Custom memory-efficient Base64 decoder
size_t OledService::decodeBase64(const char* input, uint8_t* output, size_t maxLen) {
  if (input == nullptr || output == nullptr) return 0;
  
  // Decoding lookup table (covering printable ASCII chars)
  static const int kDecodingTable[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1,  0, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1
  };

  size_t inputLen = strlen(input);
  if (inputLen % 4 != 0) return 0;

  size_t outLen = 0;
  for (size_t i = 0; i < inputLen; i += 4) {
    if (outLen >= maxLen) break;

    int n0 = kDecodingTable[(unsigned char)input[i]];
    int n1 = kDecodingTable[(unsigned char)input[i+1]];
    int n2 = (input[i+2] == '=') ? 0 : kDecodingTable[(unsigned char)input[i+2]];
    int n3 = (input[i+3] == '=') ? 0 : kDecodingTable[(unsigned char)input[i+3]];

    if (n0 == -1 || n1 == -1 || n2 == -1 || n3 == -1) return 0; // invalid chars

    output[outLen++] = (n0 << 2) | (n1 >> 4);
    if (input[i+2] != '=') {
      if (outLen >= maxLen) break;
      output[outLen++] = ((n1 & 0x0F) << 4) | (n2 >> 2);
    }
    if (input[i+3] != '=') {
      if (outLen >= maxLen) break;
      output[outLen++] = ((n2 & 0x03) << 6) | n3;
    }
  }
  return outLen;
}
