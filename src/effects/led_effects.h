#pragma once
// ============================================================================
//  EFECTOS DE LEDS · el mismo efecto para cualquier grupo
//
//  Cada efecto recibe un grupo (la B, TX, RX...) y el tiempo `t` en ms desde
//  que empezó. No guarda nada: con el mismo `t` pinta siempre lo mismo. Así la
//  máquina de estados solo tiene que decir "en este estado, estela naranja por
//  el contorno de la B" y pasarle cuánto tiempo lleva en el estado.
//
//  Los efectos solo escriben colores; quien llama a leds::show() es el loop,
//  una vez por vuelta, después de pintar todos los efectos.
//
//  Ejemplos:
//    effects::trail(groups::BOutline, colors::Gold, 6, 45, t);   // estela de zap
//    effects::blink(groups::Tx, colors::Blue, 250, t);           // TX parpadeando
//    effects::breathe(groups::B, colors::Orange, 3000, t);       // la B respirando
// ============================================================================
#include "../hardware/leds.h"

namespace effects {

// Apaga el grupo
void off(const LedGroup& g);

// Todo el grupo de un color
void solid(const LedGroup& g, Color c);

// Encendido/apagado: medio periodo cada cosa
void blink(const LedGroup& g, Color c, uint16_t periodMs, uint32_t t);

// Alterna entre dos colores: medio periodo cada uno
void alternate(const LedGroup& g, Color a, Color b, uint16_t periodMs, uint32_t t);

// Sube y baja suave, como respirando
void breathe(const LedGroup& g, Color c, uint16_t periodMs, uint32_t t);

// Una estela que recorre el grupo en bucle: la cabeza va en `head`, y detrás
// va dejando `length` LEDs que se apagan poco a poco. Nunca enciende más de
// `length` LEDs, así que sirve para mantener el consumo bajo.
void trail(const LedGroup& g, Color c, uint8_t length, uint16_t stepMs, uint32_t t, Color head = 0);

// Van encendiéndose uno a uno y se quedan; el último encendido brilla más.
void drawIn(const LedGroup& g, Color c, Color newest, uint16_t stepMs, uint32_t t);

// Como una barra de progreso: enciende la parte `fraction` (0..255) del grupo
void progress(const LedGroup& g, Color c, uint8_t fraction);

// `count` LEDs al azar que cambian cada `stepMs`, con colores de la lista
void sparkle(const LedGroup& g, const Color* palette, uint8_t paletteSize, uint8_t count,
             uint16_t stepMs, uint32_t t);

// Uno solo encendido que va dando la vuelta despacio (reposo)
void wander(const LedGroup& g, Color c, uint16_t stepMs, uint32_t t);

}  // namespace effects
