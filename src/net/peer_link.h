#pragma once
// ============================================================================
//  PEER LINK · el handshake entre dos badges por ESP-NOW
//
//  ESP-NOW manda mensajes cortos de ESP32 a ESP32 sin router ni WiFi. Solo se
//  enciende la radio mientras dura un handshake, para ahorrar batería.
//
//  Cómo se ponen de acuerdo dos badges (los dos han pulsado PEER):
//
//    1. Cada uno grita "HOLA, soy X" a todo el que escuche, cada 150 ms.
//    2. El primero que oyes es tu candidato. A partir de ahí tus HOLA llevan
//       "te oigo, Y".
//    3. Cuando Y te dice "te oigo, X", los dos sabéis que el otro os oye:
//       handshake hecho. Os quedáis con el perfil del otro.
//    4. El bloque que "minan" juntos es el más alto de los dos más uno, así los
//       dos badges apuntan el mismo número de bloque.
//
//  Si uno de los dos sabe la hora y el otro no, se la pasa.
// ============================================================================
#include "../app/badge.h"

namespace peerlink {

enum class Status : uint8_t {
  Searching,   // gritando, nadie a la vista
  Found,       // oímos a alguien, esperando a que él nos oiga
  Done,        // nos oímos los dos: tenemos peer
};

void start();                // enciende la radio y empieza a gritar
void stop();                 // apaga la radio
void loop();                 // llamar en cada vuelta mientras dure el handshake

Status status();
const Peer& other();         // el otro badge (válido desde Found)
uint32_t agreedBlock();      // el bloque acordado (válido en Done)

}  // namespace peerlink
