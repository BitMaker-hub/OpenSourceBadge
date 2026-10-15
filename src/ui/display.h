#pragma once
// ============================================================================
//  DISPLAY · la pantalla y las piezas con las que se montan las pantallas
//
//  Se dibuja todo en un lienzo en memoria (170x320) y al final de cada vuelta
//  se manda entero a la pantalla con display::push(). Así no hay parpadeos y
//  cada pantalla se dibuja desde cero, sin tener que acordarse de lo anterior.
//
//  Aquí están las piezas que se repiten (cabecera, tarjeta, chip, QR...), para
//  que screens.cpp solo diga QUÉ va en cada sitio.
//
//  Las fuentes son las de TFT_eSPI. Si más adelante convertimos las del
//  prototipo (VT323 y Silkscreen), solo cambia useFont() en display.cpp.
// ============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>

// Pasa un color 0xRRGGBB al formato de la pantalla (RGB565)
constexpr uint16_t rgb(uint32_t c) {
  return ((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F);
}

// Paleta del prototipo: fondo de tinta azulada, tarjetas y un acento
namespace ink {
  constexpr uint16_t Bg     = rgb(0x0C0E13);
  constexpr uint16_t Card   = rgb(0x151922);
  constexpr uint16_t Card2  = rgb(0x1B2030);
  constexpr uint16_t Line   = rgb(0x262C3A);
  constexpr uint16_t Fg     = rgb(0xEEF0F4);
  constexpr uint16_t Dim    = rgb(0x8A93A6);
  constexpr uint16_t Soft   = rgb(0xC3C8D4);
  constexpr uint16_t Orange = rgb(0xF7931A);
  constexpr uint16_t Gold   = rgb(0xFFC93C);   // lightning
  constexpr uint16_t Nostr  = rgb(0xA78BFA);
  constexpr uint16_t X      = rgb(0xE5E7EB);
  constexpr uint16_t Ok     = rgb(0x57D38C);
  constexpr uint16_t Red    = rgb(0xF87171);
  constexpr uint16_t White  = rgb(0xFFFFFF);
  constexpr uint16_t Pattern= rgb(0x20263A);   // trama del fondo de los avatares
}

enum class Font : uint8_t {
  Label,    // etiquetas pequeñas en mayúsculas (8 px)
  LabelBig, // la misma letra de píxel al doble (16 px), para "ZAP!" o "MINED!"
  Body,     // texto normal (16 px)
  Title,    // nombres y titulares (26 px)
};

enum class Align : uint8_t { Left, Center, Right };

// Imagen ya convertida a RGB565 (las genera tools/exportar_logos.py en ui/logos_data.h)
struct Logo { uint16_t width; uint16_t height; const uint16_t* pixels; };

enum class Icon : uint8_t { Peers, Bolt, Bars, Chain };

namespace display {

void begin();
void push();                          // manda el lienzo a la pantalla
TFT_eSprite& canvas();                // por si una pantalla necesita algo especial

// ---- formas ----
void clear(uint16_t color = ink::Bg);
void rect(int x, int y, int w, int h, uint16_t color);
void roundRect(int x, int y, int w, int h, uint16_t color);   // esquinas de 1 px, como el prototipo

// ---- texto ----
void text(const String& s, int x, int y, Font f = Font::Body, uint16_t color = ink::Fg, Align a = Align::Left);
int  textWidth(const String& s, Font f = Font::Body);
String fit(const String& s, int maxWidth, Font f = Font::Body);   // recorta con ".." si no cabe

// Para nombres: en letra grande si cabe; si no, en letra normal; y si tampoco, recortado.
// Devuelve la fuente que ha usado, por si hay que colocar algo debajo.
Font name(const String& s, int centerX, int y, int maxWidth, uint16_t color = ink::Fg);
// Parte el texto en líneas de como mucho maxWidth. Devuelve cuántas ha escrito en `lines`.
int  wrap(const String& s, int maxWidth, Font f, String* lines, int maxLines);

// ---- piezas ----
// dotColor: un puntito antes de la batería (0 = sin punto). Lo usa Nostr para decir si escucha.
void header(const String& title, uint16_t titleColor, uint8_t batteryPercent, uint16_t dotColor = 0);
int  chip(const String& label, int centerX, int y, uint16_t color);      // devuelve el ancho
void pageDots(int current, int count);
void icon(Icon which, int x, int y, uint16_t color);
void bitmap(const char* const* rows, int nRows, int x, int y, int scale, uint16_t color);   // '#' = píxel

// Un logo de ui/logos_data.h, centrado en centerX. fade = 255 se ve entero, 0 no se ve.
void logo(const Logo& l, int centerX, int y, uint8_t fade = 255);

// Mezcla dos colores de pantalla: amount = 0 -> a, 255 -> b
uint16_t blend(uint16_t a, uint16_t b, uint8_t amount);

// Dibuja un QR centrado en centerX, lo más grande posible sin pasar de maxSize.
// Devuelve el lado en píxeles (0 si el texto no cabe en un QR).
int  qr(const String& data, int centerX, int y, int maxSize);

// Manda por el puerto serie la imagen que hay ahora en el lienzo (captura de pantalla).
// Formato: una línea "SHOT 170 320", los píxeles en RGB565 (2 bytes, fila a fila) y "END".
void screenshotToSerial();

// Escribe por el puerto serie el mismo QR, con '#' y '.', para comprobarlo en el ordenador
void qrToSerial(const String& data);

}  // namespace display
