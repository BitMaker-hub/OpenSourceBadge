"""Graba un badge para el evento: número de serie, monedero LNbits y (opcional) la WiFi.

    py tools/grabar_badge.py COM67 42
    py tools/grabar_badge.py COM67 42 --sin-monedero

Lee la configuración de tools/.env (NO se sube al repo; ver tools/.env.ejemplo):

    LNBITS_URL=https://truthfulrat9.lnbits.com
    LNBITS_TOKEN=...            token de API de la cuenta (perfil de LNbits). Solo para crear monederos;
                                no va nunca al badge
    LNBITS_DOMINIO=             (opcional) el dominio de las direcciones; por defecto, el de LNBITS_URL
    WIFI_SSID=BitcoinAmsterdam  (opcional: la WiFi del evento)
    WIFI_PASS=...               (opcional)

Para cada badge:
  1. crea un monedero "Badge #042" en la cuenta,
  2. le crea su dirección Lightning badge042@dominio (extensión LNURLp) con zaps de Nostr,
  3. le crea un enlace de retirada (extensión Withdraw) que solo puede sacar de ese monedero,
  4. graba en el badge por el puerto serie: número (n042), monedero (L...) y WiFi (W...).
El badge solo se queda con la clave de lectura (inkey): ve los pagos, no puede gastar.

Adaptado a LNbits 1.6.2 (truthfulrat9.lnbits.com, 2026-10-30), mirando su /openapi.json: crear un monedero
pide el token de la cuenta (Bearer); las direcciones y las retiradas, la clave de admin del monedero nuevo.
Falta probarlo de punta a punta con un badge.
"""
import json, os, sys, time, urllib.parse, urllib.request, urllib.error

HERE = os.path.dirname(os.path.abspath(__file__))


def load_env():
    env = {}
    path = os.path.join(HERE, ".env")
    if not os.path.exists(path):
        raise SystemExit("Falta tools/.env (copia tools/.env.ejemplo y rellénalo)")
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            k, v = line.split("=", 1)
            env[k.strip()] = v.strip()
    return env


def api(base, path, key=None, body=None, token=None):
    headers = {"Content-Type": "application/json"}
    if key:
        headers["X-Api-Key"] = key                         # clave de un monedero
    if token:
        headers["Authorization"] = "Bearer " + token      # token de la cuenta
    req = urllib.request.Request(base.rstrip("/") + path, method="POST" if body is not None else "GET",
                                 data=json.dumps(body).encode() if body is not None else None, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return json.loads(r.read())
    except urllib.error.HTTPError as e:
        raise SystemExit("LNbits dice %d en %s: %s" % (e.code, path, e.read().decode(errors="replace")[:300]))


def make_wallet(env, number):
    base, token = env["LNBITS_URL"].rstrip("/"), env["LNBITS_TOKEN"]
    domain = env.get("LNBITS_DOMINIO") or urllib.parse.urlparse(base).hostname
    name = "Badge #%03d" % number
    wallet = api(base, "/api/v1/wallet", body={"name": name}, token=token)
    user = "badge%03d" % number
    api(base, "/lnurlp/api/v1/links", wallet["adminkey"], {
        "wallet": wallet["id"], "description": name, "min": 1, "max": 1_000_000, "comment_chars": 140,
        "username": user, "zaps": True})
    withdraw = api(base, "/withdraw/api/v1/links", wallet["adminkey"], {
        "title": name, "min_withdrawable": 1, "max_withdrawable": 1_000_000, "uses": 50, "wait_time": 1,
        "is_unique": False})
    address = "%s@%s" % (user, domain)
    # Comprobar que la dirección responde y admite zaps antes de grabarla en el badge
    try:
        with urllib.request.urlopen("https://%s/.well-known/lnurlp/%s" % (domain, user), timeout=20) as r:
            info = json.loads(r.read())
        print("Dirección %s: %s, zaps %s" % (address, "responde" if info.get("callback") else "RARA",
                                             "sí" if info.get("allowsNostr") else "NO"))
    except Exception as e:
        print("OJO: la dirección %s no responde todavía (%s)" % (address, e))
    return {"url": base, "inkey": wallet["inkey"], "address": address, "withdraw": "lightning:" + withdraw["lnurl"]}


def send(port, lines):
    import serial
    s = serial.Serial(port, 115200, timeout=1)
    time.sleep(9)                      # abrir el puerto reinicia el badge: esperar a que arranque
    s.reset_input_buffer()
    for line in lines:
        s.write(line.encode() + b"\n")
        time.sleep(0.8)
    s.write(b"i")
    time.sleep(1.5)
    print(s.read(4000).decode(errors="replace"))


def main():
    port, number = sys.argv[1], int(sys.argv[2])
    env = load_env()
    lines = ["n%d" % number]
    if "--sin-monedero" not in sys.argv:
        w = make_wallet(env, number)
        print("Monedero creado:", w["address"])
        lines.append("L%s|%s|%s|%s" % (w["url"], w["inkey"], w["address"], w["withdraw"]))
    if env.get("WIFI_SSID"):
        lines.append("W%s|%s" % (env["WIFI_SSID"], env.get("WIFI_PASS", "")))
    send(port, lines)


if __name__ == "__main__":
    main()
