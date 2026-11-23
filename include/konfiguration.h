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
#ifdef TEST_VERSION
constexpr const char* FIRMWARE_VERSION = TEST_VERSION;   // nur für Update-Tests (Bauschalter)
#else
constexpr const char* FIRMWARE_VERSION = "1.3.0";
#endif

// ===========================================================================
// Pins (ESP32-WROOM-32). Je RS485-Wandler SP3485: RO -> RX, DI -> TX,
// DE und /RE gebrückt -> DE. Startpins 0, 2, 5, 12, 15 und Flash-Pins 6–11
// sind bewusst frei.
// ===========================================================================
constexpr int ZAEHLER_PIN_RX = 25;   // Bus A: Netzzähler SDM630
constexpr int ZAEHLER_PIN_TX = 26;
constexpr int ZAEHLER_PIN_DE = 27;

constexpr int FRONIUS_PIN_RX = 16;   // Bus B: Fronius Datamanager
constexpr int FRONIUS_PIN_TX = 17;
constexpr int FRONIUS_PIN_DE = 4;

// ===========================================================================
// Bus A: Zähler (Vorgaben, auf der Webseite änderbar)
// ===========================================================================
// PRÜFEN am SDM630 (Setup-Menü): Werkseinstellung meist 9600 Baud, Adresse 1.
// Die Parität (None/Even) unterscheidet sich je nach Quelle: am Zähler nachsehen.
constexpr uint32_t VORGABE_ZAEHLER_BAUD    = 9600;
constexpr uint8_t  VORGABE_ZAEHLER_ADRESSE = 1;
constexpr uint32_t ZAEHLER_FORMAT          = SERIAL_8N1;   // Vorgabe, auf der Webseite änderbar

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
constexpr const char* SUNSPEC_SERIE      = "SDM630EMU001";    // Netzzähler
constexpr const char* SUNSPEC_SERIE_ZWEIT = "SOLIXEMU0002";   // Solarbank
constexpr uint16_t    SUNSPEC_GERAETEADRESSE = 240;   // Feld "DA" im Modell 1

// +1: an den Fronius positiv = Bezug, negativ = Einspeisung. PRÜFEN in
// Solar.web, sonst auf der Webseite umdrehen.
constexpr int VORGABE_VORZEICHEN = +1;

// Leistungsfaktor im SunSpec-Abbild als Prozent (0,98 -> 98). PRÜFEN.
constexpr bool LEISTUNGSFAKTOR_IN_PROZENT = true;

// Zweiter Zähler am Fronius-Bus (Anker Solarbank). 0 = aus.
// Der Fronius vergibt Sekundärzählern die Adressen 2–14.
constexpr uint8_t VORGABE_ZWEIT_ADRESSE = 2;

// ===========================================================================
// Anker SOLIX Solarbank Max AC (Modbus TCP im Heimnetz, NUR LESEN)
// Register aus der offiziellen Anker-Integration für Home Assistant
// (anker-charging/ha-anker-solix-official, Datei der Solarbank Max AC).
// ===========================================================================
constexpr uint16_t VORGABE_ANKER_PORT  = 502;
constexpr uint8_t  VORGABE_ANKER_UNIT  = 1;
constexpr uint32_t ANKER_ABFRAGE_MS    = 5000;    // wie die Anker-Integration
constexpr uint32_t ANKER_TIMEOUT_MS    = 3000;
constexpr uint32_t ANKER_MAX_ALTER_MS  = 20000;   // älter: Solarbank gilt als ausgefallen

// ===========================================================================
// WLAN und Webseite
// ===========================================================================
// Eigenes WLAN des ESP32 (immer an, Notzugang). Das Passwort ist auch das
// Passwort für Änderungen auf der Webseite (Benutzer "admin").
// Bitte nach der Inbetriebnahme auf der Webseite ändern.
constexpr const char* AP_NAME              = "SDM630-Fronius";
constexpr const char* VORGABE_AP_PASSWORT  = "sdm630fronius";  // mindestens 8 Zeichen
constexpr const char* WEB_BENUTZER         = "admin";
constexpr const char* GERAETENAME          = "sdm630-fronius"; // Name im Heimnetz
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

// ===========================================================================
// Uhrzeit (Zeitserver im Internet, deutsche Zeit mit Sommerzeit)
// ===========================================================================
constexpr const char* ZEITZONE      = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr const char* ZEITSERVER_1  = "pool.ntp.org";
constexpr const char* ZEITSERVER_2  = "ptbtime1.ptb.de";
constexpr const char* ZEITSERVER_3  = "time.google.com";

// ===========================================================================
// Ereignisprotokoll (nur im Arbeitsspeicher, nach einem Neustart leer)
// ===========================================================================
constexpr size_t   EREIGNIS_ANZAHL   = 60;    // so viele Einträge, die ältesten fallen heraus
constexpr uint32_t EREIGNIS_TAKT_MS  = 1000;  // so oft werden die Zustände verglichen
// Erst wenn ein Ausfall so lange dauert, wird er eingetragen (kurze Aussetzer nicht).
constexpr uint32_t EREIGNIS_ENTPRELL_MS = 10000;

// ===========================================================================
// Fronius Solar API (Datamanager im Heimnetz, NUR LESEN, HTTP)
// GET /solar_api/v1/GetPowerFlowRealtimeData.fcgi
// ===========================================================================
constexpr uint32_t SOLARAPI_ABFRAGE_MS = 5000;
constexpr uint32_t SOLARAPI_TIMEOUT_MS = 3000;
constexpr uint32_t SOLARAPI_MAX_ALTER_MS = 20000;

// ===========================================================================
// Verlauf und Energiewerte
// ===========================================================================
constexpr uint32_t ENERGIE_TAKT_MS    = 10000;   // so oft wird gemessen
constexpr uint32_t VERLAUF_PUNKT_S    = 300;     // ein Punkt je 5 Minuten (Mittelwert)
constexpr size_t   VERLAUF_PUNKTE     = 288;     // 24 Stunden
constexpr uint32_t HAUS_SPEICHERN_MS  = 15UL * 60UL * 1000UL;  // Hausverbrauch alle 15 min in den Flash
constexpr size_t   MONATE_ANZAHL      = 12;

// ===========================================================================
// Firmware-Update (Datei über die Webseite oder Releases auf GitHub)
// Das Repository ist öffentlich, die Box braucht keinen Schlüssel.
// Release-Kennzeichen "v<Version>", Datei "sdm630-fronius-<Version>.bin".
// ===========================================================================
constexpr const char* GITHUB_BESITZER = "sadel100";
constexpr const char* GITHUB_REPO     = "sdm630-fronius";
constexpr const char* UPDATE_DATEI_ANFANG = "sdm630-fronius-";
constexpr uint32_t    UPDATE_GUT_NACH_MS  = 60000;  // so lange muss eine neue Version fehlerfrei laufen
