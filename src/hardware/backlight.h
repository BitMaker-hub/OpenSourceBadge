#pragma once
// ============================================================================
//  BACKLIGHT · la luz de la pantalla, lo que más gasta después de la radio
//
//  Con USB, siempre a tope. Con batería:
//    - al tocar un botón (o si pasa algo: un zap, una invitación), brillo normal
//    - a los 20 s sin tocar nada, tenue
//    - a los 60 s, apagada (los LEDs siguen con su estela)
//  Con la pantalla apagada, el primer botón solo la enciende: no hace nada más.
// ============================================================================
#include <Arduino.h>

namespace backlight {

void begin();                 // después de display::begin (la librería de la pantalla la pone a tope)
bool activity();              // alguien ha tocado algo; true si la pantalla estaba apagada
void loop(bool usb);          // ajusta el brillo según el tiempo sin tocar nada
bool isOff();

}  // namespace backlight
