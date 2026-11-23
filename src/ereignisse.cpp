// ereignisse.cpp
// ---------------------------------------------------------------------------
// Ereignisprotokoll (siehe ereignisse.h).
//
// Die Überwachung vergleicht jede Sekunde den Zustand jedes Teils
// (antwortet / antwortet nicht). Ändert er sich und bleibt
// EREIGNIS_ENTPRELL_MS lang so, entsteht ein Eintrag.
// ---------------------------------------------------------------------------
#include "ereignisse.h"
#include "konfiguration.h"
#include "einstellungen.h"
#include "messwerte.h"
#include "anker.h"
#include "solarapi.h"
#include "fronius.h"
#include "mqtt_openwb.h"
#include "netz.h"
#include "uhrzeit.h"
#include <stdarg.h>
#include <esp_system.h>

static Ereignis     liste[EREIGNIS_ANZAHL];
static size_t       naechster = 0;     // Platz für den nächsten Eintrag
static size_t       anzahl = 0;
static portMUX_TYPE sperre = portMUX_INITIALIZER_UNLOCKED;
static uint32_t     letzter_takt_ms = 0;

void ereignis(bool stoerung, const char* format, ...) {
  Ereignis e;
  e.zeit = uhrzeit_jetzt();
  e.ms = millis();
  e.stoerung = stoerung;
  va_list args;
  va_start(args, format);
  vsnprintf(e.text, sizeof(e.text), format, args);
  va_end(args);
  portENTER_CRITICAL(&sperre);
  liste[naechster] = e;
  naechster = (naechster + 1) % EREIGNIS_ANZAHL;
  if (anzahl < EREIGNIS_ANZAHL) anzahl++;
  portEXIT_CRITICAL(&sperre);
  Serial.printf("[Ereignis] %s\n", e.text);
}

size_t ereignisse_lesen(Ereignis* ziel, size_t max) {
  portENTER_CRITICAL(&sperre);
  const size_t n = anzahl < max ? anzahl : max;
  for (size_t i = 0; i < n; i++) {
    ziel[i] = liste[(naechster + EREIGNIS_ANZAHL - 1 - i) % EREIGNIS_ANZAHL];
  }
  portEXIT_CRITICAL(&sperre);
  // Einträge aus der Zeit vor dem Stellen der Uhr nachträglich datieren
  const uint32_t jetzt = uhrzeit_jetzt();
  if (jetzt != 0) {
    const uint32_t ms = millis();
    for (size_t i = 0; i < n; i++) {
      if (ziel[i].zeit == 0) ziel[i].zeit = jetzt - (ms - ziel[i].ms) / 1000;
    }
  }
  return n;
}

static const char* neustart_grund() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  return "Einschalten (Strom war weg)";
    case ESP_RST_SW:       return "Neustart durch das Programm (Einstellungen, Update)";
    case ESP_RST_PANIC:    return "Absturz des Programms";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "Wächter (Programm hing)";
    case ESP_RST_BROWNOUT: return "Spannung zu niedrig (Netzteil?)";
    case ESP_RST_EXT:      return "Reset-Taster";
    case ESP_RST_DEEPSLEEP: return "Aufwachen";
    default:               return "unbekannter Grund";
  }
}

void ereignisse_start() {
  const esp_reset_reason_t g = esp_reset_reason();
  const bool schlecht = g == ESP_RST_PANIC || g == ESP_RST_INT_WDT || g == ESP_RST_TASK_WDT ||
                        g == ESP_RST_WDT || g == ESP_RST_BROWNOUT;
  ereignis(schlecht, "Start Version %s: %s", FIRMWARE_VERSION, neustart_grund());
}

// ---------------------------------------------------------------------------
// Ein überwachter Zustand mit Entprellung
// ---------------------------------------------------------------------------
struct Beobachter {
  const char* name;
  const char* gut_text;
  const char* schlecht_text;
  int8_t   gemeldet = -1;    // -1 noch nichts gemeldet, 0 schlecht, 1 gut
  int8_t   roh = -1;
  uint32_t seit_ms = 0;
};

static void pruefen(Beobachter& b, bool aktiv, bool gut) {
  if (!aktiv) {              // ausgeschaltet: vergessen, nichts melden
    b.gemeldet = -1;
    b.roh = -1;
    return;
  }
  const int8_t jetzt = gut ? 1 : 0;
  if (jetzt != b.roh) {
    b.roh = jetzt;
    b.seit_ms = millis();
  }
  // Gute Nachricht beim Start sofort, Ausfälle erst nach der Entprellzeit
  const uint32_t warten = (b.gemeldet == -1 && gut) ? 0 : EREIGNIS_ENTPRELL_MS;
  if (b.roh != b.gemeldet && (uint32_t)(millis() - b.seit_ms) >= warten) {
    b.gemeldet = b.roh;
    ereignis(!gut, "%s: %s", b.name, gut ? b.gut_text : b.schlecht_text);
  }
}

static Beobachter b_zaehler  {"Netzzähler SDM630", "antwortet", "antwortet nicht"};
static Beobachter b_anker    {"Solarbank", "antwortet", "antwortet nicht"};
static Beobachter b_solarapi {"Fronius Solar API", "antwortet", "antwortet nicht"};
static Beobachter b_wlan     {"Heimnetz", "verbunden", "getrennt"};
static Beobachter b_mqtt     {"openWB (MQTT)", "verbunden", "getrennt"};
static Beobachter b_fr_netz  {"Fronius", "fragt den Netzzähler ab", "fragt den Netzzähler nicht mehr ab"};
static Beobachter b_fr_zweit {"Fronius", "fragt den Solarbank-Zähler ab", "fragt den Solarbank-Zähler nicht ab"};

// Hat der Fronius einen Zähler in den letzten 30 s gefragt?
static bool gefragt(const FroniusZaehlerStatistik& s) {
  return s.letzte_anfrage_ms != 0 && (uint32_t)(millis() - s.letzte_anfrage_ms) < 30000;
}

void ereignisse_ueberwachen() {
  if ((uint32_t)(millis() - letzter_takt_ms) < EREIGNIS_TAKT_MS) return;
  letzter_takt_ms = millis();
  const Einstellungen& e = einstellungen();

  pruefen(b_zaehler, true, messwerte_aktuell(messwerte_lesen(), CACHE_MAX_ALTER_MS));
  pruefen(b_anker, anker_statistik().aktiv, anker_aktuell(anker_werte()));
  pruefen(b_solarapi, solarapi_statistik().aktiv, solarapi_aktuell(solarapi_werte()));
  pruefen(b_wlan, e.wlan_name.length() > 0, netz_heimnetz_verbunden());
  const MqttStand mq = mqtt_stand();
  pruefen(b_mqtt, mq.aktiv, mq.verbunden);

  const FroniusStatistik f = fronius_statistik();
  static bool erkennung_gemeldet = false;
  if (f.zustand == FroniusZustand::BEREIT) {
    if (!erkennung_gemeldet) {
      erkennung_gemeldet = true;
      ereignis(false, "Fronius-Bus: %lu Baud %s, Netzzähler Adresse %u", (unsigned long)f.baud,
               format_name(f.format), f.zaehler[0].adresse);
    }
    pruefen(b_fr_netz, true, gefragt(f.zaehler[0]));
    pruefen(b_fr_zweit, f.zaehler[1].adresse != 0, gefragt(f.zaehler[1]));
  }

  static bool uhr_gemeldet = false;
  if (!uhr_gemeldet && uhrzeit_bekannt()) {
    uhr_gemeldet = true;
    ereignis(false, "Uhrzeit vom Zeitserver gestellt");
  }
}
