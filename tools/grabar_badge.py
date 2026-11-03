"""Graba un badge para el evento: número de serie, monedero LNbits y (opcional) la WiFi.

    py tools/grabar_badge.py COM67 42
    py tools/grabar_badge.py COM67 42 --sin-monedero

Lee la configuración de tools/.env (NO se sube al repo; ver tools/.env.ejemplo):

    LNBITS_URL=https://tu-instancia.lnbits.com
    LNBITS_TOKEN=...            token de API de la cuenta (perfil de LNbits). Solo para crear monederos;
                                no va nunca al badge
    LNBITS_DOMINIO=             (opcional) el dominio de las direcciones; por defecto, el de LNBITS_URL
    WIFI_SSID=BitcoinAmsterdam  (opcional: la WiFi del evento)
    WIFI_PASS=...               (opcional)

Para cada badge (si ya existe, se reutiliza: se puede volver a grabar sin duplicar nada):
  1. su monedero "Badge #042" en la cuenta,
  2. su dirección Lightning badge042@dominio (extensión LNURLp) con zaps de Nostr,
  3. borra los enlaces de retirada que tuviera (el badge crea el suyo por el saldo al retirar),
  4. graba en el badge por el puerto serie: número (n042), monedero (L...) y WiFi (W...).
El badge se queda con la clave de lectura (ver pagos) y con la de ADMIN de SU monedero, para crear
la retirada por el saldo. Nunca con el token de la cuenta.

Hecho para LNbits 1.6.2 (mirando su /openapi.json): crear un monedero pide el token de la cuenta
(Bearer); las direcciones y las retiradas, la clave de admin del monedero nuevo.
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


def delete(base, path, key):
    req = urllib.request.Request(base + path, method="DELETE", headers={"X-Api-Key": key})
    with urllib.request.urlopen(req, timeout=20) as r:
        r.read()


def make_wallet(env, number):
    base, token = env["LNBITS_URL"].rstrip("/"), env["LNBITS_TOKEN"]
    domain = env.get("LNBITS_DOMINIO") or urllib.parse.urlparse(base).hostname
    name = "Badge #%03d" % number
    user = "badge%03d" % number
    # 1. El monedero: si ya hay uno con este nombre, ese
    existing = [w for w in api(base, "/api/v1/wallets", token=token) if w.get("name") == name and not w.get("deleted")]
    if existing:
        wallet = existing[0]
        print("Monedero %s: ya existía, se reutiliza" % name)
    else:
        wallet = api(base, "/api/v1/wallet", body={"name": name}, token=token)
        print("Monedero %s: creado" % name)
    key = wallet["adminkey"]
    # 2. La dirección Lightning (con zaps), si aún no la tiene
    links = api(base, "/lnurlp/api/v1/links", key)
    if not any(l.get("username") == user for l in links):
        api(base, "/lnurlp/api/v1/links", key, {
            "wallet": wallet["id"], "description": name, "min": 1, "max": 1_000_000, "comment_chars": 140,
            "username": user, "zaps": True})
    # 3. Fuera los enlaces de retirada que hubiera (los fijos de antes enseñaban un máximo de 1.000.000 sats)
    old = api(base, "/withdraw/api/v1/links", key)
    old = old.get("data", old) if isinstance(old, dict) else old
    for link in old:
        delete(base, "/withdraw/api/v1/links/%s" % link["id"], key)
    if old:
        print("Enlaces de retirada viejos borrados:", len(old))
    address = "%s@%s" % (user, domain)
    # Comprobar que la dirección responde y admite zaps antes de grabarla en el badge
    try:
        with urllib.request.urlopen("https://%s/.well-known/lnurlp/%s" % (domain, user), timeout=20) as r:
            info = json.loads(r.read())
        print("Dirección %s: %s, zaps %s" % (address, "responde" if info.get("callback") else "RARA",
                                             "sí" if info.get("allowsNostr") else "NO"))
    except Exception as e:
        print("OJO: la dirección %s no responde todavía (%s)" % (address, e))
    return {"url": base, "inkey": wallet["inkey"], "address": address, "admin": key}


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
        print("Monedero listo:", w["address"])
        lines.append("L%s|%s|%s||%s" % (w["url"], w["inkey"], w["address"], w["admin"]))   # sin enlace fijo
    if env.get("WIFI_SSID"):
        lines.append("W%s|%s" % (env["WIFI_SSID"], env.get("WIFI_PASS", "")))
    send(port, lines)


if __name__ == "__main__":
    main()
