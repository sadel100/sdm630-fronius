// messwerte.h
// ---------------------------------------------------------------------------
// Der Zwischenspeicher (Cache) für die Zählerwerte. Die Abfrage des Zählers
// schreibt hinein, die Antworten an den Fronius lesen daraus. Beide laufen in
// verschiedenen Tasks, deshalb schützt eine Sperre (Mutex) den Zugriff.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

struct Messwerte {
  // Pflichtwerte (laut Handbuch SDM72D-M V1.5 vorhanden)
  float leistung_w      = 0;  // + Bezug, − Einspeisung (Bezug minus Einspeisung)
  float bezug_kwh       = 0;  // Zählerstand Bezug
  float einspeisung_kwh = 0;  // Zählerstand Einspeisung

  // Zusatzwerte, nur wenn der Zähler sie liefert (siehe `phasen_gueltig`)
  bool  phasen_gueltig = false;
  float spannung_v[3]  = {0, 0, 0};
  float strom_a[3]     = {0, 0, 0};
  float leistung_phase_w[3] = {0, 0, 0};
  bool  frequenz_gueltig = false;
  float frequenz_hz    = 0;
  bool  pf_gueltig     = false;
  float leistungsfaktor = 0;

  bool     gueltig      = false;  // mindestens eine vollständige Pflichtrunde gelesen
  uint32_t zeitstempel_ms = 0;    // millis() der letzten erfolgreichen Pflichtrunde
};

// Sperre anlegen. Einmal in setup() aufrufen, vor allen Tasks.
void messwerte_start();

// Ganze Werte schreiben (Zähler-Task) bzw. eine Kopie holen (Fronius-Task, Debug).
void messwerte_schreiben(const Messwerte& neu);
Messwerte messwerte_lesen();

// true, wenn gültige Werte vorliegen, die nicht älter als `max_alter_ms` sind.
// Rechnet überlaufsicher mit Differenzen.
bool messwerte_aktuell(const Messwerte& m, uint32_t max_alter_ms);
