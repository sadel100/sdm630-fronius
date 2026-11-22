# SDM72D-M → Fronius Symo und openWB

Ein ESP32-WROOM-32 liest einen Eastron SDM72D-M am Netzanschluss und gibt die Werte weiter:

- an den **Fronius Datamanager** (Symo) als Zähler über RS485,
- an die **openWB 1.9** per MQTT (fürs Überschussladen),
- auf einer **Webseite** (Handy oder PC).

Der bisherige SDM72DR am S0-Eingang des Datamanagers entfällt. Der Anker Smart Meter der
Solarbank bleibt, wie er ist.

## Aufbau

| Bus | UART | ESP32-Rolle | Gegenstelle | Pins RX / TX / DE |
|---|---|---|---|---|
| A | UART1 | Master | SDM72D-M | 25 / 26 / 27 |
| B | UART2 | Slave | Fronius Datamanager | 16 / 17 / 4 |
| USB | UART0 | – | PC, 115200 Baud (nur Ausgabe) | – |

Feste Werte (Pins, Zeiten, Vorgaben) stehen in `include/konfiguration.h`. Alles, was sich vor Ort
ändern muss, wird auf der Webseite eingestellt und im Flash des ESP32 gespeichert.

### Verdrahtung je SP3485-Modul

| Modul | ESP32 / Bus |
|---|---|
| VCC | 3,3 V |
| GND | GND |
| RO | RX-Pin |
| DI | TX-Pin |
| DE und /RE (gebrückt) | DE-Pin |
| A / B | A / B des Busses |

- GND des Busses mitführen, wenn Zähler bzw. Datamanager eine GND-Klemme haben.
- Je Busende höchstens ein 120-Ω-Abschluss. Viele Module haben ihn schon aufgelötet.
- Bei Prüfsummenfehlern oder Fremdzeichen: Ruhepegel mit etwa 680 Ω (A nach 3,3 V, B nach GND)
  an einer Stelle des Busses festlegen.

## Funktionsweise

**Zähler (Bus A):** Jede Sekunde mit Funktionscode 04:
- Pflicht (laut Handbuch SDM72D-M V1.5): Bezug/Einspeisung in kWh (0x0048/0x004A),
  Bezugs-/Einspeiseleistung in W (0x0500/0x0502). Leistung = Bezug − Einspeisung,
  also **+ Bezug, − Einspeisung**.
- Zusatz (nur aus der Bibliothek nmakel/sdm_modbus, nicht im Handbuch): Spannung, Strom,
  Leistung je Phase, Leistungsfaktor, Frequenz. Lehnt der Zähler sie ab, werden sie nicht mehr gefragt.

**Fronius (Bus B):**
1. **Erkennung:** Nach dem Start hört der ESP32 nur zu (sendet nichts) und probiert Baudraten
   und Formate (9600/19200 8N1 zuerst). Sobald mehrere gültige Leseanfragen ankommen, sind
   Baudrate, Format und Busadresse bekannt. Fest einstellen lässt sich beides auf der Webseite.
2. **Antworten:** Der ESP32 beantwortet zwei Registerbelegungen gleichzeitig, weil nicht bekannt
   ist, welche der Datamanager erwartet:
   - **SunSpec** (Funktionscode 03, ab 40000): `SunS`, Modell 1, Modell 213 (Gleitkomma) oder
     203 (Ganzzahl), Endkennung. Bei 213 liegt W bei 40097.
   - **Eastron** (Funktionscode 04, ab 0): Belegung wie SDM72/SDM630.
3. **Veraltete Daten:** Ist die letzte Zählerrunde älter als 5 s, antwortet der ESP32 nicht.
   Der Datamanager soll einen Zählerausfall erkennen.
4. **Neue Erkennung:** Kommt im Automatik-Betrieb 5 min lang gar keine Anfrage, startet der
   ESP32 neu und sucht erneut.

**openWB 1.9 (MQTT):** Jede Sekunde, nicht „retained“, an den Broker der openWB (Port 1883).
Themen laut Hilfetext der openWB 1.9 (EVU-Modul „MQTT“):
`openWB/set/evu/W` (Ganzzahl, + Bezug), `WhImported`, `WhExported`, `APhase1–3`, `VPhase1–3`,
`WPhase1–3`, `PfPhase1–3`, `HzFrequenz`.
**Zählerausfall:** openWB 1.9 merkt nicht, wenn keine Werte mehr kommen, der letzte Wert bliebe
stehen. Deshalb sendet der ESP32 dann einen Ersatzwert als Netzbezug (Standard 10 000 W, auf
der Webseite änderbar oder abschaltbar). Die openWB sieht keinen Überschuss mehr und beendet
das Überschussladen.

**Webseite** (vier Reiter): Übersicht (Leistung mit Balken, Phasen, Zählerstände, Weitergabe),
Diagnose (Zustand beider Busse, die letzten 40 Anfragen des Fronius, Netzwerk, Version),
Einstellungen (mit Erklärungen), So funktioniert's (Bild des Aufbaus, Störungen, Erste Hilfe).
- Eigenes WLAN „SDM72-Fronius“, Passwort zuerst `sdm72fronius`, Seite unter http://192.168.4.1
- Im Heimnetz: http://sdm72-fronius.local oder die IP aus dem Router
- Speichern verlangt Benutzer `admin` und das Passwort des eigenen WLANs.
- Passwörter werden nie angezeigt. Leeres Passwortfeld = gespeichertes Passwort bleibt.

## Inbetriebnahme beim Freund

1. **Zu Hause:** flashen (`pio run -t upload`), mit dem Handy ins WLAN „SDM72-Fronius“,
   http://192.168.4.1 öffnen, **Passwort des ESP32-WLANs ändern**.
2. **Zähler anschließen** (Bus A). Auf der Seite muss „antwortet“ stehen. Leistung und
   Zählerstände mit der Anzeige am Zähler vergleichen. Bei Einspeisung muss die Leistung negativ sein.
   Antwortet er nicht: Baudrate und Adresse im Setup-Menü des Zählers nachsehen und eintragen.
3. **Heimnetz und openWB:** WLAN-Name und Passwort eintragen, Adresse der openWB eintragen,
   „Werte per MQTT senden“ anhaken, speichern. In der openWB als EVU-Modul „MQTT“ wählen.
   Auf der Seite muss bei MQTT „verbunden“ stehen. In der openWB die EVU-Leistung prüfen.
4. **Fronius:** im Datamanager Zählertyp „Fronius Smart Meter“, Einbauort Einspeisepunkt.
   Bus B anschließen. Auf der Seite erst „sucht Baudrate …“, dann „fragt ab“.
   Im Protokoll steht, ob er **SunSpec** oder **Eastron** fragt.
   - Viele Zeilen „abgelehnt“: Der Datamanager fragt Register, die der ESP32 nicht kennt.
     Die Zeilen (Funktionscode, Register, Anzahl) notieren, dann lässt sich das ergänzen.
   - „nichts empfangen“: A/B vertauscht, Leitung, oder der Datamanager steht nicht auf Smart Meter.
5. **Solar.web:** Bei Einspeisung muss Einspeisung angezeigt werden. Sonst „Vorzeichen an Fronius“
   umdrehen. Zeigt der Datamanager nichts, „SunSpec-Modell“ auf 203 versuchen.

## Offene Punkte (nicht bestätigt)

- Ob der Datamanager den Zähler über RS485 per SunSpec oder mit Eastron-Registern abfragt
  (das Protokoll auf der Webseite zeigt es).
- Ob er Identifikationstexte prüft (Hersteller „Fronius“, Gerät „Smart Meter 63A“).
- Vorzeichen und Modell 213/203 für den Fronius, Einheit des Leistungsfaktors.
- Ob der SDM72D-M V1.3 Phasenwerte, Frequenz und Leistungsfaktor liefert.
- Busparameter des Zählers (vorbelegt 9600 8N1, Adresse 1).
- Nicht auf echter Hardware getestet. Die Webseite ist nur am PC mit Testdaten geprüft.

## Dateien

| Datei | Inhalt |
|---|---|
| `include/konfiguration.h` | Pins, Zeiten, Vorgaben |
| `src/einstellungen.*` | Einstellungen im Flash |
| `src/messwerte.*` | Zwischenspeicher mit Sperre |
| `src/zaehler.*` | Abfrage des SDM72D-M |
| `src/erkennung.*` | Erkennung von Baudrate und Adresse am Fronius-Bus |
| `src/fronius.*` | SunSpec- und Eastron-Abbild, Antworten an den Fronius, Protokoll |
| `src/netz.*` | WLAN |
| `src/webseite.*` | Webserver und JSON-Schnittstelle |
| `src/seite.h` | die Webseite selbst (Reiter Übersicht, Diagnose, Einstellungen, So funktioniert's) |
| `ANLEITUNG_Ersteinrichtung.md` | Anleitung für den Betreiber (Einrichtung vor Ort) |
| `src/mqtt_openwb.*` | MQTT an die openWB 1.9 |
| `src/main.cpp` | Start, Überwachung, Ausgabe über USB |
| `referenz/` | alter OLED-Code (nur zum Nachsehen) |
