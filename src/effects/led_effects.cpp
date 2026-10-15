#include "led_effects.h"

namespace effects {

void off(const LedGroup& g) {
  solid(g, colors::Off);
}

void solid(const LedGroup& g, Color c) {
  for (int i = 0; i < g.count; i++) leds::set(g.index[i], c);
}

void blink(const LedGroup& g, Color c, uint16_t periodMs, uint32_t t) {
  alternate(g, c, colors::Off, periodMs, t);
}

void alternate(const LedGroup& g, Color a, Color b, uint16_t periodMs, uint32_t t) {
  const bool first = (t % periodMs) < periodMs / 2;
  solid(g, first ? a : b);
}

void breathe(const LedGroup& g, Color c, uint16_t periodMs, uint32_t t) {
  // Una onda triangular de 0 a 255 y vuelta, elevada al cuadrado para que
  // el ojo la vea subir y bajar de forma pareja
  const uint32_t phase = t % periodMs;
  const uint32_t half = periodMs / 2;
  const uint32_t up = phase < half ? phase * 255 / half : (periodMs - phase) * 255 / half;
  solid(g, leds::dim(c, up * up / 255));
}

void trail(const LedGroup& g, Color c, uint8_t length, uint16_t stepMs, uint32_t t, Color head) {
  off(g);
  if (head == 0) head = c;
  const int pos = (t / stepMs) % g.count;
  for (int k = 0; k < length && k < g.count; k++) {
    const int i = (pos - k + g.count) % g.count;
    const uint8_t fade = 255 - k * 255 / length;          // la cola se apaga
    leds::set(g.index[i], k == 0 ? head : leds::dim(c, fade));
  }
}

void drawIn(const LedGroup& g, Color c, Color newest, uint16_t stepMs, uint32_t t) {
  const int lit = t / stepMs;
  for (int i = 0; i < g.count; i++) {
    if (i < lit - 1)       leds::set(g.index[i], c);
    else if (i == lit - 1) leds::set(g.index[i], newest);
    else                   leds::set(g.index[i], colors::Off);
  }
}

void progress(const LedGroup& g, Color c, uint8_t fraction) {
  const int lit = (g.count * fraction + 127) / 255;
  for (int i = 0; i < g.count; i++) leds::set(g.index[i], i < lit ? c : colors::Off);
}

void sparkle(const LedGroup& g, const Color* palette, uint8_t paletteSize, uint8_t count,
             uint16_t stepMs, uint32_t t) {
  off(g);
  // Un "azar" que depende solo del paso: el mismo t da los mismos LEDs
  uint32_t seed = (t / stepMs) * 2654435761u;
  for (int k = 0; k < count; k++) {
    seed = seed * 1103515245u + 12345u;
    const int i = (seed >> 8) % g.count;
    leds::set(g.index[i], palette[k % paletteSize]);
  }
}

void wander(const LedGroup& g, Color c, uint16_t stepMs, uint32_t t) {
  off(g);
  leds::set(g.index[(t / stepMs) % g.count], c);
}

}  // namespace effects
