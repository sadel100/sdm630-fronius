// mqtt_openwb.cpp
// ---------------------------------------------------------------------------
// MQTT an die openWB 1.9 (siehe mqtt_openwb.h).
//
// Bei Zählerausfall (Werte älter als CACHE_MAX_ALTER_MS): openWB 1.9 merkt
// selbst nicht, dass keine neuen Werte kommen, der letzte Wert bliebe stehen.
// Deshalb sendet der ESP32 dann – falls eingeschaltet – einen Ersatzwert als
// Netzbezug (Standard 10 000 W). Die openWB sieht keinen Überschuss mehr und
// beendet das Überschussladen. Ist der Ersatzwert ausgeschaltet, sendet der
// ESP32 gar nichts.
// ---------------------------------------------------------------------------
#include "mqtt_openwb.h"
#include "messwerte.h"
#include "anker.h"
#include "einstellungen.h"
#include "netz.h"
#include "konfiguration.h"
#include <WiFi.h>
#include <PubSubClient.h>

static WiFiClient   netz_client;
static PubSubClient mqtt(netz_client);
static MqttStand    stand;
static uint32_t     letzter_takt_ms = 0;
static uint32_t     letzter_versuch_ms = 0;
static bool         erster_versuch = true;
static const char*  THEMA = "openWB/set/evu/";

static void senden(const char* name, const String& wert) {
  char thema[48];
  snprintf(thema, sizeof(thema), "%s%s", THEMA, name);
  mqtt.publish(thema, wert.c_str(), false);
}

// Speicher (Anker Solarbank) an die openWB 1.9, Themen laut openWB-Hilfe:
//   openWB/set/houseBattery/W          Leistung, Ganzzahl, + Laden, − Entladen
//   openWB/set/houseBattery/%Soc       Ladestand 0–100
//   openWB/set/houseBattery/WhImported geladen in Wh
//   openWB/set/houseBattery/WhExported entladen in Wh
// Sind die Werte veraltet, geht W = 0 hinaus: die openWB soll keinen
// Ladevorgang des Speichers annehmen, den es nicht gibt.
static void speicher_senden() {
  const AnkerWerte a = anker_werte();
  if (!a.gueltig) return;
  if (!anker_aktuell(a)) {
    mqtt.publish("openWB/set/houseBattery/W", "0", false);
    stand.speicher_ersatz++;
    return;
  }
  mqtt.publish("openWB/set/houseBattery/W", String(-a.batterie_w).c_str(), false);
  mqtt.publish("openWB/set/houseBattery/%Soc", String(a.soc).c_str(), false);
  mqtt.publish("openWB/set/houseBattery/WhImported", String(a.lade_kwh * 1000.0f, 0).c_str(), false);
  mqtt.publish("openWB/set/houseBattery/WhExported", String(a.entlade_kwh * 1000.0f, 0).c_str(), false);
  stand.speicher_gesendet++;
}

static void verbinden() {
  const Einstellungen& e = einstellungen();
  const String kennung = String(GERAETENAME) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  bool ok;
  if (e.mqtt_benutzer.length() > 0) {
    ok = mqtt.connect(kennung.c_str(), e.mqtt_benutzer.c_str(), e.mqtt_passwort.c_str());
  } else {
    ok = mqtt.connect(kennung.c_str());
  }
  stand.verbunden = ok;
  stand.fehlercode = mqtt.state();
  Serial.printf("[MQTT] Verbindung zu %s:%u %s (Code %d)\n", e.mqtt_server.c_str(), e.mqtt_port,
                ok ? "hergestellt" : "fehlgeschlagen", stand.fehlercode);
}

// Strom und Leistungsfaktor bekommen das Vorzeichen der Phasenleistung
// (openWB: + Bezug, − Einspeisung). Der Zähler liefert den Strom ohne Vorzeichen.
static float mit_vorzeichen(float betrag, float leistung) {
  return leistung < 0 ? -fabsf(betrag) : fabsf(betrag);
}

static void werte_senden() {
  const Einstellungen& e = einstellungen();
  const Messwerte m = messwerte_lesen();

  if (!messwerte_aktuell(m, CACHE_MAX_ALTER_MS)) {
    if (e.mqtt_ersatz_an) {
      senden("W", String(e.mqtt_ersatz_w));
      stand.ersatz_gesendet++;
    }
    return;
  }

  senden("W", String((long)lroundf(m.leistung_w)));
  senden("WhImported", String(m.bezug_kwh * 1000.0f, 1));
  senden("WhExported", String(m.einspeisung_kwh * 1000.0f, 1));
  if (m.phasen_gueltig) {
    for (int p = 0; p < 3; p++) {
      const String nr(p + 1);
      senden(("APhase" + nr).c_str(), String(mit_vorzeichen(m.strom_a[p], m.leistung_phase_w[p]), 2));
      senden(("VPhase" + nr).c_str(), String(m.spannung_v[p], 1));
      senden(("WPhase" + nr).c_str(), String(m.leistung_phase_w[p], 0));
      if (m.pf_gueltig) {
        senden(("PfPhase" + nr).c_str(), String(mit_vorzeichen(m.leistungsfaktor, m.leistung_phase_w[p]), 3));
      }
    }
  }
  if (m.frequenz_gueltig) senden("HzFrequenz", String(m.frequenz_hz, 2));
  stand.gesendet++;
}

void mqtt_start() {
  const Einstellungen& e = einstellungen();
  stand.aktiv = e.mqtt_an && e.mqtt_server.length() > 0;
  if (!stand.aktiv) return;
  // PubSubClient merkt sich nur den Zeiger auf den Namen, deshalb eine eigene Kopie
  static String server_name;
  server_name = e.mqtt_server;
  mqtt.setServer(server_name.c_str(), e.mqtt_port);
  mqtt.setSocketTimeout(2);   // Sekunden; kurz, damit die Webseite nicht hängt
  netz_client.setTimeout(2);
}

void mqtt_schritt() {
  if (!stand.aktiv || !netz_heimnetz_verbunden()) {
    stand.verbunden = false;
    return;
  }
  if (!mqtt.connected()) {
    stand.verbunden = false;
    if (erster_versuch || (uint32_t)(millis() - letzter_versuch_ms) >= MQTT_NEUVERSUCH_MS) {
      erster_versuch = false;
      letzter_versuch_ms = millis();
      verbinden();
    }
    return;
  }
  stand.verbunden = true;
  mqtt.loop();
  if ((uint32_t)(millis() - letzter_takt_ms) >= MQTT_TAKT_MS) {
    letzter_takt_ms = millis();
    werte_senden();
    if (einstellungen().mqtt_speicher_an && einstellungen().anker_an) speicher_senden();
  }
}

MqttStand mqtt_stand() {
  return stand;
}
