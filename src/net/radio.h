#pragma once
// ============================================================================
//  RADIO · quién usa la WiFi del ESP32 en cada momento
//
//  La radio es una sola y la quieren varios. Este módulo pone orden:
//
//  - En NORMAL la radio está en modo estación (STA). Si hay una WiFi con
//    internet configurada, se mantiene conectado a ella (para los zaps de
//    Nostr, LNbits, el minero...). Además, encima de eso, "peer cercano"
//    usa ESP-NOW en el mismo canal.
//
//  - En EXCLUSIVO alguien necesita la radio entera para él: el handshake
//    (ESP-NOW en el canal 1), el SETUP (punto de acceso propio) o el Seed Lab
//    (radio apagada). Al pedirlo se suelta la WiFi y se apaga todo; al
//    terminar, se vuelve a NORMAL y se reconecta solo.
//
//  La conexión a la WiFi la lleva una tarea aparte, para no congelar nunca
//  la pantalla mientras conecta.
// ============================================================================
#include <Arduino.h>

namespace radio {

enum class Status : uint8_t {
  Exclusive,     // la tiene el handshake, el SETUP o el Seed Lab
  NoNetwork,     // no hay WiFi con internet configurada (la radio está, pero sin conectar)
  Connecting,
  Online,        // conectado y con IP
};

void begin();
void reloadNetwork();          // volver a leer la WiFi guardada (después de cambiarla)

// Pedir la radio entera (true) o devolverla (false). Al pedirla espera a que esté apagada.
void exclusive(bool on);

Status status();
bool   online();
bool   hasNetwork();

}  // namespace radio
