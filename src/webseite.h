// webseite.h
// ---------------------------------------------------------------------------
// Webseite des ESP32 (Port 80): Werte, Zustand der Busse, Protokoll der
// Fronius-Anfragen, MQTT und Einstellungen. Erreichbar über das eigene WLAN
// (http://192.168.4.1) und im Heimnetz (http://sdm72-fronius.local oder IP).
// Änderungen verlangen Benutzer "admin" und das Passwort des eigenen WLANs.
// ---------------------------------------------------------------------------
#pragma once

void webseite_start();
void webseite_schritt();   // in loop() aufrufen
