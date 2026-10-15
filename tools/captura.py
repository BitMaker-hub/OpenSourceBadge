"""Capturas de la pantalla del badge por el puerto serie.

    python tools/captura.py COM67 salida/  "d" "c:perfil" "m" "c:qr" ...

Cada argumento después de la carpeta es un paso:
  - "c:nombre"  hace una captura y la guarda como nombre.png (al doble de tamaño)
  - "w:500"     espera 500 ms
  - cualquier otro texto se manda tal cual al badge (p, m, s, P, M, S, z, d...)
Entre paso y paso espera 400 ms para que el badge dibuje.
"""
import os
import sys
import time

import serial
from PIL import Image

port, outdir, steps = sys.argv[1], sys.argv[2], sys.argv[3:]
os.makedirs(outdir, exist_ok=True)
s = serial.Serial(port, 115200, timeout=2)
time.sleep(0.5)
s.reset_input_buffer()


def capture(name):
    s.reset_input_buffer()
    s.write(b"c")
    while True:
        line = s.readline()
        if not line:
            raise SystemExit("el badge no contesta")
        if line.startswith(b"SHOT"):
            break
    _, w, h = line.split()
    w, h = int(w), int(h)
    raw = s.read(w * h * 2)
    im = Image.new("RGB", (w, h))
    px = im.load()
    for i in range(w * h):
        c = raw[2 * i] << 8 | raw[2 * i + 1]
        px[i % w, i // w] = ((c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3)
    im = im.resize((w * 2, h * 2), Image.NEAREST)
    im.save(os.path.join(outdir, name + ".png"))
    print("captura", name)


for step in steps:
    if step.startswith("c:"):
        capture(step[2:])
    elif step.startswith("w:"):
        time.sleep(int(step[2:]) / 1000)
    else:
        s.write(step.encode())
    time.sleep(0.4)
