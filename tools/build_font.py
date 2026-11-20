"""Gera web/pixel.ttf: uma fonte bitmap original 5x7 (com acentos PT) para a interface.

Uso:  pip install fonttools && python tools/build_font.py
"""
from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

PX = 125          # unidades por pixel (1000 upm -> 8 pixels por em)
ASCENT_ROWS = 9   # 7 de corpo + 2 para acentos em maiúsculas
DESCENT_ROWS = 2

# Cada glifo: 7 linhas de corpo (de cima para baixo) e, opcionalmente, linhas de descendente.
GLYPHS_SRC = r"""
@
...
...
...
...
...
...
...
@!
#
#
#
#
#
.
#
@"
#.#
#.#
...
...
...
...
...
@#
.#.#.
.#.#.
#####
.#.#.
#####
.#.#.
.#.#.
@$
..#..
.####
#.#..
.###.
..#.#
####.
..#..
@%
#...#
#..#.
...#.
..#..
.#...
.#..#
#...#
@&
.##..
#..#.
.##..
.#...
#.#.#
#..#.
.##.#
@'
#
#
.
.
.
.
.
@(
..#
.#.
#..
#..
#..
.#.
..#
@)
#..
.#.
..#
..#
..#
.#.
#..
@*
.....
..#..
#.#.#
.###.
#.#.#
..#..
.....
@+
.....
..#..
..#..
#####
..#..
..#..
.....
@,
.
.
.
.
.
.
#
#
@-
.....
.....
.....
#####
.....
.....
.....
@.
.
.
.
.
.
.
#
@/
....#
...#.
...#.
..#..
.#...
.#...
#....
@0
.###.
#...#
#..##
#.#.#
##..#
#...#
.###.
@1
..#..
.##..
..#..
..#..
..#..
..#..
.###.
@2
.###.
#...#
....#
..##.
.#...
#....
#####
@3
.###.
#...#
....#
..##.
....#
#...#
.###.
@4
...#.
..##.
.#.#.
#..#.
#####
...#.
...#.
@5
#####
#....
####.
....#
....#
#...#
.###.
@6
..##.
.#...
#....
####.
#...#
#...#
.###.
@7
#####
....#
...#.
..#..
.#...
.#...
.#...
@8
.###.
#...#
#...#
.###.
#...#
#...#
.###.
@9
.###.
#...#
#...#
.####
....#
...#.
.##..
@:
.
.
#
.
.
.
#
@;
.
.
#
.
.
.
#
#
@<
...#
..#.
.#..
#...
.#..
..#.
...#
@=
.....
.....
#####
.....
#####
.....
.....
@>
#...
.#..
..#.
...#
..#.
.#..
#...
@?
.###.
#...#
....#
...#.
..#..
.....
..#..
@@
.###.
#...#
#.###
#.#.#
#.###
#....
.####
@A
.###.
#...#
#...#
#####
#...#
#...#
#...#
@B
####.
#...#
#...#
####.
#...#
#...#
####.
@C
.###.
#...#
#....
#....
#....
#...#
.###.
@D
####.
#...#
#...#
#...#
#...#
#...#
####.
@E
#####
#....
#....
####.
#....
#....
#####
@F
#####
#....
#....
####.
#....
#....
#....
@G
.####
#....
#....
#..##
#...#
#...#
.####
@H
#...#
#...#
#...#
#####
#...#
#...#
#...#
@I
###
.#.
.#.
.#.
.#.
.#.
###
@J
....#
....#
....#
....#
....#
#...#
.###.
@K
#...#
#..#.
#.#..
##...
#.#..
#..#.
#...#
@L
#....
#....
#....
#....
#....
#....
#####
@M
#...#
##.##
#.#.#
#...#
#...#
#...#
#...#
@N
#...#
##..#
#.#.#
#..##
#...#
#...#
#...#
@O
.###.
#...#
#...#
#...#
#...#
#...#
.###.
@P
####.
#...#
#...#
####.
#....
#....
#....
@Q
.###.
#...#
#...#
#...#
#...#
#..#.
.##.#
@R
####.
#...#
#...#
####.
#.#..
#..#.
#...#
@S
.####
#....
#....
.###.
....#
....#
####.
@T
#####
..#..
..#..
..#..
..#..
..#..
..#..
@U
#...#
#...#
#...#
#...#
#...#
#...#
.###.
@V
#...#
#...#
#...#
#...#
.#.#.
.#.#.
..#..
@W
#...#
#...#
#...#
#...#
#.#.#
##.##
#...#
@X
#...#
#...#
.#.#.
..#..
.#.#.
#...#
#...#
@Y
#...#
#...#
.#.#.
..#..
..#..
..#..
..#..
@Z
#####
....#
...#.
..#..
.#...
#....
#####
@[
###
#..
#..
#..
#..
#..
###
@\
#....
.#...
.#...
..#..
...#.
...#.
....#
@]
###
..#
..#
..#
..#
..#
###
@^
..#..
.#.#.
#...#
.....
.....
.....
.....
@_
.....
.....
.....
.....
.....
.....
.....
#####
@`
#.
.#
..
..
..
..
..
@a
.....
.....
.###.
....#
.####
#...#
.####
@b
#....
#....
#.##.
##..#
#...#
#...#
####.
@c
.....
.....
.###.
#....
#....
#....
.###.
@d
....#
....#
.##.#
#..##
#...#
#...#
.####
@e
.....
.....
.###.
#...#
#####
#....
.####
@f
..##
.#..
####
.#..
.#..
.#..
.#..
@g
.....
.....
.####
#...#
#...#
#...#
.####
....#
####.
@h
#....
#....
#.##.
##..#
#...#
#...#
#...#
@i
#
.
#
#
#
#
#
@j
....#
.....
....#
....#
....#
....#
....#
#...#
.###.
@k
#...
#...
#..#
#.#.
##..
#.#.
#..#
@l
#.
#.
#.
#.
#.
#.
.#
@m
.....
.....
##.#.
#.#.#
#.#.#
#...#
#...#
@n
.....
.....
####.
#...#
#...#
#...#
#...#
@o
.....
.....
.###.
#...#
#...#
#...#
.###.
@p
.....
.....
#.##.
##..#
#...#
#...#
####.
#....
#....
@q
.....
.....
.##.#
#..##
#...#
#...#
.####
....#
....#
@r
.....
.....
#.##.
##..#
#....
#....
#....
@s
.....
.....
.####
#....
.###.
....#
####.
@t
.#..
.#..
####
.#..
.#..
.#..
..##
@u
.....
.....
#...#
#...#
#...#
#...#
.####
@v
.....
.....
#...#
#...#
#...#
.#.#.
..#..
@w
.....
.....
#...#
#.#.#
#.#.#
#.#.#
.#.#.
@x
.....
.....
#...#
.#.#.
..#..
.#.#.
#...#
@y
.....
.....
#...#
#...#
#...#
#...#
.####
....#
####.
@z
.....
.....
#####
...#.
..#..
.#...
#####
@{
..##
.#..
.#..
#...
.#..
.#..
..##
@|
#
#
#
#
#
#
#
#
@}
##..
..#.
..#.
...#
..#.
..#.
##..
@~
.....
.....
.....
.##.#
#.##.
.....
.....
@•
...
...
...
###
###
###
...
@●
...
...
...
###
###
###
...
@…
.....
.....
.....
.....
.....
.....
#.#.#
@«
......
......
..#..#
.#..#.
#..#..
.#..#.
..#..#
@»
......
......
#..#..
.#..#.
..#..#
.#..#.
#..#..
@º
.##.
#..#
.##.
....
####
....
....
@ª
.##.
..##
#.##
.###
####
....
....
@°
.#.
#.#
.#.
...
...
...
...
@€
..###
.#...
####.
.#...
####.
.#...
..###
@£
..##.
.#..#
.#...
###..
.#...
.#...
#####
"""

ACCENTS = {
    "acute":      ["...#.", "..#.."],
    "grave":      [".#...", "..#.."],
    "circumflex": ["..#..", ".#.#."],
    "tilde":      [".##.#", "#.##."],
    "diaeresis":  [".#.#.", "....."],
}

COMPOSED = {
    "á": ("a", "acute"), "à": ("a", "grave"), "â": ("a", "circumflex"), "ã": ("a", "tilde"),
    "é": ("e", "acute"), "è": ("e", "grave"), "ê": ("e", "circumflex"),
    "ó": ("o", "acute"), "ò": ("o", "grave"), "ô": ("o", "circumflex"), "õ": ("o", "tilde"),
    "ú": ("u", "acute"), "ù": ("u", "grave"), "ü": ("u", "diaeresis"),
    "ñ": ("n", "tilde"),
    "Á": ("A", "acute"), "À": ("A", "grave"), "Â": ("A", "circumflex"), "Ã": ("A", "tilde"),
    "É": ("E", "acute"), "È": ("E", "grave"), "Ê": ("E", "circumflex"),
    "Í": ("I", "acute"), "Ì": ("I", "grave"),
    "Ó": ("O", "acute"), "Ò": ("O", "grave"), "Ô": ("O", "circumflex"), "Õ": ("O", "tilde"),
    "Ú": ("U", "acute"), "Ù": ("U", "grave"), "Ü": ("U", "diaeresis"), "Ñ": ("N", "tilde"),
}


def parse(src):
    """Devolve {char: {y: linha}} com y=0 na linha da base (y cresce para cima)."""
    glyphs = {}
    for block in src.split("\n@")[1:]:
        lines = block.split("\n")
        ch = lines[0][0] if lines[0] else " "
        rows = [l for l in lines[1:] if l]
        pix = {}
        for i, row in enumerate(rows):
            pix[6 - i] = row  # linhas 0..6 => y 6..0, descendentes => y -1, -2
        glyphs[ch] = pix
    return glyphs


def width_of(pix):
    return max(len(r) for r in pix.values())


def compose(glyphs, base, accent):
    pix = dict(glyphs[base])
    w = width_of(pix)
    acc = ACCENTS[accent]
    off = (5 - w) // 2
    cropped = [r[off:off + w] if off >= 0 else r.center(w, ".") for r in acc]
    if base.islower():
        # remove o ponto/linha superior e põe o acento nas linhas 6 e 5 (acima da altura-x)
        pix[6], pix[5] = cropped
    else:
        pix[8], pix[7] = cropped
    return pix


def build(out):
    glyphs = parse(GLYPHS_SRC)
    for ch, (base, acc) in COMPOSED.items():
        glyphs[ch] = compose(glyphs, base, acc)
    # í/ì: i sem ponto + acento estreito
    glyphs["í"] = {6: ".#", 5: "#.", 4: "#.", 3: "#.", 2: "#.", 1: "#.", 0: "#."}
    glyphs["ì"] = {6: "#.", 5: ".#", 4: ".#", 3: ".#", 2: ".#", 1: ".#", 0: ".#"}
    # ç / Ç: cedilha nas linhas de descendente
    for base, ced in (("c", "ç"), ("C", "Ç")):
        pix = dict(glyphs[base])
        pix[-1] = "..#.."
        pix[-2] = ".#..."
        glyphs[ced] = pix

    order = [".notdef"]
    cmap, glyf, hmtx = {}, {}, {}

    # .notdef: caixa oca
    pen = TTGlyphPen(None)
    box = [(0, 0, 4, 1), (0, 6, 4, 7), (0, 1, 1, 6), (3, 1, 4, 6)]
    for x0, y0, x1, y1 in box:
        _rect(pen, x0, y0, x1, y1)
    glyf[".notdef"] = pen.glyph()
    hmtx[".notdef"] = (5 * PX, 0)

    for i, (ch, pix) in enumerate(sorted(glyphs.items())):
        name = f"g{ord(ch):04X}"
        order.append(name)
        cmap[ord(ch)] = name
        pen = TTGlyphPen(None)
        w = width_of(pix)
        for y, row in pix.items():
            x = 0
            while x < len(row):
                if row[x] == "#":
                    s = x
                    while x < len(row) and row[x] == "#":
                        x += 1
                    _rect(pen, s, y, x, y + 1)
                else:
                    x += 1
        glyf[name] = pen.glyph()
        adv = (w + 1) * PX
        hmtx[name] = (adv, 0)
    # espaço sem quebra = espaço
    cmap[0xA0] = cmap[0x20]

    fb = FontBuilder(1000, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyf)
    fb.setupHorizontalMetrics(hmtx)
    asc, desc = ASCENT_ROWS * PX, DESCENT_ROWS * PX
    fb.setupHorizontalHeader(ascent=asc, descent=-desc)
    fb.setupNameTable({"familyName": "PixelVault", "styleName": "Regular"})
    fb.setupOS2(sTypoAscender=asc, sTypoDescender=-desc, sTypoLineGap=0,
                usWinAscent=asc, usWinDescent=desc, fsType=0)
    fb.setupPost()
    fb.save(out)
    print(f"{len(glyphs)} glifos -> {out}")


def _rect(pen, x0, y0, x1, y1):
    pen.moveTo((x0 * PX, y0 * PX))
    pen.lineTo((x0 * PX, y1 * PX))
    pen.lineTo((x1 * PX, y1 * PX))
    pen.lineTo((x1 * PX, y0 * PX))
    pen.closePath()


if __name__ == "__main__":
    build(str(Path(__file__).resolve().parent.parent / "web" / "pixel.ttf"))
