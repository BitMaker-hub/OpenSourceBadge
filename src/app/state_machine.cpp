#include "state_machine.h"
#include "badge.h"
#include "../config.h"
#include "../hardware/leds.h"
#include "../hardware/power.h"
#include "../hardware/qwiic.h"
#include "../effects/led_effects.h"
#include "../ui/display.h"
#include "../ui/screens.h"

namespace app {

// ============================================================ estado actual

static State    state = State::Boot;
static uint32_t enteredAt = 0;        // millis() al entrar en el estado
static State    returnTo = State::Profile;   // a qué página volver al salir de un estado "de paso"

static void onEnter(State s);         // lo que hace cada estado al empezar (más abajo)

static void go(State next) {
  Serial.printf("[estado] %s -> %s\n", stateName(state), stateName(next));
  state = next;
  enteredAt = millis();
  onEnter(next);
}

State current() { return state; }

const char* stateName(State s) {
  switch (s) {
    case State::Boot:         return "BOOT";
    case State::Profile:      return "PROFILE";
    case State::QrCodes:      return "QR_CODES";
    case State::Peers:        return "PEERS";
    case State::Stats:        return "STATS";
    case State::QrFullscreen: return "QR_FULLSCREEN";
    case State::Handshake:    return "HANDSHAKE";
    case State::Zap:          return "ZAP";
    case State::SeedLab:      return "SEED_LAB";
    case State::Setup:        return "SETUP";
    case State::HardwareTest: return "HARDWARE_TEST";
  }
  return "?";
}

static bool isHomePage(State s) {
  return s == State::Profile || s == State::QrCodes || s == State::Peers || s == State::Stats;
}

// Las cuatro páginas comparten botones. Devuelve true si el evento cambió de estado.
static bool homeButtons(ButtonEvent ev) {
  switch (ev) {
    case ButtonEvent::ModeShort:   // siguiente página
      if (state == State::Profile)      go(State::QrCodes);
      else if (state == State::QrCodes) go(State::Peers);
      else if (state == State::Peers)   go(State::Stats);
      else                              go(State::Profile);
      return true;
    case ButtonEvent::PeerShort: returnTo = state; go(State::Handshake);    return true;
    case ButtonEvent::PeerLong:  returnTo = state; go(State::QrFullscreen); return true;
    case ButtonEvent::ModeLong:  returnTo = state; go(State::Setup);        return true;
    case ButtonEvent::SeedShort: returnTo = state; go(State::SeedLab);      return true;
    case ButtonEvent::SeedLong:  returnTo = state; go(State::HardwareTest); return true;
    default: return false;
  }
}

// En reposo, una sola brasa dando la vuelta a la B muy despacio
static void idleLeds(uint32_t t) {
  effects::wander(groups::B, colors::Ember, 1800, t);
}

// ============================================================ BOOT

static void boot(ButtonEvent ev, uint32_t t) {
  if (ev != ButtonEvent::None || t > 3200) return go(State::Profile);

  effects::drawIn(groups::B, colors::Orange, colors::Amber, 70, t);
  screens::boot(t);
}

// ============================================================ PÁGINAS DE INICIO

static void profile(ButtonEvent ev, uint32_t t) {
  if (homeButtons(ev)) return;
  idleLeds(t);
  screens::profile();
}

static int qrShown = 0;   // el QR que se ve ahora, para que el de pantalla completa sea el mismo

static void qrCodes(ButtonEvent ev, uint32_t t) {
  if (homeButtons(ev)) return;

  QrItem items[badge::MAX_QRS];
  const int n = badge::qrItems(items);
  qrShown = (t / 6000) % n;          // cambia de QR cada 6 segundos

  idleLeds(t);
  screens::qrCodes(items, n, qrShown);
}

static void peers(ButtonEvent ev, uint32_t t) {
  if (homeButtons(ev)) return;
  idleLeds(t);
  screens::peers();
}

static void stats(ButtonEvent ev, uint32_t t) {
  if (homeButtons(ev)) return;
  idleLeds(t);
  screens::stats();
}

// ============================================================ QR A PANTALLA COMPLETA

static void qrFullscreen(ButtonEvent ev, uint32_t t) {
  if (ev == ButtonEvent::PeerShort || ev == ButtonEvent::PeerLong || ev == ButtonEvent::ModeShort)
    return go(returnTo);

  QrItem items[badge::MAX_QRS];
  const int n = badge::qrItems(items);
  screens::qrFullscreen(items[qrShown % n]);   // LEDs apagados: que nada distraiga al que escanea
}

// ============================================================ HANDSHAKE
//
// Tiene sus propios pasos, como una máquina de estados pequeña dentro de este
// estado. Ahora los pasos avanzan por tiempo; cuando esté ESP-NOW avanzarán
// cuando llegue la respuesta del otro badge.

enum class HandshakeStep : uint8_t { Searching, Found, Mining, Mined };

static HandshakeStep step;
static uint32_t      stepStartedAt;
static Peer          other;              // el badge que hemos encontrado
static String        hashes[14];         // los hashes que van pasando al minar
static int           hashCount;
static uint32_t      nextHashAt;

static void nextStep(HandshakeStep s) {
  step = s;
  stepStartedAt = millis();
}

static void startHandshake() {
  other = badge::randomDemoPeer();   // sin radio todavía: un peer inventado
  hashCount = 0;
  nextStep(HandshakeStep::Searching);
}

static void handshake(ButtonEvent ev, uint32_t t) {
  if (ev == ButtonEvent::ModeShort) return go(returnTo);   // cancelar

  // 1. ¿Toca pasar al paso siguiente?
  uint32_t s = millis() - stepStartedAt;                    // tiempo dentro del paso
  switch (step) {
    case HandshakeStep::Searching: if (s > 1700) nextStep(HandshakeStep::Found); break;
    case HandshakeStep::Found:     if (s > 1200) { nextStep(HandshakeStep::Mining); nextHashAt = 0; } break;
    case HandshakeStep::Mining:    if (s > 2100) nextStep(HandshakeStep::Mined); break;
    case HandshakeStep::Mined:
      if (s > 2600 || ev == ButtonEvent::PeerShort) {
        badge::addPeer(other);                             // ya es un peer más
        return go(returnTo);
      }
      break;
  }
  s = millis() - stepStartedAt;

  // 2. LEDs y pantalla del paso en el que estamos
  switch (step) {
    case HandshakeStep::Searching:
      effects::trail(groups::B, colors::Purple, 2, 90, s);
      effects::blink(groups::Tx, colors::Blue, 500, s);
      screens::handshakeSearching(s);
      break;

    case HandshakeStep::Found:
      effects::blink(groups::Tx, colors::Blue, 300, s);
      effects::blink(groups::Rx, colors::Blue, 300, s + 150);   // desfasados: van y vienen
      screens::handshakeFound(other, s);
      break;

    case HandshakeStep::Mining: {
      while (nextHashAt <= s) {                                // un hash nuevo arriba cada 70 ms
        for (int i = min(hashCount, 13); i > 0; i--) hashes[i] = hashes[i - 1];
        char h[17];
        snprintf(h, sizeof h, "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
        hashes[0] = h;
        hashCount = min(hashCount + 1, 14);
        nextHashAt += 70;
      }
      static const Color FIRE[] = {colors::Orange, colors::Gold, colors::Amber};
      effects::sparkle(groups::B, FIRE, 3, 6, 60, s);
      effects::blink(groups::TxRx, colors::Orange, 200, s);
      screens::handshakeMining(hashes, hashCount, s * 255 / 2100, s * 184467 / 2100);
      break;
    }

    case HandshakeStep::Mined:
      effects::alternate(groups::B, colors::Green, colors::DarkGreen, 440, s);
      screens::handshakeMined(other);
      break;
  }
}

// ============================================================ ZAP

static Zap zapShown;

void simulateZap() {
  zapShown = badge::nextDemoZap();
  badge::addSats(zapShown.amount);
  if (isHomePage(state)) returnTo = state;
  if (state != State::HardwareTest) go(State::Zap);
}

static void zap(ButtonEvent ev, uint32_t t) {
  if (ev != ButtonEvent::None || t > 6000) return go(returnTo);

  effects::trail(groups::BTopToBottom, colors::Gold, 6, 45, t, colors::Cream);   // estela: como mucho 6 encendidos
  effects::solid(groups::Rx, colors::Gold);
  screens::zap(zapShown, t);
}

// ============================================================ SEED LAB

static int seedSelected = 0;

static void seedLab(ButtonEvent ev, uint32_t t) {
  if (ev == ButtonEvent::SeedShort || ev == ButtonEvent::SeedLong) return go(returnTo);
  if (ev == ButtonEvent::PeerShort) seedSelected = (seedSelected + 1) % 3;
  // MODE: aquí arrancará el generador elegido (por hacer)

  leds::set(0, colors::Red);   // un LED rojo: la radio está apagada
  screens::seedLab(seedSelected);
}

// ============================================================ SETUP

static void setupMode(ButtonEvent ev, uint32_t t) {
  if (ev == ButtonEvent::ModeShort || ev == ButtonEvent::ModeLong || t > 300000) return go(returnTo);
  // Por hacer: levantar la WiFi propia y la página badge.local

  effects::blink(groups::BTicks, colors::Orange, 800, t);
  effects::solid(groups::Tx, colors::Blue);
  screens::setup(t);
}

// ============================================================ PRUEBA DE PLACA
//
// Recorre los 20 LEDs uno a uno en rojo, luego en verde y luego en azul, para
// ver que no falla ningún LED ni ningún color. Después los enciende todos en
// blanco (el límite de consumo los baja solo) y vuelve a empezar.

static const uint16_t TEST_STEP_MS = 350;
static const int TEST_STEPS = LED_COUNT * 3 + 1;   // 20 LEDs x 3 colores + todos a la vez

static int         testStep = 0;
static bool        testPaused = false;
static uint32_t    testStepAt = 0;
static ButtonEvent testLastEvent = ButtonEvent::None;
static uint8_t     i2cFound[8];
static uint8_t     i2cCount = 0;
static uint32_t    i2cScannedAt = 0;

static void startHardwareTest() {
  testStep = 0;
  testPaused = false;
  testStepAt = millis();
  testLastEvent = ButtonEvent::None;
  i2cScannedAt = 0;
}

static void hardwareTest(ButtonEvent ev, uint32_t t) {
  if (ev == ButtonEvent::SeedLong) return go(State::Profile);
  if (ev != ButtonEvent::None) testLastEvent = ev;
  if (ev == ButtonEvent::PeerShort) testPaused = !testPaused;
  if (ev == ButtonEvent::ModeShort && testPaused) testStep = (testStep + 1) % TEST_STEPS;

  // Avanzar el paso con el tiempo (salvo en pausa)
  if (!testPaused && millis() - testStepAt > TEST_STEP_MS) {
    testStep = (testStep + 1) % TEST_STEPS;
    testStepAt = millis();
  }

  // Encender lo que toca en este paso
  static const Color RGB[] = {0xFF0000, 0x00FF00, 0x0000FF};
  static const char* const RGB_NAME[] = {"ROJO", "VERDE", "AZUL"};
  String what;
  if (testStep < LED_COUNT * 3) {
    const int led = testStep % LED_COUNT;
    const int color = testStep / LED_COUNT;
    leds::set(led, RGB[color]);
    what = "LED " + String(led + 1) + " - " + RGB_NAME[color];
  } else {
    effects::solid(groups::All, colors::White);
    what = "TODOS - BLANCO";
  }

  // El I2C se mira cada 2 segundos
  if (i2cScannedAt == 0 || millis() - i2cScannedAt > 2000) {
    i2cCount = qwiic::scan(i2cFound, sizeof i2cFound);
    i2cScannedAt = millis();
  }

  screens::HardwareTestInfo info;
  info.ledStep = what;
  info.paused = testPaused;
  info.lastEvent = testLastEvent;
  memcpy(info.i2c, i2cFound, sizeof i2cFound);
  info.i2cCount = i2cCount;
  info.usbMv = power::usbMillivolts();
  info.batteryMv = power::batteryMillivolts();
  info.ledMa = leds::lastCurrentMa();
  screens::hardwareTest(info);
}

// ============================================================ bucle

void begin() {
  badge::loadDemo();
  // SEED pulsado al encender: directo a la prueba de placa
  if (digitalRead(PIN_BTN_SEED) == LOW) go(State::HardwareTest);
  else go(State::Boot);
}

static void onEnter(State s) {
  switch (s) {
    case State::Handshake:    startHandshake();    break;
    case State::SeedLab:      seedSelected = 0;    break;
    case State::HardwareTest: startHardwareTest(); break;
    default:                                       break;   // el resto no prepara nada
  }
}

// Llama a la función del estado actual, con la pantalla y los LEDs en blanco
static void runState(ButtonEvent ev) {
  const uint32_t t = millis() - enteredAt;
  display::clear();
  leds::clear();
  switch (state) {
    case State::Boot:         boot(ev, t);         break;
    case State::Profile:      profile(ev, t);      break;
    case State::QrCodes:      qrCodes(ev, t);      break;
    case State::Peers:        peers(ev, t);        break;
    case State::Stats:        stats(ev, t);        break;
    case State::QrFullscreen: qrFullscreen(ev, t); break;
    case State::Handshake:    handshake(ev, t);    break;
    case State::Zap:          zap(ev, t);          break;
    case State::SeedLab:      seedLab(ev, t);      break;
    case State::Setup:        setupMode(ev, t);    break;
    case State::HardwareTest: hardwareTest(ev, t); break;
  }
}

void update(ButtonEvent ev) {
  const State before = state;
  runState(ev);
  // Si el estado ha cambiado, el nuevo dibuja ya su primera imagen;
  // si no, este fotograma se quedaría en blanco
  if (state != before) runState(ButtonEvent::None);
}

}  // namespace app
