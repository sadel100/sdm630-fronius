// main.cpp
// ---------------------------------------------------------------------------
// SDM72D-M → Fronius Symo und openWB
//
// Der ESP32 liest einen Eastron SDM72D-M am Netzanschluss (Bus A) und gibt
// die Werte weiter:
//   - an den Fronius Datamanager als Zähler (Bus B, SunSpec und Eastron),
//   - an die openWB 1.9 per MQTT (WLAN im Heimnetz),
//   - auf der eigenen Webseite.
//
// Ablauf:
//   - Task "zaehler" (Kern 1): fragt jede Sekunde den Zähler ab.
//   - Task "fronius" (Kern 0): erkennt Baudrate und Adresse, startet dann
//     den eModbus-Server, der nur aus dem Zwischenspeicher antwortet.
//   - loop(): Webseite, MQTT, Ausgabe über USB (115200 Baud).
// ---------------------------------------------------------------------------
#include <Arduino.h>
#include "konfiguration.h"
#include "einstellungen.h"
#include "messwerte.h"
#include "zaehler.h"
#include "fronius.h"
#include "netz.h"
#include "webseite.h"
#include "mqtt_openwb.h"

static uint32_t letzte_uebersicht_ms = 0;

// Jede Anfrage des Datamanagers über USB zeigen (Inbetriebnahme).
static void anfragen_ausgeben() {
  FroniusAnfrage a;
  while (fronius_naechste_meldung(a)) {
    Serial.printf("[Fronius] FC%02u ab %u, %u Register (%s): %s\n", a.funktion, a.start, a.anzahl,
                  bereich_name(a.bereich), ergebnis_name(a.ergebnis));
  }
}

static void uebersicht_ausgeben() {
  const Messwerte m = messwerte_lesen();
  const ZaehlerStatistik z = zaehler_statistik();
  const FroniusStatistik f = fronius_statistik();
  const MqttStand mq = mqtt_stand();

  Serial.println(F("------------------------------------------------------------"));
  if (m.gueltig) {
    Serial.printf("[SDM72] %.0f W (%s), Bezug %.2f kWh, Einspeisung %.2f kWh%s\n", m.leistung_w,
                  m.leistung_w >= 0 ? "Bezug" : "Einspeisung", m.bezug_kwh, m.einspeisung_kwh,
                  messwerte_aktuell(m, CACHE_MAX_ALTER_MS) ? "" : "  VERALTET");
  } else {
    Serial.println(F("[SDM72] noch keine gueltigen Werte"));
  }
  Serial.printf("[SDM72] Runden ok %lu, fehlerhaft %lu, letzter Fehler: %s\n",
                (unsigned long)z.runden_ok, (unsigned long)z.runden_fehler,
                z.letzter_fehler[0] ? z.letzter_fehler : "-");
  if (f.zustand == FroniusZustand::BEREIT) {
    Serial.printf("[Fronius] %lu Baud %s, Adresse %u: beantwortet %lu, abgelehnt %lu, ohne Antwort %lu (SunSpec %lu, Eastron %lu)\n",
                  (unsigned long)f.baud, format_name(f.format), f.adresse, (unsigned long)f.beantwortet,
                  (unsigned long)f.abgelehnt, (unsigned long)f.ohne_antwort,
                  (unsigned long)f.sunspec_anfragen, (unsigned long)f.eastron_anfragen);
  } else {
    Serial.println(F("[Fronius] Erkennung laeuft"));
  }
  Serial.printf("[WLAN] Heimnetz %s %s, MQTT %s (gesendet %lu, Ersatz %lu)\n",
                netz_heimnetz_verbunden() ? "verbunden" : "getrennt", netz_heimnetz_ip().c_str(),
                !mq.aktiv ? "aus" : (mq.verbunden ? "verbunden" : "getrennt"),
                (unsigned long)mq.gesendet, (unsigned long)mq.ersatz_gesendet);
}

// Automatik: Kommt nach der Erkennung lange gar keine Anfrage mehr, war sie
// vermutlich falsch (oder der Datamanager wurde umgestellt). Dann neu starten,
// damit wieder gesucht wird.
static void erkennung_ueberwachen() {
  const Einstellungen& e = einstellungen();
  if (e.fronius_baud != 0 && e.fronius_adresse != 0) return;  // fest eingestellt
  const FroniusStatistik f = fronius_statistik();
  if (f.zustand != FroniusZustand::BEREIT) return;
  if ((uint32_t)(millis() - f.letzte_anfrage_ms) >= ERKENNUNG_NEU_NACH_MS) {
    Serial.println(F("[Fronius] lange keine Anfrage, starte neu und suche erneut"));
    delay(100);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(DEBUG_BAUD);
  delay(200);  // nur beim Start, damit der Monitor die ersten Zeilen zeigt
  Serial.println();
  Serial.println(F("SDM72D-M -> Fronius / openWB"));

  einstellungen_laden();
  messwerte_start();
  zaehler_start();
  fronius_start();
  netz_start();
  webseite_start();
  mqtt_start();
}

void loop() {
  webseite_schritt();
  mqtt_schritt();
  anfragen_ausgeben();
  erkennung_ueberwachen();
  if ((uint32_t)(millis() - letzte_uebersicht_ms) >= DEBUG_ZYKLUS_MS) {
    letzte_uebersicht_ms = millis();
    uebersicht_ausgeben();
  }
  delay(2);  // loop hat keine zeitkritische Aufgabe; die Busse laufen in eigenen Tasks
}
