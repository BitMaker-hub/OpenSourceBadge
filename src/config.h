#pragma once
// ============================================================================
//  CONFIG · todo lo que depende de la placa está aquí
//
//  Los pines salen del esquema de la placa rev0.9-final (BTCBadge.kicad_sch).
//  Si cambia la placa, se cambia este fichero y nada más.
// ============================================================================

// ---- Botones de la placa (van a GND al pulsar; usan el pull-up interno) ----
#define PIN_BTN_PEER        1     // SW2, serigrafía "PEER"
#define PIN_BTN_MODE        2     // SW3, serigrafía "MODE"
#define PIN_BTN_SEED        3     // SW4, serigrafía "SEED"

// ---- Botones del propio T-Display (solo se usan en la prueba de placa) ----
#define PIN_BTN_BOOT        0
#define PIN_BTN_KEY         14

// ---- LEDs ----
#define PIN_LED_DATA        16    // DIN del primer LED, a través de R8 (1k)
#define PIN_LED_POWER_OFF   21    // Q1 (P-MOSFET): LOW = LEDs alimentados
#define LED_COUNT           20    // LED1..LED18 = la B, LED19 = TX, LED20 = RX

// Cada canal de un WS2812B-2020 gasta 12 mA a tope. Todo sale del regulador
// de 3V3 del T-Display (600 mA, compartido con el ESP32), así que el firmware
// nunca deja pasar de este consumo. Ver REVISION-rev0.9.md de la placa.
#define LED_MAX_CURRENT_MA  200
#define LED_DEFAULT_BRIGHTNESS 25  // de 0 a 255 (unos 10 %): a más, deslumbran y no se lee la pantalla

// ---- Alimentación ----
#define PIN_POWER_ON        15    // alimenta la pantalla con batería: alto = encendido
#define PIN_USB_SENSE       10    // 5V del USB dividido entre 2 (R5/R6 100k)
#define PIN_BATTERY_SENSE   4     // batería dividida entre 2 (dentro del T-Display)

// ---- Qwiic / I2C ----
#define PIN_I2C_SDA         17
#define PIN_I2C_SCL         18

// ---- Pantalla ----
// 0 = vertical con el USB-C abajo. Si en la placa sale del revés, poner 2.
#define SCREEN_ROTATION     0
#define SCREEN_W            170
#define SCREEN_H            320

// ---- Tiempos de los botones ----
#define BUTTON_DEBOUNCE_MS  25
#define BUTTON_LONG_MS      900   // a partir de aquí la pulsación es "larga"
