// main.cpp
// ---------------------------------------------------------------------------
// SDM630 → Fronius Symo und openWB (mit Anker Solarbank)
//
// Der ESP32 liest den Netzzähler Eastron SDM630-Modbus V2 (Bus A) und die
// Anker SOLIX Solarbank Max AC (Modbus TCP im WLAN) und gibt die Werte weiter:
//   - an den Fronius Datamanager als zwei Zähler (Bus B): Netzzähler und
//     Solarbank als Sekundärzähler,
//   - an die openWB 1.9 per MQTT (Netzzähler als EVU, Solarbank als Speicher),
//   - auf der eigenen Webseite.
//
// Ablauf:
//   - Task "zaehler" (Kern 1): fragt jede Sekunde den SDM630 ab.
//   - Task "anker"   (Kern 1): fragt alle 5 s die Solarbank ab (nur lesen).
//   - Task "solarapi" (Kern 1): fragt alle 5 s die PV-Leistung des Symo ab.
//   - Task "fronius" (Kern 0): erkennt Baudrate und Adresse, startet dann
//     den eModbus-Server, der nur aus den Zwischenspeichern antwortet.
//   - loop(): Webseite, MQTT, Ereignisse, Energiewerte, Update, Ausgabe über USB.
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
#include "anker.h"
#include "uhrzeit.h"
#include "ereignisse.h"
#include "solarapi.h"
#include "energie.h"
#include "firmware_update.h"

static uint32_t letzte_uebersicht_ms = 0;

// Jede Anfrage des Datamanagers über USB zeigen (Inbetriebnahme).
static void anfragen_ausgeben() {
  FroniusAnfrage a;
  while (fronius_naechste_meldung(a)) {
    Serial.printf("[Fronius] %s FC%02u ab %u, %u Register (%s): %s\n",
                  a.zaehler == ZAEHLER_HAUPT ? "Netz" : "Solarbank", a.funktion, a.start, a.anzahl,
                  bereich_name(a.bereich), ergebnis_name(a.ergebnis));
  }
}

static void uebersicht_ausgeben() {
  const Messwerte m = messwerte_lesen();
  const ZaehlerStatistik z = zaehler_statistik();
  const FroniusStatistik f = fronius_statistik();
  const MqttStand mq = mqtt_stand();
  const AnkerWerte an = anker_werte();
  const AnkerStatistik as = anker_statistik();

  Serial.println(F("------------------------------------------------------------"));
  if (m.gueltig) {
    Serial.printf("[SDM630] %.0f W (%s), Bezug %.2f kWh, Einspeisung %.2f kWh%s\n", m.leistung_w,
                  m.leistung_w >= 0 ? "Bezug" : "Einspeisung", m.bezug_kwh, m.einspeisung_kwh,
                  messwerte_aktuell(m, CACHE_MAX_ALTER_MS) ? "" : "  VERALTET");
  } else {
    Serial.println(F("[SDM630] noch keine gueltigen Werte"));
  }
  Serial.printf("[SDM630] Runden ok %lu, fehlerhaft %lu, letzter Fehler: %s\n",
                (unsigned long)z.runden_ok, (unsigned long)z.runden_fehler,
                z.letzter_fehler[0] ? z.letzter_fehler : "-");
  if (as.aktiv) {
    Serial.printf("[Anker] %s: SOC %u %%, Batterie %ld W, Runden ok %lu, Fehler %lu (%s)\n",
                  anker_aktuell(an) ? "aktuell" : "keine aktuellen Werte", an.soc, (long)an.batterie_w,
                  (unsigned long)as.runden_ok, (unsigned long)as.runden_fehler,
                  as.letzter_fehler[0] ? as.letzter_fehler : "-");
  }
  if (f.zustand == FroniusZustand::BEREIT) {
    for (int i = 0; i < 2; i++) {
      const FroniusZaehlerStatistik& s = f.zaehler[i];
      if (s.adresse == 0) continue;
      Serial.printf("[Fronius] %s Adresse %u (%lu Baud %s): beantwortet %lu, abgelehnt %lu, "
                    "ohne Antwort %lu (SunSpec %lu, Eastron %lu)\n",
                    i == ZAEHLER_HAUPT ? "Netz" : "Solarbank", s.adresse, (unsigned long)f.baud,
                    format_name(f.format), (unsigned long)s.beantwortet, (unsigned long)s.abgelehnt,
                    (unsigned long)s.ohne_antwort, (unsigned long)s.sunspec_anfragen,
                    (unsigned long)s.eastron_anfragen);
    }
  } else {
    Serial.println(F("[Fronius] Erkennung laeuft"));
  }
  Serial.printf("[WLAN] Heimnetz %s %s, MQTT %s (gesendet %lu, Ersatz %lu, Speicher %lu)\n",
                netz_heimnetz_verbunden() ? "verbunden" : "getrennt", netz_heimnetz_ip().c_str(),
                !mq.aktiv ? "aus" : (mq.verbunden ? "verbunden" : "getrennt"),
                (unsigned long)mq.gesendet, (unsigned long)mq.ersatz_gesendet,
                (unsigned long)mq.speicher_gesendet);
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
  Serial.printf("SDM630 -> Fronius / openWB, Version %s\n", FIRMWARE_VERSION);

  einstellungen_laden();
  ereignisse_start();
  update_start();
  messwerte_start();
  zaehler_start();
  fronius_start();
  netz_start();
  uhrzeit_start();
  energie_start();
  webseite_start();
  mqtt_start();
  anker_start();
  solarapi_start();
}

void loop() {
  webseite_schritt();
  mqtt_schritt();
  anfragen_ausgeben();
  erkennung_ueberwachen();
  ereignisse_ueberwachen();
  energie_schritt();
  update_schritt();
#ifdef TEST_ABSTURZ_NACH_MS
  // NUR für den Test des Rückfalls (Bauschalter): stürzt absichtlich ab.
  if (millis() > TEST_ABSTURZ_NACH_MS) abort();
#endif
  if ((uint32_t)(millis() - letzte_uebersicht_ms) >= DEBUG_ZYKLUS_MS) {
    letzte_uebersicht_ms = millis();
    uebersicht_ausgeben();
  }
  delay(2);  // loop hat keine zeitkritische Aufgabe; Busse und Solarbank laufen in eigenen Tasks
}
