#pragma once
// ============================================================================
//  BOTONES · convierte las pulsaciones en eventos
//
//  El resto del firmware no lee pines: pregunta "¿qué ha pasado?" y recibe un
//  evento, por ejemplo MODE corto o PEER largo. Así la máquina de estados solo
//  piensa en eventos y no en rebotes ni milisegundos.
//
//  - Corta: se suelta antes de BUTTON_LONG_MS. Se avisa al soltar.
//  - Larga: se avisa en cuanto se cumple el tiempo, sin esperar a soltar,
//           para que se note la respuesta mientras aún tienes el dedo encima.
// ============================================================================
#include <Arduino.h>

enum class Button : uint8_t { Peer, Mode, Seed, Count };

enum class ButtonEvent : uint8_t {
  None,
  PeerShort, PeerLong,
  ModeShort, ModeLong,
  SeedShort, SeedLong,
};

namespace buttons {

void begin();

// Llamar una vez por vuelta del loop. Devuelve un evento o ButtonEvent::None.
ButtonEvent poll();

// Estado instantáneo (para la pantalla de prueba de placa)
bool isDown(Button b);
bool isBootDown();      // botón BOOT del T-Display
bool isKeyDown();       // botón KEY del T-Display

const char* eventName(ButtonEvent e);

}  // namespace buttons
