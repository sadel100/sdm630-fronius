// solarapi.cpp
// ---------------------------------------------------------------------------
// Abfrage der Fronius Solar API (siehe solarapi.h).
//
// Antwort (gekürzt), Werte in W bzw. Wh, fehlende Werte sind null:
//   {"Body":{"Data":{"Site":{"P_PV":1802.3,"P_Grid":-1234.5,"P_Load":-567.8,
//    "E_Day":1234,"E_Total":5678000, …}}}, "Head":{"Status":{"Code":0}}}
// Der Datamanager kennt P_Grid und P_Load nur über den Netzzähler, also über
// diese Box. Verwendet wird deshalb vor allem P_PV und E_Total.
// ---------------------------------------------------------------------------
#include "solarapi.h"
#include "einstellungen.h"
#include "netz.h"
#include "konfiguration.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static SolarApiWerte     werte;
static SolarApiStatistik statistik;
static portMUX_TYPE      sperre = portMUX_INITIALIZER_UNLOCKED;

static void fehler_merken(const char* text) {
  portENTER_CRITICAL(&sperre);
  snprintf(statistik.letzter_fehler, sizeof(statistik.letzter_fehler), "%s", text);
  statistik.runden_fehler++;
  portEXIT_CRITICAL(&sperre);
}

// Zahl aus dem JSON, NAN wenn null oder nicht vorhanden.
static double zahl(JsonVariantConst v) {
  return v.is<double>() ? v.as<double>() : NAN;
}

static void runde() {
  const String adresse = "http://" + einstellungen().fronius_ip +
                         "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(SOLARAPI_TIMEOUT_MS);
  http.setConnectTimeout(SOLARAPI_TIMEOUT_MS);
  if (!http.begin(client, adresse)) {
    fehler_merken("Adresse ungültig");
    return;
  }
  const int code = http.GET();
  if (code != 200) {
    char text[48];
    if (code < 0) snprintf(text, sizeof(text), "keine Verbindung (%d)", code);
    else snprintf(text, sizeof(text), "HTTP-Fehler %d", code);
    fehler_merken(text);
    http.end();
    return;
  }
  // Nur die benötigten Felder lesen, das spart Speicher.
  JsonDocument filter;
  JsonObject site = filter["Body"]["Data"]["Site"].to<JsonObject>();
  site["P_PV"] = true;
  site["P_Grid"] = true;
  site["P_Load"] = true;
  site["E_Day"] = true;
  site["E_Total"] = true;
  filter["Head"]["Status"]["Code"] = true;
  JsonDocument doc;
  const DeserializationError f = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (f) {
    fehler_merken("Antwort unlesbar");
    return;
  }
  JsonVariantConst s = doc["Body"]["Data"]["Site"];
  if (s.isNull()) {
    fehler_merken("Antwort ohne Site-Daten");
    return;
  }
  SolarApiWerte w;
  w.pv_w      = zahl(s["P_PV"]);
  w.netz_w    = zahl(s["P_Grid"]);
  w.last_w    = zahl(s["P_Load"]);
  w.tag_wh    = zahl(s["E_Day"]);
  w.gesamt_wh = zahl(s["E_Total"]);
  w.gueltig = true;
  w.zeitstempel_ms = millis();
  portENTER_CRITICAL(&sperre);
  werte = w;
  statistik.runden_ok++;
  portEXIT_CRITICAL(&sperre);
}

static void aufgabe(void*) {
  for (;;) {
    if (netz_heimnetz_verbunden()) {
      runde();
    }
    vTaskDelay(pdMS_TO_TICKS(SOLARAPI_ABFRAGE_MS));
  }
}

void solarapi_start() {
  const Einstellungen& e = einstellungen();
  statistik.aktiv = e.solarapi_an && e.fronius_ip.length() > 0;
  if (!statistik.aktiv) return;
  xTaskCreatePinnedToCore(aufgabe, "solarapi", 6144, nullptr, 1, nullptr, 1);
}

SolarApiWerte solarapi_werte() {
  portENTER_CRITICAL(&sperre);
  const SolarApiWerte w = werte;
  portEXIT_CRITICAL(&sperre);
  return w;
}

SolarApiStatistik solarapi_statistik() {
  portENTER_CRITICAL(&sperre);
  const SolarApiStatistik s = statistik;
  portEXIT_CRITICAL(&sperre);
  return s;
}

bool solarapi_aktuell(const SolarApiWerte& w) {
  return w.gueltig && (uint32_t)(millis() - w.zeitstempel_ms) <= SOLARAPI_MAX_ALTER_MS;
}
