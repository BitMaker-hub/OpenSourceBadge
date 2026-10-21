#include "backlight.h"

namespace backlight {

static const uint8_t  PIN = 38;           // TFT_BL del T-Display-S3
static const uint8_t  CHANNEL = 7;        // canal LEDC que no usa nadie más
static const uint8_t  FULL = 255, NORMAL = 110, DIM = 20;
static const uint32_t DIM_AFTER_MS = 20000, OFF_AFTER_MS = 60000;

static uint32_t lastActivity = 0;
static uint8_t  level = 255;

static void set(uint8_t l) {
  if (l == level) return;
  level = l;
  ledcWrite(CHANNEL, l);
}

void begin() {
  ledcSetup(CHANNEL, 5000, 8);   // 5 kHz: no se ve parpadeo ni se oye
  ledcAttachPin(PIN, CHANNEL);
  level = 0;
  set(FULL);
  lastActivity = millis();
}

bool activity() {
  const bool wasOff = level == 0;
  lastActivity = millis();
  return wasOff;
}

void loop(bool usb) {
  const uint32_t idle = millis() - lastActivity;
  if (usb)                       set(FULL);
  else if (idle < DIM_AFTER_MS)  set(NORMAL);
  else if (idle < OFF_AFTER_MS)  set(DIM);
  else                           set(0);
}

bool isOff() { return level == 0; }

}  // namespace backlight
