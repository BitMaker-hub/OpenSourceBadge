#include "seed_lab.h"
#include "../hardware/leds.h"
#include "../effects/led_effects.h"
#include "../ui/screens.h"
#include <Bitcoin.h>
#include <Hash.h>
#include <Conversion.h>

namespace seedlab {

// ---- Cuánta entropía hace falta (igual que el Seeder y que Coldcard) ----
// Un dado de 6 caras da log2(6) = 2,585 bits por tirada:
// 50 tiradas = 129 bits (12 palabras) y 99 tiradas = 256 bits (24 palabras).
// Con moneda, un bit por tirada: 128 o 256.
static const uint8_t DICE_FOR_12 = 50;
static const uint8_t DICE_FOR_24 = 99;

enum class Step : uint8_t { Menu, Words, Capture, Generating, Result };
enum class Source : uint8_t { Dice, Coin };

// En el resultado se pasa por estas páginas (la segunda solo con 24 palabras)
enum class Page : uint8_t { Words1, Words2, Address, Entropy, SeedQr, WatchOnly, Done, Count };

static Step     step;
static Source   source;
static uint8_t  nWords;
static uint8_t  entropy[32];          // los bits que pone el usuario
static uint16_t coinBits;             // monedas tiradas
static char     rolls[DICE_FOR_24 + 1];
static uint8_t  nRolls;
static uint8_t  dieValue;             // la cara que está marcando ahora
static uint8_t  lastRolls[3];         // las tres últimas, la más reciente al final
static uint32_t lastInputAt;          // para el destello de los LEDs
static bool     lastWasHeads;
static Page     page;
static String   mnemonic, firstAddress, entropyHex, watchOnly, fingerprint;

// ============================================================ utilidades

static uint16_t bitsNeeded()  { return nWords == 12 ? 128 : 256; }
static uint8_t  rollsNeeded() { return nWords == 12 ? DICE_FOR_12 : DICE_FOR_24; }

// Poner "" solo cambia la longitud: el texto seguiría en memoria. Hay que pisarlo.
static void wipeString(String& s) {
  for (unsigned i = 0; i < s.length(); i++) s[i] = 0;
  s = "";
}

void wipe() {
  wipeString(mnemonic);
  wipeString(firstAddress);
  wipeString(entropyHex);
  wipeString(watchOnly);
  wipeString(fingerprint);
  memset(entropy, 0, sizeof entropy);
  memset(rolls, 0, sizeof rolls);
  memset(lastRolls, 0, sizeof lastRolls);
  coinBits = 0;
  nRolls = 0;
  dieValue = 1;
}

void start() {
  wipe();
  step = Step::Menu;
  source = Source::Dice;
  nWords = 12;
}

static void startCapture() {
  wipe();
  step = Step::Capture;
}

// Las palabras y la primera dirección (m/84'/0'/0'/0/0), igual que el Seeder
static void generateSeed() {
  if (source == Source::Dice) sha256((const uint8_t*)rolls, nRolls, entropy);   // como Coldcard
  const size_t bytes = nWords * 4 / 3;
  mnemonic = mnemonicFromEntropy(entropy, bytes);
  entropyHex = toHex(entropy, bytes);
  entropyHex.toUpperCase();
  HDPrivateKey root(mnemonic, "");
  HDPrivateKey account = root.derive("m/84'/0'/0'/");
  firstAddress = account.derive("0/0").address();
  watchOnly = account.xpub();          // zpub de la cuenta: ve el saldo, no puede gastar
  fingerprint = root.fingerprint();    // huella de la clave maestra, como en el Seeder
}

static const int PAGES = (int)Page::Count;
static int pageCount() { return nWords == 12 ? PAGES - 1 : PAGES; }

static int pageIndex() {
  const int p = (int)page;
  return (nWords == 12 && p > (int)Page::Words1) ? p - 1 : p;   // sin la segunda página de palabras
}

static void movePage(int dir) {
  int p = (int)page;
  do {
    p = (p + dir + PAGES) % PAGES;
  } while (nWords == 12 && p == (int)Page::Words2);
  page = (Page)p;
}

// ============================================================ cada paso

static bool menu(ButtonEvent ev) {
  if (ev == ButtonEvent::SeedShort || ev == ButtonEvent::SeedLong) return false;   // salir del Seed Lab
  if (ev == ButtonEvent::PeerShort) source = source == Source::Dice ? Source::Coin : Source::Dice;
  if (ev == ButtonEvent::ModeShort) step = Step::Words;

  leds::set(0, colors::Red);   // un LED rojo: la radio está apagada
  screens::seedMenu(source == Source::Coin);
  return true;
}

static bool words(ButtonEvent ev) {
  if (ev == ButtonEvent::SeedShort) step = Step::Menu;
  if (ev == ButtonEvent::PeerShort) nWords = nWords == 12 ? 24 : 12;
  if (ev == ButtonEvent::ModeShort) startCapture();

  leds::set(0, colors::Red);
  screens::seedWords(nWords, source == Source::Coin);
  return true;
}

static bool capture(ButtonEvent ev, uint32_t now) {
  if (ev == ButtonEvent::SeedLong) { wipe(); step = Step::Menu; return true; }   // abandonar: no queda nada

  if (source == Source::Coin) {
    // PEER = cara (1), MODE = cruz (0). Se hace OR del bit, nunca suma: sumar arrastraría acarreos
    if (ev == ButtonEvent::PeerShort || ev == ButtonEvent::ModeShort) {
      const uint8_t bit = ev == ButtonEvent::PeerShort ? 1 : 0;
      entropy[coinBits / 8] |= bit << (7 - coinBits % 8);
      coinBits++;
      lastWasHeads = bit;
      lastInputAt = now;
      if (coinBits >= bitsNeeded()) { step = Step::Generating; return true; }
    }
  } else {
    // PEER cambia la cara del dado, MODE apunta la tirada
    if (ev == ButtonEvent::PeerShort) dieValue = dieValue % 6 + 1;
    if (ev == ButtonEvent::ModeShort) {
      rolls[nRolls++] = '0' + dieValue;
      lastRolls[0] = lastRolls[1];
      lastRolls[1] = lastRolls[2];
      lastRolls[2] = dieValue;
      dieValue = 1;
      lastInputAt = now;
      if (nRolls >= rollsNeeded()) { step = Step::Generating; return true; }
    }
  }

  // La ₿ se va llenando como una barra de progreso, flojita: al final son los
  // 18 LEDs a la vez durante minutos (y el verde se ve mucho)
  const Color soft = leds::dim(colors::Green, 70);
  const uint16_t done = source == Source::Coin ? coinBits : nRolls;
  const uint16_t total = source == Source::Coin ? bitsNeeded() : rollsNeeded();
  effects::progress(groups::BTopToBottom, soft, done * 255 / total);
  if (now - lastInputAt < 150 && done > 0) {   // destello en TX (cara/tirada) o RX (cruz)
    effects::solid(source == Source::Coin && !lastWasHeads ? groups::Rx : groups::Tx, colors::Green);
  }

  if (source == Source::Coin) screens::seedCoin(coinBits, bitsNeeded(), entropy);
  else                        screens::seedDice(nRolls, rollsNeeded(), dieValue, lastRolls);
  return true;
}

static bool result(ButtonEvent ev, uint32_t t) {
  if (ev == ButtonEvent::PeerShort) movePage(+1);
  if (ev == ButtonEvent::ModeShort) movePage(-1);
  // Salir es a propósito: solo desde la última página y manteniendo SEED,
  // para que nadie pierda las palabras antes de haberlas apuntado
  if (ev == ButtonEvent::SeedLong && page == Page::Done) { wipe(); step = Step::Menu; return true; }

  effects::breathe(groups::B, leds::dim(colors::Green, 70), 3000, t);
  const int index = pageIndex() + 1, total = pageCount();
  switch (page) {
    case Page::Words1:  screens::seedMnemonic(mnemonic, 0, nWords, index, total);  break;
    case Page::Words2:  screens::seedMnemonic(mnemonic, 12, nWords, index, total); break;
    case Page::Address: screens::seedAddress(firstAddress, index, total);          break;
    case Page::Entropy: screens::seedEntropy(entropyHex, source == Source::Dice, index, total); break;
    case Page::SeedQr:    screens::seedExportQr(mnemonic, nWords, index, total);   break;
    case Page::WatchOnly: screens::seedWatchOnly(watchOnly, fingerprint, index, total); break;
    case Page::Done:      screens::seedDone(index, total);                         break;
    case Page::Count:     break;
  }
  return true;
}

// ============================================================ bucle

bool update(ButtonEvent ev, uint32_t t) {
  switch (step) {
    case Step::Menu:    return menu(ev);
    case Step::Words:   return words(ev);
    case Step::Capture: return capture(ev, millis());
    case Step::Generating: {
      // Primero se dibuja "Generating" y, en la vuelta siguiente, se calcula (tarda un poco)
      static bool drawn = false;
      if (!drawn) {
        static const Color FIRE[] = {colors::Green, colors::DarkGreen, 0x9CEBB6};   // verde Seeder
        effects::sparkle(groups::B, FIRE, 3, 6, 60, t);
        screens::seedGenerating();
        drawn = true;
        return true;
      }
      drawn = false;
      generateSeed();
      page = Page::Words1;
      step = Step::Result;
      return result(ButtonEvent::None, t);
    }
    case Step::Result:  return result(ev, t);
  }
  return true;
}

}  // namespace seedlab
