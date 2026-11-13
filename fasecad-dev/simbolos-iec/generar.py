#!/usr/bin/env python3
"""Generate the FaseCAD panel symbol collection (IEC 60617 / IEC 61082-1).

Writes the .elmt files into elements/10_electric/05_iec_60617_cuadros and,
with --preview FILE.html, a page that draws every symbol for review.

Drawing rules used for every symbol:
- IEC 60617 shapes, drawn in the de-energized, non-actuated state.
- Connection points on the 10-unit grid of the folio; poles 20 units apart.
- Contacts are vertical, supply side on top; actuated contacts move to the
  right (clockwise about the lower fixed contact), so the mechanical link
  and the actuator are on the left.
- IEC 61082-1: one line width; mechanical links dashed; the reference
  designation (label) to the left of the vertical symbol, centred; terminal
  designations next to each connection point, right of the conductor.

Run again after editing: the element uuids are stable (derived from the
file path), so placed elements keep pointing to the same definition.
"""
import argparse
import math
import uuid
from html import escape
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / "elements" / "10_electric" / "05_iec_60617_cuadros"
NAMESPACE = uuid.UUID("6b1d3f2e-6c55-4c1e-9a43-0fa5eca60617")
LANGS = ["en", "es", "fr", "de", "it", "pt", "tr", "zh"]

STROKE = "line-style:normal;line-weight:normal;filling:none;color:black"
DASH = "line-style:dashed;line-weight:normal;filling:none;color:black"
FILL = "line-style:normal;line-weight:normal;filling:black;color:black"
THICK = "line-style:normal;line-weight:hight;filling:none;color:black"

LABEL_SIZE = 9      # reference designation
NUMBER_SIZE = 5     # terminal designations
PX_PER_PT = 4 / 3   # fonts are laid out at 96 dpi on the scene
CAP = 0.729         # cap height of Liberation Sans, in em
ASCENT = 0.905      # ascent of Liberation Sans, in em
DOC_MARGIN = 4      # QTextDocument margin of the dynamic texts


def num(v):
    v = round(v, 2)
    if v == int(v):
        return str(int(v))
    return ("%.2f" % v).rstrip("0").rstrip(".")


def font(size):
    return "Liberation Sans,%d,-1,5,50,0,0,0,0,0,Regular" % size


class Sym:
    """Primitives of one element, kept so they can go to XML and SVG."""

    def __init__(self):
        self.prims = []      # (kind, data)
        self.geo = []        # points of the geometric rect (as the editor)
        self.vis = []        # points actually drawn (to place the label)
        self.terms = []
        self.label_at = None

    # -- drawing -----------------------------------------------------------
    def line(self, x1, y1, x2, y2, style=STROKE):
        self.prims.append(("line", (x1, y1, x2, y2, style)))
        self.geo += [(x1, y1), (x2, y2)]
        self.vis += [(x1, y1), (x2, y2)]

    def path(self, pts, style=STROKE, closed=False):
        self.prims.append(("poly", (list(pts), style, closed)))
        self.geo += pts
        self.vis += pts

    def rect(self, x, y, w, h, style=STROKE):
        self.prims.append(("rect", (x, y, w, h, style)))
        self.geo += [(x, y), (x + w, y + h)]
        self.vis += [(x, y), (x + w, y + h)]

    def circle(self, cx, cy, r, style=STROKE):
        self.prims.append(("ellipse", (cx - r, cy - r, 2 * r, 2 * r, style)))
        self.geo += [(cx - r, cy - r), (cx + r, cy + r)]
        self.vis += [(cx - r, cy - r), (cx + r, cy + r)]

    def ellipse(self, cx, cy, rx, ry, style=STROKE):
        self.prims.append(("ellipse", (cx - rx, cy - ry, 2 * rx, 2 * ry, style)))
        self.geo += [(cx - rx, cy - ry), (cx + rx, cy + ry)]
        self.vis += [(cx - rx, cy - ry), (cx + rx, cy + ry)]

    def arc(self, cx, cy, r, start, span, style=STROKE):
        """Qt convention: degrees, 0 = 3 o'clock, positive = anticlockwise."""
        self.prims.append(("arc", (cx, cy, r, start, span, style)))
        self.geo += [(cx - r, cy - r), (cx + r, cy + r)]
        for i in range(13):
            a = math.radians(start + span * i / 12)
            self.vis.append((cx + r * math.cos(a), cy - r * math.sin(a)))

    def arrowhead(self, tip, frm, length=3.0, width=2.4):
        """Filled arrowhead with its point at tip, coming from frm."""
        dx, dy = tip[0] - frm[0], tip[1] - frm[1]
        d = math.hypot(dx, dy)
        ux, uy = dx / d, dy / d
        bx, by = tip[0] - ux * length, tip[1] - uy * length
        nx, ny = -uy * width / 2, ux * width / 2
        self.path([tip, (bx + nx, by + ny), (bx - nx, by - ny)], FILL, True)

    def text(self, cx, cy, txt, size):
        """Static text centred on (cx, cy) (cap height centred)."""
        em = size * PX_PER_PT
        base = cy + CAP * em / 2
        self.prims.append(("text", (cx, base, txt, size)))
        half = 0.31 * em * len(txt)      # rough advance of Liberation Sans
        self.vis += [(cx - half, base - CAP * em), (cx + half, base)]

    def number(self, x, cy, txt, size=NUMBER_SIZE):
        """Terminal designation, left edge at x, caps centred on cy."""
        self.prims.append(("number", (x, cy, txt, size)))

    def term(self, x, y, orientation, name=""):
        self.terms.append((x, y, orientation, name))
        self.geo.append((x, y))
        self.vis.append((x, y))

    # -- output ------------------------------------------------------------
    def size(self):
        xs = [p[0] for p in self.geo]
        ys = [p[1] for p in self.geo]
        x, y = min(xs), min(ys)
        w, h = max(xs) - x, max(ys) - y
        upw = (round(w) // 10) * 10 + 10 + (10 if round(w) % 10 > 6 else 0)
        uph = (round(h) // 10) * 10 + 10 + (10 if round(h) % 10 > 6 else 0)
        xm, ym = round(upw - w), round(uph - h)
        return upw, uph, -round(x - xm / 2), -round(y - ym / 2)

    def label_right(self):
        if self.label_at is not None:
            return self.label_at
        return min(p[0] for p in self.vis) - 4, 0


def uid(path, n):
    return "{%s}" % uuid.uuid5(NAMESPACE, "%s#%d" % (path, n))


def to_xml(sym, spec, relpath):
    width, height, hx, hy = sym.size()
    attrs = 'version="0.100.0" type="element" link_type="%s" width="%d" height="%d" hotspot_x="%d" hotspot_y="%d"' % (
        spec["link"], width, height, hx, hy)
    out = ['<definition %s>' % attrs]
    out.append('    <uuid uuid="%s"/>' % uid(relpath, 0))
    out.append("    <names>")
    for lang in sorted(LANGS):
        out.append('        <name lang="%s">%s</name>' % (lang, escape(spec["names"][lang], quote=False)))
    out.append("    </names>")
    kinds = spec.get("kind")
    if kinds:
        out.append("    <kindInformations>")
        for k, v in kinds:
            out.append('        <kindInformation name="%s">%s</kindInformation>' % (k, v))
        out.append("    </kindInformations>")
    out.append("    <informations>Author: FaseCAD. Drawn following IEC 60617 (graphical symbols) "
               "and IEC 61082-1 (document rules).\nLicense: see ELEMENTS.LICENSE (CC-BY 3.0).</informations>")
    out.append("    <description>")
    n = 1
    z = 1
    for kind, d in sym.prims:
        if kind == "line":
            x1, y1, x2, y2, style = d
            aa = "false" if (x1 == x2 or y1 == y2) else "true"
            out.append('        <line x1="%s" y1="%s" x2="%s" y2="%s" end1="none" end2="none" length1="1.5" length2="1.5" style="%s" antialias="%s"/>'
                       % (num(x1), num(y1), num(x2), num(y2), style, aa))
        elif kind == "poly":
            pts, style, closed = d
            coords = " ".join('x%d="%s" y%d="%s"' % (i + 1, num(px), i + 1, num(py)) for i, (px, py) in enumerate(pts))
            out.append('        <polygon %s closed="%s" antialias="true" style="%s"/>'
                       % (coords, "true" if closed else "false", style))
        elif kind == "rect":
            x, y, w, h, style = d
            out.append('        <rect x="%s" y="%s" width="%s" height="%s" rx="0" ry="0" style="%s" antialias="false"/>'
                       % (num(x), num(y), num(w), num(h), style))
        elif kind == "ellipse":
            x, y, w, h, style = d
            out.append('        <ellipse x="%s" y="%s" width="%s" height="%s" style="%s" antialias="true"/>'
                       % (num(x), num(y), num(w), num(h), style))
        elif kind == "arc":
            cx, cy, r, start, span, style = d
            out.append('        <arc x="%s" y="%s" width="%s" height="%s" start="%s" angle="%s" style="%s" antialias="true"/>'
                       % (num(cx - r), num(cy - r), num(2 * r), num(2 * r), num(start % 360), num(span), style))
        elif kind == "text":
            cx, base, txt, size = d
            out.append('        <text x="%s" y="%s" text="%s" font="%s" rotation="0" color="#000000" Halignment="AlignHCenter" anchor="alignment" uuid="%s"/>'
                       % (num(cx), num(base), escape(txt), font(size), uid(relpath, n)))
            n += 1
        elif kind == "number":
            x, cy, txt, size = d
            em = size * PX_PER_PT
            y = cy - (ASCENT - CAP / 2) * em - DOC_MARGIN
            z += 1
            out.append('        <dynamic_text x="%s" y="%s" z="%d" text_width="-1" Halignment="AlignLeft" Valignment="AlignTop" frame="false" rotation="0" keep_visual_rotation="false" text_from="UserText" uuid="%s" font="%s">'
                       % (num(x - DOC_MARGIN), num(y), z, uid(relpath, n), font(size)))
            out.append("            <text>%s</text>" % escape(txt))
            out.append("        </dynamic_text>")
            n += 1
    # reference designation, right-aligned so it grows to the left
    rx, cy = sym.label_right()
    em = LABEL_SIZE * PX_PER_PT
    y = cy - (ASCENT - CAP / 2) * em - DOC_MARGIN
    z += 1
    out.append('        <dynamic_text x="%s" y="%s" z="%d" text_width="-1" Halignment="AlignRight" Valignment="AlignTop" frame="false" rotation="0" keep_visual_rotation="false" text_from="ElementInfo" uuid="%s" font="%s">'
               % (num(rx - DOC_MARGIN - 1), num(y), z, uid(relpath, n), font(LABEL_SIZE)))
    out.append("            <text></text>")
    out.append("            <info_name>label</info_name>")
    out.append("        </dynamic_text>")
    n += 1
    for x, y, o, name in sym.terms:
        out.append('        <terminal uuid="%s" name="%s" x="%s" y="%s" orientation="%s" type="Generic"/>'
                   % (uid(relpath, n), escape(name), num(x), num(y), o))
        n += 1
    out.append("    </description>")
    out.append("</definition>")
    return "\n".join(out) + "\n"


# ===========================================================================
# Building blocks
# ===========================================================================
# Make contact: fixed contact (x,-20)..(x,-10), moving contact pivots at
# (x,10) and leans left to (x-8,-10). Break contact: fixed contact ends in a
# hook to the right, the moving contact leans right through the hook end.
NO_TIP = (-8, -10)
NC_TIP = (9, -12.5)


def blade_x(px, y, kind):
    """x of the moving contact at height y."""
    if kind == "NO":
        return px + NO_TIP[0] * (10 - y) / 20
    return px + NC_TIP[0] * (10 - y) / 22.5


def make(s, px, top="", bot="", qual=None, numbers=True, bot_y=20):
    s.line(px, -20, px, -10)
    s.line(px, 10, px, bot_y)
    s.line(px, 10, px + NO_TIP[0], NO_TIP[1])
    qualifier(s, px, qual)
    s.term(px, -20, "n", top)
    s.term(px, bot_y, "s", bot)
    if numbers:
        s.number(px + 2, -16.5, top)
        s.number(px + 2, bot_y - 4, bot)
    return (blade_x(px, 0, "NO"), 0)


def brk(s, px, top="", bot="", numbers=True):
    s.line(px, -20, px, -10)
    s.line(px, -10, px + 8, -10)
    s.line(px, 10, px, 20)
    s.line(px, 10, px + NC_TIP[0], NC_TIP[1])
    s.term(px, -20, "n", top)
    s.term(px, 20, "s", bot)
    if numbers:
        s.number(px + 2, -16.5, top)
        s.number(px + 2, 16, bot)
    return (blade_x(px, 0, "NC"), 0)


def qualifier(s, px, qual):
    """Function qualifiers on the fixed contact (IEC 60617 part 7)."""
    if qual == "contactor":          # semicircle
        s.arc(px, -12.5, 2.5, 90, 180)
    elif qual == "breaker":          # cross
        s.line(px - 2.5, -12.5, px + 2.5, -7.5)
        s.line(px - 2.5, -7.5, px + 2.5, -12.5)
    elif qual == "disconnector":     # bar
        s.line(px - 3, -10, px + 3, -10)
    elif qual == "switch-disconnector":  # bar and circle
        s.line(px - 3, -10, px + 3, -10)
        s.circle(px, -8, 2)


def link(s, x1, x2, y=0):
    if abs(x2 - x1) > 0.01:
        s.line(x1, y, x2, y, DASH)


def delay(s, px, kind, toward_contact):
    """Delayed-action qualifier (parachute) on the moving contact.

    The action is delayed while the moving contact travels toward the
    centre of the arc. Contacts here move right when actuated, so the
    arc centre is on the right for a delay on operation and on the left
    for a delay on release.
    """
    r = 6
    if toward_contact:
        cx = px - 9
        start, span = 120, 120
        xend = cx - math.sqrt(r * r - 4)
    else:
        cx = px - 21
        start, span = -60, 120
        xend = cx + math.sqrt(r * r - 4)
    for y in (-2, 2):
        s.line(blade_x(px, y, kind), y, xend, y)
    s.arc(cx, 0, r, start, span)


def push(s, x):
    """Operated by pushing: bracket open toward the contact."""
    s.path([(x + 3, -4), (x, -4), (x, 4), (x + 3, 4)])


def turn(s, x):
    """Operated by turning."""
    s.path([(x + 3, -4), (x, -4), (x, 4), (x - 3, 4)])


def mushroom(s, x):
    """Emergency (mushroom-head) push-button."""
    s.line(x, -5, x, 5)
    s.arc(x, 0, 5, 90, 180)


def detent(s, x):
    """Latching (stay-put) device on the mechanical link."""
    s.path([(x - 2, 0), (x, 3), (x + 2, 0)])


def positive_opening(s, x):
    """Positive opening operation: arrow in a circle on the link."""
    s.circle(x, 0, 3.5)
    s.line(x - 2.5, 0, x, 0)
    s.arrowhead((x + 2.5, 0), (x - 2.5, 0), 2.5, 2.4)


def thermal_box(s, x0, y0, w=12, h=10):
    """Thermal element (box with the bimetal jog), for releases on a link."""
    s.rect(x0, y0, w, h)
    cx = x0 + w / 2 + 1.5
    s.path([(cx, y0), (cx, y0 + 2), (cx - 3, y0 + 2), (cx - 3, y0 + h - 2), (cx, y0 + h - 2), (cx, y0 + h)])


def overcurrent_box(s, x0, y0, w=12, h=10):
    s.rect(x0, y0, w, h)
    s.text(x0 + w / 2, y0 + h / 2, "I>", 6)


def poles(n):
    return [20 * i for i in range(n)]


POWER_NAMES = [("1", "2"), ("3", "4"), ("5", "6"), ("7", "8")]


# ===========================================================================
# Symbols
# ===========================================================================
ELEMENTS = []


def element(folder, file, names, link="simple", kind=None):
    def deco(fn):
        ELEMENTS.append(dict(folder=folder, file=file, names=dict(zip(LANGS, names)),
                             link=link, kind=kind, draw=fn))
        return fn
    return deco


def slave(kind, state, count=1):
    return [("type", kind), ("state", state), ("number", str(count))]


# --- 01 contacts -----------------------------------------------------------
F1 = "01_contactos"


@element(F1, "contacto_na", ["Make contact (NO)", "Contacto NA", "Contact à fermeture (NO)", "Schließer",
                             "Contatto NA", "Contacto NA", "NA kontak (normalde açık)", "常开触点"],
         "slave", slave("simple", "NO"))
def _(s):
    make(s, 0, "13", "14")


@element(F1, "contacto_nc", ["Break contact (NC)", "Contacto NC", "Contact à ouverture (NF)", "Öffner",
                             "Contatto NC", "Contacto NF", "NK kontak (normalde kapalı)", "常闭触点"],
         "slave", slave("simple", "NC"))
def _(s):
    brk(s, 0, "21", "22")


@element(F1, "contacto_conmutado", ["Change-over contact", "Contacto conmutado", "Contact inverseur", "Wechsler",
                                    "Contatto in scambio", "Contacto comutado", "Değişken kontak", "转换触点"],
         "slave", slave("simple", "SW"))
def _(s):
    # common contact at the bottom; NC fixed contact on the left with its
    # hook, NO fixed contact straight above the pivot
    s.line(0, 10, 0, 20)
    s.line(0, 10, -9, -12)
    s.line(0, -20, 0, -10)
    s.path([(-20, -20), (-20, -6), (-4, -6)])
    s.term(0, 20, "s", "11")
    s.term(-20, -20, "n", "12")
    s.term(0, -20, "n", "14")
    s.number(2, 16, "11")
    s.number(-18, -16.5, "12")
    s.number(2, -16.5, "14")


for _file, _kind, _toward, _state, _top, _bot, _names in [
    ("contacto_na_temporizado_conexion", "NO", True, "delayOn", "67", "68",
     ["Make contact, on-delay (delayed closing)", "Contacto NA temporizado a la conexión",
      "Contact à fermeture temporisé au travail", "Schließer, ansprechverzögert",
      "Contatto NA ritardato all'eccitazione", "Contacto NA temporizado à ligação",
      "NA kontak, çekmede gecikmeli", "通电延时闭合常开触点"]),
    ("contacto_na_temporizado_desconexion", "NO", False, "delayOff", "67", "68",
     ["Make contact, off-delay (delayed opening)", "Contacto NA temporizado a la desconexión",
      "Contact à fermeture temporisé au repos", "Schließer, rückfallverzögert",
      "Contatto NA ritardato alla diseccitazione", "Contacto NA temporizado ao desligar",
      "NA kontak, bırakmada gecikmeli", "断电延时断开常开触点"]),
    ("contacto_nc_temporizado_conexion", "NC", True, "delayOn", "55", "56",
     ["Break contact, on-delay (delayed opening)", "Contacto NC temporizado a la conexión",
      "Contact à ouverture temporisé au travail", "Öffner, ansprechverzögert",
      "Contatto NC ritardato all'eccitazione", "Contacto NF temporizado à ligação",
      "NK kontak, çekmede gecikmeli", "通电延时断开常闭触点"]),
    ("contacto_nc_temporizado_desconexion", "NC", False, "delayOff", "55", "56",
     ["Break contact, off-delay (delayed closing)", "Contacto NC temporizado a la desconexión",
      "Contact à ouverture temporisé au repos", "Öffner, rückfallverzögert",
      "Contatto NC ritardato alla diseccitazione", "Contacto NF temporizado ao desligar",
      "NK kontak, bırakmada gecikmeli", "断电延时闭合常闭触点"]),
]:
    def _draw(s, k=_kind, t=_toward, top=_top, bot=_bot):
        (make if k == "NO" else brk)(s, 0, top, bot)
        delay(s, 0, k, t)
    element(F1, _file, _names, "slave", slave(_state, _kind))(_draw)


# --- 02 coils --------------------------------------------------------------
F2 = "02_bobinas"


def coil(s, extra=None):
    s.line(0, -20, 0, -6)
    s.rect(-12, -6, 24, 12)
    s.line(0, 6, 0, 20)
    if extra == "on":        # slow operating: square with a cross
        s.rect(-24, -6, 12, 12)
        s.line(-24, -6, -12, 6)
        s.line(-24, 6, -12, -6)
    elif extra == "off":     # slow releasing: filled square
        s.rect(-24, -6, 12, 12, FILL)
    s.term(0, -20, "n", "A1")
    s.term(0, 20, "s", "A2")
    s.number(2, -14, "A1")
    s.number(2, 14, "A2")


element(F2, "bobina", ["Coil (contactor or relay)", "Bobina de contactor o relé", "Bobine de contacteur ou de relais",
                       "Spule (Schütz oder Relais)", "Bobina di contattore o relè", "Bobina de contactor ou relé",
                       "Bobin (kontaktör veya röle)", "线圈（接触器或继电器）"],
        "master", [("type", "coil")])(lambda s: coil(s))
element(F2, "bobina_temporizada_conexion",
        ["Coil, on-delay (slow operating)", "Bobina temporizada a la conexión", "Bobine temporisée au travail",
         "Spule, ansprechverzögert", "Bobina ritardata all'eccitazione", "Bobina temporizada à ligação",
         "Çekmede gecikmeli bobin", "通电延时线圈"],
        "master", [("type", "coil")])(lambda s: coil(s, "on"))
element(F2, "bobina_temporizada_desconexion",
        ["Coil, off-delay (slow releasing)", "Bobina temporizada a la desconexión", "Bobine temporisée au repos",
         "Spule, rückfallverzögert", "Bobina ritardata alla diseccitazione", "Bobina temporizada ao desligar",
         "Bırakmada gecikmeli bobin", "断电延时线圈"],
        "master", [("type", "coil")])(lambda s: coil(s, "off"))


# --- 03 contactors (main contacts) -------------------------------------------
F3 = "03_contactores"


def multipole(s, n, qual, names=POWER_NAMES, bot_y=20):
    mids = [make(s, px, *names[i], qual=qual, bot_y=bot_y) for i, px in enumerate(poles(n))]
    link(s, mids[0][0], mids[-1][0])
    return mids


def pole_word(n, lang):
    return {
        "en": "1 pole" if n == 1 else "%d poles" % n,
        "es": "1 polo" if n == 1 else "%d polos" % n,
        "fr": "1 pôle" if n == 1 else "%d pôles" % n,
        "de": "%d-polig" % n,
        "it": "1 polo" if n == 1 else "%d poli" % n,
        "pt": "1 polo" if n == 1 else "%d polos" % n,
        "tr": "%d kutuplu" % n,
        "zh": "%d极" % n,
    }[lang]


def named(base, n):
    return [("%s%s%s" % (b, "" if lang == "zh" and b.endswith("，") else ", ", pole_word(n, lang))).replace("，, ", "，")
            for b, lang in zip(base, LANGS)]


for _n in (1, 3, 4):
    element(F3, "contactor_%dp" % _n,
            named(["Contactor main contacts", "Contactos principales de contactor", "Contacts principaux de contacteur",
                   "Schütz-Hauptkontakte", "Contatti principali di contattore", "Contactos principais de contactor",
                   "Kontaktör ana kontakları", "接触器主触点，"], _n),
            "slave", slave("power", "NO", _n))(lambda s, n=_n: multipole(s, n, "contactor"))


# --- 04 protection -----------------------------------------------------------
F4 = "04_proteccion"


def breaker(s, n, releases, names=POWER_NAMES):
    mids = multipole(s, n, "breaker", names)
    if releases == "magnetic":
        overcurrent_box(s, -26, -5)
        link(s, -14, mids[-1][0])
        return
    overcurrent_box(s, -26, -11)
    thermal_box(s, -26, 1)
    link(s, -14, -12, -6)
    link(s, -14, -12, 6)
    s.line(-12, -6, -12, 6, DASH)
    link(s, -12, mids[-1][0])
    if releases == "adjustable":
        s.line(-28.5, 12.5, -16.5, -0.5)
        s.arrowhead((-15.5, -1.6), (-28.5, 12.5))


for _n in (1, 2, 3, 4):
    element(F4, "interruptor_automatico_%dp" % _n,
            named(["Circuit breaker (thermal-magnetic)", "Interruptor automático magnetotérmico",
                   "Disjoncteur magnétothermique", "Leitungsschutzschalter (thermisch-magnetisch)",
                   "Interruttore magnetotermico", "Disjuntor magnetotérmico", "Termik-manyetik devre kesici",
                   "热磁式断路器，"], _n),
            "master", [("type", "protection")])(lambda s, n=_n: breaker(s, n, "thermal-magnetic"))

element(F4, "interruptor_automatico_magnetico_3p",
        named(["Circuit breaker (magnetic only)", "Interruptor automático magnético", "Disjoncteur magnétique",
               "Leistungsschalter (nur magnetisch)", "Interruttore magnetico", "Disjuntor magnético",
               "Manyetik devre kesici", "电磁式断路器，"], 3),
        "master", [("type", "protection")])(lambda s: breaker(s, 3, "magnetic"))
element(F4, "guardamotor_3p",
        named(["Motor protection circuit breaker", "Guardamotor", "Disjoncteur moteur", "Motorschutzschalter",
               "Interruttore salvamotore", "Disjuntor-motor", "Motor koruma şalteri", "电动机保护断路器，"], 3),
        "master", [("type", "protection")])(lambda s: breaker(s, 3, "adjustable"))


@element(F4, "rele_termico_3p",
         named(["Thermal overload relay", "Relé térmico", "Relais thermique", "Thermisches Überlastrelais",
                "Relè termico", "Relé térmico", "Termik aşırı yük rölesi", "热过载继电器，"], 3),
         "master", [("type", "protection")])
def _(s):
    for i, px in enumerate(poles(3)):
        s.rect(px - 6, -7, 12, 14)
        s.path([(px, -20), (px, -4), (px - 3, -4), (px - 3, 4), (px, 4), (px, 20)])
        top, bot = POWER_NAMES[i]
        s.term(px, -20, "n", top)
        s.term(px, 20, "s", bot)
        s.number(px + 2, -16.5, top)
        s.number(px + 2, 16, bot)
        if i:
            link(s, px - 14, px - 6)


def overload_contact(s, kind):
    if kind == "NC":
        mid = brk(s, 0, "95", "96")
    else:
        mid = make(s, 0, "97", "98")
    link(s, -8, mid[0])
    thermal_box(s, -20, -5)


element(F4, "rele_termico_contacto_nc",
        ["Overload relay contact NC (95-96)", "Contacto NC del relé térmico (95-96)",
         "Contact NF du relais thermique (95-96)", "Öffner des Überlastrelais (95-96)",
         "Contatto NC del relè termico (95-96)", "Contacto NF do relé térmico (95-96)",
         "Termik röle NK kontağı (95-96)", "热继电器常闭触点 (95-96)"],
        "slave", slave("simple", "NC"))(lambda s: overload_contact(s, "NC"))
element(F4, "rele_termico_contacto_na",
        ["Overload relay contact NO (97-98)", "Contacto NA del relé térmico (97-98)",
         "Contact NO du relais thermique (97-98)", "Schließer des Überlastrelais (97-98)",
         "Contatto NA del relè termico (97-98)", "Contacto NA do relé térmico (97-98)",
         "Termik röle NA kontağı (97-98)", "热继电器常开触点 (97-98)"],
        "slave", slave("simple", "NO"))(lambda s: overload_contact(s, "NO"))


def rcd(s, n):
    names = [("1", "2"), ("N", "N")] if n == 2 else [("1", "2"), ("3", "4"), ("5", "6"), ("N", "N")]
    # longer lower leads leave room for the summation current transformer
    mids = multipole(s, n, None, names, bot_y=30)
    s.rect(-30, -6, 14, 12)
    s.text(-23, 0, "IΔ", 6)
    link(s, -16, mids[-1][0])
    last = poles(n)[-1]
    s.ellipse(last / 2, 16, last / 2 + 6, 2.5)
    s.path([(-6, 16), (-23, 16), (-23, 6)])


for _n in (2, 4):
    element(F4, "interruptor_diferencial_%dp" % _n,
            named(["Residual current device (RCCB)", "Interruptor diferencial", "Interrupteur différentiel",
                   "Fehlerstrom-Schutzschalter (RCCB)", "Interruttore differenziale", "Interruptor diferencial",
                   "Kaçak akım rölesi", "剩余电流动作断路器，"], _n),
            "simple")(lambda s, n=_n: rcd(s, n))


def fuse(s, px, top, bot):
    s.line(px, -20, px, 20)
    s.rect(px - 3, -9, 6, 18)
    s.term(px, -20, "n", top)
    s.term(px, 20, "s", bot)
    s.number(px + 5, -16.5, top)
    s.number(px + 5, 16, bot)


element(F4, "fusible", ["Fuse", "Fusible", "Fusible", "Sicherung", "Fusibile", "Fusível", "Sigorta", "熔断器"],
        "simple")(lambda s: fuse(s, 0, "1", "2"))


@element(F4, "fusibles_3p", named(["Fuses", "Fusibles", "Fusibles", "Sicherungen", "Fusibili", "Fusíveis",
                                   "Sigortalar", "熔断器，"], 3))
def _(s):
    for i, px in enumerate(poles(3)):
        fuse(s, px, *POWER_NAMES[i])


def fuse_disconnector(s, n):
    mids = []
    for i, px in enumerate(poles(n)):
        top, bot = POWER_NAMES[i]
        s.line(px, -20, px, -10)
        s.line(px, 10, px, 20)
        s.line(px, 10, px + NO_TIP[0], NO_TIP[1])
        qualifier(s, px, "disconnector")
        # fuse body along the moving contact
        d = math.hypot(NO_TIP[0], NO_TIP[1] - 10)
        ux, uy = NO_TIP[0] / d, (NO_TIP[1] - 10) / d
        nx, ny = -uy, ux
        mx, my = px + NO_TIP[0] / 2, 0
        hl, hw = 7, 2.6
        s.path([(mx + ux * hl + nx * hw, my + uy * hl + ny * hw),
                (mx + ux * hl - nx * hw, my + uy * hl - ny * hw),
                (mx - ux * hl - nx * hw, my - uy * hl - ny * hw),
                (mx - ux * hl + nx * hw, my - uy * hl + ny * hw)], STROKE, True)
        s.term(px, -20, "n", top)
        s.term(px, 20, "s", bot)
        s.number(px + 2, -16.5, top)
        s.number(px + 2, 16, bot)
        mids.append(mx)
    link(s, mids[0], mids[-1])


for _n in (1, 3):
    element(F4, "fusible_seccionador_%dp" % _n,
            named(["Fuse-disconnector", "Fusible-seccionador", "Fusible-sectionneur", "Sicherungs-Trennschalter",
                   "Fusibile-sezionatore", "Fusível-seccionador", "Sigortalı ayırıcı", "熔断器式隔离器，"], _n),
            "simple")(lambda s, n=_n: fuse_disconnector(s, n))


@element(F4, "protector_sobretensiones",
         ["Surge arrester", "Protector contra sobretensiones", "Parafoudre", "Überspannungsableiter",
          "Scaricatore di sovratensioni", "Descarregador de sobretensões", "Aşırı gerilim koruyucu", "电涌保护器"])
def _(s):
    s.line(0, -20, 0, -8)
    s.rect(-4, -8, 8, 16)
    s.line(0, -8, 0, -1)
    s.arrowhead((0, 3), (0, -8), 4, 5)
    s.line(0, 8, 0, 20)
    s.term(0, -20, "n", "L")
    s.term(0, 20, "s", "PE")
    s.number(2, -16.5, "L")
    s.number(2, 16, "PE")


# --- 05 isolation ------------------------------------------------------------
F5 = "05_seccionamiento"
element(F5, "seccionador_3p",
        named(["Disconnector", "Seccionador", "Sectionneur", "Trennschalter", "Sezionatore", "Seccionador",
               "Ayırıcı", "隔离器，"], 3))(lambda s: multipole(s, 3, "disconnector"))
for _n in (3, 4):
    element(F5, "interruptor_seccionador_%dp" % _n,
            named(["Switch-disconnector", "Interruptor-seccionador", "Interrupteur-sectionneur", "Lasttrennschalter",
                   "Interruttore-sezionatore", "Interruptor-seccionador", "Yük ayırıcı", "隔离开关，"], _n)
            )(lambda s, n=_n: multipole(s, n, "switch-disconnector"))


# --- 06 push-buttons and selectors ---------------------------------------------
F6 = "06_mando"


@element(F6, "pulsador_na", ["Push-button, NO", "Pulsador NA", "Bouton-poussoir, NO", "Taster, Schließer",
                             "Pulsante NA", "Botão de pressão NA", "Buton, NA", "按钮，常开"])
def _(s):
    mid = make(s, 0, "13", "14")
    link(s, -16, mid[0])
    push(s, -16)


@element(F6, "pulsador_nc", ["Push-button, NC", "Pulsador NC", "Bouton-poussoir, NF", "Taster, Öffner",
                             "Pulsante NC", "Botão de pressão NF", "Buton, NK", "按钮，常闭"])
def _(s):
    mid = brk(s, 0, "21", "22")
    link(s, -12, mid[0])
    push(s, -12)


@element(F6, "pulsador_na_nc", ["Push-button, NO + NC", "Pulsador NA + NC", "Bouton-poussoir, NO + NF",
                                "Taster, Schließer + Öffner", "Pulsante NA + NC", "Botão de pressão NA + NF",
                                "Buton, NA + NK", "按钮，常开+常闭"])
def _(s):
    make(s, 0, "13", "14")
    mid = brk(s, 20, "21", "22")
    link(s, -16, mid[0])
    push(s, -16)


@element(F6, "parada_emergencia",
         ["Emergency stop (mushroom head, latching, positive opening)",
          "Parada de emergencia (seta, enclavamiento, apertura positiva)",
          "Arrêt d'urgence (coup de poing, accrochage, ouverture positive)",
          "Not-Halt (Pilztaster, rastend, Zwangsöffnung)",
          "Arresto di emergenza (a fungo, ritenuta, apertura positiva)",
          "Paragem de emergência (cogumelo, encravamento, abertura positiva)",
          "Acil stop (mantar başlı, kilitlemeli, pozitif açma)",
          "急停按钮（蘑菇头，自锁，强制断开）"])
def _(s):
    mid = brk(s, 0, "11", "12")
    link(s, -22, -9.5)
    link(s, -2.5, mid[0])
    positive_opening(s, -6)
    detent(s, -15)
    mushroom(s, -22)


def selector(s, kind):
    if kind == "NO":
        mid = make(s, 0, "13", "14")
        x = -22
    else:
        mid = brk(s, 0, "21", "22")
        x = -18
    link(s, x, mid[0])
    detent(s, x + 10)
    turn(s, x)


element(F6, "selector_2_posiciones_na",
        ["Selector switch, 2 positions, NO", "Selector de 2 posiciones NA", "Commutateur 2 positions, NO",
         "Wahlschalter, 2 Stellungen, Schließer", "Selettore a 2 posizioni NA", "Seletor de 2 posições NA",
         "2 konumlu seçici, NA", "两位选择开关，常开"])(lambda s: selector(s, "NO"))
element(F6, "selector_2_posiciones_nc",
        ["Selector switch, 2 positions, NC", "Selector de 2 posiciones NC", "Commutateur 2 positions, NF",
         "Wahlschalter, 2 Stellungen, Öffner", "Selettore a 2 posizioni NC", "Seletor de 2 posições NF",
         "2 konumlu seçici, NK", "两位选择开关，常闭"])(lambda s: selector(s, "NC"))


# --- 07 position switches and sensors ------------------------------------------
F7 = "07_sensores"


def position_mark(s, kind):
    """Position switch qualifier: triangle on the moving contact."""
    tip = NO_TIP if kind == "NO" else NC_TIP
    def p(t):
        return (tip[0] * t, 10 + (tip[1] - 10) * t)
    v1, v2 = p(0.7), p(0.35)
    vy = v1[1] + (v2[1] - v1[1]) * 0.35
    s.path([v1, (blade_x(0, vy, kind) - 6, vy), v2])


def sensing(s, kind, actuator):
    if kind == "NO":
        mid = make(s, 0, "13", "14")
    else:
        mid = brk(s, 0, "21", "22")
    if actuator == "position":
        position_mark(s, kind)
        return
    end = -14 if kind == "NO" else -10
    link(s, end, mid[0])
    if actuator == "proximity":
        cx = end - 5
        s.path([(end, 0), (cx, -5), (cx - 5, 0), (cx, 5)], STROKE, True)
    else:
        s.text(end - 4, 0, actuator, 8)


for _kind, _sfx, _w in (("NO", "na", ("NO", "NA", "NO", "Schließer", "NA", "NA", "NA", "常开")),
                        ("NC", "nc", ("NC", "NC", "NF", "Öffner", "NC", "NF", "NK", "常闭"))):
    element(F7, "final_de_carrera_" + _sfx,
            ["Position switch (limit switch), " + _w[0], "Final de carrera " + _w[1],
             "Interrupteur de position, " + _w[2], "Positionsschalter, " + _w[3], "Finecorsa " + _w[4],
             "Fim de curso " + _w[5], "Limit anahtarı, " + _w[6], "行程开关，" + _w[7]],
            )(lambda s, k=_kind: sensing(s, k, "position"))
    element(F7, "termostato_" + _sfx,
            ["Thermostat, " + _w[0], "Termostato " + _w[1], "Thermostat, " + _w[2], "Thermostat, " + _w[3],
             "Termostato " + _w[4], "Termóstato " + _w[5], "Termostat, " + _w[6], "温度开关，" + _w[7]],
            )(lambda s, k=_kind: sensing(s, k, "θ"))
    element(F7, "presostato_" + _sfx,
            ["Pressure switch, " + _w[0], "Presostato " + _w[1], "Pressostat, " + _w[2],
             "Druckschalter, " + _w[3], "Pressostato " + _w[4], "Pressostato " + _w[5],
             "Basınç anahtarı, " + _w[6], "压力开关，" + _w[7]],
            )(lambda s, k=_kind: sensing(s, k, "p"))

element(F7, "detector_proximidad_na",
        ["Proximity switch, NO", "Detector de proximidad NA", "Détecteur de proximité, NO",
         "Näherungsschalter, Schließer", "Sensore di prossimità NA", "Sensor de proximidade NA",
         "Yaklaşım sensörü, NA", "接近开关，常开"])(lambda s: sensing(s, "NO", "proximity"))


# --- 08 machines -------------------------------------------------------------
F8 = "08_maquinas"


def machine(s, letter, current, top, bottom=(), pe=True):
    r = 15
    s.circle(0, 0, r)
    xs = [-10, 0, 10] if len(top) == 3 else [-10, 10]
    # three leads 10 apart leave little room: smaller terminal markings
    size = 4 if len(top) == 3 else NUMBER_SIZE
    for x, name in zip(xs, top):
        s.line(x, -30, x, -math.sqrt(r * r - x * x))
        s.term(x, -30, "n", name)
        s.number(x + 1.2, -25, name, size)
    for x, name in zip(xs, bottom):
        s.line(x, 30, x, math.sqrt(r * r - x * x))
        s.term(x, 30, "s", name)
        s.number(x + 1.2, 25, name, size)
    if pe:
        s.line(r, 0, 30, 0)
        s.term(30, 0, "e", "PE")
        s.number(20, -3.5, "PE")
    s.text(0, -3, letter, 12)
    if current == "DC":
        s.line(-5, 8, 5, 8)
    else:
        s.text(0, 8, current, 7)


element(F8, "motor_trifasico",
        ["Three-phase motor", "Motor trifásico", "Moteur triphasé", "Drehstrommotor", "Motore trifase",
         "Motor trifásico", "Üç fazlı motor", "三相电动机"])(lambda s: machine(s, "M", "3~", ("U1", "V1", "W1")))
element(F8, "motor_trifasico_6_bornes",
        ["Three-phase motor, 6 terminals", "Motor trifásico de 6 bornes", "Moteur triphasé à 6 bornes",
         "Drehstrommotor, 6 Anschlüsse", "Motore trifase a 6 morsetti", "Motor trifásico de 6 terminais",
         "Üç fazlı motor, 6 uçlu", "三相电动机，6个接线端"]
        )(lambda s: machine(s, "M", "3~", ("U1", "V1", "W1"), ("U2", "V2", "W2")))
element(F8, "motor_monofasico",
        ["Single-phase motor", "Motor monofásico", "Moteur monophasé", "Einphasenmotor", "Motore monofase",
         "Motor monofásico", "Tek fazlı motor", "单相电动机"])(lambda s: machine(s, "M", "1~", ("U1", "U2")))
element(F8, "motor_corriente_continua",
        ["DC motor", "Motor de corriente continua", "Moteur à courant continu", "Gleichstrommotor",
         "Motore in corrente continua", "Motor de corrente contínua", "Doğru akım motoru", "直流电动机"]
        )(lambda s: machine(s, "M", "DC", ("A1", "A2")))
element(F8, "generador_trifasico",
        ["Three-phase generator", "Generador trifásico", "Générateur triphasé", "Drehstromgenerator",
         "Generatore trifase", "Gerador trifásico", "Üç fazlı jeneratör", "三相发电机"]
        )(lambda s: machine(s, "G", "3~", ("U1", "V1", "W1")))


# --- 09 transformers and supplies -----------------------------------------------
F9 = "09_transformadores"


@element(F9, "transformador_monofasico",
         ["Single-phase transformer", "Transformador monofásico", "Transformateur monophasé",
          "Einphasentransformator", "Trasformatore monofase", "Transformador monofásico",
          "Tek fazlı transformatör", "单相变压器"])
def _(s):
    r, c = 12, 8
    s.circle(0, -c, r)
    s.circle(0, c, r)
    k = math.sqrt(r * r - 100)
    for x, top, bot in ((-10, "1U1", "2U1"), (10, "1U2", "2U2")):
        s.line(x, -30, x, -c - k)
        s.line(x, 30, x, c + k)
        s.term(x, -30, "n", top)
        s.term(x, 30, "s", bot)
        s.number(x + 1.5, -25, top)
        s.number(x + 1.5, 25, bot)


@element(F9, "transformador_intensidad",
         ["Current transformer", "Transformador de intensidad", "Transformateur de courant", "Stromwandler",
          "Trasformatore di corrente", "Transformador de corrente", "Akım transformatörü", "电流互感器"])
def _(s):
    s.line(0, -20, 0, 20)
    s.circle(0, 0, math.sqrt(50))
    s.path([(5, -5), (10, -10), (20, -10)])
    s.path([(5, 5), (10, 10), (20, 10)])
    s.term(0, -20, "n", "P1")
    s.term(0, 20, "s", "P2")
    s.term(20, -10, "e", "S1")
    s.term(20, 10, "e", "S2")
    s.number(2, -16.5, "P1")
    s.number(2, 16.5, "P2")
    s.number(13, -14, "S1")
    s.number(13, 14, "S2")


@element(F9, "fuente_alimentacion_ca_cc",
         ["Power supply AC/DC", "Fuente de alimentación CA/CC", "Alimentation CA/CC", "Netzteil AC/DC",
          "Alimentatore CA/CC", "Fonte de alimentação CA/CC", "AC/DC güç kaynağı", "交流/直流电源"])
def _(s):
    s.rect(-15, -15, 30, 30)
    s.line(-15, 15, 15, -15)
    s.text(-7, -7, "~", 10)
    s.line(3, 7, 11, 7)
    for x, top, bot in ((-10, "L", "+"), (10, "N", "-")):
        s.line(x, -30, x, -15)
        s.line(x, 15, x, 30)
        s.term(x, -30, "n", top)
        s.term(x, 30, "s", bot)
        s.number(x + 1.5, -25, top)
        s.number(x + 1.5, 25, bot)


@element(F9, "bateria", ["Battery", "Batería", "Batterie", "Batterie", "Batteria", "Bateria", "Akü", "蓄电池"])
def _(s):
    s.line(0, -20, 0, -2)
    s.line(-8, -2, 8, -2)
    s.line(-4, 2, 4, 2, THICK)
    s.line(0, 2, 0, 20)
    s.term(0, -20, "n", "+")
    s.term(0, 20, "s", "-")
    s.number(2, -16.5, "+")
    s.number(2, 16, "-")


# --- 10 signalling and measuring -------------------------------------------------
F10 = "10_senalizacion_medida"


@element(F10, "piloto_senalizacion",
         ["Indicator lamp", "Piloto de señalización", "Voyant lumineux", "Leuchtmelder",
          "Lampada di segnalazione", "Sinalizador luminoso", "Sinyal lambası", "指示灯"])
def _(s):
    r = 8
    s.line(0, -20, 0, -r)
    s.circle(0, 0, r)
    k = r / math.sqrt(2)
    s.line(-k, -k, k, k)
    s.line(-k, k, k, -k)
    s.line(0, r, 0, 20)
    s.term(0, -20, "n", "X1")
    s.term(0, 20, "s", "X2")
    s.number(2, -16.5, "X1")
    s.number(2, 16, "X2")


def indicating(s, unit, size=10):
    r = 10
    s.line(0, -20, 0, -r)
    s.circle(0, 0, r)
    s.line(0, r, 0, 20)
    s.text(0, 0, unit, size)
    s.term(0, -20, "n", "1")
    s.term(0, 20, "s", "2")


element(F10, "amperimetro", ["Ammeter", "Amperímetro", "Ampèremètre", "Amperemeter", "Amperometro", "Amperímetro",
                             "Ampermetre", "电流表"])(lambda s: indicating(s, "A"))
element(F10, "voltimetro", ["Voltmeter", "Voltímetro", "Voltmètre", "Voltmeter", "Voltmetro", "Voltímetro",
                            "Voltmetre", "电压表"])(lambda s: indicating(s, "V"))
element(F10, "frecuencimetro", ["Frequency meter", "Frecuencímetro", "Fréquencemètre", "Frequenzmesser",
                                "Frequenzimetro", "Frequencímetro", "Frekansmetre", "频率表"]
        )(lambda s: indicating(s, "Hz", 7))


@element(F10, "contador_energia_monofasico",
         ["Energy meter, single-phase", "Contador de energía monofásico", "Compteur d'énergie monophasé",
          "Wechselstromzähler", "Contatore di energia monofase", "Contador de energia monofásico",
          "Tek fazlı enerji sayacı", "单相电能表"])
def _(s):
    s.rect(-15, -15, 30, 30)
    s.line(-15, -9, 15, -9)
    s.text(0, 3, "Wh", 8)
    for x, top, bot in ((-10, "1", "3"), (10, "4", "6")):
        s.line(x, -30, x, -15)
        s.line(x, 15, x, 30)
        s.term(x, -30, "n", top)
        s.term(x, 30, "s", bot)
        s.number(x + 1.5, -25, top)
        s.number(x + 1.5, 25, bot)


@element(F10, "contador_horas", ["Hour meter", "Contador de horas", "Compteur horaire", "Betriebsstundenzähler",
                                 "Contaore", "Contador de horas", "Çalışma saati sayacı", "计时器"])
def _(s):
    s.line(0, -20, 0, -10)
    s.rect(-10, -10, 20, 20)
    s.line(-10, -5, 10, -5)
    s.text(0, 2.5, "h", 8)
    s.line(0, 10, 0, 20)
    s.term(0, -20, "n", "1")
    s.term(0, 20, "s", "2")


# --- 11 terminals, earthing, connectors --------------------------------------------
F11 = "11_conexiones"


def terminal_block(s, pe=False):
    s.line(0, -10, 0, -2.5)
    s.circle(0, 0, 2.5)
    s.line(0, 2.5, 0, 10)
    s.term(0, -10, "n")
    s.term(0, 10, "s")
    s.label_at = (-6, 0)
    if pe:
        s.number(4, 0, "PE")


element(F11, "borne", ["Terminal", "Borne", "Borne", "Klemme", "Morsetto", "Borne", "Klemens", "端子"],
        "terminal", [("type", "generic"), ("function", "generic")])(lambda s: terminal_block(s))
element(F11, "borne_tierra_pe",
        ["Protective earth terminal (PE)", "Borne de tierra (PE)", "Borne de terre (PE)",
         "Schutzleiterklemme (PE)", "Morsetto di terra (PE)", "Borne de terra (PE)",
         "Koruma topraklama klemensi (PE)", "保护接地端子 (PE)"],
        "terminal", [("type", "ground"), ("function", "generic")])(lambda s: terminal_block(s, True))


@element(F11, "tierra", ["Earth", "Tierra", "Terre", "Erde", "Terra", "Terra", "Toprak", "接地"])
def _(s):
    s.line(0, -10, 0, 0)
    s.line(-8, 0, 8, 0)
    s.line(-5, 3, 5, 3)
    s.line(-2, 6, 2, 6)
    s.term(0, -10, "n")


@element(F11, "tierra_proteccion", ["Protective earth", "Tierra de protección", "Terre de protection",
                                    "Schutzerde", "Terra di protezione", "Terra de proteção",
                                    "Koruma topraklaması", "保护接地"])
def _(s):
    s.line(0, -10, 0, 1)
    s.circle(0, 4, 8)
    s.line(-5, 1, 5, 1)
    s.line(-3, 4, 3, 4)
    s.line(-1.2, 7, 1.2, 7)
    s.term(0, -10, "n", "PE")


@element(F11, "masa", ["Frame, chassis", "Masa", "Masse", "Masse", "Massa", "Massa", "Şase", "机壳"])
def _(s):
    s.line(0, -10, 0, 0)
    s.line(-8, 0, 8, 0)
    for x in (-6, 0, 6):
        s.line(x, 0, x - 3, 5)
    s.term(0, -10, "n")


@element(F11, "conexion_equipotencial",
         ["Equipotential bonding", "Conexión equipotencial", "Liaison équipotentielle", "Potentialausgleich",
          "Collegamento equipotenziale", "Ligação equipotencial", "Eş potansiyel bağlantı", "等电位连接"])
def _(s):
    s.line(0, -10, 0, 0)
    s.path([(-7, 0), (7, 0), (0, 7)], STROKE, True)
    s.term(0, -10, "n")


@element(F11, "conector_clavija_base",
         ["Plug and socket", "Conector (clavija y base)", "Fiche et socle", "Steckverbindung", "Spina e presa",
          "Ficha e tomada", "Fiş ve priz", "插头和插座"])
def _(s):
    s.line(0, -20, 0, -6)
    s.arc(0, -2, 4, 0, 180)
    s.line(0, -4, 0, 6, THICK)
    s.line(0, 6, 0, 20)
    s.term(0, -20, "n", "1")
    s.term(0, 20, "s", "1")
    s.number(2, -16.5, "1")
    s.number(2, 16, "1")


# --- 12 components -------------------------------------------------------------
F12 = "12_componentes"


def two_leads(s, top_end, bot_end, top="1", bot="2"):
    s.line(0, -20, 0, top_end)
    s.line(0, bot_end, 0, 20)
    s.term(0, -20, "n", top)
    s.term(0, 20, "s", bot)


def resistor(s, mark=None):
    two_leads(s, -9, 9)
    s.rect(-3, -9, 6, 18)
    if mark == "adjustable":
        s.line(-8, 9, 6.5, -7.5)
        s.arrowhead((8, -9.5), (-8, 9))
    elif mark == "varistor":
        s.path([(-10, 9), (-6, 9), (7, -9)])
        s.text(-8, -5, "U", 7)


element(F12, "resistencia", ["Resistor", "Resistencia", "Résistance", "Widerstand", "Resistore", "Resistência",
                             "Direnç", "电阻器"])(lambda s: resistor(s))
element(F12, "resistencia_ajustable", ["Adjustable resistor", "Resistencia ajustable", "Résistance réglable",
                                       "Einstellbarer Widerstand", "Resistore regolabile", "Resistência ajustável",
                                       "Ayarlı direnç", "可调电阻器"])(lambda s: resistor(s, "adjustable"))
element(F12, "varistor", ["Varistor (VDR)", "Varistor", "Varistance", "Varistor", "Varistore", "Varístor",
                          "Varistör", "压敏电阻器"])(lambda s: resistor(s, "varistor"))


def capacitor(s, polarized=False):
    if polarized:
        two_leads(s, -2, 2, "+", "-")
        s.text(6, -7, "+", 7)
    else:
        two_leads(s, -2, 2)
    s.line(-7, -2, 7, -2)
    s.line(-7, 2, 7, 2)


element(F12, "condensador", ["Capacitor", "Condensador", "Condensateur", "Kondensator", "Condensatore",
                             "Condensador", "Kondansatör", "电容器"])(lambda s: capacitor(s))
element(F12, "condensador_polarizado", ["Polarized capacitor", "Condensador polarizado", "Condensateur polarisé",
                                        "Gepolter Kondensator", "Condensatore polarizzato", "Condensador polarizado",
                                        "Kutuplu kondansatör", "极性电容器"])(lambda s: capacitor(s, True))


@element(F12, "inductancia", ["Inductor", "Inductancia", "Inductance", "Induktivität", "Induttore", "Indutância",
                              "Endüktans", "电感器"])
def _(s):
    two_leads(s, -12, 12)
    for cy in (-9, -3, 3, 9):
        s.arc(0, cy, 3, -90, 180)


def diode(s, led=False):
    s.line(0, -20, 0, 20)
    s.path([(-5, -4), (5, -4), (0, 4)], STROKE, True)
    s.line(-5, 4, 5, 4)
    s.term(0, -20, "n", "A")
    s.term(0, 20, "s", "K")
    s.number(2, -16.5, "A")
    s.number(2, 16, "K")
    if led:
        for y in (-3, 1):
            s.line(6, y, 10, y - 4)
            s.arrowhead((11.5, y - 5.5), (6, y), 2.5, 2.2)


element(F12, "diodo", ["Diode", "Diodo", "Diode", "Diode", "Diodo", "Díodo", "Diyot", "二极管"])(lambda s: diode(s))
element(F12, "diodo_led", ["Light-emitting diode (LED)", "Diodo LED", "Diode électroluminescente (LED)",
                           "Leuchtdiode (LED)", "Diodo LED", "Díodo emissor de luz (LED)",
                           "Işık yayan diyot (LED)", "发光二极管"])(lambda s: diode(s, True))


# ===========================================================================
# Folders
# ===========================================================================
FOLDERS = {
    "": ["IEC 60617 – Control panels", "IEC 60617 – Cuadros eléctricos", "IEC 60617 – Armoires électriques",
         "IEC 60617 – Schaltschrank", "IEC 60617 – Quadri elettrici", "IEC 60617 – Quadros elétricos",
         "IEC 60617 – Elektrik panoları", "IEC 60617 – 电气控制柜"],
    F1: ["Contacts", "Contactos", "Contacts", "Kontakte", "Contatti", "Contactos", "Kontaklar", "触点"],
    F2: ["Coils", "Bobinas", "Bobines", "Spulen", "Bobine", "Bobinas", "Bobinler", "线圈"],
    F3: ["Contactors (main contacts)", "Contactores (contactos principales)", "Contacteurs (contacts principaux)",
         "Schütze (Hauptkontakte)", "Contattori (contatti principali)", "Contactores (contactos principais)",
         "Kontaktörler (ana kontaklar)", "接触器（主触点）"],
    F4: ["Protection", "Protección", "Protection", "Schutzgeräte", "Protezione", "Proteção", "Koruma", "保护器件"],
    F5: ["Isolation and switching", "Seccionamiento", "Sectionnement", "Trennen und Schalten", "Sezionamento",
         "Seccionamento", "Ayırma ve anahtarlama", "隔离与开关"],
    F6: ["Push-buttons and selectors", "Pulsadores y selectores", "Boutons-poussoirs et commutateurs",
         "Taster und Wahlschalter", "Pulsanti e selettori", "Botões e seletores", "Butonlar ve seçiciler",
         "按钮与选择开关"],
    F7: ["Position switches and sensors", "Finales de carrera y detectores", "Interrupteurs de position et détecteurs",
         "Positionsschalter und Sensoren", "Finecorsa e sensori", "Fins de curso e sensores",
         "Limit anahtarları ve sensörler", "行程开关与传感器"],
    F8: ["Motors and generators", "Motores y generadores", "Moteurs et générateurs", "Motoren und Generatoren",
         "Motori e generatori", "Motores e geradores", "Motorlar ve jeneratörler", "电动机与发电机"],
    F9: ["Transformers and power supplies", "Transformadores y fuentes", "Transformateurs et alimentations",
         "Transformatoren und Netzteile", "Trasformatori e alimentatori", "Transformadores e fontes",
         "Transformatörler ve güç kaynakları", "变压器与电源"],
    F10: ["Signalling and measuring", "Señalización y medida", "Signalisation et mesure", "Melden und Messen",
          "Segnalazione e misura", "Sinalização e medição", "Sinyalizasyon ve ölçüm", "信号与测量"],
    F11: ["Terminals, earthing and connectors", "Bornes, tierras y conectores", "Bornes, terres et connecteurs",
          "Klemmen, Erdung und Steckverbinder", "Morsetti, terre e connettori", "Bornes, terras e conectores",
          "Klemensler, topraklama ve konnektörler", "端子、接地与连接器"],
    F12: ["Components", "Componentes", "Composants", "Bauelemente", "Componenti", "Componentes", "Bileşenler",
          "元器件"],
}


def directory_xml(names):
    out = ["<qet-directory>", "    <names>"]
    for lang in sorted(LANGS):
        out.append('        <name lang="%s">%s</name>' % (lang, escape(names[LANGS.index(lang)], quote=False)))
    out += ["    </names>", "</qet-directory>"]
    return "\n".join(out) + "\n"


# ===========================================================================
# Preview (SVG in an HTML page)
# ===========================================================================
def to_svg(sym, scale=3.0):
    xs = [p[0] for p in sym.vis]
    ys = [p[1] for p in sym.vis]
    rx, cy = sym.label_right()
    x0, x1 = min(min(xs), rx - 22) - 6, max(xs) + 18
    y0, y1 = min(ys) - 8, max(ys) + 8
    w, h = x1 - x0, y1 - y0
    out = ['<svg viewBox="%s %s %s %s" width="%d" height="%d" xmlns="http://www.w3.org/2000/svg">'
           % (num(x0), num(y0), num(w), num(h), w * scale, h * scale)]
    def stroke(style):
        st = dict(kv.split(":") for kv in style.split(";"))
        sw = {"thin": 0.5, "normal": 1, "hight": 2.2}.get(st.get("line-weight"), 1)
        fill = "#000" if st.get("filling") == "black" else "none"
        dash = ' stroke-dasharray="3 2"' if st.get("line-style") == "dashed" else ""
        return 'stroke="#000" stroke-width="%s" fill="%s"%s' % (sw, fill, dash)
    for kind, d in sym.prims:
        if kind == "line":
            out.append('<line x1="%s" y1="%s" x2="%s" y2="%s" %s/>' % (*map(num, d[:4]), stroke(d[4])))
        elif kind == "poly":
            pts, style, closed = d
            tag = "polygon" if closed else "polyline"
            out.append('<%s points="%s" %s/>' % (tag, " ".join("%s,%s" % (num(a), num(b)) for a, b in pts), stroke(style)))
        elif kind == "rect":
            out.append('<rect x="%s" y="%s" width="%s" height="%s" %s/>' % (*map(num, d[:4]), stroke(d[4])))
        elif kind == "ellipse":
            x, y, ew, eh, style = d
            out.append('<ellipse cx="%s" cy="%s" rx="%s" ry="%s" %s/>' % (num(x + ew / 2), num(y + eh / 2), num(ew / 2), num(eh / 2), stroke(style)))
        elif kind == "arc":
            cx, cy2, r, start, span, style = d
            a0, a1 = math.radians(start), math.radians(start + span)
            p0 = (cx + r * math.cos(a0), cy2 - r * math.sin(a0))
            p1 = (cx + r * math.cos(a1), cy2 - r * math.sin(a1))
            large = 1 if abs(span) > 180 else 0
            sweep = 0 if span > 0 else 1
            out.append('<path d="M%s %s A%s %s 0 %d %d %s %s" %s/>' % (num(p0[0]), num(p0[1]), num(r), num(r), large, sweep, num(p1[0]), num(p1[1]), stroke(style)))
        elif kind == "text":
            cx, base, txt, size = d
            out.append('<text x="%s" y="%s" font-size="%s" text-anchor="middle" font-family="Liberation Sans, Arial">%s</text>'
                       % (num(cx), num(base), num(size * PX_PER_PT), escape(txt)))
        elif kind == "number":
            x, cyn, txt, size = d
            em = size * PX_PER_PT
            out.append('<text x="%s" y="%s" font-size="%s" fill="#c00" font-family="Liberation Sans, Arial">%s</text>'
                       % (num(x), num(cyn + CAP * em / 2), num(em), escape(txt)))
    em = LABEL_SIZE * PX_PER_PT
    out.append('<text x="%s" y="%s" font-size="%s" text-anchor="end" fill="#06c" font-family="Liberation Sans, Arial">-K1</text>'
               % (num(rx), num(cy + CAP * em / 2), num(em)))
    for x, y, o, name in sym.terms:
        out.append('<circle cx="%s" cy="%s" r="1.2" fill="#0a0"/>' % (num(x), num(y)))
    out.append("</svg>")
    return "".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", help="write an HTML page with every symbol")
    ap.add_argument("--no-write", action="store_true", help="do not write the .elmt files")
    args = ap.parse_args()

    cards = []
    for spec in ELEMENTS:
        sym = Sym()
        spec["draw"](sym)
        rel = "%s/%s.elmt" % (spec["folder"], spec["file"])
        if not args.no_write:
            path = OUT / spec["folder"] / (spec["file"] + ".elmt")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(to_xml(sym, spec, rel), encoding="utf-8", newline="\n")
        cards.append('<figure>%s<figcaption><b>%s</b><br>%s</figcaption></figure>'
                     % (to_svg(sym), escape(spec["names"]["es"]), escape(rel)))
    if not args.no_write:
        for folder, names in FOLDERS.items():
            d = OUT / folder if folder else OUT
            d.mkdir(parents=True, exist_ok=True)
            (d / "qet_directory").write_text(directory_xml(names), encoding="utf-8", newline="\n")
    if args.preview:
        page = ('<!doctype html><meta charset="utf-8"><style>body{font:12px Arial;background:#fff;margin:8px}'
                'figure{display:inline-block;vertical-align:top;margin:4px;padding:4px;border:1px solid #ddd;'
                'text-align:center;min-width:230px}svg{display:block;margin:auto}figcaption{margin-top:2px}</style>'
                + "".join(cards))
        Path(args.preview).write_text(page, encoding="utf-8")
    print("%d elements" % len(ELEMENTS))


if __name__ == "__main__":
    main()
