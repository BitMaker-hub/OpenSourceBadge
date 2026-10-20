#include "power.h"
#include "../config.h"

namespace power {

void begin() {
  // Con batería, el T-Display no alimenta la pantalla hasta que GPIO15 está alto.
  // Si se olvida, con USB funciona y con batería parece que la placa está muerta.
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  analogReadResolution(12);
}

uint16_t usbMillivolts()     { return analogReadMilliVolts(PIN_USB_SENSE) * 2; }
bool     usbConnected()      { return usbMillivolts() > 4000; }
uint16_t batteryMillivolts() { return analogReadMilliVolts(PIN_BATTERY_SENSE) * 2; }
bool     charging()          { return usbConnected() && batteryMillivolts() < 4400; }

uint8_t batteryPercent() {
  // Tensión en reposo de una LiPo frente a la carga que le queda
  static const uint16_t MV[]  = {3300, 3600, 3700, 3750, 3800, 3900, 4000, 4100, 4200};
  static const uint8_t  PCT[] = {   0,    5,   15,   25,   40,   60,   75,   90,  100};
  const uint16_t mv = batteryMillivolts();

  if (mv <= MV[0]) return 0;
  for (int i = 1; i < 9; i++) {
    if (mv <= MV[i]) return PCT[i - 1] + (PCT[i] - PCT[i - 1]) * (mv - MV[i - 1]) / (MV[i] - MV[i - 1]);
  }
  return 100;
}

}  // namespace power
