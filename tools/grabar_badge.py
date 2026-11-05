"""Graba un badge para el evento: número de serie, monedero LNbits y (opcional) la WiFi.

    py tools/grabar_badge.py COM67 42
    py tools/grabar_badge.py COM67 42 --sin-monedero
    py tools/grabar_badge.py --lote 101-130      en lote: graba cada badge según se enchufa (ver abajo)
    py tools/grabar_badge.py --wifi "Red" "clave"   solo la WiFi, a cada badge según se enchufa: sin
                                                 tocar número ni monedero, y sin .env ni internet

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

En lote (--lote 101-130, o --lote 101 para seguir hasta Ctrl+C) no hace falta decir el puerto:
espera a que se enchufe un badge, lo graba, lo reinicia y comprueba lo que dice tras el reinicio
(así se sabe que está en la flash y no solo en la memoria). Luego, a por el siguiente; con un hub
se pueden enchufar varios a la vez. No toca nada que no conteste como un badge, ni el número y el
monedero de uno que ya tenga monedero: a ese, como mucho, le pone la WiFi del .env si no la tiene
(así se puede pasar el lote otra vez solo para la WiFi). Se salta los números que ya tienen monedero en LNbits o están en tools/lote.csv,
el registro de lo grabado (no lleva claves, pero sí las direcciones: no se sube al repo).
"""
import codecs, csv, json, os, re, sys, time, urllib.parse, urllib.request, urllib.error

HERE = os.path.dirname(os.path.abspath(__file__))
REGISTRO = os.path.join(HERE, "lote.csv")
ESPRESSIF = 0x303A          # el USB propio del ESP32-S3: así sale el badge (y cualquier otro S3 por USB)


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


def badge_lines(env, number, wallet):
    lines = ["n%d" % number]
    if wallet:
        lines.append("L%s|%s|%s||%s" % (wallet["url"], wallet["inkey"], wallet["address"], wallet["admin"]))   # sin enlace fijo
    if env.get("WIFI_SSID"):
        lines.append("W%s|%s" % (env["WIFI_SSID"], env.get("WIFI_PASS", "")))
    return lines


# ============================================================ en lote

# Lo que contesta a «i»: "Badge #042 Nombre, firmware v1.0.0 (0d61bf7)", más abajo
# "  WiFi:  Red (con contraseña)" y "  LNbits: escuchando, badge042@dominio, saldo 0 sats".
# Los firmwares de antes de la 1.0.0 no dicen la versión.
INFO = re.compile(r"Badge #(\d+)[^\r\n]*?(?:, firmware v(\S+) \(([^)]*)\))?\r?\n"
                  r"[\s\S]*?WiFi:\s+([^\r\n]*)\r?\n"
                  r"[\s\S]*?LNbits: ([^,\r\n]*), ([^,\r\n]*), saldo")
CON_CLAVE = " (con contraseña)"


def badge_ports():
    from serial.tools import list_ports
    return {p.device for p in list_ports.comports() if p.vid == ESPRESSIF}


class Badge:
    """Un badge enchufado. Se abre con DTR y RTS a cero ANTES de abrir, para no reiniciarlo."""

    def __init__(self, port):
        import serial
        self.port = port
        self.s = serial.Serial()
        self.s.port, self.s.baudrate, self.s.timeout = port, 115200, 0.2
        self.s.dtr = self.s.rts = False
        self.s.open()
        self.decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")   # la ñ puede llegar partida

    def info(self, timeout=30):
        """Lo que contesta a «i», troceado, o None si no contesta como un badge. Pregunta cada 1,5 s:
        uno que aún arranca (y el primer arranque formatea la flash) pierde lo que le llega antes."""
        text, end, ask = "", time.time() + timeout, 0
        while time.time() < end:
            if time.time() >= ask:
                self.s.write(b"i")
                ask = time.time() + 1.5
            text += self.decoder.decode(self.s.read(4096))
            m = INFO.search(text)
            if m:
                wifi = m.group(4)
                return {"number": int(m.group(1)),
                        "firmware": "v%s (%s)" % (m.group(2), m.group(3)) if m.group(2) else "?",
                        "ssid": "" if wifi == "(ninguna)" else wifi[:-len(CON_CLAVE)] if wifi.endswith(CON_CLAVE) else wifi,
                        "address": m.group(6).strip()}
        return None

    def send(self, lines):
        for line in lines:
            self.s.write(line.encode() + b"\n")
            time.sleep(0.8)

    def restart(self):
        """Reinicia en el firmware, con BOOT suelto (DTR a cero), como el «hard reset» de esptool."""
        self.s.rts = True
        self.s.dtr = False      # el driver de Windows solo manda el RTS nuevo si se toca también el DTR
        time.sleep(0.1)
        self.s.rts = False
        self.s.dtr = False

    def info_after_restart(self):
        import serial
        self.restart()
        time.sleep(1)
        try:
            return self.info()
        except serial.SerialException:
            # Si el puerto se ha ido con el reinicio, se espera a que vuelva y se pregunta otra vez
            self.s.close()
            end = time.time() + 15
            while self.port not in badge_ports() and time.time() < end:
                time.sleep(0.5)
            self.s.open()
            return self.info()

    def close(self):
        self.s.close()


def taken_numbers(env, with_wallet):
    """Los números que ya son de alguien: los del registro y los que tienen monedero en LNbits."""
    taken = set()
    if os.path.exists(REGISTRO):
        with open(REGISTRO, encoding="utf-8", newline="") as f:
            taken |= {int(row["numero"]) for row in csv.DictReader(f) if row.get("resultado") == "ok"}
    if with_wallet:
        for w in api(env["LNBITS_URL"].rstrip("/"), "/api/v1/wallets", token=env["LNBITS_TOKEN"]):
            m = re.fullmatch(r"Badge #(\d+)", w.get("name", ""))
            if m and not w.get("deleted"):
                taken.add(int(m.group(1)))
    return taken


def log(number, port, address, ssid, firmware, result):
    new = not os.path.exists(REGISTRO)
    with open(REGISTRO, "a", encoding="utf-8", newline="") as f:
        out = csv.writer(f)
        if new:
            out.writerow(["fecha", "numero", "puerto", "direccion", "wifi", "firmware", "resultado"])
        out.writerow([time.strftime("%Y-%m-%d %H:%M:%S"), number, port, address, ssid, firmware, result])


def provision(port, number, last, taken, env, with_wallet):
    """Graba el badge de ese puerto con el primer número libre desde «number». Devuelve el
    número por el que seguir, o None si ya no quedan del lote."""
    badge = Badge(port)
    try:
        before = badge.info()
        if not before:
            print("%s: no contesta como un badge (otro aparato, o un badge sin el firmware). No le toco nada." % port)
            return number
        if before["address"]:
            # Ya grabado: el número y el monedero no se tocan. Como mucho, la WiFi del .env si no la tiene
            ssid = env.get("WIFI_SSID", "").strip()
            if not ssid or before["ssid"] == ssid:
                print("%s: es el #%03d y ya tiene monedero (%s)%s. No le toco nada."
                      % (port, before["number"], before["address"], " y la WiFi " + ssid if ssid else ""))
                return number
            print("%s: es el #%03d y ya tiene monedero (%s). Solo le pongo la WiFi %s."
                  % (port, before["number"], before["address"], ssid))
            badge.send(["W%s|%s" % (ssid, env.get("WIFI_PASS", ""))])
            after = badge.info()
            ok = after and after["ssid"] == ssid and after["address"] == before["address"]
            print("  #%03d %s" % (before["number"], "listo. Desenchúfalo y enchufa el siguiente." if ok else
                  "FALLO: dice tener la WiFi %s" % (after["ssid"] or "ninguna") if after else "FALLO: ha dejado de contestar"))
            log(before["number"], port, before["address"], ssid, before["firmware"], "ok, solo WiFi" if ok else "fallo al poner la WiFi")
            return number
        if not with_wallet and before["number"] in taken:
            print("%s: es el #%03d, ya grabado en este lote. No le toco nada." % (port, before["number"]))
            return number
        while number in taken:
            number += 1
        if last is not None and number > last:
            print("%s: ya no quedan números en el lote (el último era el #%03d). No le toco nada." % (port, last))
            return None
        print("%s: firmware %s, será el #%03d" % (port, before["firmware"], number))
        wallet = make_wallet(env, number) if with_wallet else None
        badge.send(badge_lines(env, number, wallet))
        after = badge.info_after_restart()
        problems = []
        if not after:
            problems.append("no ha vuelto a contestar tras reiniciarlo")
        else:
            if after["number"] != number:
                problems.append("dice ser el #%03d" % after["number"])
            if wallet and after["address"] != wallet["address"]:
                problems.append("su monedero es %s" % (after["address"] or "ninguno"))
            if env.get("WIFI_SSID") and after["ssid"] != env["WIFI_SSID"].strip():
                problems.append("su WiFi es %s" % (after["ssid"] or "ninguna"))
        taken.add(number)
        log(number, port, wallet["address"] if wallet else "", env.get("WIFI_SSID", ""), before["firmware"],
            "ok" if not problems else "; ".join(problems))
        if problems:
            # A mano reutiliza su monedero (va por el nombre), y el lote ya no lo toca si lo tiene
            print("  FALLO en el #%03d: %s. Grábalo a mano: py tools/grabar_badge.py %s %d"
                  % (number, ", ".join(problems), port, number))
        else:
            print("  #%03d listo%s%s. Desenchúfalo y enchufa el siguiente."
                  % (number, ": " + wallet["address"] if wallet else "",
                     ", WiFi " + env["WIFI_SSID"] if env.get("WIFI_SSID") else ""))
        return number + 1
    finally:
        badge.close()


def set_wifi(port, ssid, password):
    """Solo la WiFi: ni número ni monedero, y sin LNbits ni internet."""
    badge = Badge(port)
    try:
        before = badge.info()
        if not before:
            print("%s: no contesta como un badge (otro aparato, o un badge sin el firmware). No le toco nada." % port)
            return
        badge.send(["W%s|%s" % (ssid, password)])
        after = badge.info()
        ok = after and after["ssid"] == ssid and after["number"] == before["number"] and after["address"] == before["address"]
        if ok:
            print("%s: #%03d con la WiFi %s%s. Desenchúfalo y enchufa el siguiente."
                  % (port, after["number"], ssid, " (su monedero, %s, sigue ahí)" % after["address"] if after["address"] else " (no tiene monedero)"))
        else:
            print("%s: FALLO en el #%03d: %s" % (port, before["number"], "dice tener la WiFi %s" % (after["ssid"] or "ninguna")
                                                  if after else "ha dejado de contestar"))
        log(before["number"], port, before["address"], ssid, before["firmware"], "ok, solo WiFi" if ok else "fallo al poner la WiFi")
    finally:
        badge.close()


def watch(handle):
    """Espera badges y se los pasa a handle(puerto) de uno en uno. Acaba con Ctrl+C o si handle
    devuelve False."""
    import serial
    done, tries = set(), {}
    try:
        while True:
            present = badge_ports()
            done &= present             # el que se desenchufa se olvida: su puerto puede volver con otro badge
            tries = {p: n for p, n in tries.items() if p in present}
            waiting = sorted(present - done)
            if not waiting:
                time.sleep(0.5)
                continue
            port = waiting[0]
            done.add(port)
            try:
                if handle(port) is False:
                    break
            except serial.SerialException as e:
                # Recién enchufado, a veces el puerto aparece y desaparece un momento: se reintenta.
                # Repetirlo no lía nada, porque lo que ya tenga grabado no se vuelve a tocar.
                tries[port] = tries.get(port, 0) + 1
                if tries[port] < 4:
                    print("%s: no se deja abrir todavía, lo reintento en 2 s" % port)
                    done.discard(port)
                    time.sleep(2)
                else:
                    print("%s: no se deja abrir (%s). Desenchúfalo y vuelve a enchufarlo." % (port, e))
    except KeyboardInterrupt:
        pass


def batch(env, spec, with_wallet):
    first, _, last = spec.partition("-")
    number, last = int(first), int(last) if last else None
    if not 0 < number <= (last or 999) <= 999:
        raise SystemExit("El lote va del 1 al 999: --lote 101-130")
    taken = taken_numbers(env, with_wallet)
    print("Lote desde el #%03d%s. Enchufa los badges, de uno en uno o varios en un hub. Ctrl+C para acabar."
          % (number, " hasta el #%03d" % last if last else ""))
    state = {"number": number}

    def handle(port):
        state["number"] = provision(port, state["number"], last, taken, env, with_wallet)
        return state["number"] is not None and (last is None or state["number"] <= last)

    watch(handle)
    print("Fin del lote. Lo grabado está en", REGISTRO)


def wifi_batch(ssid, password):
    # Lo mismo que rechaza el flasher: el badge recorta los dos campos y parte la línea en la primera barra
    if not ssid or len(ssid.encode()) > 32 or "|" in ssid or ssid != ssid.strip():
        raise SystemExit("La red: de 1 a 32 caracteres, sin | ni espacios al principio o al final")
    if password != password.strip() or (password and not 8 <= len(password) <= 63):
        raise SystemExit("La contraseña: de 8 a 63 caracteres sin espacios al principio o al final, o nada si es abierta")
    print("Solo la WiFi %s%s. Enchufa los badges, de uno en uno o varios en un hub. Ctrl+C para acabar."
          % (ssid, " con contraseña" if password else ", abierta"))
    watch(lambda port: set_wifi(port, ssid, password))
    print("Fin. Lo grabado está en", REGISTRO)


def main():
    if "--wifi" in sys.argv:                    # sin .env: no hace falta LNbits
        args = sys.argv[sys.argv.index("--wifi") + 1:]
        if not args:
            raise SystemExit('Falta la red: py tools/grabar_badge.py --wifi "Red" "contraseña"')
        wifi_batch(args[0], args[1] if len(args) > 1 else "")
        return
    with_wallet = "--sin-monedero" not in sys.argv
    env = load_env()
    if "--lote" in sys.argv:
        batch(env, sys.argv[sys.argv.index("--lote") + 1], with_wallet)
        return
    port, number = sys.argv[1], int(sys.argv[2])
    w = None
    if with_wallet:
        w = make_wallet(env, number)
        print("Monedero listo:", w["address"])
    send(port, badge_lines(env, number, w))


if __name__ == "__main__":
    main()
