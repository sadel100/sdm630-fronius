// zaehler.h
// ---------------------------------------------------------------------------
// Abfrage des Eastron SDM72D-M über RS485 (Bus A, ESP32 ist Master).
// Ein eigener Task liest etwa jede Sekunde die Werte und legt sie im
// Zwischenspeicher ab (messwerte.h).
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

// Zähler für die Debug-Ausgabe
struct ZaehlerStatistik {
  uint32_t runden_ok     = 0;  // vollständige Pflichtrunden
  uint32_t runden_fehler = 0;  // Runden, in denen ein Pflichtwert fehlte
  char     letzter_fehler[48] = "";
  bool     phasen_vorhanden   = true;  // false, sobald der Zähler die Register ablehnt
  bool     frequenz_vorhanden = true;
  bool     pf_vorhanden       = true;
};

// UART1 und eModbus-Client einrichten, Abfrage-Task starten.
void zaehler_start();

ZaehlerStatistik zaehler_statistik();
