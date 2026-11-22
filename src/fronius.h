// fronius.h
// ---------------------------------------------------------------------------
// Bus B: Der ESP32 spielt gegenüber dem Fronius Datamanager den Zähler
// (Modbus-RTU-Slave). Er beantwortet Leseanfragen ausschließlich aus dem
// Zwischenspeicher und wartet nie auf den SDM72D-M.
//
// Weil nicht bekannt ist, welche Register der Datamanager erwartet, stellt
// der ESP32 zwei Abbilder gleichzeitig bereit:
//   - SunSpec:  Funktionscode 03 (und 04) ab Adresse 40000
//   - Eastron:  Funktionscode 04 ab Adresse 0 (Registerbelegung wie SDM72/SDM630)
// Die beiden Bereiche überschneiden sich nicht. Was der Datamanager wirklich
// fragt, zeigt das Protokoll auf der Webseite.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

enum class FroniusZustand : uint8_t {
  ERKENNUNG,   // sucht Baudrate und Adresse
  BEREIT,      // Server läuft
};

enum class Bereich : uint8_t { SUNSPEC, EASTRON, UNBEKANNT };

enum class Ergebnis : uint8_t {
  BEANTWORTET,
  ABGELEHNT,       // Adresse oder Funktion nicht im Abbild
  OHNE_ANTWORT,    // Zählerdaten veraltet, absichtlich keine Antwort
};

struct FroniusAnfrage {
  uint32_t zeit_ms;
  uint8_t  funktion;
  uint16_t start;
  uint16_t anzahl;
  Bereich  bereich;
  Ergebnis ergebnis;
};

struct FroniusStatistik {
  FroniusZustand zustand = FroniusZustand::ERKENNUNG;
  uint32_t baud = 0;
  uint32_t format = 0;
  uint8_t  adresse = 0;
  uint32_t beantwortet = 0;
  uint32_t abgelehnt = 0;
  uint32_t ohne_antwort = 0;
  uint32_t sunspec_anfragen = 0;
  uint32_t eastron_anfragen = 0;
  uint32_t letzte_anfrage_ms = 0;   // millis() der letzten Anfrage an unsere Adresse (egal mit welchem Ergebnis)
  bool     schon_beantwortet = false;
};

// Startet einen Task: erst Erkennung (falls nötig), dann den Server.
void fronius_start();

FroniusStatistik fronius_statistik();

// Kopiert die letzten Anfragen (neueste zuerst) nach `ziel`, liefert die Anzahl.
size_t fronius_protokoll(FroniusAnfrage* ziel, size_t max_anzahl);

// Neue Anfragen für die USB-Ausgabe (jede nur einmal).
bool fronius_naechste_meldung(FroniusAnfrage& a);

const char* bereich_name(Bereich b);
const char* ergebnis_name(Ergebnis e);
