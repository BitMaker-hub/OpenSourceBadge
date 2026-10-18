"""Los logos del menú LAB (Seeder y NerdMiner) con transparencia: src/ui/lab_logos_data.h

    "C:\\Program Files\\KiCad\\9.0\\bin\\python.exe" tools/exportar_lab_logos.py [vista.png]

A diferencia de exportar_logos.py, aquí cada píxel lleva su alfa (0..255), porque
se pintan encima de tarjetas que cambian de color al seleccionarlas.

  - Seeder: el brote y la palabra SEEDER de Seeder/Logo/LOGO SEEDER.jpg. En el
    original son negros sobre un degradado; se saca la forma por lo oscuro que es
    cada píxel y se pinta de verde Seeder (hojas) y marrón (la semilla).
  - NerdMiner: el logotipo de arriba a la derecha de NerdMiner_v2/images/bgNerdMinerV2.png,
    blanco y amarillo sobre negro; lo negro pasa a transparente.

Con un segundo argumento guarda una vista previa sobre el fondo de las tarjetas.
"""
import os
import sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "assets", "logos")
DEST = os.path.join(HERE, "..", "src", "ui", "lab_logos_data.h")

GREEN = (0x5C, 0xD6, 0x8A)        # el verde de la Seeder, un pelo más vivo para el fondo oscuro
GREEN_LIGHT = (0x9C, 0xEB, 0xB6)  # el brillo de las hojas
BROWN = (0x9A, 0x63, 0x36)        # la semilla
BROWN_LIGHT = (0xC8, 0x8E, 0x55)


def lum(p):
    return 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]


def seeder_part(box, width, colorize):
    """Recorta del JPG y convierte lo oscuro en forma con color propio."""
    im = Image.open(os.path.join(SRC, "seeder.jpg")).convert("RGB").crop(box)
    out = Image.new("RGBA", im.size)
    for y in range(im.height):
        for x in range(im.width):
            p = im.getpixel((x, y))
            a = max(0.0, min(1.0, (175 - lum(p)) / 130))   # el fondo ronda 190-215 de luminancia
            if a <= 0:
                continue
            out.putpixel((x, y), colorize(p, x, y) + (round(a * 255),))
    out = out.crop(out.getbbox())
    h = round(out.height * width / out.width)
    return out.resize((width, h), Image.LANCZOS)


def sprout_color(p, x, y):
    seed = y > 285                       # coordenadas dentro del recorte: la semilla está abajo
    brownish = p[0] > p[2] + 12          # el reflejo marrón del original
    grey = lum(p) > 35                   # el brillo gris de las hojas
    if seed:
        return BROWN_LIGHT if brownish else BROWN
    return GREEN_LIGHT if grey else GREEN


def nerdminer(width):
    im = Image.open(os.path.join(SRC, "nerdminer_bg.png")).convert("RGB").crop((1338, 108, 1572, 268))
    out = Image.new("RGBA", im.size)
    for y in range(im.height):
        for x in range(im.width):
            p = im.getpixel((x, y))
            m = max(p)
            a = max(0.0, min(1.0, (m - 40) / 120))
            if (y > 135 and p[0] > 2 * max(p[1], p[2]) + 20) or (x < 20 and y > 90):
                continue                                     # el resplandor rojo de la carcasa de la foto
            if a <= 0:
                continue
            k = 255 / max(m, 1)                                  # el color sin el negro mezclado
            c = tuple(min(255, round(v * k)) for v in p)
            out.putpixel((x, y), c + (round(a * 255),))
    out = out.crop(out.getbbox())
    h = round(out.height * width / out.width)
    return out.resize((width, h), Image.LANCZOS)


LOGOS = [
    ("LAB_SPROUT", seeder_part((225, 245, 565, 835), 44, sprout_color)),
    ("LAB_SEEDER_WORD", seeder_part((580, 280, 1565, 485), 84, lambda p, x, y: GREEN)),
    ("LAB_NERDMINER", nerdminer(112)),
]


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


out = ["#pragma once",
       "// GENERADO por tools/exportar_lab_logos.py a partir de assets/logos. No se edita a mano.",
       "// Solo lo incluye ui/screens_seed.cpp.",
       "#include \"display.h\"",
       ""]
for cname, im in LOGOS:
    px = [im.getpixel((x, y)) for y in range(im.height) for x in range(im.width)]
    out.append(f"// {im.width} x {im.height}, color + alfa")
    out.append(f"static const uint16_t {cname}_PIXELS[] = {{")
    for i in range(0, len(px), 16):
        out.append("  " + ", ".join(f"0x{rgb565(*p[:3]):04X}" for p in px[i:i + 16]) + ",")
    out.append("};")
    out.append(f"static const uint8_t {cname}_ALPHA[] = {{")
    for i in range(0, len(px), 24):
        out.append("  " + ",".join(str(p[3]) for p in px[i:i + 24]) + ",")
    out.append("};")
    out.append(f"static const AlphaLogo {cname} = {{{im.width}, {im.height}, {cname}_PIXELS, {cname}_ALPHA}};")
    out.append("")
    print(f"{cname}: {im.width} x {im.height}")

with open(DEST, "w", encoding="utf-8") as f:
    f.write("\n".join(out))
print("escrito", DEST)

if len(sys.argv) > 1:
    card = Image.new("RGBA", (360, 120), (0x1A, 0x1D, 0x26, 255))
    x = 8
    for _, im in LOGOS:
        card.alpha_composite(im, (x, 8))
        x += im.width + 12
    card.resize((card.width * 3, card.height * 3), Image.NEAREST).save(sys.argv[1])
