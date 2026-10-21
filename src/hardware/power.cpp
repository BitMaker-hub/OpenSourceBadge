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
// Sin USB, a este pin le llega la batería por detrás (hasta 4,2 V con la batería
// llena: con el corte en 4 V, sin cable decía que había USB). Con USB lee
// 4,55-4,7 V. Y si hay un ordenador al otro lado, el propio USB del chip lo sabe.
bool     usbConnected()      { return usbMillivolts() > 4450 || HWCDC::isConnected(); }
uint16_t batteryMillivolts() { return analogReadMilliVolts(PIN_BATTERY_SENSE) * 2; }
bool     charging()          { return usbConnected() && batteryMillivolts() < 4650; }

// Lo que lee el badge con carga frente al tiempo que le queda. Medido en el #980
// con la EEMB LP603449 de 1100 mAh (2026-10-22): 4 h 58 min en el perfil, sin
// ahorro, ~220 mA. La lectura con carga va más baja que la celda en reposo:
// llena da ~3,94 V y se apaga hacia 3,0 V (cae en picado desde 3,25 V).
// El porcentaje es el tiempo que le queda, no la tensión.
static uint8_t percentFor(float mv) {
  static const uint16_t MV[]  = {3050, 3250, 3335, 3420, 3462, 3497, 3541, 3596, 3675, 3772, 3882, 3940};
  static const uint8_t  PCT[] = {   0,    5,   10,   20,   30,   40,   50,   60,   70,   80,   90,  100};
  if (mv <= MV[0]) return 0;
  for (int i = 1; i < 12; i++) {
    if (mv <= MV[i]) return PCT[i - 1] + (PCT[i] - PCT[i - 1]) * (mv - MV[i - 1]) / (MV[i] - MV[i - 1]);
  }
  return 100;
}

uint8_t batteryPercent() {
  // Con la radio y la pantalla la lectura baila unos mV, y el número saltaba
  // 36-37-36 sin parar. Por eso: una media lenta (una lectura cada medio
  // segundo, que pesa 1/8) y el número solo cambia si se mueve 2 puntos o más.
  static float    mv = 0;
  static uint32_t lastAt = 0;
  static uint8_t  shown = 255;
  if (mv == 0 || millis() - lastAt >= 500) {
    const float now = batteryMillivolts();
    mv = mv == 0 ? now : mv + (now - mv) / 8;
    lastAt = millis();
  }
  const uint8_t pct = percentFor(mv);
  if (shown == 255 || abs((int)pct - (int)shown) >= 2 || pct == 0 || pct == 100) shown = pct;
  return shown;
}

}  // namespace power
