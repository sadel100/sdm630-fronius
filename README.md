# SDM630 → Fronius Symo und openWB (mit Anker Solarbank)

Ein ESP32-WROOM-32 liest

- den Netzzähler **Eastron SDM630-Modbus V2** am Netzanschluss (RS485) und
- die **Anker SOLIX Solarbank Max AC** per Modbus TCP im WLAN (nur lesen)

und gibt die Werte weiter:

- an den **Fronius Datamanager** (Symo) als **zwei Zähler** über RS485: Netzzähler (Primärzähler)
  und Solarbank (Sekundärzähler),
- an die **openWB 1.9** per MQTT: Netzanschluss als EVU, Solarbank als Speicher,
- auf einer **Webseite** (Handy oder PC) mit Energiefluss, Tagesverlauf, Tages- und Monatswerten,
  Ereignisprotokoll und Firmware-Update.

Zusätzlich liest er (nur lesen) die PV-Leistung des Symo über die **Fronius Solar API** im Heimnetz,
damit er den Hausverbrauch ausrechnen kann.

Der bisherige SDM72DR am S0-Eingang des Datamanagers entfällt. Der Anker Smart Meter der
Solarbank bleibt, die Solarbank regelt weiter selbst.

## Aufbau

| Bus | UART | ESP32-Rolle | Gegenstelle | Pins RX / TX / DE |
|---|---|---|---|---|
| A | UART1 | Master | SDM630 | 25 / 26 / 27 |
| B | UART2 | Slave (zwei Adressen) | Fronius Datamanager | 16 / 17 / 4 |
| WLAN | – | Modbus-TCP-Client | Solarbank Max AC, Port 502 | – |
| WLAN | – | MQTT-Client | openWB, Port 1883 | – |
| WLAN | – | HTTP-Client | Fronius Solar API, Port 80 | – |
| WLAN | – | HTTPS-Client | GitHub (nur Update-Suche) | – |
| USB | UART0 | – | PC, 115200 Baud (nur Ausgabe) | – |

Feste Werte (Pins, Zeiten, Vorgaben) stehen in `include/konfiguration.h`. Alles, was sich vor Ort
ändern muss, wird auf der Webseite eingestellt und im Flash des ESP32 gespeichert.
Bau der Platine: [docs/Lochraster_Bauanleitung.md](docs/Lochraster_Bauanleitung.md).

## Funktionsweise

**Netzzähler SDM630 (Bus A):** jede Sekunde, Funktionscode 04, je Anfrage höchstens 40 Register:
- Pflicht 0x0000–0x004F: Spannung, Strom, Leistung und Leistungsfaktor je Phase,
  Gesamtleistung 0x0034 (**+ Bezug, − Einspeisung**), Frequenz 0x0046, Bezug 0x0048, Einspeisung 0x004A.
- Zusatz 0x00C8–0x00EF und 0x0156–0x0187 (Außenleiterspannungen, N-Strom, Zählerstände je Phase).
- Baudrate, Parität und Adresse sind auf der Webseite einstellbar (Vorgabe 9600 8N1, Adresse 1;
  die Parität am Zähler nachsehen, die Quellen widersprechen sich).

**Anker Solarbank (Modbus TCP):** alle 5 s, nur Funktionscode 04. Register aus der offiziellen
Anker-Integration für Home Assistant (`anker-charging/ha-anker-solix-official`, Gerätedatei der
Solarbank Max AC): 10001 Status, 10002/10004 PV, **10008 Batterieleistung (+ Entladen, − Laden)**,
10010 Last, 10012 Netz, **10014 Ladestand**, 10208 AC-Abgabe, 10250 Kapazität, **10262/10264 geladen/
entladen** (0,1 kWh), 32768 Kennung. In der Anker-App muss „Modbus TCP“ eingeschaltet sein
(Gerät → Zahnrad → Three-Party Control Settings). Der ESP32 schreibt nie in die Solarbank.

**Fronius (Bus B):**
1. **Erkennung:** Nach dem Start hört der ESP32 nur zu und probiert Baudraten und Formate. Sobald
   gültige Leseanfragen ankommen, sind Baudrate und Format bekannt. Die kleinste gefragte Adresse
   gilt als Netzzähler (Fronius: Primärzähler 1, Sekundärzähler 2–14).
2. **Zwei Zähler:** Netzzähler an der erkannten Adresse, Solarbank an der eingestellten Adresse
   (Vorgabe 2). Im Datamanager wird die Solarbank als **Sekundärzähler „Fronius Smart Meter“** angelegt.
   Laut Fronius-Anleitung erscheint ein Sekundärzähler beim Datamanager 2.0 als Lastkurve im
   „Energy Profiling“ von Solar.web. Eine Kategorie „Batterie“ und einen Ladestand gibt es dort nicht.
3. **Zwei Registerbelegungen je Zähler gleichzeitig:**
   - **SunSpec** (Funktionscode 03, ab 40000): `SunS`, Modell 1, Modell 213 oder 203, Endkennung.
   - **Eastron** (Funktionscode 04, ab 0): beim Netzzähler die **Rohregister des echten SDM630**,
     bei der Solarbank gerechnete Werte (Leistung auf L1, Spannung und Frequenz vom Netzzähler).
4. **Vorzeichen Solarbank:** Laden positiv (wie ein Verbraucher), Entladen negativ.
   Zählerstände: Bezug = geladen, Einspeisung = entladen.
5. **Veraltete Daten:** Ist die letzte Zählerrunde älter als 5 s bzw. die Solarbank älter als 20 s,
   antwortet der ESP32 für diesen Zähler nicht. Der Datamanager soll einen Ausfall erkennen.
6. **Neue Erkennung:** Kommt im Automatik-Betrieb 5 min lang keine Anfrage, startet der ESP32 neu.

**openWB 1.9 (MQTT):** jede Sekunde, nicht „retained“, an den Broker der openWB (Port 1883).
Themen laut Hilfetext der openWB 1.9:
- EVU: `openWB/set/evu/W` (+ Bezug), `WhImported`, `WhExported`, `APhase1–3`, `VPhase1–3`,
  `WPhase1–3`, `PfPhase1–3`, `HzFrequenz`. Bei Zählerausfall ein Ersatzwert (Standard 10 000 W
  Bezug), weil openWB 1.9 veraltete Werte nicht erkennt.
- Speicher: `openWB/set/houseBattery/W` (+ Laden, − Entladen), `%Soc`, `WhImported`, `WhExported`.
  Antwortet die Solarbank nicht, geht W = 0 hinaus.

**Fronius Solar API (WLAN, nur lesen):** alle 5 s `GET /solar_api/v1/GetPowerFlowRealtimeData.fcgi`,
verwendet werden `P_PV` (PV-Leistung, nachts null = 0 W), `E_Total` (Gesamtzähler) und zur Anzeige `E_Day`
und `P_Grid`. Muss im Datamanager eingeschaltet sein.

**Energiefluss:** Netz (SDM630) + PV Symo (Solar API) + Solarbank = Hausverbrauch. Die Solarbank zählt
dabei mit dem, was sie ins Haus abgibt (+) bzw. zum Laden daraus nimmt (−), gerechnet als
PV der Solarbank + Batterieleistung (Register 10002/10004 + 10008). Verluste der Solarbank fehlen.
Der Hausverbrauch erscheint nur, wenn alle eingerichteten Quellen aktuelle Werte haben.

**Tagesverlauf:** alle 10 s gemessen, je 5 Minuten gemittelt (an der Uhr ausgerichtet), 288 Punkte =
24 h, nur im Arbeitsspeicher. Kurven: Netz, PV Symo, Haus, Solarbank (+ lädt), Ladestand.

**Tages- und Monatswerte:** aus den Zählerständen zu Tages- bzw. Monatsbeginn (SDM630 Bezug/Einspeisung,
Solar API `E_Total`, Solarbank 10262/10264). Der Hausverbrauch hat keinen Zähler und wird aus der
Leistung aufsummiert (nur solange alle Werte da sind). Gespeichert im Flash (Bereich `energie`):
heute, gestern, laufender Monat, die letzten 12 Monate. Der Hausverbrauch wird alle 15 min
gesichert, bei einem Stromausfall fehlen also höchstens 15 min. Braucht die Uhrzeit (Zeitserver).
War die Box über Mitternacht aus, enthält „gestern“ auch den Teil des neuen Tages bis zum Start.

**Ereignisprotokoll:** die letzten 60 Ereignisse im Arbeitsspeicher, z. B. Ausfall und Rückkehr von
SDM630, Solarbank, Solar API, Heimnetz, openWB und Fronius-Anfragen (erst nach 10 s Dauer),
Startgrund (Strom weg, Absturz, Wächter, Unterspannung), Uhrzeit gestellt, Firmware-Wechsel.

**Firmware-Update** (Einstellungen → Firmware-Update), installiert nur auf Knopfdruck:
- **GitHub:** Die Box fragt `api.github.com/repos/sadel100/sdm630-fronius/releases/latest`
  (öffentlich, ohne Schlüssel, Zertifikat wird geprüft). Release-Kennzeichen `v<Version>`, Datei
  `sdm630-fronius-<Version>.bin`. Nur eine höhere Versionsnummer wird angeboten.
- **Datei:** `sdm630-fronius-<Version>.bin` auf der Webseite hochladen (geht ohne Internet).
  Der Kopf der Datei wird geprüft (Kennung 0xE9, Chip ESP32), am Ende die Prüfsumme.
- **Probezeit:** Eine neue Version muss 60 s fehlerfrei laufen, sonst startet der Bootloader wieder
  die alte (Rollback des ESP-IDF). Beides steht im Ereignisprotokoll.
- Programmspeicher: Aufteilung `min_spiffs.csv`, zwei Hälften zu je 1,9 MB. Die Einstellungen (NVS)
  bleiben bei Updates erhalten. **Wechsel der Aufteilung nur per USB** (einmalig bei Version 1.3.0).
- Release bauen: `py tools/release_erstellen.py` legt `release/sdm630-fronius-<Version>.bin` an
  (ohne Testschalter). Hochladen auf GitHub macht das Skript nicht.

**Webseite** (fünf Reiter): Übersicht (Energiefluss, Netz, Phasen, Zählerstände, Solarbank, Weitergabe),
Verlauf (24 h, Tages- und Monatswerte), Diagnose (Netzzähler, Fronius je Zähler, Solarbank, Solar API,
Netzwerk, Ereignisse, die letzten 40 Fronius-Anfragen, Knopf „Diagnose-Datei herunterladen“),
Einstellungen (mit Erklärungen und Firmware-Update), So funktioniert's (Aufbau, Vorzeichen,
Energiefluss, Updates, Störungen, Erste Hilfe).
Die Diagnose-Datei (`/api/diagnose`, JSON) enthält Status, Einstellungen ohne Passwörter,
Ereignisse, Energiewerte und Verlauf.
- Eigenes WLAN „SDM630-Fronius“, Passwort zuerst `sdm630fronius`, Seite unter http://192.168.4.1
- Im Heimnetz: http://sdm630-fronius.local oder die IP aus dem Router
- Speichern verlangt Benutzer `admin` und das Passwort des Geräte-WLANs.
- Passwörter werden nie angezeigt. Leeres Passwortfeld = gespeichertes Passwort bleibt.

## Tests am PC

- `py tools/solarbank_simulator.py` spielt eine Solarbank (Modbus TCP, Port 502, nur FC 04,
  Schreibversuche werden abgelehnt und gemeldet).
- `py tools/solarapi_simulator.py [Port]` spielt die Solar API des Datamanagers (Standard Port 80).
- Bauschalter **nur für Tests**, nie für die Anlage (über `PLATFORMIO_BUILD_FLAGS`):
  - `-DTEST_ANKER_IP=\"<IP des PCs>\"`, `-DTEST_FRONIUS_IP=\"<IP des PCs>:<Port>\"`:
    Simulatoren als Vorgabe, solange auf der Webseite nichts gespeichert ist.
  - `-DTEST_UPLOAD_OHNE_ANMELDUNG`: Update (Datei und GitHub-Installation) ohne Passwort.
  - `-DTEST_VERSION=\"1.3.1\"`: andere Versionsnummer (für Update-Tests).
  - `-DTEST_ABSTURZ_NACH_MS=25000`: stürzt absichtlich ab (Test des Rückfalls).
- Ein Werkzeug, das den Fronius Datamanager (RS485) oder den SDM630 spielt, gibt es nicht.

## Offene Punkte (nicht bestätigt)

- Ob der Datamanager die Zähler über RS485 per SunSpec oder mit Eastron-Registern abfragt
  (das Protokoll auf der Webseite zeigt es).
- Ob der Datamanager 2.0 des Freundes Sekundärzähler anbietet (Firmware) und wie Solar.web die
  Solarbank darstellt.
- Vorzeichen der Register 10008 und 10208 der Solarbank (laut Anker-Integration: 10008 positiv =
  Entladen). Am echten Gerät beim Laden und Entladen prüfen.
- Unit-ID der Solarbank (Vorgabe 1).
- Parität des SDM630 (Werkseinstellung laut Quellen 8N1 oder 8E1).
- Ob Datamanager Identifikationstexte prüft, Modell 213/203, Einheit des Leistungsfaktors.
- Solar API am echten Datamanager 2.0 (Menüname zum Einschalten, ob `E_Total` geliefert wird).
- Ob „PV der Solarbank + Batterieleistung“ ungefähr der Abgabe der Solarbank ins Haus entspricht
  (Vergleich mit Register 10208 und der Anker-App).
- Update von GitHub bis zum Ende: getestet sind nur die Suche (verschlüsselte Verbindung, Antwort
  „keine Release“), Update per Datei, Probezeit und Rückfall. Der Download einer echten Release
  ist erst nach der ersten Veröffentlichung prüfbar.

## Dateien

| Datei | Inhalt |
|---|---|
| `include/konfiguration.h` | Pins, Zeiten, Vorgaben |
| `src/einstellungen.*` | Einstellungen im Flash |
| `src/messwerte.*` | Zwischenspeicher des Netzzählers (mit Rohregistern) |
| `src/zaehler.*` | Abfrage des SDM630 |
| `src/anker.*` | Abfrage der Solarbank per Modbus TCP |
| `src/erkennung.*` | Erkennung von Baudrate und Adresse am Fronius-Bus |
| `src/fronius.*` | Zwei Zähler (SunSpec und Eastron) für den Fronius, Protokoll |
| `src/netz.*` | WLAN |
| `src/webseite.*` | Webserver und JSON-Schnittstelle |
| `src/seite.h` | die Webseite selbst |
| `src/mqtt_openwb.*` | MQTT an die openWB 1.9 (EVU und Speicher) |
| `src/solarapi.*` | PV-Leistung des Symo über die Fronius Solar API |
| `src/energie.*` | Energiefluss, Tagesverlauf, Tages- und Monatswerte |
| `src/ereignisse.*` | Ereignisprotokoll |
| `src/uhrzeit.*` | Uhrzeit vom Zeitserver |
| `src/firmware_update.*` | Update per Datei und von GitHub, Probezeit |
| `src/main.cpp` | Start, Überwachung, Ausgabe über USB |
| `tools/solarbank_simulator.py` | Solarbank zum Testen am PC |
| `tools/solarapi_simulator.py` | Fronius Solar API zum Testen am PC |
| `tools/release_erstellen.py` | baut die Datei für eine Release |
| `tools/lochraster_plan.py` | erzeugt den Bestückungsplan |
| `ANLEITUNG_Ersteinrichtung.md` | Anleitung für den Betreiber |
| `docs/Lochraster_Bauanleitung.md` | Platine bauen |
