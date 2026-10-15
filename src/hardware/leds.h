#pragma once
// ============================================================================
//  LEDS · los 20 WS2812B de la placa
//
//  Este módulo solo sabe "pon este LED de este color" y "manda los colores".
//  Los efectos (estela, parpadeo, respiración...) están en effects/led_effects
//  y trabajan sobre GRUPOS de LEDs, así el mismo efecto sirve para la B, para
//  TX/RX o para cualquier grupo nuevo que se defina aquí.
//
//  Al mandar los colores se hacen dos cosas por seguridad:
//    1. Se aplica el brillo general.
//    2. Se calcula el consumo y, si pasa de LED_MAX_CURRENT_MA, se baja todo
//       en proporción. Ningún efecto puede quemar el regulador del T-Display.
//  Si todos están apagados se corta la alimentación de los LEDs (Q1), porque
//  cada WS2812 gasta ~1 mA aunque esté en negro.
//
//  Cadena de la placa (LEDn de la serigrafía = índice n-1):
//
//        17 18            LED17/18: rayitas de arriba de la ₿
//      5  6  7            LED1-5:   palo vertical, de abajo arriba
//      4        8         LED6-9:   panza de arriba
//      3  10 9            LED10:    el centro
//      2       11 12      LED11-13: panza de abajo
//      1  14  13          LED14:    abajo en medio
//        16 15            LED15/16: rayitas de abajo
//                         LED19 = TX, LED20 = RX (arriba a la derecha)
// ============================================================================
#include <Arduino.h>

using Color = uint32_t;   // 0xRRGGBB

// Colores con nombre, para que los efectos se lean solos
namespace colors {
  constexpr Color Off      = 0x000000;
  constexpr Color Orange   = 0xF7931A;   // naranja bitcoin
  constexpr Color Amber    = 0xFFD29A;
  constexpr Color Gold     = 0xFFC93C;   // lightning
  constexpr Color Cream    = 0xFFF2B0;
  constexpr Color Purple   = 0x7C3AED;   // nostr
  constexpr Color Blue     = 0x38BDF8;
  constexpr Color Green    = 0x22C55E;
  constexpr Color DarkGreen= 0x0C3B1D;
  constexpr Color Red      = 0xF87171;
  constexpr Color White    = 0xFFFFFF;
  constexpr Color Ember    = 0x5A3410;   // naranja muy apagado, para el reposo
}

// Un grupo es una lista de LEDs en el orden en que los recorren los efectos
struct LedGroup {
  const uint8_t* index;
  uint8_t count;
};

namespace groups {
  extern const LedGroup B;              // los 18 de la ₿, en orden de la cadena
  extern const LedGroup BOutline;       // el contorno de la B como un circuito cerrado
  extern const LedGroup BTicks;         // las 4 rayitas de arriba y abajo
  extern const LedGroup BTopToBottom;   // la ₿ ordenada de arriba abajo
  extern const LedGroup Tx;
  extern const LedGroup Rx;
  extern const LedGroup TxRx;
  extern const LedGroup All;
}

namespace leds {

void begin();

void set(uint8_t index, Color c);
Color get(uint8_t index);
void clear();                        // todo en negro (no manda nada aún)

void setBrightness(uint8_t level);   // 0..255
uint8_t brightness();

// Manda los colores a la placa, con el brillo y el límite de consumo aplicados
void show();

// Consumo estimado del último show(), en mA (para la prueba de placa)
uint16_t lastCurrentMa();

// Mezcla dos colores: amount = 0 -> a, 255 -> b
Color blend(Color a, Color b, uint8_t amount);
// Escala un color: level = 255 lo deja igual, 0 lo apaga
Color dim(Color c, uint8_t level);

}  // namespace leds
