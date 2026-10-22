#include "power.h"
#include "../config.h"
#include <Preferences.h>

namespace power {

// Lo que se sabe de la batería entre encendidos: si alguna vez la ha tenido y
// cuánto tenía la última vez. Con el interruptor en OFF y el USB puesto, la
// lectura es la misma que con la batería llena; esto lo desempata (chargeState).
static Preferences memory;
static bool    everHadBattery = false;
static uint8_t lastKnownPercent = 0;

static void rememberPercent(uint8_t pct) {
  // Solo si cambia 5 puntos (o llega a lleno): la flash no se gasta por nada
  if (everHadBattery && abs((int)pct - (int)lastKnownPercent) < 5 && !(pct == 100 && lastKnownPercent != 100)) return;
  everHadBattery = true;
  lastKnownPercent = pct;
  memory.putBool("bat_ever", true);
  memory.putUChar("bat_pct", pct);
}

void begin() {
  // Con batería, el T-Display no alimenta la pantalla hasta que GPIO15 está alto.
  // Si se olvida, con USB funciona y con batería parece que la placa está muerta.
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  analogReadResolution(12);
  memory.begin("badge", false);
  everHadBattery = memory.isKey("bat_ever") && memory.getBool("bat_ever");
  lastKnownPercent = memory.isKey("bat_pct") ? memory.getUChar("bat_pct") : 0;
}

uint16_t usbMillivolts()     { return analogReadMilliVolts(PIN_USB_SENSE) * 2; }
// Sin USB, a este pin le llega la batería por detrás (hasta 4,2 V con la batería
// llena: con el corte en 4 V, sin cable decía que había USB). Con USB lee
// 4,55-4,7 V. Y si hay un ordenador al otro lado, el propio USB del chip lo sabe.
bool     usbConnected()      { return usbMillivolts() > 4450 || HWCDC::isConnected(); }
uint16_t batteryMillivolts() { return analogReadMilliVolts(PIN_BATTERY_SENSE) * 2; }
Charge chargeState() {
  static bool     seenBattery = false;   // ¿ha funcionado con batería, o la ha visto cargando, desde que arrancó?
  static uint32_t signalSince = 0;       // desde cuándo hay señal de batería, sin cortes
  // Al encender, las lecturas salen bajas unos segundos (~4,45 V con USB y sin
  // batería): el #308, sin batería, se creía "lleno". Así que la batería solo
  // cuenta como vista pasados 15 s del arranque y con la señal 5 s seguidos.
  auto battery = [&](bool signal) {
    if (!signal) { signalSince = 0; return; }
    if (signalSince == 0) signalSince = millis();
    if (millis() > 15000 && millis() - signalSince > 5000) seenBattery = true;
  };

  if (!usbConnected()) {
    battery(true);
    return Charge::OnBattery;
  }
  // Media de 4 lecturas: una sola, con un pico de la radio, bailaba entre cargando y llena
  uint32_t sum = 0;
  for (int i = 0; i < 4; i++) sum += batteryMillivolts();
  const bool low = sum / 4 < 4650;
  battery(low);
  if (low) return Charge::Charging;
  if (seenBattery) return Charge::Full;
  // Arrancó ya enchufado y la lectura es alta: o está llena, o el interruptor
  // está en OFF (o no hay batería). Si la última vez estaba llena, se da por
  // llena (y si fuese el interruptor, da igual: no le hacía falta cargar). Si
  // estaba a medias, cargando leería menos de 4,65 V: es que no está cargando.
  return everHadBattery && lastKnownPercent >= 90 ? Charge::Full : Charge::NoBattery;
}

bool charging() { return chargeState() == Charge::Charging; }

// Lo que lee el badge con carga frente al tiempo que le queda, con la EEMB
// LP603449 de 1100 mAh del #980. Primera medida (2026-10-22, sin ahorro, ~220 mA):
// 4 h 58 min. Segunda (2026-10-23, con la luz que se apaga y CPU a 80 MHz,
// ~117 mA): 9 h 23 min, de 4,09 V a 3,15 V (el corte). Esta tabla es la de la
// segunda, que es el uso de verdad; con más consumo la tensión va más baja y el
// porcentaje sale algo por debajo (mejor eso que al revés).
// El porcentaje es el tiempo que le queda, no la tensión.
static uint8_t percentFor(float mv) {
  static const uint16_t MV[]  = {3150, 3346, 3400, 3472, 3513, 3556, 3587, 3652, 3728, 3825, 3934, 4060};
  static const uint8_t  PCT[] = {   0,    5,   10,   20,   30,   40,   50,   60,   70,   80,   90,  100};
  if (mv <= MV[0]) return 0;
  for (int i = 1; i < 12; i++) {
    if (mv <= MV[i]) return PCT[i - 1] + (PCT[i] - PCT[i - 1]) * (mv - MV[i - 1]) / (MV[i] - MV[i - 1]);
  }
  return 100;
}

uint8_t batteryPercent() {
  static float    mv = 0;          // media lenta de la tensión con batería
  static uint32_t lastAt = 0;
  static uint8_t  shown = 255;     // lo que se enseña (con zona muerta)
  static float    estimate = -1;   // cargando: el porcentaje estimado por tiempo
  static uint32_t estimateAt = 0;
  static bool     wasUsb = false;
  const Charge c = chargeState();
  const uint32_t now = millis();

  if (c == Charge::OnBattery) {
    // Con la radio y la pantalla la lectura baila unos mV, y el número saltaba
    // 36-37-36 sin parar. Por eso: una media lenta (una lectura cada medio
    // segundo, que pesa 1/8) y el número solo cambia si se mueve 2 puntos o más.
    if (wasUsb) { mv = 0; shown = 255; }      // recién desenchufado: la media empieza de cero
    wasUsb = false;
    if (mv == 0 || now - lastAt >= 500) {
      const float read = batteryMillivolts();
      mv = mv == 0 ? read : mv + (read - mv) / 8;
      lastAt = now;
    }
    const uint8_t pct = percentFor(mv);
    if (shown == 255 || abs((int)pct - (int)shown) >= 2 || pct == 0 || pct == 100) shown = pct;
    estimate = shown;
    estimateAt = now;
    if (now > 20000) rememberPercent(shown);   // pasados los primeros segundos, que la media aún se asienta
    return shown;
  }

  // Con USB la tensión no dice cuánto lleva (medido en el #980): mientras carga
  // la lectura se queda plana en ~4,58 V y solo al final salta a ~4,73 V. Así
  // que se estima por tiempo, desde el último porcentaje con batería: ~1 % por
  // minuto (de vacía al 65 % en 64 min con el badge encendido), y en el tramo
  // final, despacio hasta el 100 %.
  wasUsb = true;
  const float minutes = (now - estimateAt) / 60000.0f;
  estimateAt = now;
  if (c == Charge::NoBattery) return 0;
  if (estimate < 0) {                                        // arrancó con USB: no sabemos de dónde parte
    if (c == Charge::Charging) return UNKNOWN_PERCENT;       // mejor no decir nada que inventárselo
    estimate = max(90, (int)lastKnownPercent);              // llena: parte de lo que tenía la última vez
  }
  // Nunca hacia abajo: una lectura suelta por debajo del corte no puede devolverlo al 95
  if (c == Charge::Charging) estimate = max(estimate, min(estimate + minutes * 1.0f, 95.0f));
  else                       estimate = min(max(estimate, 90.0f) + minutes * 0.5f, 100.0f);
  rememberPercent((uint8_t)estimate);
  return (uint8_t)estimate;
}

}  // namespace power
