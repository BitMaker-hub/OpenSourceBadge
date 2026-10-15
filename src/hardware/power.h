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
uint8_t  batteryPercent();   // aproximado, por la curva típica de una LiPo

}  // namespace power
