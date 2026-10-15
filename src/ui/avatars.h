#pragma once
// ============================================================================
//  AVATARES · los personajes en pixel art (32 x 32)
//
//  Los dibujos están en avatars_data.h, sacados del prototipo de pantallas con
//  tools/exportar_sprites.js. Aquí solo se pintan, a la escala que se pida.
// ============================================================================
#include "../app/badge.h"

namespace avatars {

// Pinta el personaje tal cual. faded = medio fundido con el fondo (roles aún no conocidos)
void draw(Role role, int x, int y, int scale, bool faded = false);

// El personaje dentro de su tarjeta: marco del color del rol y fondo con trama
void card(Role role, int x, int y, int scale);

// El personaje como imagen SVG, para la página de configuración del móvil
String svg(Role role);

}  // namespace avatars
