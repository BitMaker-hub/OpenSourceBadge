#pragma once
// ============================================================================
//  MINER · el modo NerdMiner del badge
//
//  Envuelve lib/nerdminer (el núcleo del NerdMiner_v2) con lo que pone el
//  badge: el pool y la dirección se guardan en la memoria (badge.local o el
//  comando serie "G"), y si estaba minando al apagarlo, vuelve a minar al
//  encenderlo. Necesita internet (net/radio): durante el handshake, el SETUP o
//  el Seed Lab se queda esperando y sigue después solo.
//
//  Gasta: el acelerador SHA del chip va a tope. Mejor enchufado por USB.
// ============================================================================
#include <Arduino.h>
#include <nerdminer.h>

namespace miner {

constexpr const char* DEFAULT_POOL = "public-pool.io";
constexpr uint16_t    DEFAULT_PORT = 21496;

void begin();                 // lee la configuración y, si estaba minando, sigue

void setConfig(const String& pool, uint16_t port, const String& address);
String   pool();
uint16_t port();
String   address();           // "" = aún no hay dirección: no se puede minar

bool start();                 // false si falta la dirección
void stop();
bool enabled();               // el usuario lo ha encendido

nerdminer::Stats stats();

}  // namespace miner
