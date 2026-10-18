#pragma once
// ============================================================================
//  BITCOIN INFO · el precio y el estado de la red, de mempool.space
//
//  Lo que enseña la segunda pantalla del NerdMiner, como las pantallas del
//  NerdMiner de verdad: precio, bloque actual, comisiones, dificultad y
//  hashrate de la red. Del bloque sale cuánto falta para el halving.
//
//  Solo pregunta mientras alguien lo pide con want(true) (la pantalla del
//  minero), cada minuto, y suelta la conexión cuando net/radio lo pide.
// ============================================================================
#include <Arduino.h>

namespace btcinfo {

struct Info {
  bool     valid = false;      // false hasta la primera respuesta buena
  uint32_t priceUsd = 0;
  uint32_t height = 0;         // bloque actual
  float    feeFast = 0;        // sat/vB para el siguiente bloque (puede ser menos de 1)
  float    feeHour = 0;        // sat/vB para dentro de una hora
  double   difficulty = 0;
  double   hashrate = 0;       // de la red, en H/s
  uint32_t updatedAt = 0;      // millis() de la última respuesta
};

constexpr uint32_t HALVING_INTERVAL = 210000;

void begin();
void want(bool on);            // la pantalla que lo enseña está a la vista
Info get();

// Del bloque actual: cuánto llevamos del periodo hasta el siguiente halving
inline uint32_t blocksToHalving(uint32_t height) { return HALVING_INTERVAL - height % HALVING_INTERVAL; }
inline float    halvingProgress(uint32_t height) { return (height % HALVING_INTERVAL) / (float)HALVING_INTERVAL; }

}  // namespace btcinfo
