"""
lochraster_plan.py
---------------------------------------------------------------------------
Zeichnet den Bestückungsplan der Lochrasterplatine (Ansicht von oben,
Bauteilseite) als SVG nach docs/Lochraster_Bestueckung.svg.

Alle Bauteile und Verbindungen stehen in den Listen unten. Änderungen nur
dort, dann neu erzeugen:   py tools/lochraster_plan.py

Raster: 2,54 mm. Spalten A–X (24), Zeilen 1–30 (Platine 70 × 90 mm).
Ein Loch wird als "B11" geschrieben (Spalte B, Zeile 11).
"""
from pathlib import Path

SPALTEN = 24
ZEILEN = 30
RASTER = 22          # Pixel je 2,54 mm
RAND_L, RAND_O = 70, 110

# ---------------------------------------------------------------------------
# ESP32-DevKitC (38 Pins), Antenne oben, USB unten. Belegung nach Espressif
# DevKitC V4 – vor dem Löten mit dem Aufdruck auf dem eigenen Board vergleichen!
# ---------------------------------------------------------------------------
ESP_LINKS = ["3V3", "EN", "VP", "VN", "34", "35", "32", "33", "25", "26", "27", "14",
             "12", "GND", "13", "D2", "D3", "CMD", "5V"]
ESP_RECHTS = ["GND", "23", "22", "TX", "RX", "21", "GND", "19", "18", "5", "17", "16",
              "4", "0", "2", "15", "D1", "D0", "CLK"]
ESP_SPALTE_L, ESP_SPALTE_R, ESP_ZEILE0 = "B", "M", 11

# RS485-Module (SparkFun RS485 Breakout), stehend eingelötet, Stiftleiste senkrecht
MODUL_PINS = ["GND", "RTS", "TX-O", "RX-I", "3-5V"]
MODULE = [
    ("U2", "RS485 Netzzähler", "T", 4),     # Spalte, erste Zeile
    ("U3", "RS485 Fronius", "T", 16),
]

# Schraubklemmen (Raster 2,54 mm) am rechten Rand
KLEMMEN = [
    ("J2", "Zähler", "X", 4, ["A", "B", "GND"]),
    ("J3", "Fronius", "X", 17, ["A", "B", "GND"]),
    ("J1", "5 V ein", "X", 27, ["+5V", "GND"]),
]

# Bauteile mit zwei Beinen: (Name, Wert, Loch 1, Loch 2, Art)
BAUTEILE = [
    ("R3", "10 kΩ", "O5", "S5", "widerstand"),
    ("R4", "10 kΩ", "O17", "S17", "widerstand"),
    ("D1", "SS14 / 1N5817", "T27", "P27", "diode"),       # Anode, Kathode
    ("C1", "100 µF 10 V", "P28", "P29", "elko"),          # Plus, Minus
]

# Lötbrücken auf der Unterseite (benachbarte Löcher mit Zinn verbinden)
BRUECKEN = [
    ("T5", "S5"), ("T17", "S17"),
    ("T27", "U27"), ("U27", "V27"), ("V27", "W27"), ("W27", "X27"),
    ("P27", "P28"),
]

# Drahtverbindungen: (von, nach, Farbe, Bedeutung)
DRAEHTE = [
    # Versorgung
    ("P27", "B29", "#d62728", "5 V hinter der Diode → ESP32 5V"),
    ("B11", "T8", "#ff7f0e", "3,3 V → U2 3-5V"),
    ("T8", "T20", "#ff7f0e", "3,3 V → U3 3-5V"),
    # Masse
    ("X28", "P29", "#333333", "GND Netzteil → C1 −"),
    ("P29", "B24", "#333333", "GND → ESP32 GND (links)"),
    ("B24", "M11", "#333333", "GND → ESP32 GND (rechts)"),
    ("M11", "T4", "#333333", "GND → U2 GND"),
    ("T4", "O5", "#333333", "GND → R3"),
    ("T4", "X6", "#333333", "GND → J2 GND"),
    ("M11", "T16", "#333333", "GND → U3 GND"),
    ("T16", "O17", "#333333", "GND → R4"),
    ("T16", "X19", "#333333", "GND → J3 GND"),
    # Zähler (UART1)
    ("B19", "T6", "#1f77b4", "IO25 (RX1) ← U2 TX-O"),
    ("B20", "T7", "#17becf", "IO26 (TX1) → U2 RX-I"),
    ("B21", "T5", "#9467bd", "IO27 → U2 RTS"),
    # Fronius (UART2)
    ("M22", "T18", "#2ca02c", "IO16 (RX2) ← U3 TX-O"),
    ("M21", "T19", "#8fbc3f", "IO17 (TX2) → U3 RX-I"),
    ("M23", "T17", "#e377c2", "IO4 → U3 RTS"),
]

# Fliegende Drähte vom oberen Rand der stehenden Module zu den Klemmen
BUSDRAEHTE = [
    ("U2", "A", "X4"), ("U2", "B", "X5"),
    ("U3", "A", "X17"), ("U3", "B", "X18"),
]


# ---------------------------------------------------------------------------
def loch(name):
    spalte = ord(name[0]) - ord("A")
    zeile = int(name[1:]) - 1
    return RAND_L + spalte * RASTER, RAND_O + zeile * RASTER


def svg():
    breite = RAND_L + SPALTEN * RASTER + 360
    hoehe = RAND_O + ZEILEN * RASTER + 80
    t = []
    t.append(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {breite} {hoehe}" '
             f'width="{breite}" height="{hoehe}" font-family="Segoe UI, Arial, sans-serif">')
    t.append(f'<rect width="{breite}" height="{hoehe}" fill="#ffffff"/>')
    t.append(f'<text x="{RAND_L}" y="28" font-size="17" font-weight="bold">Lochrasterplatine SDM630 → Fronius '
             f'– Bauteilseite (von oben)</text>')
    t.append(f'<text x="{RAND_L}" y="50" font-size="12" fill="#555">70 × 90 mm, 24 × 30 Löcher, Raster 2,54 mm. '
             f'Drähte auf der Unterseite verlegen.</text>')

    # Platine und Löcher
    x0, y0 = loch("A1")
    t.append(f'<rect x="{x0 - RASTER * 0.7}" y="{y0 - RASTER * 0.7}" width="{(SPALTEN - 1 + 1.4) * RASTER}" '
             f'height="{(ZEILEN - 1 + 1.4) * RASTER}" rx="6" fill="#f3e9c6" stroke="#b59b52"/>')
    for s in range(SPALTEN):
        buchstabe = chr(ord("A") + s)
        x, _ = loch(f"{buchstabe}1")
        t.append(f'<text x="{x}" y="{y0 - RASTER * 0.95}" font-size="10" text-anchor="middle" fill="#777">{buchstabe}</text>')
        for z in range(1, ZEILEN + 1):
            xx, yy = loch(f"{buchstabe}{z}")
            t.append(f'<circle cx="{xx}" cy="{yy}" r="3" fill="#fff" stroke="#c9a94e"/>')
    for z in range(1, ZEILEN + 1):
        _, y = loch(f"A{z}")
        t.append(f'<text x="{x0 - RASTER * 0.95}" y="{y + 3}" font-size="10" text-anchor="end" fill="#777">{z}</text>')

    # ESP32
    xl, yo = loch(f"{ESP_SPALTE_L}{ESP_ZEILE0}")
    xr, yu = loch(f"{ESP_SPALTE_R}{ESP_ZEILE0 + 18}")
    t.append(f'<rect x="{xl - 1.3 * RASTER}" y="{yo - 2.2 * RASTER}" width="{xr - xl + 2.6 * RASTER}" '
             f'height="{yu - yo + 4.0 * RASTER}" rx="5" fill="#2b4a7a" fill-opacity="0.12" stroke="#2b4a7a" stroke-dasharray="5 3"/>')
    t.append(f'<rect x="{xl + 1.5 * RASTER}" y="{yo - 2.1 * RASTER}" width="{xr - xl - 3 * RASTER}" height="{1.4 * RASTER}" '
             f'fill="none" stroke="#2b4a7a" stroke-dasharray="2 2"/>')
    t.append(f'<text x="{(xl + xr) / 2}" y="{yo - 1.2 * RASTER}" font-size="10" text-anchor="middle" fill="#2b4a7a">Antenne – nichts darüber/darunter</text>')
    t.append(f'<text x="{(xl + xr) / 2}" y="{(yo + yu) / 2 - 8}" font-size="15" font-weight="bold" text-anchor="middle" fill="#2b4a7a">U1 ESP32-DevKitC</text>')
    t.append(f'<text x="{(xl + xr) / 2}" y="{(yo + yu) / 2 + 10}" font-size="11" text-anchor="middle" fill="#2b4a7a">auf 2 × 19-pol. Buchsenleiste</text>')
    t.append(f'<rect x="{(xl + xr) / 2 - 18}" y="{yu + 0.9 * RASTER}" width="36" height="{1.3 * RASTER}" fill="#888" stroke="#555"/>')
    t.append(f'<text x="{(xl + xr) / 2}" y="{yu + 2.9 * RASTER}" font-size="10" text-anchor="middle" fill="#555">USB (zum Flashen)</text>')
    for i, name in enumerate(ESP_LINKS):
        x, y = loch(f"{ESP_SPALTE_L}{ESP_ZEILE0 + i}")
        t.append(f'<rect x="{x - 4}" y="{y - 4}" width="8" height="8" fill="#2b4a7a"/>')
        t.append(f'<text x="{x + 8}" y="{y + 3.5}" font-size="9.5" fill="#2b4a7a">{name}</text>')
    for i, name in enumerate(ESP_RECHTS):
        x, y = loch(f"{ESP_SPALTE_R}{ESP_ZEILE0 + i}")
        t.append(f'<rect x="{x - 4}" y="{y - 4}" width="8" height="8" fill="#2b4a7a"/>')
        t.append(f'<text x="{x - 8}" y="{y + 3.5}" font-size="9.5" text-anchor="end" fill="#2b4a7a">{name}</text>')

    # Lötbrücken
    for a, b in BRUECKEN:
        (xa, ya), (xb, yb) = loch(a), loch(b)
        t.append(f'<line x1="{xa}" y1="{ya}" x2="{xb}" y2="{yb}" stroke="#9a9a9a" stroke-width="6" stroke-linecap="round" opacity="0.8"/>')

    # Drähte (L-förmig: erst waagerecht, dann senkrecht)
    # Gezeichnet zwischen den Lochreihen, damit kein Draht über fremde Löcher läuft
    for i, (von, nach, farbe, _) in enumerate(DRAEHTE):
        (xa, ya), (xb, yb) = loch(von), loch(nach)
        oy = RASTER / 2 + (i % 3 - 1) * 2.5
        ox = -RASTER / 2 + (i % 3 - 1) * 2.5
        t.append(f'<path d="M{xa} {ya} V{ya + oy} H{xb + ox} V{yb} H{xb}" fill="none" stroke="{farbe}" '
                 f'stroke-width="2.2" opacity="0.85"/>')

    # Module (stehend): als Balken über die Stiftleiste
    modul_lage = {}
    for name, text, spalte, zeile in MODULE:
        x, y1 = loch(f"{spalte}{zeile}")
        _, y2 = loch(f"{spalte}{zeile + 4}")
        modul_lage[name] = (x, y1, y2)
        t.append(f'<rect x="{x - 6}" y="{y1 - 2.8 * RASTER}" width="12" height="{y2 - y1 + 5.6 * RASTER}" rx="3" fill="#d42a2a" opacity="0.85"/>')
        for i, pin in enumerate(MODUL_PINS):
            xx, yy = loch(f"{spalte}{zeile + i}")
            t.append(f'<circle cx="{xx}" cy="{yy}" r="4" fill="#fff" stroke="#d42a2a" stroke-width="2"/>')
            t.append(f'<text x="{xx - 9}" y="{yy + 3.5}" font-size="9.5" text-anchor="end" fill="#a01818">{pin}</text>')
        t.append(f'<text x="{x}" y="{y1 - 3.1 * RASTER}" font-size="11" font-weight="bold" text-anchor="middle" fill="#a01818">{name} {text}</text>')

    # Busdrähte (fliegend, gestrichelt)
    for modul, ader, klemme in BUSDRAEHTE:
        x, y1, _ = modul_lage[modul]
        xb, yb = loch(klemme)
        ya = y1 - 2.4 * RASTER + (0 if ader == "A" else 8)
        farbe = "#c47a00" if ader == "A" else "#6b4f2a"
        t.append(f'<path d="M{x + 6} {ya} C{x + 40} {ya}, {xb - 30} {yb}, {xb} {yb}" fill="none" stroke="{farbe}" stroke-width="2" stroke-dasharray="6 3"/>')

    # Klemmen
    for name, text, spalte, zeile, pins in KLEMMEN:
        x, y1 = loch(f"{spalte}{zeile}")
        _, y2 = loch(f"{spalte}{zeile + len(pins) - 1}")
        t.append(f'<rect x="{x - 2.4 * RASTER}" y="{y1 - 0.5 * RASTER}" width="{2.9 * RASTER}" height="{y2 - y1 + RASTER}" '
                 f'rx="3" fill="#3a8f4a" fill-opacity="0.25" stroke="#2f7a3e"/>')
        for i, pin in enumerate(pins):
            xx, yy = loch(f"{spalte}{zeile + i}")
            t.append(f'<circle cx="{xx}" cy="{yy}" r="4" fill="#2f7a3e"/>')
            t.append(f'<text x="{xx + 12}" y="{yy + 4}" font-size="10.5" fill="#1f5a2c">{pin}</text>')
        t.append(f'<text x="{x + 12}" y="{y1 - 0.8 * RASTER}" font-size="11" font-weight="bold" fill="#1f5a2c">{name} {text}</text>')

    # Zweibeinige Bauteile
    for name, wert, a, b, art in BAUTEILE:
        (xa, ya), (xb, yb) = loch(a), loch(b)
        xm, ym = (xa + xb) / 2, (ya + yb) / 2
        t.append(f'<line x1="{xa}" y1="{ya}" x2="{xb}" y2="{yb}" stroke="#555" stroke-width="2"/>')
        if art == "widerstand":
            t.append(f'<rect x="{xm - 22}" y="{ym - 6}" width="44" height="12" rx="5" fill="#e8c68a" stroke="#8a6a2a"/>')
        elif art == "diode":
            t.append(f'<rect x="{xm - 20}" y="{ym - 6}" width="40" height="12" rx="3" fill="#222"/>')
            seite = 1 if xb > xa else -1
            t.append(f'<rect x="{xm + seite * 12 - 2}" y="{ym - 6}" width="4" height="12" fill="#ddd"/>')
        elif art == "elko":
            t.append(f'<circle cx="{xa + 9}" cy="{ym}" r="13" fill="#1f3c88" fill-opacity="0.8" stroke="#13265a"/>')
            t.append(f'<text x="{xa - 12}" y="{ya + 4}" font-size="11" fill="#13265a">+</text>')
        for xx, yy in ((xa, ya), (xb, yb)):
            t.append(f'<circle cx="{xx}" cy="{yy}" r="3.5" fill="#555"/>')
        dy = -12 if art != "elko" else 0
        dx = 0 if art != "elko" else 34
        t.append(f'<text x="{xm + dx}" y="{ym + dy}" font-size="10" text-anchor="middle" fill="#333">{name} {wert}</text>')

    # Legende rechts
    lx = RAND_L + SPALTEN * RASTER + 40
    ly = RAND_O
    t.append(f'<text x="{lx}" y="{ly}" font-size="13" font-weight="bold">Drähte (Unterseite)</text>')
    for i, (von, nach, farbe, text) in enumerate(DRAEHTE):
        y = ly + 20 + i * 17
        t.append(f'<line x1="{lx}" y1="{y - 4}" x2="{lx + 22}" y2="{y - 4}" stroke="{farbe}" stroke-width="3"/>')
        t.append(f'<text x="{lx + 30}" y="{y}" font-size="10.5">{von} → {nach}: {text}</text>')
    y = ly + 30 + len(DRAEHTE) * 17
    t.append(f'<line x1="{lx}" y1="{y - 4}" x2="{lx + 22}" y2="{y - 4}" stroke="#9a9a9a" stroke-width="6" stroke-linecap="round"/>')
    t.append(f'<text x="{lx + 30}" y="{y}" font-size="10.5">Lötbrücke (Nachbarlöcher)</text>')
    y += 18
    t.append(f'<line x1="{lx}" y1="{y - 4}" x2="{lx + 22}" y2="{y - 4}" stroke="#c47a00" stroke-width="2" stroke-dasharray="6 3"/>')
    t.append(f'<text x="{lx + 30}" y="{y}" font-size="10.5">Litze vom Modul (A/B oben) zur Klemme</text>')
    y += 26
    for zeile in ["RS485-Module stehen senkrecht in Spalte T,",
                  "Bauteilseite der Module nach links (Richtung ESP32).",
                  "",
                  "120 Ω Abschluss: bei Bedarf direkt mit in",
                  "die Klemmen A und B schrauben.",
                  "",
                  "ESP32-Pinbelegung vor dem Löten mit dem",
                  "Aufdruck auf dem eigenen Board vergleichen!"]:
        t.append(f'<text x="{lx}" y="{y}" font-size="10.5" fill="#444">{zeile}</text>')
        y += 15
    t.append("</svg>")
    return "\n".join(t)


if __name__ == "__main__":
    ziel = Path(__file__).resolve().parent.parent / "docs" / "Lochraster_Bestueckung.svg"
    ziel.parent.mkdir(exist_ok=True)
    ziel.write_text(svg(), encoding="utf-8")
    print("geschrieben:", ziel)
