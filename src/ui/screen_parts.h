#pragma once
// Piezas que comparten los ficheros de pantallas (screens.cpp, screens_seed.cpp y screens_miner.cpp).
// No es para el resto del firmware: desde fuera se usa screens.h.
#include <Arduino.h>

namespace screens {

// La cabecera de siempre, con la batería y el punto de Nostr
void header(const String& title, uint16_t color);

// "12600" -> "12,600"  y  1234567 -> "1.23M"
String thousands(uint32_t n);
String shortNumber(double v);

// Texto partido en líneas y centrado
void centeredLines(const String& s, int y, int maxWidth, int maxLines, int lineHeight, uint16_t color);

}  // namespace screens
