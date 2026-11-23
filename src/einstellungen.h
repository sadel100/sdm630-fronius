// einstellungen.h
// ---------------------------------------------------------------------------
// Einstellungen, die auf der Webseite geändert werden können. Sie liegen im
// Flash des ESP32 (Preferences, Bereich "sdm72") und überstehen Neustarts.
// Fehlt ein Wert, gilt die Vorgabe aus konfiguration.h.
//
// Passwörter (WLAN, MQTT, eigenes WLAN) werden nie auf der Webseite angezeigt
// und nie über USB ausgegeben.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

struct Einstellungen {
  // Bus A: Zähler
  uint32_t zaehler_baud;
  uint8_t  zaehler_adresse;
  uint32_t zaehler_format;     // SERIAL_8N1, SERIAL_8E1 …

  // Bus B: Fronius. 0 = automatisch
  uint32_t fronius_baud;
  uint32_t fronius_format;     // SERIAL_8N1, SERIAL_8E1 … (nur wenn baud != 0)
  uint8_t  fronius_adresse;
  uint16_t sunspec_modell;     // 213 oder 203
  int8_t   vorzeichen;         // +1 oder -1
  uint8_t  zweit_adresse;      // zweiter Zähler (Solarbank), 0 = aus

  // Zuletzt erkannt (nur Anzeige und schneller Start)
  uint32_t erkannt_baud;
  uint32_t erkannt_format;
  uint8_t  erkannt_adresse;

  // WLAN im Heimnetz
  String   wlan_name;
  String   wlan_passwort;
  String   ap_passwort;        // eigenes WLAN und Webseiten-Passwort

  // Anker Solarbank (Modbus TCP)
  bool     anker_an;
  String   anker_ip;
  uint16_t anker_port;
  uint8_t  anker_unit;

  // Fronius Solar API (Datamanager im Heimnetz, nur lesen)
  bool     solarapi_an;
  String   fronius_ip;

  // MQTT an die openWB
  bool     mqtt_an;
  String   mqtt_server;
  uint16_t mqtt_port;
  String   mqtt_benutzer;
  String   mqtt_passwort;
  bool     mqtt_ersatz_an;     // bei Zählerausfall Ersatzwert senden
  int32_t  mqtt_ersatz_w;
  bool     mqtt_speicher_an;   // Solarbank als Speicher an die openWB
};

// Aus dem Flash laden (einmal in setup()).
void einstellungen_laden();

// Aktuelle Einstellungen (Kopie). Nach dem Laden nur noch lesend benutzt,
// Änderungen gelten erst nach einem Neustart.
const Einstellungen& einstellungen();

// Geänderte Einstellungen in den Flash schreiben.
void einstellungen_speichern(const Einstellungen& neu);

// Nur die erkannten Fronius-Werte schreiben (aus der Erkennung heraus).
void einstellungen_erkannt_speichern(uint32_t baud, uint32_t format, uint8_t adresse);

// Lesbarer Name eines seriellen Formats, z. B. "8N1".
const char* format_name(uint32_t format);
