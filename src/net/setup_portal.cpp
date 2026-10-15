#include "setup_portal.h"
#include "setup_page.h"
#include "../app/badge.h"
#include "../ui/avatars.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>

namespace portal {

static WebServer server(80);
static DNSServer dns;
static bool running = false;
static bool saved = false;

// Texto para meter dentro de un JSON: con comillas y sin romperlo
static String json(const String& s) {
  String out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

static String hexColor(uint16_t c565) {
  char hex[8];
  snprintf(hex, sizeof hex, "#%02X%02X%02X", (c565 >> 8) & 0xF8, (c565 >> 3) & 0xFC, (c565 << 3) & 0xF8);
  return hex;
}

// ---------------------------------------------------------------- páginas

static void sendPage() {
  server.send_P(200, "text/html", SETUP_PAGE);
}

static void sendProfile() {
  const Profile& me = badge::me();
  String roles = "[";
  for (int r = 0; r < (int)Role::Count; r++) {
    if (r) roles += ",";
    roles += "[" + json(roleInfo((Role)r).name) + "," + json(hexColor(roleInfo((Role)r).color)) + "]";
  }
  roles += "]";

  String body = "{";
  body += "\"number\":" + String(badge::number());
  body += ",\"name\":" + json(badge::isConfigured() ? me.name : "");
  body += ",\"handle\":" + json(me.handle);
  body += ",\"npub\":" + json(me.npub);
  body += ",\"ln\":" + json(me.lightning);
  body += ",\"tagline\":" + json(badge::isConfigured() ? me.tagline : "");
  body += ",\"role\":" + json(roleInfo(me.role).name);
  body += ",\"roles\":" + roles + "}";
  server.send(200, "application/json", body);
}

static void saveProfile() {
  Profile& me = badge::me();
  me.role = roleFromName(server.arg("role"));
  me.name = badge::clean(server.arg("name"), NAME_LEN);
  if (me.name.length() == 0) me.name = "Badge #" + String(badge::number());

  String handle = badge::clean(server.arg("handle"), HANDLE_LEN + 1);
  handle.replace("@", "");
  handle.replace(" ", "");
  me.handle = handle.length() ? "@" + handle.substring(0, HANDLE_LEN - 1) : "";

  String npub = badge::clean(server.arg("npub"), NPUB_LEN);
  me.npub = npub.startsWith("npub1") ? npub : "";

  String ln = badge::clean(server.arg("ln"), LIGHTNING_LEN);
  ln.replace(" ", "");
  ln.toLowerCase();
  me.lightning = ln;

  me.tagline = badge::clean(server.arg("tagline"), TAGLINE_LEN);
  badge::saveMe();

  // De paso, el móvil nos dice la hora
  const uint32_t now = server.arg("now").toInt();
  if (now > 1700000000) badge::setTime(now);
  if (server.hasArg("tz")) badge::setTimezone(server.arg("tz").toInt());

  saved = true;
  Serial.printf("[portal] perfil guardado: %s (%s)\n", me.name.c_str(), roleInfo(me.role).name);
  server.send(200, "application/json", "{\"ok\":true}");
}

static void sendAvatar() {
  server.sendHeader("Cache-Control", "max-age=86400");
  server.send(200, "image/svg+xml", avatars::svg(roleFromName(server.arg("r"))));
}

// Cualquier otra dirección: al portal. Así el móvil abre la página solo al conectarse.
static void redirectToPortal() {
  server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
  server.send(302, "text/plain", "");
}

// ---------------------------------------------------------------- arrancar y parar

void start() {
  if (running) return;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(badge::wifiName().c_str());   // abierta: sin contraseña
  delay(100);

  dns.start(53, "*", WiFi.softAPIP());   // todas las direcciones apuntan a nosotros
  MDNS.begin("badge");                   // y además badge.local

  server.on("/", HTTP_GET, sendPage);
  server.on("/api/profile", HTTP_GET, sendProfile);
  server.on("/api/profile", HTTP_POST, saveProfile);
  server.on("/avatar.svg", HTTP_GET, sendAvatar);
  server.onNotFound(redirectToPortal);
  server.begin();

  running = true;
  saved = false;
  Serial.printf("[portal] WiFi %s en %s\n", badge::wifiName().c_str(), WiFi.softAPIP().toString().c_str());
}

void stop() {
  if (!running) return;
  server.stop();
  dns.stop();
  MDNS.end();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  running = false;
  Serial.println("[portal] WiFi apagada");
}

void loop() {
  if (!running) return;
  dns.processNextRequest();
  server.handleClient();
}

bool savedSinceLastAsk() {
  const bool s = saved;
  saved = false;
  return s;
}

String wifiQr() { return "WIFI:S:" + badge::wifiName() + ";T:nopass;;"; }

int connectedPhones() { return running ? WiFi.softAPgetStationNum() : 0; }

}  // namespace portal
