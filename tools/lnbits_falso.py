"""Un LNbits de mentira para probar el badge sin servidor de verdad.

    py tools/lnbits_falso.py 5055

Responde como LNbits en lo que usa el badge:
  - GET  /api/v1/wallet           (cabecera X-Api-Key)  -> {"balance": msat}
  - WS   /api/v1/ws/<inkey>                              -> avisos de pago
  - GET  /api/v1/payments?limit=10                        -> los últimos pagos (lo pide al reconectar)
Y, para lanzar pagos de prueba:
  - GET  /pagar?sats=2100&comentario=hola                -> pago normal (LNURLp con comentario)
  - GET  /zap?sats=500&mensaje=gm                        -> zap de Nostr (con la petición de zap en extra.nostr)
  Con &silencio=1 el pago se apunta pero NO se avisa por el websocket: como si llegara con la radio dormida.
La hora de los pagos va en texto ISO, como en LNbits 1.x.
El formato de los avisos es el de lnbits/core/services/notifications.py:
  {"wallet_balance": sats, "payment": {...}}
"""
import asyncio, json, sys, time, hashlib, urllib.parse
from http import HTTPStatus
from websockets.asyncio.server import serve
from websockets.http11 import Response
from websockets.datastructures import Headers

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 5055
balance_msat = 21_000_000
clients = set()
counter = 0
history = []          # los pagos, del más nuevo al más antiguo


def http(status, body):
    data = json.dumps(body).encode()
    return Response(status.value, status.phrase, Headers([("Content-Type", "application/json"), ("Content-Length", str(len(data)))]), data)


def payment(sats, extra):
    global counter, balance_msat
    counter += 1
    balance_msat += sats * 1000
    bolt11 = "lnbc%dn1p%s" % (sats * 10, hashlib.sha256(str(time.time() + counter).encode()).hexdigest()[:50])
    return {
        "wallet_balance": balance_msat // 1000,
        "payment": {"checking_id": str(counter), "pending": False, "status": "success", "amount": sats * 1000,
                    "fee": 0, "memo": "Badge #308", "time": time.strftime("%Y-%m-%dT%H:%M:%S.000000+00:00", time.gmtime()), "bolt11": bolt11,
                    "payment_hash": hashlib.sha256(bolt11.encode()).hexdigest(), "extra": extra},
    }


async def broadcast(msg):
    for ws in list(clients):
        try:
            await ws.send(json.dumps(msg))
        except Exception:
            pass
    print("aviso enviado a", len(clients), "badge(s):", msg["payment"]["amount"] // 1000, "sats", flush=True)


def process_request(connection, request):
    url = urllib.parse.urlparse(request.path)
    q = dict(urllib.parse.parse_qsl(url.query))
    if url.path.startswith("/api/v1/ws/"):
        return None                                   # websocket: lo atiende handler()
    if url.path == "/api/v1/payments":
        print("lista de pagos pedida", flush=True)
        return http(HTTPStatus.OK, history[:int(q.get("limit", 10))])
    if url.path == "/api/v1/wallet":
        print("saldo pedido con la clave", request.headers.get("X-Api-Key"), flush=True)
        return http(HTTPStatus.OK, {"id": "w1", "name": "Badge #308", "balance": balance_msat})
    if url.path == "/pagar":
        sats = int(q.get("sats", 2100))
        msg = payment(sats, {"tag": "lnurlp", "link": "abc", "extra": str(sats * 1000),
                             "comment": q.get("comentario", "Gracias por la charla"), "lnaddress": "badge308@localhost"})
        history.insert(0, msg["payment"])
        if q.get("silencio") != "1": asyncio.get_event_loop().create_task(broadcast(msg))
        else: print("pago apuntado SIN aviso:", sats, "sats", flush=True)
        return http(HTTPStatus.OK, {"ok": True})
    if url.path == "/zap":
        sats = int(q.get("sats", 500))
        zap_request = {"kind": 9734, "pubkey": "40b9c85fffeafc1cadf8c30a4e5c88660ff6e4971a0dc723d5ab674b5e61b451",
                       "content": q.get("mensaje", "Zap desde el LNbits de prueba"), "tags": [["amount", str(sats * 1000)]],
                       "created_at": int(time.time()), "id": "0" * 64, "sig": "0" * 128}
        msg = payment(sats, {"tag": "lnurlp", "link": "abc", "extra": str(sats * 1000), "nostr": json.dumps(zap_request),
                             "lnaddress": "badge308@localhost"})
        history.insert(0, msg["payment"])
        if q.get("silencio") != "1": asyncio.get_event_loop().create_task(broadcast(msg))
        else: print("pago apuntado SIN aviso:", sats, "sats", flush=True)
        return http(HTTPStatus.OK, {"ok": True})
    return http(HTTPStatus.NOT_FOUND, {"detail": "no"})


async def handler(ws):
    print("badge conectado al websocket:", ws.request.path, flush=True)
    clients.add(ws)
    try:
        async for _ in ws:
            pass
    finally:
        clients.discard(ws)
        print("badge desconectado", flush=True)


async def main():
    async with serve(handler, "0.0.0.0", PORT, process_request=process_request):
        print("LNbits de prueba en el puerto", PORT, flush=True)
        await asyncio.Future()

asyncio.run(main())
