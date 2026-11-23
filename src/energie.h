// energie.h
// ---------------------------------------------------------------------------
// Leistungsfluss, Tagesverlauf und Tages-/Monatswerte.
//
// Leistungsfluss (in Watt, NAN = unbekannt):
//   Netz        vom SDM630 (+ Bezug, − Einspeisung)
//   PV Symo     von der Fronius Solar API
//   Solarbank   was die Solarbank ins Haus abgibt (+) bzw. zum Laden aus dem
//               Haus nimmt (−), gerechnet als PV der Solarbank + Batterie-
//               leistung (Verluste der Solarbank sind nicht berücksichtigt)
//   Haus        Netz + PV Symo + Solarbank
//
// Tagesverlauf: alle 10 s gemessen, je 5 Minuten ein Mittelwert, 24 Stunden.
// Liegt nur im Arbeitsspeicher, nach einem Neustart beginnt er neu.
//
// Tages- und Monatswerte: aus den Zählerständen (SDM630, Solarbank, Symo)
// zu Beginn des Tages bzw. Monats. Der Hausverbrauch hat keinen Zählerstand,
// er wird aus der Leistung aufsummiert. Braucht die Uhrzeit vom Zeitserver.
// Gespeichert im Flash (Bereich "energie"), übersteht Neustarts.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>
#include "konfiguration.h"

struct Fluss {
  float netz_w = NAN;
  float pv_symo_w = NAN;
  float solarbank_w = NAN;      // + gibt ab, − nimmt auf
  float solarbank_laden_w = NAN; // + lädt, − entlädt (Batterie)
  float solarbank_pv_w = NAN;
  float soc = NAN;
  float haus_w = NAN;
  bool  solarbank_aktiv = false;
  bool  solarapi_aktiv = false;
};

// Die Energiewerte. Reihenfolge der Felder in allen Listen:
enum EnergieArt : uint8_t { E_BEZUG = 0, E_EINSPEISUNG, E_PV_SYMO, E_GELADEN, E_ENTLADEN, E_HAUS, E_ANZAHL };

struct EnergieSumme {
  uint32_t kennung = 0;           // Tag JJJJMMTT oder Monat JJJJMM, 0 = leer
  float    kwh[E_ANZAHL];         // NAN = unbekannt
};

struct EnergieStand {
  bool          uhrzeit = false;  // ohne Uhrzeit keine Tages-/Monatswerte
  EnergieSumme  heute;
  EnergieSumme  gestern;
  EnergieSumme  monat;            // laufender Monat
  EnergieSumme  monate[MONATE_ANZAHL];  // abgeschlossene Monate, neuester zuerst
};

struct VerlaufPunkt {
  uint32_t ms;                    // millis() am Ende der 5 Minuten
  int16_t  netz, pv_symo, solarbank, haus;   // W, VERLAUF_LEER = kein Wert
  int8_t   soc;                   // %, −1 = kein Wert
};
constexpr int16_t VERLAUF_LEER = INT16_MIN;

// Gespeicherte Werte laden (einmal in setup()).
void energie_start();

// Messen, mitteln, Tageswechsel (in loop(), arbeitet alle ENERGIE_TAKT_MS).
void energie_schritt();

// Leistungsfluss jetzt.
Fluss energie_fluss();

// Tages- und Monatswerte.
EnergieStand energie_stand();

// Tagesverlauf, ältester Punkt zuerst. Gibt die Anzahl zurück.
size_t energie_verlauf(VerlaufPunkt* ziel, size_t max);
