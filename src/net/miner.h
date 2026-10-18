#pragma once
// ============================================================================
//  MINER · el modo NerdMiner del badge
//
//  Envuelve lib/nerdminer (el núcleo del NerdMiner_v2) con lo que pone el
//  badge: el pool y la dirección se guardan en la memoria (badge.local o el
//  comando serie "G").
//
//  Cuándo mina lo decide la máquina de estados (app/state_machine, minerPolicy):
//  solo mientras se mira la pantalla del NerdMiner.
//
//  De fábrica mina sin tocar nada: al pool de NerdMiners de Bitronics y a la
//  dirección de Bitronics (worker "nerdminerBadge"). Si el dueño pone la suya
//  en badge.local, mina para él; si la borra, vuelve a la de fábrica. Necesita internet (net/radio): durante el handshake, el SETUP o
//  el Seed Lab se queda esperando y sigue después solo.
//
//  Gasta: el acelerador SHA del chip va a tope. Mejor enchufado por USB.
// ============================================================================
#include <Arduino.h>
#include <nerdminer.h>

namespace miner {

constexpr const char* DEFAULT_POOL    = "pool.bitronics.store";   // el pool de NerdMiners (dificultad fraccionaria)
constexpr uint16_t    DEFAULT_PORT    = 3334;
constexpr const char* DEFAULT_ADDRESS = "bc1qcnwjerakcqn5etns97ma3vfk9e4mp66ghscmft";
constexpr const char* WORKER          = "nerdminerBadge";

void begin();                 // lee la configuración y, si estaba minando, sigue

void setConfig(const String& pool, uint16_t port, const String& address);
String   pool();
uint16_t port();
String   address();           // la que cobra: la del dueño o, si no puso ninguna, la de fábrica
String   ownAddress();        // la que puso el dueño ("" = ninguna)
bool     defaultAddress();    // true si está minando para la dirección de fábrica

void run(bool on);            // arrancar o parar el núcleo (no se guarda)
bool running();

nerdminer::Stats stats();

}  // namespace miner
