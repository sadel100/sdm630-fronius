// uhrzeit.h
// ---------------------------------------------------------------------------
// Uhrzeit vom Zeitserver (NTP) über das Heimnetz, deutsche Zeit mit
// Sommerzeit. Ohne Heimnetz gibt es keine Uhrzeit: Ereignisse stehen dann
// mit „seit dem Start“, Tages- und Monatswerte werden nicht gezählt.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>
#include <time.h>

// Zeitserver einstellen (einmal in setup(), nach netz_start()).
void uhrzeit_start();

// true, sobald die Uhrzeit einmal vom Zeitserver gestellt wurde.
bool uhrzeit_bekannt();

// Aktuelle Zeit als Sekunden seit 1970 (UTC), 0 wenn unbekannt.
uint32_t uhrzeit_jetzt();

// Ortszeit zu einem Zeitpunkt (Sekunden seit 1970).
void uhrzeit_lokal(uint32_t zeit, struct tm& t);

// "23.11.2026 14:05:09" in `ziel` (mindestens 20 Zeichen).
void uhrzeit_text(uint32_t zeit, char* ziel, size_t groesse);
