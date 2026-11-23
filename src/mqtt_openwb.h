// mqtt_openwb.h
// ---------------------------------------------------------------------------
// Schickt die Zählerwerte jede Sekunde per MQTT an die openWB (Software 1.9).
// In der openWB als EVU-Modul "MQTT" einstellen. Themen laut Hilfetext der
// openWB 1.9 (web/settings/modulconfigevu.php):
//   openWB/set/evu/W           Leistung in W, Ganzzahl, + Bezug, − Einspeisung
//   openWB/set/evu/WhImported  Bezug in Wh          openWB/set/evu/WhExported  Einspeisung in Wh
//   openWB/set/evu/APhase1..3  Strom in A (+ Bezug)  openWB/set/evu/VPhase1..3  Spannung in V
//   openWB/set/evu/WPhase1..3  Leistung je Phase     openWB/set/evu/PfPhase1..3 Leistungsfaktor
//   openWB/set/evu/HzFrequenz  Frequenz in Hz
// Speicher (Solarbank): openWB/set/houseBattery/W, %Soc, WhImported, WhExported.
// Nicht "retained" senden: die openWB löscht gehaltene set-Themen selbst.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

struct MqttStand {
  bool     aktiv = false;       // in den Einstellungen eingeschaltet
  bool     verbunden = false;
  int      fehlercode = 0;      // PubSubClient::state()
  uint32_t gesendet = 0;        // Runden mit echten Werten
  uint32_t ersatz_gesendet = 0; // Runden mit Ersatzwert (Zählerausfall)
  uint32_t speicher_gesendet = 0;  // Runden mit Speicherwerten (Solarbank)
  uint32_t speicher_ersatz = 0;    // Runden mit W = 0, weil die Solarbank nicht antwortet
};

void mqtt_start();
void mqtt_schritt();   // in loop() aufrufen
MqttStand mqtt_stand();
