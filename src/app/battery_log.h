#pragma once
// ============================================================================
//  BATTERY LOG · cuánto dura la batería, medido de verdad
//
//  Con el cable USB puesto la batería se está cargando y no se puede medir lo
//  que gasta el badge. Por eso el badge apunta él solo, cada 30 segundos, la
//  tensión de la batería, si hay USB y qué está haciendo, en un fichero de su
//  memoria (LittleFS). Se desenchufa, se usa, se vuelve a enchufar y se lee:
//
//    B  -> vuelca el registro por el puerto serie (CSV)
//    b  -> lo borra
//
//  Cada línea: arranque, segundos desde ese arranque, mV de batería, mV del
//  USB, estado, radio, minando (1/0). Sobrevive a los reinicios (cada arranque
//  tiene su número), así que también se ve si se apagó por batería baja.
// ============================================================================
#include <Arduino.h>

namespace batterylog {

void begin();
void loop(const char* stateName);   // apunta una línea cuando toca
void dump();
void clear();

}  // namespace batterylog
