#pragma once
// ============================================================================
//  ALIMENTACIÓN · USB, batería y el encendido de la pantalla
//
//  - USB:     GPIO10 ve los 5 V del USB divididos entre 2.
//  - Batería: GPIO4 ve la batería dividida entre 2 (divisor del T-Display).
//             Sin batería conectada y con USB, ahí aparecen los ~4,2 V del
//             cargador, así que la lectura solo tiene sentido con batería.
// ============================================================================
#include <Arduino.h>

namespace power {

void begin();             // enciende la pantalla y prepara las lecturas

uint16_t usbMillivolts();
bool     usbConnected();

uint16_t batteryMillivolts();
// Con USB, qué pasa con la batería. Con el cable no se lee la celda sino lo que
// da el cargador (medido en el #980): cargando ~4,58 V, y tanto llena como con
// el interruptor en OFF (que corta también la carga) ~4,72 V. Esos dos se
// distinguen por la historia: si el badge ha funcionado con batería desde que
// arrancó, el interruptor está en ON y es que está llena; si arrancó ya con el
// USB y nunca ha visto la batería, es que está en OFF (o no hay batería).
enum class Charge : uint8_t { OnBattery, Charging, Full, NoBattery };
Charge   chargeState();
bool     charging();          // chargeState() == Charging
uint8_t  batteryPercent();   // con USB, estimado por tiempo; UNKNOWN_PERCENT si no se sabe
constexpr uint8_t UNKNOWN_PERCENT = 255;   // aproximado, por la curva típica de una LiPo

}  // namespace power
