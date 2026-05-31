#include <PCF8575.h>

PCF8575 PCF(0x27);

#define BUTTON_DOWN 11 // P13
#define BUTTON_UP 10 // P12
#define BUTTON_LEFT 12 // P14
#define BUTTON_RIGHT 13 // P15

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <FluxGarage_RoboEyes.h>

TFT_eSPI tft = TFT_eSPI();
Adafruit_SH1106G display(128, 64, &Wire, -1);
RoboEyes<Adafruit_SH1106G> roboEyes(display);

static const uint8_t OLED_SDA_PIN = 33;  
static const uint8_t OLED_SCL_PIN = 32;

static const uint8_t ROTARY_CLK_PIN = 34;
static const uint8_t ROTARY_DT_PIN = 5;
static const uint8_t ROTARY_SW_PIN = 12;
static const int ROTARY_STEP_PERCENT = 5;
static const bool ROTARY_DIRECTION_INVERTED = true;
static int volumePercent = 50;
static int volumeBeforeMute = 50;
static int lastVolumeShownOnTft = -1;
static int lastRotaryClkState = HIGH;
static int lastRotaryDtState = HIGH;
static int rotaryTransitionAccumulator = 0;
static bool lastRotarySwState = HIGH;
static uint32_t lastRotaryButtonMs = 0;
static const uint32_t ROTARY_BTN_DEBOUNCE_MS = 250;

static uint32_t lastTftStepMs = 0;
static uint32_t lastMoodStepMs = 0;
static uint8_t tftStep = 0;
static uint8_t moodStep = 0;
static bool oledReady = false;
static bool roboEyesEnabled = false;

static bool lastButtonState = HIGH;
static uint32_t lastDebounceMs = 0;
static const uint32_t DEBOUNCE_MS = 30;

static const int16_t BUTTON_MSG_X = 10;
static const int16_t BUTTON_MSG_Y = 125;
static const int16_t BUTTON_MSG_W_PADDING = 20;
static const int16_t BUTTON_MSG_H = 20;
static const uint16_t BUTTON_MSG_ENTER_MS = 180;
static const uint16_t BUTTON_MSG_HOLD_MS = 700;
static const uint16_t BUTTON_MSG_EXIT_MS = 280;
static bool buttonMsgActive = false;
static uint32_t buttonMsgStartMs = 0;
static int16_t buttonMsgLastY = -1000;
static uint8_t buttonMsgLastShade = 0;
static char buttonMsgText[32] = {0};

#include "Org_01.h"

static const unsigned char PROGMEM image_ButtonCenter_bits[] = {0x38,0x44,0xba,0xba,0xba,0x44,0x38};
static const unsigned char PROGMEM image_ButtonLeftSmall_bits[] = {0x20,0x60,0xe0,0x60,0x20};
static const unsigned char PROGMEM image_ButtonRightSmall_bits[] = {0x80,0xc0,0xe0,0xc0,0x80};
static const unsigned char PROGMEM image_cards_hearts_bits[] = {0x00,0x00,0x00,0x00,0x38,0x38,0x7c,0x7c,0xfe,0xfe,0xff,0xfe,0xff,0xfe,0xff,0xfe,0x7f,0xfc,0x3f,0xf8,0x1f,0xf0,0x0f,0xe0,0x07,0xc0,0x03,0x80,0x01,0x00,0x00,0x00};
static const unsigned char PROGMEM image_download**copy**bits[] = {0x01,0x00,0x02,0x80,0x02,0x40,0x22,0x20,0x12,0x20,0x4a,0x48,0x26,0x90,0x33,0x30,0x26,0x90,0x4a,0x48,0x12,0x20,0x22,0x20,0x02,0x40,0x02,0x80,0x01,0x00,0x00,0x00};
static const unsigned char PROGMEM image_download_1_bits[] = {0x00,0x18,0x00,0x60,0x01,0x80,0x06,0x00,0x18,0x00,0x60,0x00,0x7f,0xfe,0x9c,0x01,0xaa,0x7d,0xc1,0x45,0xeb,0x7d,0xc1,0x01,0xaa,0x55,0x9c,0x01,0x7f,0xfe,0x00,0x00};
static const unsigned char PROGMEM image_download_2_bits[] = {0x00,0x00,0xff,0xff,0x80,0x01,0xbf,0xfd,0xa0,0x05,0xa0,0x05,0xa0,0x05,0xa0,0x05,0xa0,0x05,0xbf,0xfd,0x80,0x01,0xff,0xff,0x03,0xc0,0x03,0xc0,0x0f,0xf0,0x00,0x00};
static const unsigned char PROGMEM image_download_3_bits[] = {0x07,0xc0,0x18,0x30,0x27,0xc8,0x48,0x24,0x93,0x92,0xa4,0x4a,0xa9,0x2a,0xa3,0x8a,0x06,0xc0,0x03,0x80,0x01,0x00,0x03,0x80,0x02,0x80,0x06,0xc0,0x04,0x40,0x00,0x00};
static const unsigned char PROGMEM image_FaceNormal_bits[] = {0x00,0x00,0x00,0x00,0x3c,0x00,0x01,0xe0,0x7a,0x00,0x03,0xd0,0x7e,0x00,0x03,0xf0,0x7e,0x00,0x03,0xf0,0x7e,0x00,0x03,0xf0,0x3c,0x00,0x01,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x40,0x00,0x00,0x10,0x40,0x00,0x00,0x10,0x40,0x00,0x00,0x08,0x80,0x00,0x00,0x07,0x00,0x00};
static const unsigned char PROGMEM image_network_3_bars_bits[] = {0x00,0x0e,0x00,0x0a,0x00,0x0a,0x00,0x0a,0x00,0xea,0x00,0xea,0x00,0xea,0x00,0xea,0x0e,0xea,0x0e,0xea,0x0e,0xea,0x0e,0xea,0xee,0xea,0xee,0xea,0xee,0xee,0x00,0x00};
static const unsigned char PROGMEM image_SmallArrowDown_bits[] = {0xf8,0x70,0x20};
static const unsigned char PROGMEM image_SmallArrowUp_bits[] = {0x20,0x70,0xf8};
static const unsigned char PROGMEM image_wifi_75_bits[] = {0x01,0xf0,0x00,0x06,0x0c,0x00,0x18,0x03,0x00,0x21,0xf0,0x80,0x47,0xfc,0x40,0x8f,0x1e,0x20,0x5c,0xe7,0x40,0x3b,0xfb,0x80,0x17,0x1d,0x00,0x0e,0xee,0x00,0x05,0xf4,0x00,0x03,0xb8,0x00,0x01,0x50,0x00,0x00,0xe0,0x00,0x00,0x40,0x00,0x00,0x00,0x00};

static void drawVolumeOnTft(bool forceRedraw = false) {
if (!forceRedraw && volumePercent == lastVolumeShownOnTft) {
return;
}

const int16_t panelX = 12;
const int16_t panelY = 12;
const int16_t panelW = tft.width() - 24;
const int16_t panelH = 58;
const int16_t barX = panelX + 10;
const int16_t barY = panelY + 30;
const int16_t barW = panelW - 20;
const int16_t barH = 14;

const uint16_t panelColor = tft.color565(12, 18, 24);
const uint16_t borderColor = tft.color565(60, 130, 170);
const uint16_t fillColor = tft.color565(40, 220, 140);

tft.fillRoundRect(panelX, panelY, panelW, panelH, 8, panelColor);
tft.drawRoundRect(panelX, panelY, panelW, panelH, 8, borderColor);

tft.setTextSize(1);
tft.setTextColor(TFT_WHITE, panelColor);
tft.setCursor(panelX + 10, panelY + 10);
tft.print("Volume");

tft.fillRect(panelX + panelW - 55, panelY + 8, 45, 12, panelColor);
tft.setCursor(panelX + panelW - 54, panelY + 10);
tft.printf("%3d%%", volumePercent);

tft.drawRect(barX, barY, barW, barH, TFT_DARKGREY);
tft.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, TFT_BLACK);
const int16_t fillW = (int16_t)(((int32_t)(barW - 2) \* volumePercent) / 100);
if (fillW > 0) {
tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, fillColor);
}

lastVolumeShownOnTft = volumePercent;
}

static void setButtonPressMessage(const char \*msg) {
snprintf(buttonMsgText, sizeof(buttonMsgText), "%s", msg);
buttonMsgStartMs = millis();
buttonMsgActive = true;
buttonMsgLastY = -1000;
buttonMsgLastShade = 0;
}

static void updateButtonPressMessageAnimation() {
if (!buttonMsgActive) {
return;
}

const uint32_t now = millis();
const uint32_t elapsed = now - buttonMsgStartMs;
const uint32_t total = BUTTON_MSG_ENTER_MS + BUTTON_MSG_HOLD_MS + BUTTON_MSG_EXIT_MS;

if (elapsed >= total) {
tft.fillRect(BUTTON_MSG_X, BUTTON_MSG_Y - 4, tft.width() - BUTTON_MSG_W_PADDING, BUTTON_MSG_H + 10, TFT_BLACK);
buttonMsgActive = false;
return;
}

int16_t textY = BUTTON_MSG_Y + 8;
uint8_t shade = 255;

if (elapsed < BUTTON_MSG_ENTER_MS) {
const uint32_t p = (elapsed _ 100U) / BUTTON_MSG_ENTER_MS;
textY = BUTTON_MSG_Y + 8 - (int16_t)((p _ 8U) / 100U);
shade = (uint8_t)(150U + ((p _ 105U) / 100U));
} else if (elapsed > (BUTTON_MSG_ENTER_MS + BUTTON_MSG_HOLD_MS)) {
const uint32_t exitElapsed = elapsed - (BUTTON_MSG_ENTER_MS + BUTTON_MSG_HOLD_MS);
const uint32_t p = (exitElapsed _ 100U) / BUTTON_MSG_EXIT_MS;
textY = BUTTON_MSG_Y - (int16_t)((p _ 5U) / 100U);
shade = (uint8_t)(255U - ((p _ 125U) / 100U));
}

if (textY == buttonMsgLastY && shade == buttonMsgLastShade) {
return;
}
buttonMsgLastY = textY;
buttonMsgLastShade = shade;

const int16_t msgW = tft.width() - BUTTON_MSG_W_PADDING;
const uint16_t panelColor = tft.color565(8, 14, 20);
const uint16_t borderColor = tft.color565(40, 120, 170);
const uint16_t textColor = tft.color565(shade, shade, shade);

tft.fillRect(BUTTON_MSG_X, BUTTON_MSG_Y - 4, msgW, BUTTON_MSG_H + 10, TFT_BLACK);
tft.fillRoundRect(BUTTON_MSG_X, BUTTON_MSG_Y - 2, msgW, BUTTON_MSG_H, 4, panelColor);
tft.drawRoundRect(BUTTON_MSG_X, BUTTON_MSG_Y - 2, msgW, BUTTON_MSG_H, 4, borderColor);

const int16_t accentW = (int16_t)((msgW \* (shade - 130U)) / 125U);
if (accentW > 0) {
tft.drawFastHLine(BUTTON_MSG_X + 2, BUTTON_MSG_Y + BUTTON_MSG_H - 3, accentW - 4, TFT_CYAN);
}

tft.setTextSize(1);
tft.setTextColor(textColor, panelColor);
tft.setCursor(BUTTON_MSG_X + 7, textY);
tft.print(buttonMsgText);

}
static void updateRoboMood() {
if (!oledReady) {
return;
}

const uint32_t now = millis();
if (now - lastMoodStepMs < 3000) {
return;
}
lastMoodStepMs = now;

roboEyes.setCyclops(OFF);
roboEyes.setCuriosity(OFF);

switch (moodStep) {
case 0: roboEyes.setMood(DEFAULT); break;
case 1: roboEyes.setMood(HAPPY); break;
case 2: roboEyes.setMood(ANGRY); break;
case 3: roboEyes.setMood(TIRED); break;
case 4: roboEyes.setMood(DEFAULT); roboEyes.setCuriosity(ON); break;
default: roboEyes.setMood(DEFAULT); roboEyes.setCyclops(ON); break;
}

moodStep = (moodStep + 1) % 6;
}

static void printVolume() {
Serial.print("Volume: ");
Serial.print(volumePercent);
Serial.print("% [");

const int bars = volumePercent / 5;
for (int i = 0; i < 20; i++) {
Serial.print(i < bars ? '#' : '-');
}
Serial.println("]");
}

static void updateRotaryVolume() {
const int currentClk = digitalRead(ROTARY_CLK_PIN);
const int currentDt = digitalRead(ROTARY_DT_PIN);
bool volumeChanged = false;
int direction = 0;

// Quadrature transition decoder: robust against missed edges and direction glitches.
const uint8_t transition = (uint8_t)((lastRotaryClkState << 3) | (lastRotaryDtState << 2) | (currentClk << 1) | currentDt);
switch (transition) {
case 0b0001:
case 0b0111:
case 0b1110:
case 0b1000:
rotaryTransitionAccumulator++;
break;
case 0b0010:
case 0b0100:
case 0b1101:
case 0b1011:
rotaryTransitionAccumulator--;
break;
default:
break;
}

if (rotaryTransitionAccumulator >= 4) {
direction = 1;
rotaryTransitionAccumulator = 0;
} else if (rotaryTransitionAccumulator <= -4) {
direction = -1;
rotaryTransitionAccumulator = 0;
}

if (ROTARY_DIRECTION_INVERTED) {
direction = -direction;
}

if (direction != 0) {
const int previousVolume = volumePercent;
volumePercent = constrain(volumePercent + (direction \* ROTARY_STEP_PERCENT), 0, 100);
volumeChanged = (volumePercent != previousVolume);

    if (volumeChanged) {
      if (volumePercent > 0) {
        volumeBeforeMute = volumePercent;
      }
      Serial.print(direction > 0 ? "ROTARY UP -> " : "ROTARY DOWN -> ");
      printVolume();
    }

}
lastRotaryClkState = currentClk;
lastRotaryDtState = currentDt;

const bool swPressed = (digitalRead(ROTARY_SW_PIN) == LOW);
const uint32_t now = millis();
if (swPressed && lastRotarySwState == HIGH && (now - lastRotaryButtonMs) > ROTARY_BTN_DEBOUNCE_MS) {
const int previousVolume = volumePercent;
if (volumePercent > 0) {
volumeBeforeMute = volumePercent;
volumePercent = 0;
Serial.println("ROTARY MUTE");
} else {
volumePercent = constrain(volumeBeforeMute > 0 ? volumeBeforeMute : 50, 0, 100);
Serial.print("ROTARY UNMUTE -> ");
Serial.print(volumePercent);
Serial.println("%");
}
volumeChanged = (volumePercent != previousVolume);
if (volumeChanged) {
printVolume();
}
lastRotaryButtonMs = now;
}
lastRotarySwState = swPressed ? LOW : HIGH;

if (volumeChanged) {
drawVolumeOnTft();
}
}

void setup() {
Serial.begin(115200);
// GPIO34 is input-only and has no internal pull-up/pull-down.
// Use an external 10k pull-up to 3.3V, button to GND.

pinMode(ROTARY_CLK_PIN, INPUT_PULLUP);
pinMode(ROTARY_DT_PIN, INPUT_PULLUP);
pinMode(ROTARY_SW_PIN, INPUT_PULLUP);
lastRotaryClkState = digitalRead(ROTARY_CLK_PIN);
lastRotaryDtState = digitalRead(ROTARY_DT_PIN);
lastRotarySwState = digitalRead(ROTARY_SW_PIN);
Serial.println("Rotary volume controller ready");
printVolume();

Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
PCF.isConnected();
if (!display.begin(0x3C, true)) {
Serial.println("OLED init failed at 0x3C");
if (display.begin(0x3D, true)) {
Serial.println("OLED found at 0x3D");
oledReady = true;
}
} else {
Serial.println("OLED found at 0x3C");
oledReady = true;
}

if (oledReady) {
// Stage 1: prove raw OLED drawing works before RoboEyes starts.
display.clearDisplay();
display.setTextSize(1);
display.setTextColor(SH110X_WHITE);
display.setCursor(0, 0);
display.println("OLED init OK");
display.println("Starting RoboEyes...");
display.display();
delay(1200);

    // Stage 2: start RoboEyes animation engine.
    roboEyes.begin(128, 64, 60);
    roboEyes.setAutoblinker(ON, 2, 2);
    roboEyes.setIdleMode(ON, 2, 2);
    roboEyesEnabled = true;

}

tft.init();
tft.setRotation(1);
tft.fillScreen(TFT_BLACK);
drawVolumeOnTft(true);
// drawTftHeader("Dual Display Demo");

lastTftStepMs = millis();
lastMoodStepMs = millis();

}

void loop() {
if (oledReady && roboEyesEnabled) {
roboEyes.update();
}
updateRoboMood();
updateRotaryVolume();
updateButtonPressMessageAnimation();
// updateTftDemo();

static bool lastFlickerState = HIGH;
const uint32_t now = millis();

uint8_t up = PCF.read(BUTTON_UP);
uint8_t down = PCF.read(BUTTON_DOWN);
uint8_t left = PCF.read(BUTTON_LEFT);
uint8_t right = PCF.read(BUTTON_RIGHT);

if (PCF.lastError() != 0) {
delay(10);
return; // skip on I2C error
}

static uint8_t prevUp = HIGH;
static uint8_t prevDown = HIGH;
static uint8_t prevLeft = HIGH;
static uint8_t prevRight = HIGH;

if (prevUp == HIGH && up == LOW) {
Serial.println("UP pressed");
setButtonPressMessage("UP pressed");
}
if (prevDown == HIGH && down == LOW) {
setButtonPressMessage("DOWN pressed");
Serial.println("DOWN pressed");
}
if (prevLeft == HIGH && left == LOW) {
Serial.println("LEFT pressed");
setButtonPressMessage("LEFT pressed");
}
if (prevRight == HIGH && right == LOW) {
setButtonPressMessage("RIGHT pressed");
Serial.println("RIGHT pressed");
}

prevUp = up;
prevDown = down;
prevLeft = left;
prevRight = right;

delay(10); // small poll delay

}
