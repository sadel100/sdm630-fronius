// anker.h
// ---------------------------------------------------------------------------
// Liest die Anker SOLIX Solarbank Max AC per Modbus TCP im Heimnetz.
// NUR LESEN (Funktionscode 04). Der ESP32 schreibt nie in die Solarbank.
//
// In der Anker-App einschalten: Gerät → Zahnrad → „Three-Party Control
// Settings“ → „Modbus TCP“. Dort steht auch die IP-Adresse.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

struct AnkerWerte {
  bool     gueltig = false;
  uint32_t zeitstempel_ms = 0;
  uint16_t soc = 0;              // Ladestand in %
  int32_t  batterie_w = 0;       // + Entladen, − Laden (wie im Register 10008)
  int32_t  pv_w = 0;             // PV-Eingänge der Solarbank (10002 + 10004)
  int32_t  last_w = 0;           // 10010 Lastleistung laut Solarbank
  int32_t  netz_w = 0;           // 10012 Netzleistung laut Solarbank (+ Bezug)
  int32_t  ac_w = 0;             // 10208 AC-Ausgangsleistung zum Hausnetz
  uint16_t status = 0;           // 10001: 1 lädt, 2 entlädt
  float    lade_kwh = 0;         // 10262 geladen gesamt
  float    entlade_kwh = 0;      // 10264 entladen gesamt
  float    kapazitaet_kwh = 0;   // 10250 Nennkapazität
};

struct AnkerStatistik {
  bool     aktiv = false;        // in den Einstellungen eingeschaltet
  uint32_t runden_ok = 0;
  uint32_t runden_fehler = 0;
  char     letzter_fehler[48] = "";
  char     modell[12] = "";      // Kennung aus Register 0x8000 (Text)
};

void anker_start();
AnkerWerte anker_werte();
AnkerStatistik anker_statistik();
bool anker_aktuell(const AnkerWerte& w);
