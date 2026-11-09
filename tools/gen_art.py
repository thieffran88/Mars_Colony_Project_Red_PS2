#!/usr/bin/env python3
"""Extrai a arte enviada (art_src/*.png) e gera as texturas do jogo.

Saidas:
  data/art.rle    textura ART   512x512 ABGR1555 (cartas, edificios, mascotes,
                                 Marte, fonte cromada)
  data/title.rle  textura TITLE 512x512 ABGR1555 (key art + botoes do controle)
  src/art.h       posicoes dos sprites e metricas da fonte cromada

As texturas sao de 16 bits (1 bit de alpha) para caber na VRAM do PS2.
Uso: python3 tools/gen_art.py  (requer Pillow, numpy, scipy)
"""
import math, os, struct, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from scipy import ndimage as nd

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "art_src")
TEX = 512

# ----------------------------------------------------------------------------
# recorte de "adesivos" com fundo branco


def _fit(rgba, maxw, maxh, thr=128):
    """Redimensiona mantendo proporcao (alpha pre-multiplicado), alpha 1 bit."""
    w, h = rgba.size
    s = min(maxw / w, maxh / h)
    nw, nh = max(1, int(round(w * s))), max(1, int(round(h * s)))
    a = np.array(rgba).astype(np.float32) / 255.0
    a[..., :3] *= a[..., 3:4]
    pm = Image.fromarray((a * 255).astype(np.uint8), "RGBA").resize((nw, nh), Image.LANCZOS)
    b = np.array(pm).astype(np.float32) / 255.0
    al = b[..., 3:4]
    rgb = np.where(al > 0.01, b[..., :3] / np.maximum(al, 0.01), 0)
    out = np.zeros((nh, nw, 4), np.uint8)
    out[..., :3] = np.clip(rgb * 255, 0, 255).astype(np.uint8)
    if thr is None:
        out[..., 3] = np.clip(b[..., 3] * 255, 0, 255).astype(np.uint8)
    else:
        out[..., 3] = np.where(b[..., 3] * 255 >= thr, 255, 0)
    return Image.fromarray(out, "RGBA")


def sticker(img, box, maxw, maxh, erase=(), tol=226, keep=0.02, thr=128):
    c = np.array(img.crop(box).convert("RGB")).astype(int)
    for (x0, y0, x1, y1) in erase:
        c[y0:y1, x0:x1] = 255
    mn, mx = c.min(2), c.max(2)
    white = (mn > tol) & ((mx - mn) < 32)
    lab, n = nd.label(white)
    border = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
    bg = np.isin(lab, list(border))
    fg = ~bg
    lab2, n2 = nd.label(fg)
    if n2:
        sizes = nd.sum(fg, lab2, range(1, n2 + 1))
        big = sizes.max()
        ok = [i + 1 for i, s in enumerate(sizes) if s >= big * keep]
        fg = np.isin(lab2, ok)
    rgba = np.zeros(c.shape[:2] + (4,), np.uint8)
    rgba[..., :3] = c.astype(np.uint8)
    rgba[..., 3] = fg * 255
    im = Image.fromarray(rgba, "RGBA")
    im = im.crop(im.getbbox())
    return _fit(im, maxw, maxh, thr)


# ----------------------------------------------------------------------------
# cartas: ilustracoes das folhas (grupo g contem as cartas g, g+7, ... (1-based))

CARD_SHEETS = {1: "cards_g1.png", 2: "cards_g2.png", 3: "cards_g3.png", 4: "cards_g4.png",
               5: "cards_g5.png", 6: "cards_g6.png", 7: "cards_g7.png"}
CARD_W, CARD_H = 60, 45


def card_arts():
    arts = {}
    for g, fn in CARD_SHEETS.items():
        im = Image.open(os.path.join(SRC, fn)).convert("RGB")
        k = im.width / 2000.0
        for i in range(5):
            x0 = int((75 + 386 * i) * k) + 10
            x1 = int((383 + 386 * i) * k) - 10
            y0 = int(213 * k) + 10
            y1 = int(418 * k) - 10
            w, h = x1 - x0, y1 - y0
            erase = [(0, 0, int(80 * k), int(42 * k)), (0, h - int(26 * k), w, h)]
            idx = (g - 1) + 7 * i          # indice 0-based em CARDS
            arts[idx] = sticker(im, (x0, y0, x1, y1), CARD_W, CARD_H, erase=erase)
    return arts


# ----------------------------------------------------------------------------
# fonte cromada (letras italicas que se tocam: corta apos "desentortar")

CAP = 24
SLANT = 0.22


def chrome_glyphs():
    im = Image.open(os.path.join(SRC, "logo_font.png")).convert("RGB")
    a = np.array(im).astype(int)
    mx = a.max(2); mn = a.min(2); sat = mx - mn
    letter = (mx < 110) | ((sat < 28) & (mx < 246))
    rows = [(25, 182, "ABCDEFGHIJKLM"), (205, 382, "NOPQRSTUVWXYZ"),
            (388, 548, "0123456789"), (560, 712, ".,:;!?-_/()")]
    capnat = 150.0
    out = {}
    for y0, y1, chars in rows:
        m = letter[y0:y1]; H, W = m.shape
        pad = int(H * SLANT) + 2
        prof = np.zeros(W + pad, int)
        for y in range(H):
            xs = np.nonzero(m[y])[0]
            np.add.at(prof, xs - int((H - y) * SLANT) + pad, 1)
        nz = prof > 0
        segs = []; inside = False
        for x, v in enumerate(nz):
            if v and not inside: st = x; inside = True
            if not v and inside: segs.append([st, x]); inside = False
        segs = [s for s in segs if s[1] - s[0] > 6]
        while len(segs) < len(chars):
            i = max(range(len(segs)), key=lambda k: segs[k][1] - segs[k][0])
            s0, s1 = segs[i]; w = s1 - s0
            lo, hi = s0 + int(w * 0.3), s0 + int(w * 0.7)
            cut = lo + int(np.argmin(prof[lo:hi]))
            segs[i:i + 1] = [[s0, cut], [cut, s1]]
        for (s0, s1), ch in zip(segs, chars):
            gx0 = max(0, s0 - pad - 2); gx1 = min(W, s1 - pad + int(H * SLANT) + 2)
            crop = a[y0:y1, gx0:gx1]
            h, w = crop.shape[:2]
            inseg = np.zeros((h, w), bool)
            for y in range(h):
                off = int((H - y) * SLANT)
                xs = np.arange(w) + gx0 - off + pad
                inseg[y] = (xs >= s0 - 1) & (xs < s1 + 1)
            cmn, cmx = crop.min(2), crop.max(2)
            csat = cmx - cmn
            white = (cmn > 236) & (csat < 22)
            blue = (csat > 35) & (crop[..., 2] > crop[..., 0] + 20)
            core = nd.binary_erosion(white, iterations=2)
            lab, n = nd.label(core)
            transparent = np.zeros((h, w), bool)
            for i in range(1, n + 1):
                comp = lab == i
                ys, xs = np.nonzero(comp)
                touches = ys.min() <= 3 or xs.min() <= 3 or ys.max() >= h - 4 or xs.max() >= w - 4
                ring = nd.binary_dilation(comp, iterations=2) & ~comp
                fb = blue[ring].mean() if ring.any() else 0
                if touches or fb > 0.25:
                    transparent |= comp
            transparent = nd.binary_dilation(transparent, iterations=3) & white
            softglow = blue & (csat < 70)
            alpha = inseg & ~transparent & ~softglow & ~((cmn > 236) & (csat >= 22) & (csat < 40))
            rgba = np.zeros((h, w, 4), np.uint8)
            rgba[..., :3] = crop.astype(np.uint8)
            rgba[..., 3] = alpha * 255
            g = Image.fromarray(rgba, "RGBA")
            bb = g.getbbox()
            out[ch] = (g.crop(bb), bb[1])        # imagem nativa, topo relativo a linha
    # acentos sintetizados
    def accent(base, kind):
        img, top = base
        w, h = img.size
        up = 46 if kind != "cedil" else 0
        dn = 40 if kind == "cedil" else 0
        can = Image.new("RGBA", (w + 10, h + up + dn), (0, 0, 0, 0))
        can.alpha_composite(img, (0, up))
        cx = w * 0.56 + 6
        lay = Image.new("RGBA", can.size, (0, 0, 0, 0))
        d = ImageDraw.Draw(lay)
        if kind == "acute":
            pts = [(cx - 6, up - 8), (cx + 20, up - 38)]
        elif kind == "grave":
            pts = [(cx + 10, up - 8), (cx - 14, up - 38)]
        elif kind == "circ":
            pts = [(cx - 28, up - 8), (cx + 2, up - 38), (cx + 30, up - 8)]
        elif kind == "tilde":
            pts = [(cx - 30 + i * 4, up - 22 - 9 * math.sin(i * 4 / 60.0 * 2 * math.pi)) for i in range(16)]
        else:  # cedilha
            pts = [(cx - 14, up + h - 4), (cx - 8, up + h + 16), (cx - 26, up + h + 32)]
        for width, col in ((30, (70, 150, 255, 255)), (22, (5, 5, 10, 255)), (11, (235, 236, 242, 255))):
            d.line(pts, fill=col, width=width, joint="curve")
            for p in (pts[0], pts[-1]):
                r = width / 2
                d.ellipse((p[0] - r, p[1] - r, p[0] + r, p[1] + r), fill=col)
        can.alpha_composite(lay)
        return (can.crop(can.getbbox()), top - up)
    acc = {"Á": ("A", "acute"), "À": ("A", "grave"), "Â": ("A", "circ"), "Ã": ("A", "tilde"),
           "É": ("E", "acute"), "Ê": ("E", "circ"), "Í": ("I", "acute"), "Ó": ("O", "acute"),
           "Ô": ("O", "circ"), "Õ": ("O", "tilde"), "Ú": ("U", "acute"), "Ç": ("C", "cedil")}
    for ch, (b, kind) in acc.items():
        out[ch] = accent(out[b], kind)
    s = CAP / capnat
    res = {}
    for ch, (img, top) in out.items():
        g = _fit(img, max(1, int(img.width * s)), max(1, int(img.height * s)), thr=100)
        res[ch] = (g, int(round(top * s)))
    return res


# ----------------------------------------------------------------------------
# Marte (procedural, mesmo estilo do gen_assets)

def mars(sz):
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import gen_assets
    return gen_assets.make_mars(sz)


# ----------------------------------------------------------------------------
# empacotamento e escrita


def pack(items, W, H, x0=0, y0=0):
    items = sorted(items, key=lambda it: (-it[1].size[1], -it[1].size[0]))
    x, y, shelf = x0, y0, 0
    pos = {}
    for key, im in items:
        w, h = im.size
        if x + w > W:
            x = x0; y += shelf + 1; shelf = 0
        if y + h > H:
            raise SystemExit("textura cheia em %r (y=%d)" % (key, y))
        pos[key] = (x, y, w, h)
        x += w + 1
        shelf = max(shelf, h)
    return pos, y + shelf


def to1555(img):
    a = np.array(img.convert("RGBA")).astype(np.uint32)
    r, g, b, al = a[..., 0] >> 3, a[..., 1] >> 3, a[..., 2] >> 3, (a[..., 3] >= 128)
    v = (al.astype(np.uint32) << 15) | (b << 10) | (g << 5) | r
    v = np.where(al, v, 0)
    return v.astype(np.uint16).flatten()


def rle16(arr):
    out = bytearray()
    i, n = 0, len(arr)
    while i < n:
        v = arr[i]; j = i + 1
        while j < n and arr[j] == v and j - i < 65535:
            j += 1
        out += struct.pack("<HH", j - i, int(v))
        i = j
    return bytes(out)


def main():
    os.makedirs(os.path.join(ROOT, "data"), exist_ok=True)
    ui = Image.open(os.path.join(SRC, "ui_mock.png")).convert("RGB")
    pad = Image.open(os.path.join(SRC, "pad_buttons.png")).convert("RGB")

    # ---------------- ART
    items = []
    for i, im in card_arts().items():
        items.append((("card", i), im))
    blds = [(1153, 439, 1303, 539), (1305, 423, 1429, 540), (1429, 430, 1531, 539),
            (1153, 547, 1290, 665), (1409, 544, 1533, 665)]
    for i, b in enumerate(blds):
        items.append((("bld", i), sticker(ui, b, CARD_W, CARD_H)))
    masc = [("point", (1160, 229, 1369, 428), 84, 80), ("cheer", (1175, 69, 1350, 222), 72, 64),
            ("thumbs", (1354, 78, 1520, 228), 72, 64), ("robot", (1347, 245, 1530, 411), 72, 64)]
    for name, b, mw, mh in masc:
        items.append((("masc", name), sticker(ui, b, mw, mh)))
    items.append((("deco", "bush"), sticker(ui, (1153, 672, 1329, 761), 64, 32)))
    items.append((("deco", "rock"), sticker(ui, (1329, 670, 1534, 761), 64, 30)))
    m = mars(96)
    mm = np.array(m); mm[..., 3] = np.where(mm[..., 3] >= 128, 255, 0)
    items.append((("mars", 0), Image.fromarray(mm, "RGBA")))
    glyphs = chrome_glyphs()
    for ch, (g, top) in glyphs.items():
        items.append((("glyph", ch), g))
    pos, used = pack(items, TEX, TEX)
    print("ART usado: %dx%d" % (TEX, used))
    art = Image.new("RGBA", (TEX, TEX), (0, 0, 0, 0))
    for key, im in items:
        art.alpha_composite(im, pos[key][:2])
    with open(os.path.join(ROOT, "data", "art.rle"), "wb") as f:
        f.write(rle16(to1555(art)))

    # ---------------- TITLE
    key = Image.open(os.path.join(SRC, "keyart.png")).convert("RGB")
    ch = int(key.width / (640 / 448.0))
    ky0 = 8
    kimg = key.crop((0, ky0, key.width, ky0 + ch)).resize((512, 358), Image.LANCZOS).convert("RGBA")
    titems = []
    pads = [("dpad", (46, 362, 354, 706), 96, 96), ("cross", (562, 587, 691, 727), 42, 42),
            ("circle", (682, 461, 806, 596), 42, 42), ("tri", (564, 337, 686, 469), 42, 42),
            ("square", (449, 461, 573, 596), 42, 42), ("l1", (50, 96, 281, 262), 64, 46),
            ("r1", (1242, 95, 1483, 262), 64, 46), ("start", (1104, 832, 1247, 986), 56, 50),
            ("select", (274, 835, 423, 982), 56, 50), ("stick", (903, 413, 1134, 698), 70, 86)]
    for name, b, mw, mh in pads:
        titems.append((name, sticker(pad, b, mw, mh)))
    tpos, tused = pack(titems, TEX, TEX, 0, 360)
    print("TITLE usado: %dx%d" % (TEX, tused))
    title = Image.new("RGBA", (TEX, TEX), (0, 0, 0, 0))
    title.paste(kimg, (0, 0))
    for name, im in titems:
        title.alpha_composite(im, tpos[name][:2])
    with open(os.path.join(ROOT, "data", "title.rle"), "wb") as f:
        f.write(rle16(to1555(title)))

    prev = os.environ.get("ART_PREVIEW")
    if prev:
        art.save(os.path.join(prev, "art_tex.png")); title.save(os.path.join(prev, "title_tex.png"))

    # ---------------- header
    with open(os.path.join(ROOT, "src", "art.h"), "w") as f:
        f.write("/* Gerado por tools/gen_art.py a partir de art_src/ - nao editar. */\n")
        f.write("#ifndef ART_H\n#define ART_H\n#include <stdint.h>\n")
        f.write("typedef struct { int16_t x, y, w, h; } SprRect;\n")
        f.write("static const SprRect ART_CARD[35] = {%s};\n" % ",".join("{%d,%d,%d,%d}" % pos[("card", i)] for i in range(35)))
        f.write("static const SprRect ART_BLD[5] = {%s};\n" % ",".join("{%d,%d,%d,%d}" % pos[("bld", i)] for i in range(5)))
        f.write("enum { MASC_POINT, MASC_CHEER, MASC_THUMBS, MASC_ROBOT };\n")
        f.write("static const SprRect ART_MASC[4] = {%s};\n" % ",".join("{%d,%d,%d,%d}" % pos[("masc", n)] for n, _, _, _ in masc))
        f.write("static const SprRect ART_BUSH = {%d,%d,%d,%d};\n" % pos[("deco", "bush")])
        f.write("static const SprRect ART_ROCK = {%d,%d,%d,%d};\n" % pos[("deco", "rock")])
        f.write("static const SprRect ART_MARS = {%d,%d,%d,%d};\n" % pos[("mars", 0)])
        f.write("#define CHROME_CAP %d\n" % CAP)
        f.write("typedef struct { uint16_t cp; int16_t x, y, w, h, oy; } ChromeGlyph;\n")
        chars = sorted(glyphs.keys(), key=lambda c: ord(c))
        f.write("#define CHROME_N %d\n" % len(chars))
        f.write("static const ChromeGlyph CHROME[CHROME_N] = {\n")
        for c in chars:
            x, y, w, h = pos[("glyph", c)]
            f.write("{%d,%d,%d,%d,%d,%d},\n" % (ord(c), x, y, w, h, glyphs[c][1]))
        f.write("};\n")
        f.write("static const SprRect TITLE_KEYART = {0,0,512,358};\n")
        f.write("enum { %s };\n" % ", ".join("PAD_%s" % n.upper() for n, _, _, _ in pads))
        f.write("static const SprRect TITLE_PAD[%d] = {%s};\n" % (len(pads), ",".join("{%d,%d,%d,%d}" % tpos[n] for n, _, _, _ in pads)))
        f.write("#endif\n")
    print("ok")


if __name__ == "__main__":
    main()
