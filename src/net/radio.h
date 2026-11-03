#pragma once
// ============================================================================
//  RADIO · quién usa la WiFi del ESP32 en cada momento
//
//  La radio es una sola y la quieren varios. Este módulo pone orden:
//
//  - En NORMAL la radio está en modo estación (STA). Si hay una WiFi con
//    internet configurada, se mantiene conectado a ella (para los zaps de
//    Nostr, LNbits, el minero...). Además, encima de eso, "peer cercano"
//    usa ESP-NOW en el mismo canal.
//
//  - En EXCLUSIVO alguien necesita la radio entera para él: el handshake
//    (ESP-NOW en el canal 1), el SETUP (punto de acceso propio) o el Seed Lab
//    (radio apagada). Al pedirlo se suelta la WiFi y se apaga todo; al
//    terminar, se vuelve a NORMAL y se reconecta solo.
//
//  La conexión a la WiFi la lleva una tarea aparte, para no congelar nunca
//  la pantalla mientras conecta.
//
//  OJO al soltar la radio: quien tenga conexiones abiertas (Nostr, LNbits, el
//  minero) tiene que cerrarlas ANTES de que se apague la WiFi. Si no, al
//  cerrarlas después liberan buffers que el driver ya ha soltado y el chip se
//  reinicia (pasó el 2026-10-18 con Nostr al entrar en el Seed Lab). Por eso
//  cada uno avisa con busy() y la radio espera a que todos hayan cerrado.
// ============================================================================
#include <Arduino.h>

namespace radio {

enum class Status : uint8_t {
  Exclusive,     // la tiene el handshake, el SETUP o el Seed Lab
  NoNetwork,     // no hay WiFi con internet configurada (la radio está, pero sin conectar)
  Connecting,
  Online,        // conectado y con IP
  Sleeping,      // ahorro: apagada entre ventana y ventana
};

// Los que abren conexiones por internet
enum class User : uint8_t { Nostr, Lnbits, Miner, BitcoinInfo, Count };

void begin();
void reloadNetwork();          // volver a leer la WiFi guardada (después de cambiarla)

// Pedir la radio entera (true) o devolverla (false). Al pedirla espera a que esté apagada.
void exclusive(bool on);

// Ahorro: con batería y la pantalla apagada, la radio solo a ratos (ver radio.cpp)
void setEco(bool on);
bool eco();

Status status();
bool   online();
bool   hasNetwork();
uint32_t onlineSince();         // millis() en que conectó (0 si no lo está): para medir cuánto tardan los demás

// "Tengo conexiones abiertas" (true) o "ya las he cerrado" (false).
// Cuando online() pasa a false, cada uno cierra lo suyo y avisa.
void busy(User who, bool open);

// "Estoy abriendo una conexión TLS": mientras viva el objeto, el loop pone la CPU a
// 240. El saludo TLS es cálculo pesado en el núcleo 0: con batería (80 MHz), los 3
// relays y LNbits saludando a la vez lo dejaban más de 5 s sin respiro, saltaba el
// watchdog y el badge se reiniciaba en bucle nada más conectar (#308, 2026-10-30).
struct Handshake {
  Handshake();
  ~Handshake();
};
bool handshaking();

}  // namespace radio
