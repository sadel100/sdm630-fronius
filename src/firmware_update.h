// firmware_update.h
// ---------------------------------------------------------------------------
// Neue Firmware einspielen, ohne USB-Kabel:
//
//  1. Datei hochladen: Auf der Webseite (Einstellungen → Update) eine
//     Datei sdm630-fronius-<Version>.bin auswählen. Geht immer, auch ohne
//     Internet (Notweg).
//  2. GitHub: „Nach Updates suchen“ fragt die neueste Release des
//     öffentlichen Repositorys GITHUB_BESITZER/GITHUB_REPO ab. „Installieren“
//     lädt die Datei direkt in den zweiten Programmspeicher.
//
// Installiert wird nur auf Knopfdruck, nie von selbst.
//
// Schutz: Eine neue Version muss UPDATE_GUT_NACH_MS lang fehlerfrei laufen.
// Stürzt sie vorher ab, startet der ESP32 wieder mit der alten Version.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

enum class UpdateZustand : uint8_t {
  RUHE,         // nichts gesucht
  SUCHT,        // fragt GitHub
  AKTUELL,      // keine neuere Version
  GEFUNDEN,     // neuere Version bereit zum Installieren
  LAEDT,        // lädt und schreibt
  FERTIG,       // geschrieben, Neustart folgt
  FEHLER,
};

struct UpdateStand {
  UpdateZustand zustand = UpdateZustand::RUHE;
  char    neue_version[16] = "";
  char    beschreibung[400] = "";  // Text der Release (gekürzt)
  uint8_t prozent = 0;
  char    meldung[120] = "";
  bool    probezeit = false;       // neue Version läuft, ist noch nicht als gut markiert
};

// Beim Start: Versionswechsel ins Ereignisprotokoll (nach ereignisse_start()).
void update_start();

// In loop(): neue Version nach der Probezeit als gut markieren, Neustart nach Update.
void update_schritt();

UpdateStand update_stand();

// GitHub (laufen im Hintergrund; der Stand zeigt das Ergebnis).
bool update_suchen();
bool update_installieren();

// Datei-Upload von der Webseite: Anfang, Teile, Ende.
// Gibt nullptr zurück, wenn alles stimmt, sonst den Grund.
const char* update_datei_anfang();
const char* update_datei_teil(const uint8_t* daten, size_t laenge);
const char* update_datei_ende();
void update_datei_abbrechen();
