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
// Con USB: ¿hay una batería cargándose? Medido en el #980 (2026-10-21): sin
// batería (o con el interruptor que la corta) GPIO4 lee ~4,72 V, y cargando
// ~4,58 V (con el cable no se lee la celda). Corte en 4,65 V; si al llenarse
// pasa de ahí, sale "USB", que también es verdad: ya no está cargando.
bool     charging();
uint8_t  batteryPercent();   // aproximado, por la curva típica de una LiPo

}  // namespace power
