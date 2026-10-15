#include "leds.h"
#include "../config.h"
#include <driver/rmt.h>

// ---------------------------------------------------------------- grupos
namespace groups {
  static const uint8_t B_IDX[]          = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
  // Palo de abajo arriba, panza de arriba, centro, panza de abajo y vuelta al inicio
  static const uint8_t OUTLINE_IDX[]    = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
  static const uint8_t TICKS_IDX[]      = {16, 17, 15, 14};
  static const uint8_t TOP_BOTTOM_IDX[] = {16, 17, 4, 5, 6, 7, 8, 9, 3, 2, 10, 11, 1, 12, 0, 13, 15, 14};
  static const uint8_t TX_IDX[]         = {18};
  static const uint8_t RX_IDX[]         = {19};
  static const uint8_t TXRX_IDX[]       = {18, 19};
  static const uint8_t ALL_IDX[]        = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

  const LedGroup B            = {B_IDX,          sizeof(B_IDX)};
  const LedGroup BOutline     = {OUTLINE_IDX,    sizeof(OUTLINE_IDX)};
  const LedGroup BTicks       = {TICKS_IDX,      sizeof(TICKS_IDX)};
  const LedGroup BTopToBottom = {TOP_BOTTOM_IDX, sizeof(TOP_BOTTOM_IDX)};
  const LedGroup Tx           = {TX_IDX,         sizeof(TX_IDX)};
  const LedGroup Rx           = {RX_IDX,         sizeof(RX_IDX)};
  const LedGroup TxRx         = {TXRX_IDX,       sizeof(TXRX_IDX)};
  const LedGroup All          = {ALL_IDX,        sizeof(ALL_IDX)};
}

// ---------------------------------------------------------------- tira
namespace leds {

// ---- Envío de los bits por el periférico RMT del ESP32 ----
//
// No usamos Adafruit NeoPixel: manda el "0" con un pulso de 400 ns y el
// WS2812B-2020-V6 solo acepta de 220 a 380 ns. Con 400 ns lee todos los bits
// como "1" y los 20 LEDs se encienden en blanco a tope (~700 mA).
//
// Tiempos del datasheet V6 (docs/WS2812B-2020-V6_datasheet.pdf, pág. 5):
//   bit 0: alto 220-380 ns, bajo 580-1000 ns
//   bit 1: alto 580-1000 ns, bajo 580-1000 ns
//   reset: bajo 280 us o más
// El RMT cuenta a 40 MHz (80 MHz / 2), es decir, 25 ns por tick.
static const rmt_channel_t RMT_CH = RMT_CHANNEL_0;
static const uint16_t T0H = 12;   // 300 ns
static const uint16_t T0L = 34;   // 850 ns
static const uint16_t T1H = 30;   // 750 ns
static const uint16_t T1L = 22;   // 550 ns
static rmt_item32_t bits[LED_COUNT * 24];

static void sendToStrip(const Color* c) {
  int n = 0;
  for (int i = 0; i < LED_COUNT; i++) {
    // El LED espera verde, rojo, azul, empezando por el bit más alto
    const uint32_t grb = ((c[i] >> 8) & 0xFF) << 16 | ((c[i] >> 16) & 0xFF) << 8 | (c[i] & 0xFF);
    for (int b = 23; b >= 0; b--) {
      const bool one = grb & (1UL << b);
      bits[n].level0 = 1;  bits[n].duration0 = one ? T1H : T0H;
      bits[n].level1 = 0;  bits[n].duration1 = one ? T1L : T0L;
      n++;
    }
  }
  rmt_write_items(RMT_CH, bits, n, true);   // espera a que salga entero
  delayMicroseconds(300);                    // reset: la línea baja más de 280 us
}

static Color    wanted[LED_COUNT];      // lo que piden los efectos
static uint8_t  level = LED_DEFAULT_BRIGHTNESS;
static bool     powered = false;
static uint16_t currentMa = 0;

static uint8_t red(Color c)   { return (c >> 16) & 0xFF; }
static uint8_t green(Color c) { return (c >> 8) & 0xFF; }
static uint8_t blue(Color c)  { return c & 0xFF; }

static void powerOn(bool on) {
  if (on == powered) return;
  digitalWrite(PIN_LED_POWER_OFF, on ? LOW : HIGH);
  powered = on;
  if (on) delay(1);   // dejar que suba la tensión antes de mandar datos
}

void begin() {
  pinMode(PIN_LED_POWER_OFF, OUTPUT);
  digitalWrite(PIN_LED_POWER_OFF, HIGH);   // empezamos apagados
  rmt_config_t config = RMT_DEFAULT_CONFIG_TX((gpio_num_t)PIN_LED_DATA, RMT_CH);
  config.clk_div = 2;                        // 80 MHz / 2 = 40 MHz, 25 ns por tick
  rmt_config(&config);
  rmt_driver_install(RMT_CH, 0, 0);
  clear();
  show();
}

void set(uint8_t index, Color c) { if (index < LED_COUNT) wanted[index] = c; }
Color get(uint8_t index)         { return index < LED_COUNT ? wanted[index] : colors::Off; }
void clear()                     { for (auto& c : wanted) c = colors::Off; }

void setBrightness(uint8_t l) { level = l; }
uint8_t brightness()          { return level; }
uint16_t lastCurrentMa()      { return currentMa; }

Color dim(Color c, uint8_t l) {
  return ((uint32_t)red(c) * l / 255) << 16 | ((uint32_t)green(c) * l / 255) << 8 | (uint32_t)blue(c) * l / 255;
}

Color blend(Color a, Color b, uint8_t t) {
  auto mix = [t](uint8_t x, uint8_t y) { return (uint32_t)(x + ((int)y - x) * t / 255); };
  return mix(red(a), red(b)) << 16 | mix(green(a), green(b)) << 8 | mix(blue(a), blue(b));
}

void show() {
  // 1. Brillo general y suma de lo que gastaría (12 mA por canal a 255)
  Color out[LED_COUNT];
  uint32_t channelSum = 0;
  bool anyOn = false;
  for (int i = 0; i < LED_COUNT; i++) {
    out[i] = dim(wanted[i], level);
    channelSum += red(out[i]) + green(out[i]) + blue(out[i]);
    if (out[i]) anyOn = true;
  }
  uint32_t ma = channelSum * 12 / 255;

  // 2. Si nos pasamos del límite, bajamos todos los LEDs en la misma proporción
  if (ma > LED_MAX_CURRENT_MA) {
    const uint8_t scale = LED_MAX_CURRENT_MA * 255 / ma;
    for (auto& c : out) c = dim(c, scale);
    ma = LED_MAX_CURRENT_MA;
  }

  // 3. Si no hay nada encendido, cortamos la alimentación y listo
  if (!anyOn) {
    if (powered) {
      sendToStrip(out);   // todo a negro antes de cortar
      powerOn(false);
    }
    currentMa = 0;
    return;
  }

  powerOn(true);
  sendToStrip(out);
  currentMa = ma + LED_COUNT;   // + ~1 mA de cada LED solo por estar alimentado
}

}  // namespace leds
