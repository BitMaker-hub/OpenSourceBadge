#include "setup_portal.h"
#include "setup_page.h"
#include "../app/badge.h"
#include "../ui/avatars.h"
#include "../app/storage.h"
#include "lnbits.h"
#include "miner.h"
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
  String ssid, password;
  storage::loadNetwork(ssid, password);
  body += ",\"wifi\":" + json(ssid);
  body += ",\"wallet\":" + json(lnbits::address());
  body += ",\"mpool\":" + json(miner::pool() + ":" + String(miner::port()));
  body += ",\"maddr\":" + json(miner::address());
  body += ",\"wifiSaved\":" + String(password.length() ? "true" : "false");
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

  // La WiFi con internet para los zaps. Contraseña vacía con la misma red = se queda la que había.
  if (server.hasArg("wifi")) {
    String ssid = server.arg("wifi");
    ssid.trim();
    String oldSsid, password;
    storage::loadNetwork(oldSsid, password);
    if (server.arg("wifipass").length() || ssid != oldSsid) password = server.arg("wifipass");
    storage::saveNetwork(ssid.substring(0, 32), password.substring(0, 63));
  }

  // De paso, el móvil nos dice la hora
  const uint32_t now = server.arg("now").toInt();
  if (now > 1700000000) badge::setTime(now);
  if (server.hasArg("tz")) badge::setTimezone(server.arg("tz").toInt());

  // NerdMiner: pool (host:puerto) y dirección que cobra
  if (server.hasArg("maddr")) {
    String pool = server.arg("mpool");
    pool.trim();
    const int colon = pool.lastIndexOf(':');
    const String host = colon > 0 ? pool.substring(0, colon) : pool;
    const uint16_t port = colon > 0 ? pool.substring(colon + 1).toInt() : 0;
    String addr = server.arg("maddr");
    addr.trim();
    miner::setConfig(host, port, addr);
  }

  saved = true;
  Serial.printf("[portal] perfil guardado: %s (%s)\n", me.name.c_str(), roleInfo(me.role).name);
  server.send(200, "application/json", "{\"ok\":true}");
}

// Las WiFi que ve el badge, de la más fuerte a la más débil, sin repetir
static void sendNetworks() {
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) WiFi.scanNetworks(true);      // no hay búsqueda en marcha: empezamos una
  String body = "[";
  for (int i = 0; i < n; i++) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty() || body.indexOf(json(ssid)) >= 0) continue;
    if (body.length() > 1) body += ",";
    body += json(ssid);
  }
  server.send(200, "application/json", body + "]");
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
  WiFi.mode(WIFI_AP_STA);                // AP para el móvil y STA para poder buscar redes
  WiFi.softAP(badge::wifiName().c_str());   // abierta: sin contraseña
  delay(100);
  WiFi.scanNetworks(true);               // la lista de redes para la página, sin esperar

  dns.start(53, "*", WiFi.softAPIP());   // todas las direcciones apuntan a nosotros
  MDNS.begin("badge");                   // y además badge.local

  server.on("/", HTTP_GET, sendPage);
  server.on("/api/profile", HTTP_GET, sendProfile);
  server.on("/api/profile", HTTP_POST, saveProfile);
  server.on("/avatar.svg", HTTP_GET, sendAvatar);
  server.on("/api/networks", HTTP_GET, sendNetworks);
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
