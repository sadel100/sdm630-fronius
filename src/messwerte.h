// messwerte.h
// ---------------------------------------------------------------------------
// Der Zwischenspeicher (Cache) für die Werte des Netzzählers SDM630.
// Die Abfrage des Zählers schreibt hinein, die Antworten an den Fronius, MQTT
// und die Webseite lesen daraus. Beide laufen in verschiedenen Tasks, deshalb
// schützt eine Sperre (Mutex) den Zugriff.
//
// Neben den umgerechneten Werten liegen hier auch die Rohregister des
// Zählers. Fragt der Fronius die Eastron-Register ab, bekommt er sie 1:1.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

// Rohregister-Bereiche des SDM630 (Input-Register, Funktionscode 04)
constexpr uint16_t ROH_A_START = 0x0000, ROH_A_ANZAHL = 80;   // 0x0000–0x004F: Spannung … Energie
constexpr uint16_t ROH_B_START = 0x00C8, ROH_B_ANZAHL = 40;   // 0x00C8–0x00EF: Außenleiterspannung, N-Strom, THD
constexpr uint16_t ROH_C_START = 0x0156, ROH_C_ANZAHL = 50;   // 0x0156–0x0187: Summen- und Phasenzählerstände

struct Messwerte {
  // Umgerechnete Werte (Vorzeichen: + Bezug, − Einspeisung)
  float leistung_w      = 0;
  float bezug_kwh       = 0;
  float einspeisung_kwh = 0;
  bool  phasen_gueltig  = false;
  float spannung_v[3]   = {0, 0, 0};
  float strom_a[3]      = {0, 0, 0};
  float leistung_phase_w[3] = {0, 0, 0};
  float pf_phase[3]     = {0, 0, 0};
  bool  frequenz_gueltig = false;
  float frequenz_hz     = 0;
  bool  pf_gueltig      = false;
  float leistungsfaktor = 0;

  // Rohregister für die Eastron-Antworten an den Fronius
  uint16_t roh_a[ROH_A_ANZAHL] = {0};
  uint16_t roh_b[ROH_B_ANZAHL] = {0};
  uint16_t roh_c[ROH_C_ANZAHL] = {0};
  bool     roh_b_ok = false;
  bool     roh_c_ok = false;

  bool     gueltig = false;       // mindestens eine vollständige Pflichtrunde gelesen
  uint32_t zeitstempel_ms = 0;    // millis() der letzten erfolgreichen Pflichtrunde
};

// Sperre anlegen. Einmal in setup() aufrufen, vor allen Tasks.
void messwerte_start();

// Ganze Werte schreiben (Zähler-Task) bzw. eine Kopie holen.
void messwerte_schreiben(const Messwerte& neu);
Messwerte messwerte_lesen();

// true, wenn gültige Werte vorliegen, die nicht älter als `max_alter_ms` sind.
bool messwerte_aktuell(const Messwerte& m, uint32_t max_alter_ms);
