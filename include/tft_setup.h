#pragma once
// Pantalla del LilyGO T-Display-S3: ST7789 de 170x320 por bus paralelo de 8 bits.
// Es el Setup206 de TFT_eSPI, copiado aquí para que no dependa de la librería.

#define USER_SETUP_ID 206

#define ST7789_DRIVER
#define INIT_SEQUENCE_3
#define CGRAM_OFFSET
#define TFT_RGB_ORDER TFT_RGB
#define TFT_INVERSION_ON
#define TFT_PARALLEL_8_BIT

#define TFT_WIDTH  170
#define TFT_HEIGHT 320

#define TFT_CS  6
#define TFT_DC  7
#define TFT_RST 5
#define TFT_WR  8
#define TFT_RD  9

#define TFT_D0 39
#define TFT_D1 40
#define TFT_D2 41
#define TFT_D3 42
#define TFT_D4 45
#define TFT_D5 46
#define TFT_D6 47
#define TFT_D7 48

#define TFT_BL 38
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD     // fuente 1: 6x8 de píxel, la usamos para las etiquetas
#define LOAD_FONT2    // fuente 2: 16 px, el texto normal
#define LOAD_FONT4    // fuente 4: 26 px, los titulares
#define LOAD_GFXFF
#define SMOOTH_FONT
