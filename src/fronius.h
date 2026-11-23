// fronius.h
// ---------------------------------------------------------------------------
// Bus B: Der ESP32 spielt gegenüber dem Fronius Datamanager zwei Zähler
// (Modbus-RTU-Slave):
//   - Zähler 1 (Primärzähler, meist Adresse 1): der Netzzähler SDM630
//   - Zähler 2 (Sekundärzähler, meist Adresse 2): die Anker Solarbank
//     (Laden positiv wie ein Verbraucher, Entladen negativ)
// Er beantwortet Leseanfragen nur aus den Zwischenspeichern und wartet nie
// auf den Zähler oder die Solarbank.
//
// Je Zähler gibt es zwei Abbilder gleichzeitig (der Fronius sucht sich aus,
// was er fragt; das Protokoll auf der Webseite zeigt es):
//   - SunSpec:  Funktionscode 03 (und 04) ab Adresse 40000
//   - Eastron:  Funktionscode 04 ab Adresse 0 (Belegung wie SDM630)
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

enum class FroniusZustand : uint8_t { ERKENNUNG, BEREIT };
enum class Bereich : uint8_t { SUNSPEC, EASTRON, UNBEKANNT };
enum class Ergebnis : uint8_t {
  BEANTWORTET,
  ABGELEHNT,       // Adresse oder Funktion nicht im Abbild
  OHNE_ANTWORT,    // Daten veraltet, absichtlich keine Antwort
};

constexpr int ZAEHLER_HAUPT = 0;   // Netzzähler
constexpr int ZAEHLER_ZWEIT = 1;   // Solarbank

struct FroniusAnfrage {
  uint32_t zeit_ms;
  uint8_t  zaehler;    // ZAEHLER_HAUPT oder ZAEHLER_ZWEIT
  uint8_t  funktion;
  uint16_t start;
  uint16_t anzahl;
  Bereich  bereich;
  Ergebnis ergebnis;
};

struct FroniusZaehlerStatistik {
  uint8_t  adresse = 0;            // 0 = nicht aktiv
  uint32_t beantwortet = 0;
  uint32_t abgelehnt = 0;
  uint32_t ohne_antwort = 0;
  uint32_t sunspec_anfragen = 0;
  uint32_t eastron_anfragen = 0;
  uint32_t letzte_anfrage_ms = 0;
};

struct FroniusStatistik {
  FroniusZustand zustand = FroniusZustand::ERKENNUNG;
  uint32_t baud = 0;
  uint32_t format = 0;
  FroniusZaehlerStatistik zaehler[2];
  uint32_t letzte_anfrage_ms = 0;  // irgendeine Anfrage an einen unserer Zähler
};

void fronius_start();
FroniusStatistik fronius_statistik();
size_t fronius_protokoll(FroniusAnfrage* ziel, size_t max_anzahl);
bool fronius_naechste_meldung(FroniusAnfrage& a);
const char* bereich_name(Bereich b);
const char* ergebnis_name(Ergebnis e);

// Leistung, die der zweite Zähler dem Fronius meldet (für die Anzeige)
float fronius_zweit_leistung_w();
