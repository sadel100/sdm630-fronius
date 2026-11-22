// netz.cpp
// ---------------------------------------------------------------------------
// WLAN-Start: eigenes WLAN (Access Point) und Verbindung zum Heimnetz.
// Den Wiederaufbau einer abgerissenen Verbindung übernimmt der ESP32 selbst
// (setAutoReconnect).
// ---------------------------------------------------------------------------
#include "netz.h"
#include "einstellungen.h"
#include "konfiguration.h"
#include <WiFi.h>
#include <ESPmDNS.h>

void netz_start() {
  const Einstellungen& e = einstellungen();
  WiFi.persistent(false);       // Zugangsdaten nicht zusätzlich im WLAN-Speicher ablegen
  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(GERAETENAME);
  WiFi.softAP(AP_NAME, e.ap_passwort.c_str());
  if (e.wlan_name.length() > 0) {
    WiFi.setAutoReconnect(true);
    WiFi.begin(e.wlan_name.c_str(), e.wlan_passwort.c_str());
  }
  if (MDNS.begin(GERAETENAME)) {
    MDNS.addService("http", "tcp", 80);
  }
  Serial.printf("[WLAN] eigenes WLAN \"%s\", Adresse %s\n", AP_NAME,
                WiFi.softAPIP().toString().c_str());
  if (e.wlan_name.length() > 0) {
    Serial.printf("[WLAN] verbinde mit Heimnetz \"%s\"\n", e.wlan_name.c_str());
  }
}

bool netz_heimnetz_verbunden() {
  return WiFi.status() == WL_CONNECTED;
}

String netz_heimnetz_ip() {
  return netz_heimnetz_verbunden() ? WiFi.localIP().toString() : String("");
}

int netz_empfang_dbm() {
  return WiFi.RSSI();
}

String netz_ap_ip() {
  return WiFi.softAPIP().toString();
}
