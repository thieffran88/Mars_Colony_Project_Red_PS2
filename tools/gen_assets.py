#!/usr/bin/env python3
"""Gera src/assets.h: atlas (alpha) com fontes bitmap + icones, e o planeta
Marte em RGBA. Tudo e embutido no executavel (o PS2 nao precisa ler arquivos).

Uso: python3 tools/gen_assets.py
Fonte: DejaVu Sans (licenca Bitstream Vera / DejaVu, livre para embutir).
"""
import math, os, random, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "assets.h")
FONT_DIR = "/usr/share/fonts/truetype/dejavu"
W = 512

FONTS = [  # nome, arquivo, tamanho
    ("S", "DejaVuSans.ttf", 12),
    ("M", "DejaVuSans-Bold.ttf", 15),
]
CPS = list(range(32, 256))
ICON_NAMES = ["leaf", "bulb", "atom", "drop", "gear", "star", "bolt", "colonist",
              "hand", "module", "earth", "rocket", "card", "arrow", "crown", "chip",
              "bcross", "bcircle", "btri", "bsquare"]
SS = 4  # supersampling dos icones


def draw_icon(name, s):
    S = s * SS
    im = Image.new("L", (S, S), 0)
    d = ImageDraw.Draw(im)
    c = S / 2
    u = S / 32.0
    def P(*pts):
        return [(x * u, y * u) for x, y in pts]
    if name == "leaf":
        ax, ay, bx, by = 5.0, 27.0, 28.0, 4.0
        dx, dy = bx - ax, by - ay
        ln = math.hypot(dx, dy)
        nx, ny = -dy / ln, dx / ln
        side1, side2 = [], []
        for k in range(41):
            t = k / 40.0
            wdt = math.sin(math.pi * t) ** 0.9 * 9.0
            px_, py_ = ax + dx * t, ay + dy * t
            side1.append(((px_ + nx * wdt) * u, (py_ + ny * wdt) * u))
            side2.append(((px_ - nx * wdt) * u, (py_ - ny * wdt) * u))
        d.polygon(side1 + side2[::-1], fill=255)
        d.line(P((3, 30), (7, 25), (22, 10)), fill=0, width=int(2.0*u))
        d.line(P((2, 31), (6, 26)), fill=255, width=int(2.4*u))
    elif name == "bulb":
        d.ellipse((7*u, 2*u, 25*u, 20*u), fill=255)
        d.polygon(P((10, 15), (22, 15), (20, 24), (12, 24)), fill=255)
        d.rectangle((11*u, 25*u, 21*u, 27*u), fill=255)
        d.rectangle((12*u, 28.5*u, 20*u, 30.5*u), fill=255)
        d.line(P((14, 22), (14, 14), (16, 11), (18, 14), (18, 22)), fill=0, width=int(1.8*u))
    elif name == "atom":
        w = int(2.6*u)
        for ang in (0, 60, 120):
            orb = Image.new("L", (S, S), 0)
            od = ImageDraw.Draw(orb)
            od.ellipse((2*u, 11*u, 30*u, 21*u), outline=255, width=w)
            orb = orb.rotate(ang, resample=Image.BICUBIC, center=(c, c))
            im.paste(255, (0, 0), orb)
        d.ellipse((12.5*u, 12.5*u, 19.5*u, 19.5*u), fill=255)
    elif name == "drop":
        d.ellipse((6*u, 11*u, 26*u, 31*u), fill=255)
        d.polygon(P((16, 1), (6.6, 18), (25.4, 18)), fill=255)
        d.arc((10*u, 15*u, 22*u, 27*u), 100, 170, fill=0, width=int(2.2*u))
    elif name == "gear":
        teeth = 8
        pts = []
        for i in range(teeth * 4):
            a = (i / (teeth * 4.0)) * 2 * math.pi
            r = 15 if (i % 4) in (0, 1) else 11
            pts.append((c + r * u * math.cos(a), c + r * u * math.sin(a)))
        d.polygon(pts, fill=255)
        d.ellipse((10*u, 10*u, 22*u, 22*u), fill=0)
        d.ellipse((13*u, 13*u, 19*u, 19*u), fill=255)
    elif name == "star":
        pts = []
        for i in range(10):
            a = -math.pi / 2 + i * math.pi / 5
            r = 15.5 if i % 2 == 0 else 6.5
            pts.append((c + r * u * math.cos(a), c + 1*u + r * u * math.sin(a)))
        d.polygon(pts, fill=255)
    elif name == "bolt":
        d.polygon(P((19, 1), (6, 18), (15, 18), (12, 31), (26, 12), (17, 12), (21, 1)), fill=255)
    elif name == "colonist":
        d.ellipse((10*u, 1*u, 22*u, 13*u), fill=255)
        d.pieslice((4*u, 13*u, 28*u, 43*u), 180, 360, fill=255)
        d.rectangle((6*u, 27*u, 26*u, 31*u), fill=255)
        d.ellipse((12.5*u, 4*u, 19.5*u, 9*u), fill=0)
    elif name == "hand":
        for i, (x, top) in enumerate([(7, 6), (11.5, 3), (16, 2), (20.5, 4)]):
            d.rounded_rectangle((x*u, top*u, (x+4)*u, 20*u), radius=2*u, fill=255)
        d.rounded_rectangle((7*u, 14*u, 25*u, 30*u), radius=5*u, fill=255)
        d.polygon(P((25, 22), (28, 13), (31, 14), (28, 26), (24, 28)), fill=255)
    elif name == "module":
        w = int(3*u)
        L = 9
        for (x, y, dx, dy) in [(3, 3, 1, 1), (29, 3, -1, 1), (3, 29, 1, -1), (29, 29, -1, -1)]:
            d.line(P((x, y + dy * L), (x, y), (x + dx * L, y)), fill=255, width=w)
        d.rectangle((11*u, 11*u, 21*u, 21*u), fill=255)
    elif name == "earth":
        d.ellipse((2*u, 2*u, 30*u, 30*u), fill=255)
        d.polygon(P((8, 8), (15, 6), (17, 11), (12, 15), (13, 20), (8, 18), (6, 12)), fill=0)
        d.polygon(P((19, 16), (25, 15), (27, 21), (22, 27), (19, 22)), fill=0)
    elif name == "rocket":
        d.polygon(P((16, 1), (22, 9), (22, 23), (10, 23), (10, 9)), fill=255)
        d.polygon(P((10, 15), (4, 24), (4, 28), (10, 23)), fill=255)
        d.polygon(P((22, 15), (28, 24), (28, 28), (22, 23)), fill=255)
        d.polygon(P((12, 24), (20, 24), (18, 31), (14, 31)), fill=180)
        d.ellipse((13*u, 9*u, 19*u, 15*u), fill=0)
    elif name == "card":
        d.rounded_rectangle((6*u, 2*u, 26*u, 30*u), radius=3*u, fill=255)
        d.rounded_rectangle((9*u, 5*u, 23*u, 27*u), radius=2*u, fill=0)
        d.ellipse((12*u, 12*u, 20*u, 20*u), fill=255)
    elif name == "arrow":
        d.polygon(P((4, 12), (18, 12), (18, 5), (29, 16), (18, 27), (18, 20), (4, 20)), fill=255)
    elif name == "crown":
        d.polygon(P((3, 10), (10, 17), (16, 6), (22, 17), (29, 10), (26, 26), (6, 26)), fill=255)
        d.rectangle((6*u, 27*u, 26*u, 30*u), fill=255)
    elif name == "chip":
        d.rectangle((8*u, 8*u, 24*u, 24*u), fill=255)
        for i in range(4):
            y = (10 + i * 4) * u
            d.rectangle((3*u, y, 7*u, y + 2*u), fill=255)
            d.rectangle((25*u, y, 29*u, y + 2*u), fill=255)
            d.rectangle((y, 3*u, y + 2*u, 7*u), fill=255)
            d.rectangle((y, 25*u, y + 2*u, 29*u), fill=255)
        d.rectangle((12*u, 12*u, 20*u, 20*u), fill=0)
    elif name in ("bcross", "bcircle", "btri", "bsquare"):
        w = int(3.2*u)
        if name == "bcross":
            d.line(P((8, 8), (24, 24)), fill=255, width=w)
            d.line(P((24, 8), (8, 24)), fill=255, width=w)
        elif name == "bcircle":
            d.ellipse((6*u, 6*u, 26*u, 26*u), outline=255, width=w)
        elif name == "btri":
            d.polygon(P((16, 5), (27, 25), (5, 25)), outline=255, width=w)
        else:
            d.rectangle((7*u, 7*u, 25*u, 25*u), outline=255, width=w)
    return im.resize((s, s), Image.LANCZOS)


def make_mars(s):
    rnd = random.Random(7)
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    px = im.load()
    craters = [(rnd.uniform(-0.8, 0.8), rnd.uniform(-0.8, 0.8), rnd.uniform(0.04, 0.16)) for _ in range(18)]
    bands = [rnd.uniform(0, 6.28) for _ in range(4)]
    for y in range(s):
        for x in range(s):
            nx = (x + 0.5) / s * 2 - 1
            ny = (y + 0.5) / s * 2 - 1
            r2 = nx * nx + ny * ny
            if r2 > 1.0:
                continue
            nz = math.sqrt(1 - r2)
            light = max(0.0, nx * -0.55 + ny * -0.45 + nz * 0.70)
            shade = 0.18 + 0.95 * light
            base = 0.55 + 0.12 * math.sin(ny * 7 + bands[0]) + 0.08 * math.sin(nx * 11 + ny * 5 + bands[1])
            for cx, cy, cr in craters:
                dd = math.hypot(nx - cx, ny - cy)
                if dd < cr:
                    base -= 0.10 * (1 - dd / cr)
                elif dd < cr * 1.25:
                    base += 0.03
            rr = min(255, int((200 * base + 30) * shade))
            gg = min(255, int((80 * base + 20) * shade))
            bb = min(255, int((40 * base + 15) * shade))
            cap = -0.86 + 0.04 * math.sin(nx * 9.0)
            if ny < cap:
                k = min(1.0, (cap - ny) / 0.05)
                rr = int(rr * (1 - k) + min(255, 235 * shade) * k)
                gg = int(gg * (1 - k) + min(255, 220 * shade) * k)
                bb = int(bb * (1 - k) + min(255, 215 * shade) * k)
            edge = min(1.0, (1 - math.sqrt(r2)) * s / 2.0)
            px[x, y] = (rr, gg, bb, int(255 * edge))
    return im


def main():
    items = []  # (key, image L)
    glyph_meta = {}
    for fname, ffile, size in FONTS:
        font = ImageFont.truetype(os.path.join(FONT_DIR, ffile), size)
        asc, desc = font.getmetrics()
        glyph_meta[fname] = {"asc": asc, "h": asc + desc, "g": {}}
        for cp in CPS:
            ch = chr(cp)
            if cp < 127 or cp >= 160:
                bbox = font.getbbox(ch)
                adv = int(round(font.getlength(ch)))
            else:
                bbox, adv = (0, 0, 0, 0), 0
            w = max(0, bbox[2] - bbox[0])
            h = max(0, bbox[3] - bbox[1])
            if w == 0 or h == 0 or cp == 32:
                glyph_meta[fname]["g"][cp] = [0, 0, 0, 0, 0, 0, adv]
                continue
            gi = Image.new("L", (w, h), 0)
            ImageDraw.Draw(gi).text((-bbox[0], -bbox[1]), ch, font=font, fill=255)
            glyph_meta[fname]["g"][cp] = [0, 0, w, h, bbox[0], bbox[1], adv]
            items.append((("G", fname, cp), gi))
    for i, n in enumerate(ICON_NAMES):
        items.append((("I32", i), draw_icon(n, 32)))
        items.append((("I16", i), draw_icon(n, 16)))
    # botoes do controle (coloridos, alpha suave) para os textos de ajuda
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import gen_art
    padimg = Image.open(os.path.join(ROOT, "art_src", "pad_buttons.png")).convert("RGB")
    PADBOX = [(562, 587, 691, 727), (682, 461, 806, 596), (564, 337, 686, 469),
              (449, 461, 573, 596), (1104, 832, 1247, 941)]
    pads = [gen_art.sticker(padimg, b, 26 if i == 4 else 16, 16, thr=None) for i, b in enumerate(PADBOX)]
    PW = sum(p.width + 1 for p in pads)
    padstrip = Image.new("RGBA", (PW, 16), (0, 0, 0, 0))
    padpos = []
    px = 0
    for p in pads:
        padstrip.alpha_composite(p, (px, (16 - p.height) // 2))
        padpos.append((px, p.width))
        px += p.width + 1
    items.append((("PADS",), Image.new("L", (PW, 16), 0)))

    # empacotamento em prateleiras
    items.sort(key=lambda it: -it[1].size[1])
    x = y = shelf = 0
    pos = {}
    for key, im in items:
        w, h = im.size
        if x + w + 1 > W:
            x = 0; y += shelf + 1; shelf = 0
        pos[key] = (x, y)
        x += w + 1
        shelf = max(shelf, h)
    H = y + shelf
    H2 = 1
    while H2 < H:
        H2 *= 2
    atlas = Image.new("L", (W, H2), 0)
    for key, im in items:
        atlas.paste(im, pos[key])
        if key[0] == "G":
            glyph_meta[key[1]]["g"][key[2]][0:2] = list(pos[key])
    print("atlas %dx%d (usado %d)" % (W, H2, H))

    # RLE do canal alpha: 0x00/0xFF seguidos de contagem
    data = list(atlas.tobytes())
    rle = []
    i = 0
    while i < len(data):
        v = data[i]
        if v in (0, 255):
            j = i
            while j < len(data) and data[j] == v and j - i < 255:
                j += 1
            rle += [v, j - i]
            i = j
        else:
            rle.append(v); i += 1

    with open(OUT, "w") as f:
        f.write("/* Gerado por tools/gen_assets.py - nao editar. */\n")
        f.write("/* Fonte: DejaVu Sans (c) Bitstream / DejaVu, licenca livre. */\n")
        f.write("#ifndef ASSETS_H\n#define ASSETS_H\n#include <stdint.h>\n")
        f.write("#define ATLAS_W %d\n#define ATLAS_H %d\n" % (W, H2))
        f.write("#define ATLAS_RLE_LEN %d\n" % len(rle))
        f.write("static const uint8_t ATLAS_RLE[] = {\n")
        for k in range(0, len(rle), 32):
            f.write(",".join(str(b) for b in rle[k:k + 32]) + ",\n")
        f.write("};\n")
        f.write("typedef struct { int16_t x, y; uint8_t w, h; int8_t ox, oy; uint8_t adv; } Glyph;\n")
        for fname, _, _ in FONTS:
            m = glyph_meta[fname]
            f.write("#define FONT_%s_H %d\n#define FONT_%s_ASC %d\n" % (fname, m["h"], fname, m["asc"]))
            f.write("static const Glyph FONT_%s[%d] = {\n" % (fname, len(CPS)))
            for cp in CPS:
                g = m["g"][cp]
                f.write("{%d,%d,%d,%d,%d,%d,%d}," % tuple(g))
                if cp % 8 == 7:
                    f.write("\n")
            f.write("};\n")
        f.write("#define ICON_COUNT %d\n" % len(ICON_NAMES))
        for nm, key in (("ICON32", "I32"), ("ICON16", "I16")):
            f.write("static const int16_t %s_POS[%d][2] = {" % (nm, len(ICON_NAMES)))
            f.write(",".join("{%d,%d}" % pos[(key, i)] for i in range(len(ICON_NAMES))))
            f.write("};\n")
        f.write('#include "icons.h"\n')
        mx, my = pos[("PADS",)]
        f.write("#define PADS_X %d\n#define PADS_Y %d\n#define PADS_W %d\n#define PADS_H 16\n" % (mx, my, PW))
        f.write("static const int16_t PADS_POS[5][2] = {%s};\n" % ",".join("{%d,%d}" % (mx + a, b) for a, b in padpos))
        f.write("static const uint8_t PADS_RGBA[] = {\n")
        mb = padstrip.tobytes()
        for k in range(0, len(mb), 48):
            f.write(",".join(str(b) for b in mb[k:k + 48]) + ",\n")
        f.write("};\n#endif\n")
    with open(os.path.join(ROOT, "src", "icons.h"), "w") as f:
        f.write("/* Gerado por tools/gen_assets.py */\n#ifndef ICONS_H\n#define ICONS_H\n")
        f.write("enum { %s };\n#endif\n" % ", ".join("IC_%s" % n.upper() for n in ICON_NAMES))
    print("ok:", OUT, os.path.getsize(OUT), "bytes")


if __name__ == "__main__":
    main()
