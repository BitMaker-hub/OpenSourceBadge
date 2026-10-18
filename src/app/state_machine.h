#pragma once
// ============================================================================
//  MÁQUINA DE ESTADOS · qué está haciendo el badge en cada momento
//
//  El badge siempre está en UN estado. Cada estado tiene su función, que en
//  cada vuelta del loop hace tres cosas, siempre en este orden:
//
//    1. Mira los botones (y el tiempo) y decide si hay que cambiar de estado.
//    2. Enciende los LEDs que tocan.
//    3. Dibuja su pantalla.
//
//  Para cambiar de estado se llama a go(NuevoEstado). Eso apunta la hora, para
//  que el estado nuevo sepa cuánto tiempo lleva (la `t` de cada función), y lo
//  escribe por el puerto serie para poder seguirlo.
//
//    BOOT ──> PROFILE ──MODE──> QR_CODES ──MODE──> PEERS ──MODE──> STATS ──MODE──┐
//                ^                                                             │
//                └─────────────────────────────────────────────────────────────┘
//    Desde cualquiera de las cuatro páginas:
//      PEER corto  -> HANDSHAKE (buscar, encontrar, minar, minado) y vuelve
//      PEER largo  -> QR_FULLSCREEN, y PEER vuelve
//      MODE largo  -> SETUP (WiFi y badge.local), y MODE vuelve
//      SEED corto  -> SEED_LAB, y SEED vuelve
//      SEED largo  -> HARDWARE_TEST (prueba de placa)
//    Un zap que llega -> ZAP, y vuelve solo a los 6 s
//    Otro badge que nos invita (pulsó PEER a nuestro lado) -> INVITE:
//      PEER acepta (-> HANDSHAKE), MODE la ignora
//
//  En las páginas de inicio, si hay un badge pegado al nuestro, sale un aviso
//  abajo ("Satoshi is nearby") y TX/RX laten en morado (ver net/nearby).
//
//  Si al encender se tiene pulsado SEED, se arranca directamente en la prueba
//  de placa.
// ============================================================================
#include "../hardware/buttons.h"

enum class State : uint8_t {
  Boot,
  Profile,
  QrCodes,
  Peers,
  Stats,
  QrFullscreen,
  Handshake,
  Invite,          // otro badge nos invita a hacer handshake
  Zap,
  SeedLab,
  Setup,
  HardwareTest,
};

namespace app {

void begin();
void update(ButtonEvent event);   // una vez por vuelta del loop

State current();
const char* stateName(State s);

// Para probar sin radio: hace como si acabara de llegar un zap
void simulateZap();

}  // namespace app
