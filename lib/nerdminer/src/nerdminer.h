#pragma once
// ============================================================================
//  NERDMINER · minar Bitcoin en solitario con el badge
//
//  El núcleo es el del NerdMiner_v2: un trabajador habla con el pool por
//  stratum y otro calcula SHA-256 con el acelerador por hardware del ESP32-S3.
//  Es lotería pura: con unos cientos de kH/s, encontrar un bloque es casi
//  imposible... pero no imposible, y si pasa, el bloque entero es tuyo.
//
//  Uso:
//    nerdminer::start({"public-pool.io", 21496, "bc1q...", "x"}, hayInternet);
//    nerdminer::stats();   // hashrate, shares, mejor dificultad...
//    nerdminer::stop();
// ============================================================================
#include <Arduino.h>

namespace nerdminer {

struct Config {
  String   poolHost;
  uint16_t poolPort;
  String   address;      // la dirección Bitcoin que cobra si sale un bloque
  String   password;     // casi todos los pools aceptan "x"
};

struct Stats {
  bool     running;      // minando (aunque esté esperando al pool)
  bool     connected;    // conectado al pool y con trabajo
  float    khs;          // kilohashes por segundo (media de los últimos segundos)
  uint32_t accepted;     // shares que el pool ha aceptado (los que cuenta la web del pool)
  uint32_t rejected;     // shares que el pool ha rechazado (tarde, repetidos...)
  uint32_t shares;       // de los aceptados, los de 32 bits (lo que el NerdMiner original llama "32BITS SHARES")
  uint32_t blocks;       // bloques encontrados (ojalá algún día sea 1)
  double   bestDiff;     // la mejor dificultad conseguida
  uint32_t templates;    // trabajos recibidos del pool
  uint32_t lastJobAt;    // millis() del último trabajo nuevo (0 = ninguno)
  uint32_t lastShareAt;  // millis() del último share aceptado (0 = ninguno)
  double   poolDiff;     // la dificultad que pide el pool para aceptar un share
  uint32_t minedMs;      // tiempo minando con trabajo del pool desde que se encendió
};

// online(): cómo sabe el minero si hay internet (en el badge lo dice net/radio)
// socketOpen(true/false): avisa al abrir y al cerrar la conexión con el pool (puede ser nullptr)
void start(const Config& config, bool (*online)(), void (*socketOpen)(bool) = nullptr);
void stop();
Stats stats();

}  // namespace nerdminer
