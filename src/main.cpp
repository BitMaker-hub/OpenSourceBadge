// ============================================================================
//  OPEN SOURCE BADGE · Proof of Peer · Bitcoin Amsterdam 2026
//  Diseñado por Bitmaker · bitronics.store
//
//  Cómo está organizado:
//
//    config.h              pines y límites de la placa
//    hardware/             lo que toca hierro: botones, LEDs, alimentación, Qwiic
//    effects/              efectos de LEDs reutilizables (estela, parpadeo...)
//    ui/                   pantalla: piezas comunes, personajes y una función por pantalla
//    app/badge             los datos: mi perfil, mis peers, los sats recibidos
//    app/state_machine     QUÉ hace el badge en cada momento (empezar a leer por aquí)
//
//  El loop es siempre igual: leer botones -> el estado actual decide, enciende
//  LEDs y dibuja -> mandar LEDs y pantalla.
// ============================================================================
#include <Arduino.h>
#include "config.h"
#include "hardware/buttons.h"
#include "hardware/leds.h"
#include "hardware/power.h"
#include "hardware/qwiic.h"
#include "ui/display.h"
#include "app/state_machine.h"
#include "app/badge.h"
#include "app/storage.h"
#include "net/setup_portal.h"
#include "net/nostr_zaps.h"
#include "net/radio.h"
#include "net/nearby.h"
#include "net/lnbits.h"
#include "net/miner.h"
#include "app/battery_log.h"

static const uint32_t FRAME_MS = 33;   // ~30 imágenes por segundo

// Para probar desde el ordenador con el monitor serie:
//   p / m / s  = PEER / MODE / SEED corto
//   P / M / S  = los mismos, pulsación larga
//   z          = simular que llega un zap
//   d          = cargar los datos de demo del prototipo (no se guardan)
//   n042       = grabar el número de serie del badge (fábrica)
//   Ycampo=valor = cambiar el perfil: name, role (DEV, MINER, PLEB, WHALE, NOSTRICH, ANON), handle, tagline
//   Nnpub1...  = cambiar el npub del perfil (y Nostr empieza a escuchar ese)
//   Wred|clave = guardar la WiFi con internet para los zaps
//   Lurl|inkey|direccion|acceso = grabar el monedero LNbits del badge (fábrica)
//   Gpool|puerto|direccion = configurar el NerdMiner (y g = encender/apagar)
//   i          = estado: perfil, WiFi de internet y Nostr
//   Z          = poner a cero el contador de sats recibidos
//   X          = borrar perfil y peers (dejarlo como nuevo)
//   c          = captura de la pantalla (la lee tools/captura.py)
//   J{...}     = probar un mensaje de relay de Nostr (un recibo de zap en una línea)
//   q          = escribir los QR del badge con '#' y '.' para comprobarlos
static bool screenshotPending = false;

// Por qué se reinició la última vez. Si fue un fallo ("panic"), el volcado queda
// en la partición coredump y se lee con esptool + esp-coredump (ver README).
static const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "encendido normal";
    case ESP_RST_SW:       return "reinicio por software";
    case ESP_RST_PANIC:    return "FALLO (panic)";
    case ESP_RST_INT_WDT:  return "FALLO (watchdog de interrupciones)";
    case ESP_RST_TASK_WDT: return "FALLO (watchdog de tareas)";
    case ESP_RST_WDT:      return "FALLO (watchdog)";
    case ESP_RST_BROWNOUT: return "BAJADA DE TENSIÓN";
    default:               return "otro";
  }
}

static ButtonEvent readSerialCommand() {
  if (!Serial.available()) return ButtonEvent::None;
  switch (Serial.read()) {
    case 'd': badge::loadDemo(); return ButtonEvent::None;
    case 'n': {
      const int n = Serial.parseInt();
      if (n > 0 && n < 1000) { badge::setNumber(n); Serial.printf("Badge #%03d\n", n); }
      return ButtonEvent::None;
    }
    case 'c': screenshotPending = true; return ButtonEvent::None;
    case 'J': nostrzaps::injectForTest(Serial.readStringUntil('\n')); return ButtonEvent::None;
    case 'q': {
      display::qrToSerial(portal::wifiQr());
      QrItem items[badge::MAX_QRS];
      const int n = badge::qrItems(items);
      for (int i = 0; i < n; i++) display::qrToSerial(items[i].data);
      return ButtonEvent::None;
    }
    case 'Y': {
      const String line = Serial.readStringUntil('\n');
      const int eq = line.indexOf('=');
      const String field = line.substring(0, eq);
      String value = line.substring(eq + 1);
      value.trim();
      Profile& me = badge::me();
      if (field == "name")    me.name = badge::clean(value, NAME_LEN);
      if (field == "role")    me.role = roleFromName(value);
      if (field == "handle")  me.handle = value.length() ? "@" + badge::clean(value, HANDLE_LEN - 1).substring(value.startsWith("@") ? 1 : 0) : "";
      if (field == "tagline") me.tagline = badge::clean(value, TAGLINE_LEN);
      badge::saveMe();
      Serial.printf("Perfil: %s (%s)\n", me.name.c_str(), roleInfo(me.role).name);
      return ButtonEvent::None;
    }
    case 'N': {
      String npub = Serial.readStringUntil('\n');
      npub.trim();
      badge::me().npub = npub;
      badge::saveMe();
      nostrzaps::reload();
      Serial.printf("npub: %s\n", npub.c_str());
      return ButtonEvent::None;
    }
    case 'W': {
      const String line = Serial.readStringUntil('\n');
      const int bar = line.indexOf('|');
      String ssid = bar < 0 ? line : line.substring(0, bar);
      String password = bar < 0 ? "" : line.substring(bar + 1);
      ssid.trim();
      password.trim();
      storage::saveNetwork(ssid, password);
      radio::reloadNetwork();
      Serial.printf("WiFi guardada: %s\n", ssid.c_str());
      return ButtonEvent::None;
    }
    case 'L': {
      // url|inkey|dirección|enlace de acceso, separados por |
      String parts[4];
      String line = Serial.readStringUntil('\n');
      line.trim();
      for (int i = 0, from = 0; i < 4; i++) {
        const int bar = line.indexOf('|', from);
        parts[i] = bar < 0 ? line.substring(from) : line.substring(from, bar);
        from = bar < 0 ? line.length() : bar + 1;
      }
      lnbits::configure(parts[0], parts[1], parts[2], parts[3]);
      badge::setWalletAddress(parts[2]);
      Serial.printf("Monedero: %s en %s\n", parts[2].c_str(), parts[0].c_str());
      return ButtonEvent::None;
    }
    case 'G': {
      String parts[3];
      String line = Serial.readStringUntil('\n');
      line.trim();
      for (int i = 0, from = 0; i < 3; i++) {
        const int bar = line.indexOf('|', from);
        parts[i] = bar < 0 ? line.substring(from) : line.substring(from, bar);
        from = bar < 0 ? line.length() : bar + 1;
      }
      miner::setConfig(parts[0], parts[1].toInt(), parts[2]);
      Serial.printf("Minero: %s:%u -> %s\n", miner::pool().c_str(), miner::port(), miner::address().c_str());
      return ButtonEvent::None;
    }
    case 'i': {
      String ssid, password;
      storage::loadNetwork(ssid, password);
      static const char* const NOSTR[] = {"sin configurar", "en pausa", "conectando WiFi", "conectando relay", "escuchando"};
      Serial.printf("Badge #%03u %s\n", badge::number(), badge::me().name.c_str());
      Serial.printf("  npub:  %s\n", badge::me().npub.c_str());
      Serial.printf("  WiFi:  %s%s\n", ssid.length() ? ssid.c_str() : "(ninguna)", password.length() ? " (con contraseña)" : "");
      Serial.printf("  Nostr: %s\n", NOSTR[(int)nostrzaps::status()]);
      static const char* const RADIO[] = {"exclusiva", "sin WiFi", "conectando", "conectada"};
      Serial.printf("  Radio: %s, canal %u, %d badges cerca\n", RADIO[(int)radio::status()], nearby::channel(), nearby::count());
      const nearby::Neighbor* n = nearby::closest();
      if (n) Serial.printf("  Pegado: #%03u %s (%d dBm)\n", n->number, n->name, n->rssi);
      static const char* const LN[] = {"sin monedero", "sin internet", "conectando", "escuchando"};
      Serial.printf("  LNbits: %s, %s, saldo %ld sats\n", LN[(int)lnbits::status()], lnbits::address().c_str(), (long)lnbits::balanceSats());
      const nerdminer::Stats ms = miner::stats();
      Serial.printf("  Minero: %s, %s, %.1f kH/s, %lu shares aceptados (%lu de 32 bits, %lu rechazados), mejor %.4f, pool pide %.4f, %lu trabajos\n",
                    miner::running() ? "minando" : "parado (solo mina en su pantalla)", ms.connected ? "con trabajo" : "sin trabajo", ms.khs,
                    (unsigned long)ms.accepted, (unsigned long)ms.shares, (unsigned long)ms.rejected, ms.bestDiff, ms.poolDiff,
                    (unsigned long)ms.templates);
      Serial.printf("  Batería: %u mV (%u %%), USB: %u mV%s\n", power::batteryMillivolts(), power::batteryPercent(),
                    power::usbMillivolts(), power::usbConnected() ? " (cargando)" : "");
      Serial.printf("  Encendido hace %lu min; último reinicio: %s\n", millis() / 60000, resetReason());
      return ButtonEvent::None;
    }
    case 'B': batterylog::dump();  return ButtonEvent::None;
    case 'b': batterylog::clear(); return ButtonEvent::None;
    case 'Z': badge::resetSats(); Serial.println("Sats a cero"); return ButtonEvent::None;
    case 'X': badge::factoryReset(); Serial.println("Badge borrado"); return ButtonEvent::None;
    case 'p': return ButtonEvent::PeerShort;
    case 'P': return ButtonEvent::PeerLong;
    case 'm': return ButtonEvent::ModeShort;
    case 'M': return ButtonEvent::ModeLong;
    case 's': return ButtonEvent::SeedShort;
    case 'S': return ButtonEvent::SeedLong;
    case 'z': app::simulateZap(); return ButtonEvent::None;
    default:  return ButtonEvent::None;
  }
}

void setup() {
  Serial.setRxBufferSize(8192);   // para el comando J: un recibo de zap ocupa varios KB
  Serial.begin(115200);
  power::begin();     // lo primero: sin GPIO15 alto, con batería no hay pantalla
  buttons::begin();
  leds::begin();
  qwiic::begin();
  display::begin();
  app::begin();
  batterylog::begin();   // después de app::begin, que monta LittleFS
  Serial.println("Open Source Badge listo. Teclas: p m s (P M S largas), z zap, d demo, n042 numero, X borrar");
  Serial.printf("Último reinicio: %s\n", resetReason());
}

void loop() {
  ButtonEvent event = buttons::poll();
  if (event == ButtonEvent::None) event = readSerialCommand();
  if (event != ButtonEvent::None) Serial.printf("[boton] %s\n", buttons::eventName(event));

  // Sin eventos, solo se redibuja cada FRAME_MS; con un evento, al momento
  static uint32_t lastFrame = 0;
  if (event == ButtonEvent::None && millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();

  nearby::loop();       // quién hay cerca (y si alguien nos invita)
  app::update(event);   // el estado actual pinta LEDs y pantalla en memoria
  batterylog::loop(app::stateName(app::current()));
  leds::show();
  display::push();

  if (screenshotPending) {
    // En el Seed Lab puede haber palabras en pantalla: por el USB no salen nunca
    bool allowed = app::current() != State::SeedLab;
#ifdef SEEDLAB_ALLOW_SCREENSHOTS   // solo para revisar las pantallas en el taller, nunca en un badge de verdad
    allowed = true;
#endif
    if (allowed) display::screenshotToSerial();
    else Serial.println("Captura no permitida en el Seed Lab");
    screenshotPending = false;
  }
}
