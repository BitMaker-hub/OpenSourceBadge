#include "avatars.h"
#include "avatars_data.h"
#include "display.h"

namespace avatars {

struct Sprite {
  const char* const*  rows;
  const PaletteEntry* palette;
};

// En el mismo orden que el enum Role
static const Sprite SPRITES[] = {
  {AVATAR_DEV,      AVATAR_DEV_PALETTE},
  {AVATAR_MINER,    AVATAR_MINER_PALETTE},
  {AVATAR_PLEB,     AVATAR_PLEB_PALETTE},
  {AVATAR_WHALE,    AVATAR_WHALE_PALETTE},
  {AVATAR_NOSTRICH, AVATAR_NOSTRICH_PALETTE},
  {AVATAR_ANON,     AVATAR_ANON_PALETTE},
};

static uint16_t colorOf(const PaletteEntry* palette, char letter) {
  for (const PaletteEntry* p = palette; p->letter; p++)
    if (p->letter == letter) return p->color;
  return ink::Fg;
}

// Mezcla un color RGB565 con el fondo, a la mitad
static uint16_t fade(uint16_t c) {
  return ((c >> 1) & 0x7BEF) + ((ink::Pattern >> 1) & 0x7BEF);
}

void draw(Role role, int x, int y, int scale, bool faded) {
  const Sprite& s = SPRITES[(int)role];
  for (int j = 0; j < 32; j++) {
    for (int i = 0; i < 32; i++) {
      const char letter = s.rows[j][i];
      if (letter == ' ') continue;
      uint16_t c = colorOf(s.palette, letter);
      if (faded) c = fade(fade(c));   // dos veces: queda a un cuarto
      display::rect(x + i * scale, y + j * scale, scale, scale, c);
    }
  }
}

void card(Role role, int x, int y, int scale) {
  const int side = 32 * scale + 8;
  display::roundRect(x - 4, y - 4, side, side, roleInfo(role).color);
  display::rect(x - 2, y - 2, side - 4, side - 4, ink::Card2);
  // trama de puntos, igual que el prototipo
  for (int j = 0; j < side - 4; j += 2)
    for (int i = (j / 2) % 2 * 2; i < side - 4; i += 4)
      display::rect(x - 2 + i, y - 2 + j, 1, 1, ink::Pattern);
  draw(role, x, y, scale);
}

String svg(Role role) {
  const Sprite& s = SPRITES[(int)role];
  String out = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32' shape-rendering='crispEdges'>";
  // Una tira por cada tramo seguido del mismo color: así salen unos cientos de rectángulos, no mil
  for (int j = 0; j < 32; j++) {
    int i = 0;
    while (i < 32) {
      const char letter = s.rows[j][i];
      int run = 1;
      while (i + run < 32 && s.rows[j][i + run] == letter) run++;
      if (letter != ' ') {
        const uint16_t c = colorOf(s.palette, letter);
        char rect[80];
        snprintf(rect, sizeof rect, "<rect x='%d' y='%d' width='%d' height='1' fill='#%02X%02X%02X'/>",
                 i, j, run, (c >> 8) & 0xF8, (c >> 3) & 0xFC, (c << 3) & 0xF8);
        out += rect;
      }
      i += run;
    }
  }
  return out + "</svg>";
}

}  // namespace avatars
