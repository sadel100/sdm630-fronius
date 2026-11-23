// ereignisse.h
// ---------------------------------------------------------------------------
// Ereignisprotokoll: Was ist wann passiert? Zum Beispiel „SDM630 antwortet
// nicht“, „Heimnetz getrennt“, „Neustart nach Stromausfall“.
//
// Die Einträge liegen nur im Arbeitsspeicher (die letzten EREIGNIS_ANZAHL).
// Nach einem Neustart steht als erster Eintrag, warum neu gestartet wurde.
// Ausfälle werden erst eingetragen, wenn sie länger als
// EREIGNIS_ENTPRELL_MS dauern, kurze Aussetzer füllen die Liste nicht.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

struct Ereignis {
  uint32_t zeit;        // Sekunden seit 1970, 0 = Uhrzeit war noch unbekannt
  uint32_t ms;          // millis() beim Eintrag
  bool     stoerung;    // true = etwas ist ausgefallen (Anzeige rot)
  char     text[80];
};

// Erster Eintrag mit dem Grund des Neustarts (einmal in setup()).
void ereignisse_start();

// Eintrag hinzufügen (aus jedem Task erlaubt). Wie printf.
void ereignis(bool stoerung, const char* format, ...);

// Zustände aller Teile vergleichen und Änderungen eintragen (in loop()).
void ereignisse_ueberwachen();

// Kopie der Einträge, neueste zuerst. Gibt die Anzahl zurück.
size_t ereignisse_lesen(Ereignis* ziel, size_t max);
