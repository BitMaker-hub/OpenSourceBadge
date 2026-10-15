#pragma once
// ============================================================================
//  STORAGE · lo que se guarda aunque se apague el badge
//
//  - El perfil y los ajustes van en la NVS del ESP32 (Preferences): son pocos
//    datos y se leen al arrancar.
//  - Los peers van en un fichero de LittleFS (/peers.bin), un registro fijo
//    por peer. Pueden ser cientos y así no llenamos la NVS.
// ============================================================================
#include "badge.h"

namespace storage {

void begin();

// Perfil. Devuelve false si el badge aún no se ha configurado nunca.
bool loadProfile(Profile& p);
void saveProfile(const Profile& p);

// Número de serie del badge (el "#042"). Se graba en fábrica con el comando serie "n".
uint16_t loadNumber();
void     saveNumber(uint16_t n);

// Zona horaria, en minutos respecto a UTC (la manda el móvil al configurar)
int16_t  loadTimezone();
void     saveTimezone(int16_t minutes);

// WiFi con internet para enterarse de los zaps (la del evento o la del móvil)
void loadNetwork(String& ssid, String& password);
void saveNetwork(const String& ssid, const String& password);

// Contador de bloques "minados" (handshakes hechos)
uint32_t loadBlockHeight();
void     saveBlockHeight(uint32_t h);

uint32_t loadSats();
void     saveSats(uint32_t sats);

// Peers: se leen todos al arrancar y se reescriben al cambiar
int  loadPeers(Peer* out, int max);
void savePeers(const Peer* peers, int count);

// Borra perfil, peers y contadores (para dejar un badge como nuevo)
void factoryReset();

}  // namespace storage
