#pragma once
// ============================================================================
//  SETUP PORTAL · la WiFi propia del badge y la página badge.local
//
//  Mientras el badge está en modo SETUP:
//    - levanta su WiFi "BADGE-042" con contraseña 21000000 (solo números: se
//      teclea rápido en el móvil y se lee bien en la pantalla),
//    - responde a cualquier dirección con su página (portal cautivo: el móvil
//      la abre solo al conectarse) y también en http://badge.local,
//    - guarda lo que le manden en el perfil.
// ============================================================================
#include <Arduino.h>

namespace portal {

constexpr const char* PASSWORD = "21000000";

void start();
void stop();
void loop();            // atender al móvil; llamar en cada vuelta mientras dure el SETUP

bool savedSinceLastAsk();   // true una vez después de cada guardado
int  connectedPhones();

}  // namespace portal
