#pragma once
// ============================================================================
//  NOSTR ZAPS · el badge se entera de los zaps que te hacen en Nostr
//
//  Cuando alguien te hace un zap, tu wallet publica en los relays de Nostr un
//  "recibo de zap" (evento de tipo 9735) que dice a quién va, cuánto y con qué
//  mensaje. El badge se conecta a internet con la WiFi que le pongas en
//  badge.local, se suscribe en un par de relays a los recibos dirigidos a tu
//  npub y, cuando llega uno, lo celebra.
//
//  Los sats van a tu wallet de siempre: el badge no guarda nada, solo escucha.
//
//  Todo esto corre en una tarea aparte (núcleo 0), para que conectarse a un
//  relay nunca congele la pantalla. La WiFi en sí la lleva net/radio.
// ============================================================================
#include "../app/badge.h"

namespace nostrzaps {

enum class Status : uint8_t {
  NotConfigured,    // falta la WiFi o el npub
  Paused,           // la radio la está usando el SETUP o el handshake
  ConnectingWifi,
  ConnectingRelay,
  Listening,        // conectado a algún relay, esperando zaps
};

void begin();          // lee el npub y arranca la tarea (la WiFi la lleva net/radio)
void reload();         // volver a leer el npub (después de guardar en badge.local)

Status status();

// Si ha llegado un zap, lo copia en `z` y devuelve true
bool nextZap(Zap& z);

// Para pruebas: procesa un mensaje de relay como si hubiera llegado por internet
// (comando serie "J" seguido del JSON en una línea)
void injectForTest(const String& relayMessage);

}  // namespace nostrzaps
