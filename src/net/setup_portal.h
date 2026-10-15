#pragma once
// ============================================================================
//  SETUP PORTAL · la WiFi propia del badge y la página badge.local
//
//  Mientras el badge está en modo SETUP:
//    - levanta su WiFi "BADGE-042", abierta y sin contraseña: solo está
//      encendida mientras su dueño tiene el badge en SETUP (como mucho 5 min),
//    - responde a cualquier dirección con su página (portal cautivo: el móvil
//      la abre solo al conectarse) y también en http://badge.local,
//    - guarda lo que le manden en el perfil.
// ============================================================================
#include <Arduino.h>

namespace portal {

// Lo que lleva el QR para que el móvil se conecte solo a la WiFi del badge
String wifiQr();

void start();
void stop();
void loop();            // atender al móvil; llamar en cada vuelta mientras dure el SETUP

bool savedSinceLastAsk();   // true una vez después de cada guardado
int  connectedPhones();

}  // namespace portal
