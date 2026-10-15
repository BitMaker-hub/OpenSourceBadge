"""Pasa los logos de assets/logos a imágenes RGB565 para la pantalla (src/ui/logos_data.h).

    "C:\\Program Files\\KiCad\\9.0\\bin\\python.exe" tools/exportar_logos.py

(cualquier python con Pillow vale; el de KiCad ya lo trae)

La transparencia se mezcla aquí con el color de fondo de la pantalla (0x0C0E13),
así en el badge cada logo es un rectángulo de píxeles y se pinta de golpe.
El logo de Bitronics viene en SVG: se pasa antes a PNG con Chrome (ver README de assets).
"""
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "assets", "logos")
DEST = os.path.join(HERE, "..", "src", "ui", "logos_data.h")
BG = (0x0C, 0x0E, 0x13)

# nombre en C, fichero, recorte opcional (x0, y0, x1, y1) en px del original, ancho final
LOGOS = [
    ("LOGO_BAMS_MEDALLION", "bAMS_Medallion_Orange.png", None, 112),
    ("LOGO_BAMS_WORDMARK", "bAMS_Logo_OrangeWhite.png", (560, 0, 1803, 515), 150),
    ("LOGO_BITMAKER", "bitmaker.png", None, 96),
    ("LOGO_BITRONICS", "bitronics.png", None, 150),
]


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def load(name, crop, width):
    im = Image.open(os.path.join(SRC, name)).convert("RGBA")
    if crop:
        im = im.crop(crop)
    im = im.crop(im.getbbox())                      # fuera el margen transparente
    height = round(im.height * width / im.width)
    im = im.resize((width, height), Image.LANCZOS)
    flat = Image.new("RGBA", im.size, BG + (255,))
    flat.alpha_composite(im)
    return flat.convert("RGB")


out = ["#pragma once",
       "// GENERADO por tools/exportar_logos.py a partir de assets/logos. No se edita a mano.",
       "// Solo lo incluye ui/screens.cpp: si lo incluyera otro fichero, las imágenes irían dos veces a la flash.",
       "#include \"display.h\"",
       ""]
for cname, fname, crop, width in LOGOS:
    im = load(fname, crop, width)
    px = [rgb565(*im.getpixel((x, y))) for y in range(im.height) for x in range(im.width)]
    out.append(f"// {fname}: {im.width} x {im.height}")
    out.append(f"static const uint16_t {cname}_PIXELS[] = {{")
    for i in range(0, len(px), 16):
        out.append("  " + ", ".join(f"0x{p:04X}" for p in px[i:i + 16]) + ",")
    out.append("};")
    out.append(f"static const Logo {cname} = {{{im.width}, {im.height}, {cname}_PIXELS}};")
    out.append("")
    print(f"{cname}: {im.width} x {im.height}")

with open(DEST, "w", encoding="utf-8") as f:
    f.write("\n".join(out))
print("escrito", DEST)
