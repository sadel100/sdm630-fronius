// konfiguration.h
// ---------------------------------------------------------------------------
// Feste Einstellwerte: Pins, Zeiten, Grenzen und die Vorgaben für alle Werte,
// die sich später auf der Webseite ändern lassen (die geänderten Werte liegen
// dann im Flash des ESP32, siehe einstellungen.h).
// Werte mit "PRÜFEN" sind nicht aus einer verlässlichen Quelle bestätigt.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

// Versionsnummer der Firmware (auf der Webseite sichtbar)
constexpr const char* FIRMWARE_VERSION = "1.1.0";

// ===========================================================================
// Pins (ESP32-WROOM-32). Je RS485-Wandler SP3485: RO -> RX, DI -> TX,
// DE und /RE gebrückt -> DE. Startpins 0, 2, 5, 12, 15 und Flash-Pins 6–11
// sind bewusst frei.
// ===========================================================================
constexpr int ZAEHLER_PIN_RX = 25;   // Bus A: SDM72D-M
constexpr int ZAEHLER_PIN_TX = 26;
constexpr int ZAEHLER_PIN_DE = 27;

constexpr int FRONIUS_PIN_RX = 16;   // Bus B: Fronius Datamanager
constexpr int FRONIUS_PIN_TX = 17;
constexpr int FRONIUS_PIN_DE = 4;

// ===========================================================================
// Bus A: Zähler (Vorgaben, auf der Webseite änderbar)
// ===========================================================================
// PRÜFEN am Zähler (Setup-Menü): üblich 9600 Baud, 8N1, Adresse 1.
constexpr uint32_t VORGABE_ZAEHLER_BAUD    = 9600;
constexpr uint8_t  VORGABE_ZAEHLER_ADRESSE = 1;
constexpr uint32_t ZAEHLER_FORMAT          = SERIAL_8N1;

constexpr uint32_t ZAEHLER_ABFRAGE_MS = 1000;  // eine Runde je Sekunde
constexpr uint32_t ZAEHLER_TIMEOUT_MS = 500;   // Warten auf eine Antwort
constexpr uint32_t ZAEHLER_PAUSE_MS   = 30;    // Ruhe zwischen zwei Anfragen

// ===========================================================================
// Bus B: Fronius (Vorgaben, auf der Webseite änderbar)
// ===========================================================================
// 0 = automatisch erkennen (Baudrate/Format bzw. Busadresse aus den Anfragen
// des Datamanagers ablesen).
constexpr uint32_t VORGABE_FRONIUS_BAUD    = 0;
constexpr uint8_t  VORGABE_FRONIUS_ADRESSE = 0;

// Pause auf dem Bus, nach der eine Anfrage als zu Ende gilt (eModbus).
constexpr uint32_t FRONIUS_SERVER_TIMEOUT_MS = 2000;

// Sind die Zählerwerte älter als diese Zeit, antwortet der ESP32 dem Fronius
// NICHT. Der Datamanager soll einen Zählerausfall erkennen.
constexpr uint32_t CACHE_MAX_ALTER_MS = 5000;

// Erkennung: so lange je Baudrate/Format zuhören, so viele gültige Anfragen
// müssen kommen, bevor die Einstellung gilt.
constexpr uint32_t ERKENNUNG_LAUSCHEN_MS    = 4000;
constexpr uint8_t  ERKENNUNG_MIN_ANFRAGEN   = 2;
// Automatik: Kommt so lange keine beantwortete Anfrage, neu erkennen (Neustart).
constexpr uint32_t ERKENNUNG_NEU_NACH_MS    = 5UL * 60UL * 1000UL;

// ===========================================================================
// SunSpec-Abbild (Vorgaben, auf der Webseite änderbar)
// ===========================================================================
// 213 = Gleitkomma, 203 = Ganzzahl mit Skalierungsfaktor. PRÜFEN.
constexpr uint16_t VORGABE_SUNSPEC_MODELL = 213;
constexpr uint16_t SUNSPEC_BASIS = 40000;   // Register 40001 = Adresse 40000

constexpr const char* SUNSPEC_HERSTELLER = "Fronius";
constexpr const char* SUNSPEC_GERAET     = "Smart Meter 63A";
constexpr const char* SUNSPEC_OPTIONEN   = "";
constexpr const char* SUNSPEC_VERSION    = "1.0";
constexpr const char* SUNSPEC_SERIE      = "SDM72EMU0001";
constexpr uint16_t    SUNSPEC_GERAETEADRESSE = 240;   // Feld "DA" im Modell 1

// +1: an den Fronius positiv = Bezug, negativ = Einspeisung. PRÜFEN in
// Solar.web, sonst auf der Webseite umdrehen.
constexpr int VORGABE_VORZEICHEN = +1;

// Leistungsfaktor im SunSpec-Abbild als Prozent (0,98 -> 98). PRÜFEN.
constexpr bool LEISTUNGSFAKTOR_IN_PROZENT = true;

// ===========================================================================
// WLAN und Webseite
// ===========================================================================
// Eigenes WLAN des ESP32 (immer an, Notzugang). Das Passwort ist auch das
// Passwort für Änderungen auf der Webseite (Benutzer "admin").
// Bitte nach der Inbetriebnahme auf der Webseite ändern.
constexpr const char* AP_NAME              = "SDM72-Fronius";
constexpr const char* VORGABE_AP_PASSWORT  = "sdm72fronius";   // mindestens 8 Zeichen
constexpr const char* WEB_BENUTZER         = "admin";
constexpr const char* GERAETENAME          = "sdm72-fronius";  // Name im Heimnetz
constexpr uint32_t    WLAN_NEUVERSUCH_MS   = 30000;

// ===========================================================================
// MQTT an die openWB 1.9 (Themen laut openWB-Hilfe "MQTT"-EVU-Modul)
// ===========================================================================
constexpr uint16_t VORGABE_MQTT_PORT   = 1883;
constexpr uint32_t MQTT_TAKT_MS        = 1000;   // Werte jede Sekunde senden
constexpr uint32_t MQTT_NEUVERSUCH_MS  = 10000;
// openWB 1.9 erkennt veraltete Werte nicht selbst, ein alter Wert bliebe
// stehen. Deshalb sendet der ESP32 bei Zählerausfall diesen Ersatzwert als
// Netzbezug (W). Ein hoher Bezug lässt die openWB das Überschussladen beenden.
constexpr int32_t VORGABE_MQTT_ERSATZ_W = 10000;

// ===========================================================================
// Debug-Ausgabe über USB
// ===========================================================================
constexpr uint32_t DEBUG_BAUD      = 115200;
constexpr uint32_t DEBUG_ZYKLUS_MS = 5000;

// Protokoll der Fronius-Anfragen für die Webseite
constexpr size_t PROTOKOLL_EINTRAEGE = 40;
