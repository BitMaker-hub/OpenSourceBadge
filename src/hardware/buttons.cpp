#include "buttons.h"
#include "../config.h"

namespace buttons {

// Lo que sabemos de cada botón entre una vuelta y la siguiente
struct ButtonState {
  uint8_t  pin;
  bool     down;            // estado ya sin rebotes
  bool     lastRaw;         // última lectura del pin
  uint32_t rawChangedAt;    // cuándo cambió la lectura por última vez
  uint32_t pressedAt;       // cuándo se pulsó
  bool     longSent;        // ya avisamos de la pulsación larga
};

static ButtonState state[(int)Button::Count] = {
  {PIN_BTN_PEER, false, false, 0, 0, false},
  {PIN_BTN_MODE, false, false, 0, 0, false},
  {PIN_BTN_SEED, false, false, 0, 0, false},
};

static const ButtonEvent SHORT_EVENT[] = {ButtonEvent::PeerShort, ButtonEvent::ModeShort, ButtonEvent::SeedShort};
static const ButtonEvent LONG_EVENT[]  = {ButtonEvent::PeerLong,  ButtonEvent::ModeLong,  ButtonEvent::SeedLong};

void begin() {
  for (auto& b : state) pinMode(b.pin, INPUT_PULLUP);
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  pinMode(PIN_BTN_KEY, INPUT_PULLUP);
}

ButtonEvent poll() {
  const uint32_t now = millis();

  for (int i = 0; i < (int)Button::Count; i++) {
    ButtonState& b = state[i];
    const bool raw = digitalRead(b.pin) == LOW;   // pulsado = a GND

    // 1. Quitar rebotes: la lectura tiene que mantenerse un rato para creerla
    if (raw != b.lastRaw) {
      b.lastRaw = raw;
      b.rawChangedAt = now;
    }
    const bool stable = now - b.rawChangedAt >= BUTTON_DEBOUNCE_MS;

    // 2. Se acaba de pulsar
    if (stable && raw && !b.down) {
      b.down = true;
      b.pressedAt = now;
      b.longSent = false;
    }

    // 3. Lleva rato pulsado: pulsación larga
    if (b.down && !b.longSent && now - b.pressedAt >= BUTTON_LONG_MS) {
      b.longSent = true;
      return LONG_EVENT[i];
    }

    // 4. Se acaba de soltar: si no fue larga, fue corta
    if (stable && !raw && b.down) {
      b.down = false;
      if (!b.longSent) return SHORT_EVENT[i];
    }
  }
  return ButtonEvent::None;
}

bool isDown(Button b) { return state[(int)b].down; }
bool isBootDown()     { return digitalRead(PIN_BTN_BOOT) == LOW; }
bool isKeyDown()      { return digitalRead(PIN_BTN_KEY) == LOW; }

const char* eventName(ButtonEvent e) {
  switch (e) {
    case ButtonEvent::PeerShort: return "PEER";
    case ButtonEvent::PeerLong:  return "PEER (larga)";
    case ButtonEvent::ModeShort: return "MODE";
    case ButtonEvent::ModeLong:  return "MODE (larga)";
    case ButtonEvent::SeedShort: return "SEED";
    case ButtonEvent::SeedLong:  return "SEED (larga)";
    default:                     return "-";
  }
}

}  // namespace buttons
