#pragma once
// ============================================================================
//  QWIIC · el conector I2C de la parte de atrás (J5)
//
//  De momento solo busca qué hay conectado, para la prueba de placa.
// ============================================================================
#include <Arduino.h>

namespace qwiic {

void begin();

// Busca dispositivos y guarda hasta `max` direcciones. Devuelve cuántos hay.
uint8_t scan(uint8_t* found, uint8_t max);

}  // namespace qwiic
