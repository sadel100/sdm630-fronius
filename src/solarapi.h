// solarapi.h
// ---------------------------------------------------------------------------
// Liest die PV-Leistung des Fronius Symo über die „Solar API“ des
// Datamanagers im Heimnetz (HTTP, NUR LESEN):
//   GET http://<Datamanager>/solar_api/v1/GetPowerFlowRealtimeData.fcgi
//
// Daraus kennt die Box die PV-Leistung des Symo und kann den Hausverbrauch
// ausrechnen. Die Solar API muss im Datamanager eingeschaltet sein
// (Einstellungen → Kommunikation bzw. „Solar API“).
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

struct SolarApiWerte {
  bool     gueltig = false;
  uint32_t zeitstempel_ms = 0;
  float    pv_w = NAN;          // P_PV: PV-Leistung des Symo (nachts NAN, gilt als 0)
  float    netz_w = NAN;        // P_Grid laut Fronius (+ Bezug)
  float    last_w = NAN;        // P_Load laut Fronius (negativ = Verbrauch)
  double   gesamt_wh = NAN;     // E_Total: erzeugt seit Inbetriebnahme
  float    tag_wh = NAN;        // E_Day: erzeugt heute
};

struct SolarApiStatistik {
  bool     aktiv = false;
  uint32_t runden_ok = 0;
  uint32_t runden_fehler = 0;
  char     letzter_fehler[48] = "";
};

void solarapi_start();
SolarApiWerte solarapi_werte();
SolarApiStatistik solarapi_statistik();
bool solarapi_aktuell(const SolarApiWerte& w);
