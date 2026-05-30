#include "services/InputService.h"
#include "services/NavigationService.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

// ── Rotary Encoder ISR (file-scope, IRAM-safe) ─────────────────────────────
namespace {
volatile int g_encoderDelta = 0;
volatile int g_lastClkState = HIGH;

void IRAM_ATTR rotaryClkISR() {
  const int clk = digitalRead(kRotaryClkPin);
  const int dt  = digitalRead(kRotaryDtPin);
  if (clk != g_lastClkState) {
    if (clk == LOW) {                // falling edge of CLK
      g_encoderDelta += (dt == HIGH) ? 1 : -1;
    }
    g_lastClkState = clk;
  }
}
}  // namespace

// ── Constructor ─────────────────────────────────────────────────────────────
InputService::InputService(NavigationService& nav)
    : pcf_(kPcfAddress, &Wire), nav_(nav) {}

// ── begin() — call AFTER Wire.begin() in main ──────────────────────────────
bool InputService::begin() {
  // ── Rotary Encoder GPIO setup ──
  pinMode(kRotaryClkPin, INPUT_PULLUP);
  pinMode(kRotaryDtPin, INPUT_PULLUP);
  pinMode(kRotarySwPin, INPUT_PULLUP);

  g_lastClkState = digitalRead(kRotaryClkPin);
  g_encoderDelta = 0;
  prevRotarySw_  = digitalRead(kRotarySwPin);

  attachInterrupt(digitalPinToInterrupt(kRotaryClkPin), rotaryClkISR, CHANGE);

  // ── PCF8575 I2C expander — match the working config-notes pattern ──
  // The working code uses PCF.isConnected(), not pcf_.begin().
  // begin() writes 0xFFFF to the device which is fine for inputs, but
  // isConnected() is the proven handshake.
  if (!pcf_.isConnected()) {
    Serial.printf("[InputService] PCF8575 at 0x%02X NOT connected!\n", kPcfAddress);
    return false;
  }

  // Do a test read to prime the internal buffer
  pcf_.read(kButtonUpPin);
  if (pcf_.lastError() != 0) {
    Serial.printf("[InputService] PCF8575 test read failed, error: %d\n", pcf_.lastError());
    return false;
  }

  Serial.println("[InputService] Ready. PCF8575 connected, rotary ISR attached.");
  return true;
}

void InputService::tick() {
  uint8_t up    = pcf_.read(kButtonUpPin);
  uint8_t down  = pcf_.read(kButtonDownPin);
  uint8_t left  = pcf_.read(kButtonLeftPin);
  uint8_t right = pcf_.read(kButtonRightPin);

  if (pcf_.lastError() != 0) {
    // I2C glitch — skip this frame entirely (same as working code)
    delay(10);
    return;
  }

  // UP
  if (prevUp_ == HIGH && up == LOW) {
    Serial.println("[Input] UP pressed");
    nav_.injectEvent(InputEvent::DPAD_UP);
  }
  // DOWN
  if (prevDown_ == HIGH && down == LOW) {
    Serial.println("[Input] DOWN pressed");
    nav_.injectEvent(InputEvent::DPAD_DOWN);
  }
  // LEFT
  if (prevLeft_ == HIGH && left == LOW) {
    Serial.println("[Input] LEFT pressed");
    nav_.injectEvent(InputEvent::DPAD_LEFT);
  }
  // RIGHT
  if (prevRight_ == HIGH && right == LOW) {
    Serial.println("[Input] RIGHT pressed");
    nav_.injectEvent(InputEvent::DPAD_RIGHT);
  }

  prevUp_    = up;
  prevDown_  = down;
  prevLeft_  = left;
  prevRight_ = right;

  // ════════════════════════════════════════════════════════════════════════
  // 2. Rotary encoder rotation (interrupt-driven, zero-miss)
  // ════════════════════════════════════════════════════════════════════════
  int delta = 0;
  noInterrupts();
  delta = g_encoderDelta;
  g_encoderDelta = 0;
  interrupts();

  if (kRotaryInverted) {
    delta = -delta;
  }

  if (delta > 0) {
    for (int i = 0; i < delta; i++) {
      Serial.println("[Input] ENCODER CW");
      nav_.injectEvent(InputEvent::ENCODER_CW);
    }
  } else if (delta < 0) {
    for (int i = 0; i < -delta; i++) {
      Serial.println("[Input] ENCODER CCW");
      nav_.injectEvent(InputEvent::ENCODER_CCW);
    }
  }

  // ════════════════════════════════════════════════════════════════════════
  // 3. Rotary encoder push button (direct GPIO, debounced 250ms)
  //    Matches the config-notes.md ROTARY_BTN_DEBOUNCE_MS = 250
  // ════════════════════════════════════════════════════════════════════════
  const bool swPressed = (digitalRead(kRotarySwPin) == LOW);
  const uint32_t now = millis();

  if (swPressed && prevRotarySw_ == HIGH &&
      (now - lastRotaryBtnMs_) > 250) {
    Serial.println("[Input] ENCODER PRESS");
    nav_.injectEvent(InputEvent::ENCODER_PRESS);
    lastRotaryBtnMs_ = now;
  }
  prevRotarySw_ = swPressed ? LOW : HIGH;
}
