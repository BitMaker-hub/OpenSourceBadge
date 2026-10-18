#pragma once
// ============================================================================
//  NEARBY · qué badges hay cerca, sin que nadie pulse nada
//
//  Cada badge grita "estoy aquí" (su número, nombre y personaje) una vez por
//  segundo por ESP-NOW. Un mensaje así dura menos de un milisegundo. Con la
//  fuerza de la señal (RSSI) sabemos si el otro está pegado a nosotros.
//
//  El canal: ESP-NOW solo oye a los badges que están en el mismo canal de
//  radio. Los que están conectados a una WiFi están en el canal de esa WiFi.
//  Los que no tienen WiFi van saltando de canal hasta que oyen a alguien y se
//  quedan en el suyo. Para que dos sin WiFi se encuentren rápido, uno de cada
//  dos saltos es al canal 1.
//
//  Además de "estoy aquí" hay "INVITO": al pulsar PEER con alguien cerca, se
//  le avisa para que en su pantalla salga la invitación.
//
//  Solo funciona con la radio en NORMAL (ver net/radio). Durante el handshake,
//  el SETUP o el Seed Lab está parado.
// ============================================================================
#include "../app/badge.h"

namespace nearby {

constexpr int8_t CLOSE_RSSI = -60;   // a partir de aquí, "pegado" (palmo o dos)

struct Neighbor {
  uint8_t  mac[6];
  uint16_t number;
  Role     role;
  char     name[NAME_LEN + 1];
  int8_t   rssi;          // suavizado
  uint32_t lastSeen;      // millis()
  bool     close;         // RSSI por encima de CLOSE_RSSI
};

void begin();
void loop();              // llamar en cada vuelta del loop

// Para el handshake, el SETUP y el Seed Lab: suelta ESP-NOW antes de que se lleven la radio
void stop();

// El badge más cerca de los que están "pegados" (o nullptr si no hay ninguno)
const Neighbor* closest();
int             count();  // cuántos se oyen ahora mismo (cerca o no)

// Avisar a `who` de que queremos hacer handshake. Se manda tres veces por si se pierde.
void invite(const Neighbor& who);

// Si alguien nos ha invitado, lo copia en `from` y devuelve true (una sola vez por invitación)
bool takeInvite(Neighbor& from);

// El canal de radio en el que estamos (para la pantalla de prueba)
uint8_t channel();

}  // namespace nearby
