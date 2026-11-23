# Lochrasterplatine bauen

Die Platine ist dieselbe für SDM72 und SDM630: Der Netzzähler hängt an Bus A, der Fronius an Bus B.
Die Anker Solarbank wird über WLAN gelesen und braucht keine Bauteile.

Bild des Bestückungsplans: [Lochraster_Bestueckung.svg](Lochraster_Bestueckung.svg)
(erzeugt mit `py tools/lochraster_plan.py`, Änderungen nur in den Listen dort).

Ansicht von **oben (Bauteilseite)**. Spalten A–X (24), Zeilen 1–30, Raster 2,54 mm,
Platine 70 × 90 mm mit Lötpunkten. Ein Loch heißt z. B. **B11** = Spalte B, Zeile 11.
Drähte laufen auf der **Unterseite**.

## Stückliste

| Nr. | Teil | Anzahl | Bemerkung |
|---|---|---|---|
| U1 | ESP32-DevKitC, 38 Pins | 1 | Reihenabstand 11 Löcher (27,9 mm) |
| – | Buchsenleiste 1 × 19, 2,54 mm | 2 | ESP32 steckbar, zum Flashen abnehmbar |
| U2, U3 | SparkFun RS485 Breakout (SP3485) | 2 | U2 = Netzzähler SDM630, U3 = Fronius |
| – | Stiftleiste 1 × 5, gerade | 2 | Module stehend einlöten |
| – | Stiftleiste 1 × 2 oder Litze | 2 | an A/B oben am Modul |
| J1 | Schraubklemme 2-pol., 2,54 mm | 1 | 5 V vom Netzteil |
| J2, J3 | Schraubklemme 3-pol., 2,54 mm | 2 | Bus Zähler / Bus Fronius |
| D1 | Schottky-Diode SS14 oder 1N5817 | 1 | schützt das Netzteil, wenn der ESP32 am USB hängt |
| C1 | Elko 100 µF, 10 V oder mehr | 1 | Puffer für 5 V |
| R3, R4 | Widerstand 10 kΩ | 2 | hält RTS beim Start auf „Empfangen“ |
| – | 120 Ω | 2 | nur bei Bedarf, siehe unten |
| – | Kupferlackdraht oder dünne Litze | | Verdrahtung Unterseite |
| – | Netzteil 5 V Hutschiene, z. B. Mean Well HDR-15-5 (5 V, 2,4 A) | 1 | ESP32 braucht kurzzeitig bis 0,5 A |

## Platzierung

| Teil | Löcher | Hinweis |
|---|---|---|
| U1 ESP32 | Buchsenleisten in **B11–B29** und **M11–M29** | Antenne oben (Zeile 9/10), USB unten über den Rand |
| U2 RS485 Netzzähler (SDM630) | Stiftleiste **T4–T8** (T4 GND, T5 RTS, T6 TX-O, T7 RX-I, T8 3-5V) | stehend, Bauteilseite zum ESP32 |
| U3 RS485 Fronius | Stiftleiste **T16–T20** (T16 GND, T17 RTS, T18 TX-O, T19 RX-I, T20 3-5V) | stehend, Bauteilseite zum ESP32 |
| J2 Zähler | **X4** A, **X5** B, **X6** GND | Kabeleinführung nach rechts |
| J3 Fronius | **X17** A, **X18** B, **X19** GND | Kabeleinführung nach rechts |
| J1 5 V | **X27** +5V, **X28** GND | |
| R3 10 kΩ | **O5–S5** | |
| R4 10 kΩ | **O17–S17** | |
| D1 | Anode **T27**, Kathode (Ring) **P27** | Ring zeigt zum ESP32 |
| C1 | Plus **P28**, Minus **P29** | Minus ist am Elko markiert |

Unter und über der Antenne des ESP32 (oberes Ende, etwa Zeile 8–10, Spalten B–M) keine
Drähte und kein Metall.

## Lötbrücken (Unterseite, Nachbarlöcher)

| Von | Nach | Zweck |
|---|---|---|
| T5 | S5 | RTS Zähler → R3 |
| T17 | S17 | RTS Fronius → R4 |
| T27 – U27 – V27 – W27 – X27 | | +5V Klemme → Anode D1 |
| P27 | P28 | Kathode D1 → C1 Plus |

## Drähte (Unterseite)

**Versorgung**

| Von | Nach | Bedeutung |
|---|---|---|
| P27 | B29 | 5 V hinter der Diode → ESP32 **5V** |
| B11 | T8 | ESP32 **3V3** → U2 3-5V |
| T8 | T20 | 3,3 V → U3 3-5V |

**Masse (GND)**

| Von | Nach |
|---|---|
| X28 | P29 (C1 Minus) |
| P29 | B24 (ESP32 GND links) |
| B24 | M11 (ESP32 GND rechts) |
| M11 | T4 (U2 GND) |
| T4 | O5 (R3) |
| T4 | X6 (J2 GND) |
| M11 | T16 (U3 GND) |
| T16 | O17 (R4) |
| T16 | X19 (J3 GND) |

**Netzzähler SDM630 (UART1)**

| ESP32 | Modul U2 | Bedeutung |
|---|---|---|
| B19 = IO25 | T6 = TX-O | Daten vom Bus zum ESP32 (RX) |
| B20 = IO26 | T7 = RX-I | Daten vom ESP32 zum Bus (TX) |
| B21 = IO27 | T5 = RTS | Senden/Empfangen umschalten |

**Fronius (UART2)**

| ESP32 | Modul U3 | Bedeutung |
|---|---|---|
| M22 = IO16 | T18 = TX-O | Daten vom Bus zum ESP32 (RX) |
| M21 = IO17 | T19 = RX-I | Daten vom ESP32 zum Bus (TX) |
| M23 = IO4 | T17 = RTS | Senden/Empfangen umschalten |

**Bus (Litze vom oberen Rand der Module)**

| Modul | Nach |
|---|---|
| U2 A | X4 (J2 A) |
| U2 B | X5 (J2 B) |
| U3 A | X17 (J3 A) |
| U3 B | X18 (J3 B) |

Der Pin „G“ oben am Modul bleibt frei, GND kommt über die Stiftleiste.

## Bevor du lötest: drei Dinge prüfen

1. **ESP32-Belegung.** Der Plan folgt der Belegung des Espressif DevKitC V4
   (links von oben: 3V3, EN, VP, VN, 34, 35, 32, 33, **25, 26, 27**, 14, 12, **GND**, 13, D2, D3, CMD, **5V**;
   rechts von oben: **GND**, 23, 22, TX, RX, 21, GND, 19, 18, 5, **17, 16, 4**, 0, 2, 15, D1, D0, CLK).
   Nachbauten weichen manchmal ab. Mit dem Aufdruck auf deinem Board vergleichen, Antenne oben.
2. **RS485-Modul.** Mit dem Durchgangsprüfer am Chip SP3485 nachmessen (Pin 1 ist am Punkt):
   - TX-O ↔ Chip-Pin 1 (RO, Empfänger-Ausgang)
   - RX-I ↔ Chip-Pin 4 (DI, Sender-Eingang)
   - RTS ↔ Chip-Pin 2 und 3 (/RE und DE)

   Passt das, stimmt die Verdrahtung oben. Ist TX-O mit Pin 4 verbunden, sind die Namen
   andersherum gemeint. Dann an beiden Modulen TX-O und RX-I tauschen.
3. **Lage auf deiner Platine.** ESP32 und Module einmal lose aufstecken. Die Module stehen
   senkrecht und sind etwa 28 mm breit. Sie dürfen den ESP32 und die Klemmen nicht berühren.

## Reihenfolge beim Bauen

1. Flache Teile zuerst: R3, R4, D1 (Ring Richtung ESP32), Lötbrücken.
2. Buchsenleisten für den ESP32 (ESP32 dabei eingesteckt lassen, dann sitzen sie gerade).
3. Klemmen J1, J2, J3, Elko C1 (Minus beachten).
4. Stiftleisten in die Module, Module stehend einlöten, mit einem Tropfen Heißkleber stützen.
5. Drähte auf der Unterseite: erst GND, dann 5 V/3,3 V, dann die Signale.
6. Litzen von A/B oben an den Modulen zu den Klemmen.

## Prüfen vor dem ersten Einschalten

**Ohne ESP32, ohne Netzteil, Messgerät auf Durchgang/Widerstand:**

| Messen zwischen | Erwartet |
|---|---|
| X27 (+5V) und X28 (GND) | kein Durchgang (kein Kurzschluss) |
| B11 (3V3) und B24 (GND) | kein Durchgang |
| B29 (5V) und B24 (GND) | kein Durchgang (kurzes Piepen durch C1 ist normal) |
| X28 und T4, T16, X6, X19, M11 | Durchgang (GND überall) |
| T5 und O5 bzw. GND | etwa 10 kΩ |

**Mit Netzteil, noch ohne ESP32:**
- Zwischen B29 und B24 etwa **4,7 V** (5 V minus Diode).

**Dann ESP32 einstecken:**
- Webseite muss wie bisher kommen. Am USB-Kabel darf das Netzteil gleichzeitig angeschlossen
  sein, D1 verhindert, dass der USB-Anschluss das Netzteil speist.

## Abschluss und Ruhepegel

- **120 Ω Abschluss:** Je Bus hängen nur zwei Geräte, die Leitungen sind kurz, bei 9600 Baud
  geht es meist ohne. Gibt es Übertragungsfehler (Diagnose: Zähler „Timeout“ oder Fronius
  „Bytes empfangen, aber keine gültige Anfrage“), je einen 120-Ω-Widerstand mit in die Klemmen
  A und B schrauben, an beiden Enden des Busses (am Zähler bzw. Datamanager auch).
  Der Datamanager hat dafür eventuell einen eigenen Schalter.
- **Ruhepegel:** Laut Fronius-Anleitung hat der Datamanager keine eigenen Bias-Widerstände.
  Hilft der Abschluss nicht, auf der Platine je Bus 680 Ω von A nach 3,3 V und von B nach GND
  nachrüsten. Das ist im Plan bewusst nicht drin.

## Gehäuse und Einbau

- **Größe:** Platine 70 × 90 mm. Höhe innen mindestens etwa **35 mm** (Module stehen,
  ESP32 auf Buchsenleisten).
- **Hutschiene:** ein Hutschienengehäuse mit so viel Innenraum, oder die Platine auf eine
  Hutschienen-Halterung für Platinen schrauben.
- **USB:** Der USB-Anschluss des ESP32 zeigt zum unteren Rand. Beim Gehäuse eine Öffnung
  vorsehen oder zum Flashen den ESP32 abziehen.
- **WLAN:** Ein Stahlschrank schirmt stark ab. Vor dem festen Einbau bei geschlossener Tür auf
  der Seite „Diagnose“ den Empfang ansehen (besser als −75 dBm). Reicht er nicht, hilft ein
  ESP32 mit Antennenbuchse (ESP32-WROOM-32**U**) und Außenantenne.
- **Leitungen:** verdrillte Zweidrahtleitung für A/B, z. B. ein Aderpaar aus einem
  Netzwerkkabel oder J-Y(St)Y 2 × 2 × 0,8, dazu eine Ader für GND.
- **Sicherheit:** Platine und Kleinspannungsleitungen getrennt von 230-V-Leitungen führen.
  Arbeiten an der 230/400-V-Seite nur durch eine Elektrofachkraft.

## Was nicht geprüft ist

- Der Plan ist nur gezeichnet, nicht nachgebaut. Maße von Modul und Gehäuse sind geschätzt.
- Die Bezeichnung TX-O/RX-I am Modul ist nicht nachgemessen (siehe „Bevor du lötest“, Punkt 2).
- Die Belegung des ESP32 folgt dem Espressif-Original, nicht deinem Board.
