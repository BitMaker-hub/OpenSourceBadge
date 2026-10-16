#pragma once
// Piezas que comparten los ficheros de pantallas (screens.cpp y screens_seed.cpp).
// No es para el resto del firmware: desde fuera se usa screens.h.
#include <Arduino.h>

namespace screens {

// La cabecera de siempre, con la batería y el punto de Nostr
void header(const String& title, uint16_t color);

// Texto partido en líneas y centrado
void centeredLines(const String& s, int y, int maxWidth, int maxLines, int lineHeight, uint16_t color);

}  // namespace screens
