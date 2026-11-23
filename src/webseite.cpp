// webseite.cpp
// ---------------------------------------------------------------------------
// Webseite und Schnittstelle für die Seite (JSON).
//
//   GET  /                  die Seite selbst (HTML unten in SEITE)
//   GET  /api/status        alle Werte und Zustände (JSON), die Seite fragt alle 2 s
//   GET  /api/einstellungen Einstellungen ohne Passwörter (JSON)
//   POST /api/einstellungen Einstellungen speichern, danach Neustart  (Passwort nötig)
//   POST /api/neustart      Neustart, im Automatik-Modus auch neue Erkennung (Passwort nötig)
//   GET  /api/wlan_suche    startet die Suche nach WLANs bzw. liefert das Ergebnis
//   GET  /api/ereignisse    Ereignisprotokoll (neueste zuerst)
//   GET  /api/energie       Tages- und Monatswerte
//   GET  /api/verlauf       Tagesverlauf (5-Minuten-Werte, 24 h)
//   GET  /api/diagnose      alles zusammen als Datei zum Weitergeben (ohne Passwörter)
//   GET  /api/update        Stand des Updates
//   POST /api/update/suchen      neueste Version auf GitHub suchen
//   POST /api/update/installieren   gefundene Version installieren (Passwort nötig)
//   POST /api/update/datei  Firmware-Datei hochladen und installieren (Passwort nötig)
//   POST /api/anmelden      nur Anmeldung prüfen (vor dem Hochladen)
//
// Passwörter werden nie an die Seite geschickt. Ein leeres Passwortfeld beim
// Speichern behält das gespeicherte Passwort.
// ---------------------------------------------------------------------------
#include "webseite.h"
#include "einstellungen.h"
#include "messwerte.h"
#include "zaehler.h"
#include "fronius.h"
#include "erkennung.h"
#include "netz.h"
#include "mqtt_openwb.h"
#include "anker.h"
#include "solarapi.h"
#include "energie.h"
#include "ereignisse.h"
#include "uhrzeit.h"
#include "firmware_update.h"
#include "konfiguration.h"
#include <WebServer.h>
#include <WiFi.h>

static WebServer server(80);
static uint32_t  neustart_um_ms = 0;   // 0 = kein Neustart geplant

// ---------------------------------------------------------------------------
// Kleine Helfer für JSON
// ---------------------------------------------------------------------------
static String json_text(const String& s) {
  String r = "\"";
  for (size_t i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c == '"' || c == '\\') { r += '\\'; r += c; }
    else if ((uint8_t)c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); r += b; }
    else r += c;
  }
  return r + "\"";
}

static String json_zahl(float v, int stellen) {
  if (isnan(v) || isinf(v)) return "null";
  return String(v, stellen);
}

static const char* jn(bool b) { return b ? "true" : "false"; }

static bool angemeldet() {
  if (server.authenticate(WEB_BENUTZER, einstellungen().ap_passwort.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "SDM72-Fronius",
                               "Bitte als admin mit dem Passwort des ESP32-WLANs anmelden");
  return false;
}

static String baud_text(uint32_t baud, uint32_t format) {
  return String(baud) + ":" + format_name(format);
}

static bool baud_lesen(const String& text, uint32_t& baud, uint32_t& format) {
  // "0" = automatisch, sonst "9600:8N1"
  if (text == "0") { baud = 0; format = SERIAL_8N1; return true; }
  const int p = text.indexOf(':');
  if (p < 0) return false;
  baud = text.substring(0, p).toInt();
  const String f = text.substring(p + 1);
  if (f == "8N1") format = SERIAL_8N1;
  else if (f == "8E1") format = SERIAL_8E1;
  else if (f == "8O1") format = SERIAL_8O1;
  else if (f == "8N2") format = SERIAL_8N2;
  else return false;
  return baud >= 1200 && baud <= 115200;
}

// ---------------------------------------------------------------------------
// GET /api/status
// ---------------------------------------------------------------------------
static String status_json() {
  const Messwerte m = messwerte_lesen();
  const ZaehlerStatistik z = zaehler_statistik();
  const FroniusStatistik f = fronius_statistik();
  const ErkennungStand er = erkennung_stand();
  const MqttStand mq = mqtt_stand();
  const Einstellungen& e = einstellungen();
  const uint32_t jetzt = millis();

  String j;
  j.reserve(7168);
  j += "{\"laufzeit_s\":" + String(jetzt / 1000);
  j += ",\"version\":" + json_text(FIRMWARE_VERSION);
  j += ",\"speicher_frei\":" + String(ESP.getFreeHeap());

  // Zähler
  j += ",\"zaehler\":{\"gueltig\":"; j += jn(m.gueltig);
  j += ",\"aktuell\":"; j += jn(messwerte_aktuell(m, CACHE_MAX_ALTER_MS));
  j += ",\"alter_ms\":" + String(m.gueltig ? jetzt - m.zeitstempel_ms : 0);
  j += ",\"leistung_w\":" + json_zahl(m.leistung_w, 0);
  j += ",\"bezug_kwh\":" + json_zahl(m.bezug_kwh, 2);
  j += ",\"einspeisung_kwh\":" + json_zahl(m.einspeisung_kwh, 2);
  j += ",\"phasen\":";
  if (m.phasen_gueltig) {
    j += "[";
    for (int p = 0; p < 3; p++) {
      if (p) j += ",";
      j += "{\"u\":" + json_zahl(m.spannung_v[p], 1) + ",\"i\":" + json_zahl(m.strom_a[p], 2) +
           ",\"p\":" + json_zahl(m.leistung_phase_w[p], 0) + "}";
    }
    j += "]";
  } else {
    j += "null";
  }
  j += ",\"zusatz\":"; j += jn(m.roh_b_ok && m.roh_c_ok);
  j += ",\"hz\":" + (m.frequenz_gueltig ? json_zahl(m.frequenz_hz, 2) : String("null"));
  j += ",\"pf\":" + (m.pf_gueltig ? json_zahl(m.leistungsfaktor, 3) : String("null"));
  j += ",\"runden_ok\":" + String(z.runden_ok);
  j += ",\"runden_fehler\":" + String(z.runden_fehler);
  j += ",\"fehler\":" + json_text(z.letzter_fehler);
  j += ",\"baud\":" + String(e.zaehler_baud);
  j += ",\"format\":" + json_text(format_name(e.zaehler_format));
  j += ",\"adresse\":" + String(e.zaehler_adresse);
  j += "}";

  // Fronius
  j += ",\"fronius\":{\"bereit\":"; j += jn(f.zustand == FroniusZustand::BEREIT);
  j += ",\"automatik\":"; j += jn(e.fronius_baud == 0 || e.fronius_adresse == 0);
  j += ",\"erkennung\":{\"baud\":" + String(er.baud) + ",\"format\":" + json_text(format_name(er.format)) +
       ",\"durchlaeufe\":" + String(er.durchlaeufe) + ",\"bytes\":" + String(er.bytes_gesehen) + "}";
  j += ",\"baud\":" + String(f.baud);
  j += ",\"format\":" + json_text(format_name(f.format));
  j += ",\"zaehler\":[";
  for (int z = 0; z < 2; z++) {
    const FroniusZaehlerStatistik& s = f.zaehler[z];
    if (z) j += ",";
    j += "{\"adresse\":" + String(s.adresse) + ",\"beantwortet\":" + String(s.beantwortet) +
         ",\"abgelehnt\":" + String(s.abgelehnt) + ",\"ohne_antwort\":" + String(s.ohne_antwort) +
         ",\"sunspec\":" + String(s.sunspec_anfragen) + ",\"eastron\":" + String(s.eastron_anfragen) +
         ",\"seit_s\":" + String(s.letzte_anfrage_ms ? (long)((jetzt - s.letzte_anfrage_ms) / 1000) : -1L) + "}";
  }
  j += "]";
  j += ",\"seit_letzter_s\":" + String(f.zustand == FroniusZustand::BEREIT && f.letzte_anfrage_ms
                                            ? (jetzt - f.letzte_anfrage_ms) / 1000 : 0);
  j += ",\"modell\":" + String(e.sunspec_modell);
  j += ",\"vorzeichen\":" + String(e.vorzeichen);
  j += ",\"an_fronius_w\":" + json_zahl(e.vorzeichen * m.leistung_w, 0);
  j += ",\"zweit_w\":" + json_zahl(fronius_zweit_leistung_w(), 0);
  static FroniusAnfrage liste[PROTOKOLL_EINTRAEGE];
  const size_t n = fronius_protokoll(liste, PROTOKOLL_EINTRAEGE);
  j += ",\"protokoll\":[";
  for (size_t i = 0; i < n; i++) {
    const FroniusAnfrage& a = liste[i];
    if (i) j += ",";
    j += "{\"vor_s\":" + json_zahl((jetzt - a.zeit_ms) / 1000.0f, 1) + ",\"z\":" + String(a.zaehler) +
         ",\"fc\":" + String(a.funktion) +
         ",\"start\":" + String(a.start) + ",\"anzahl\":" + String(a.anzahl) +
         ",\"bereich\":" + json_text(bereich_name(a.bereich)) +
         ",\"ergebnis\":" + json_text(ergebnis_name(a.ergebnis)) + "}";
  }
  j += "]}";

  // Anker Solarbank
  {
    const AnkerWerte a = anker_werte();
    const AnkerStatistik as = anker_statistik();
    j += ",\"anker\":{\"aktiv\":"; j += jn(as.aktiv);
    j += ",\"gueltig\":"; j += jn(a.gueltig);
    j += ",\"aktuell\":"; j += jn(anker_aktuell(a));
    j += ",\"alter_s\":" + String(a.gueltig ? (jetzt - a.zeitstempel_ms) / 1000 : 0);
    j += ",\"soc\":" + String(a.soc);
    j += ",\"batterie_w\":" + String(a.batterie_w);
    j += ",\"pv_w\":" + String(a.pv_w);
    j += ",\"ac_w\":" + String(a.ac_w);
    j += ",\"last_w\":" + String(a.last_w);
    j += ",\"netz_w\":" + String(a.netz_w);
    j += ",\"status\":" + String(a.status);
    j += ",\"lade_kwh\":" + json_zahl(a.lade_kwh, 1);
    j += ",\"entlade_kwh\":" + json_zahl(a.entlade_kwh, 1);
    j += ",\"kapazitaet_kwh\":" + json_zahl(a.kapazitaet_kwh, 1);
    j += ",\"runden_ok\":" + String(as.runden_ok);
    j += ",\"runden_fehler\":" + String(as.runden_fehler);
    j += ",\"fehler\":" + json_text(as.letzter_fehler);
    j += ",\"modell\":" + json_text(as.modell);
    j += ",\"ip\":" + json_text(e.anker_ip);
    j += "}";
  }

  // WLAN
  j += ",\"wlan\":{\"name\":" + json_text(e.wlan_name);
  j += ",\"verbunden\":"; j += jn(netz_heimnetz_verbunden());
  j += ",\"ip\":" + json_text(netz_heimnetz_ip());
  j += ",\"dbm\":" + String(netz_heimnetz_verbunden() ? netz_empfang_dbm() : 0);
  j += ",\"ap_name\":" + json_text(AP_NAME);
  j += ",\"ap_ip\":" + json_text(netz_ap_ip());
  j += "}";

  // MQTT
  j += ",\"mqtt\":{\"aktiv\":"; j += jn(mq.aktiv);
  j += ",\"verbunden\":"; j += jn(mq.verbunden);
  j += ",\"code\":" + String(mq.fehlercode);
  j += ",\"gesendet\":" + String(mq.gesendet);
  j += ",\"ersatz\":" + String(mq.ersatz_gesendet);
  j += ",\"speicher_an\":"; j += jn(e.mqtt_speicher_an && e.anker_an);
  j += ",\"speicher_gesendet\":" + String(mq.speicher_gesendet);
  j += ",\"speicher_ersatz\":" + String(mq.speicher_ersatz);
  j += ",\"server\":" + json_text(e.mqtt_server);
  j += "}";

  // Fronius Solar API
  {
    const SolarApiWerte s = solarapi_werte();
    const SolarApiStatistik st = solarapi_statistik();
    j += ",\"solarapi\":{\"aktiv\":"; j += jn(st.aktiv);
    j += ",\"aktuell\":"; j += jn(solarapi_aktuell(s));
    j += ",\"alter_s\":" + String(s.gueltig ? (jetzt - s.zeitstempel_ms) / 1000 : 0);
    j += ",\"pv_w\":" + json_zahl(s.pv_w, 0);
    j += ",\"netz_w\":" + json_zahl(s.netz_w, 0);
    j += ",\"tag_kwh\":" + json_zahl(s.tag_wh / 1000.0f, 2);
    j += ",\"gesamt_kwh\":" + json_zahl(s.gesamt_wh / 1000.0, 1);
    j += ",\"runden_ok\":" + String(st.runden_ok);
    j += ",\"runden_fehler\":" + String(st.runden_fehler);
    j += ",\"fehler\":" + json_text(st.letzter_fehler);
    j += ",\"ip\":" + json_text(e.fronius_ip);
    j += "}";
  }

  // Leistungsfluss
  {
    const Fluss fl = energie_fluss();
    j += ",\"fluss\":{\"netz\":" + json_zahl(fl.netz_w, 0);
    j += ",\"pv\":" + json_zahl(fl.pv_symo_w, 0);
    j += ",\"bank\":" + json_zahl(fl.solarbank_w, 0);
    j += ",\"bank_laden\":" + json_zahl(fl.solarbank_laden_w, 0);
    j += ",\"bank_pv\":" + json_zahl(fl.solarbank_pv_w, 0);
    j += ",\"soc\":" + json_zahl(fl.soc, 0);
    j += ",\"haus\":" + json_zahl(fl.haus_w, 0);
    j += ",\"pv_aktiv\":"; j += jn(fl.solarapi_aktiv);
    j += ",\"bank_aktiv\":"; j += jn(fl.solarbank_aktiv);
    j += "}";
  }

  // Uhrzeit und Update
  {
    const uint32_t zeit = uhrzeit_jetzt();
    char text[24];
    uhrzeit_text(zeit, text, sizeof(text));
    j += ",\"zeit\":" + String(zeit) + ",\"zeit_text\":" + json_text(text);
    j += ",\"probezeit\":"; j += jn(update_stand().probezeit);
  }
  j += "}";
  return j;
}

static void status_senden() {
  server.send(200, "application/json", status_json());
}

// ---------------------------------------------------------------------------
// GET/POST /api/einstellungen
// ---------------------------------------------------------------------------
static String einstellungen_json() {
  const Einstellungen& e = einstellungen();
  String j = "{";
  j += "\"zaehler_baud\":" + json_text(baud_text(e.zaehler_baud, e.zaehler_format));
  j += ",\"zaehler_adresse\":" + String(e.zaehler_adresse);
  j += ",\"fronius_baud\":" + json_text(e.fronius_baud == 0 ? String("0") : baud_text(e.fronius_baud, e.fronius_format));
  j += ",\"fronius_adresse\":" + String(e.fronius_adresse);
  j += ",\"modell\":" + String(e.sunspec_modell);
  j += ",\"vorzeichen\":" + String(e.vorzeichen);
  j += ",\"zweit_adresse\":" + String(e.zweit_adresse);
  j += ",\"anker_an\":"; j += jn(e.anker_an);
  j += ",\"anker_ip\":" + json_text(e.anker_ip);
  j += ",\"anker_port\":" + String(e.anker_port);
  j += ",\"anker_unit\":" + String(e.anker_unit);
  j += ",\"mqtt_speicher_an\":"; j += jn(e.mqtt_speicher_an);
  j += ",\"wlan_name\":" + json_text(e.wlan_name);
  j += ",\"wlan_passwort_gesetzt\":"; j += jn(e.wlan_passwort.length() > 0);
  j += ",\"mqtt_an\":"; j += jn(e.mqtt_an);
  j += ",\"mqtt_server\":" + json_text(e.mqtt_server);
  j += ",\"mqtt_port\":" + String(e.mqtt_port);
  j += ",\"mqtt_benutzer\":" + json_text(e.mqtt_benutzer);
  j += ",\"mqtt_passwort_gesetzt\":"; j += jn(e.mqtt_passwort.length() > 0);
  j += ",\"ersatz_an\":"; j += jn(e.mqtt_ersatz_an);
  j += ",\"ersatz_w\":" + String(e.mqtt_ersatz_w);
  j += ",\"solarapi_an\":"; j += jn(e.solarapi_an);
  j += ",\"fronius_ip\":" + json_text(e.fronius_ip);
  j += "}";
  return j;
}

static void einstellungen_senden() {
  server.send(200, "application/json", einstellungen_json());
}

static void fehler(const String& text) {
  server.send(400, "text/plain; charset=utf-8", text);
}

static void einstellungen_empfangen() {
  if (!angemeldet()) return;
  Einstellungen neu = einstellungen();

  if (!baud_lesen(server.arg("zaehler_baud"), neu.zaehler_baud, neu.zaehler_format) || neu.zaehler_baud == 0)
    return fehler("Baudrate Zähler ungültig");
  const long za = server.arg("zaehler_adresse").toInt();
  if (za < 1 || za > 247) return fehler("Adresse Zähler muss 1–247 sein");
  neu.zaehler_adresse = za;

  if (!baud_lesen(server.arg("fronius_baud"), neu.fronius_baud, neu.fronius_format))
    return fehler("Baudrate Fronius ungültig");
  const long fa = server.arg("fronius_adresse").toInt();
  if (fa < 0 || fa > 247) return fehler("Adresse Fronius muss 0 (automatisch) oder 1–247 sein");
  neu.fronius_adresse = fa;

  const long modell = server.arg("modell").toInt();
  if (modell != 213 && modell != 203) return fehler("SunSpec-Modell muss 213 oder 203 sein");
  neu.sunspec_modell = modell;
  neu.vorzeichen = server.arg("vorzeichen").toInt() < 0 ? -1 : 1;
  const long za2 = server.arg("zweit_adresse").toInt();
  if (za2 < 0 || za2 > 247) return fehler("Adresse zweiter Zähler muss 0 (aus) oder 1–247 sein");
  if (za2 != 0 && za2 == neu.fronius_adresse) return fehler("Die beiden Zähler brauchen verschiedene Adressen");
  neu.zweit_adresse = za2;

  neu.anker_an = server.arg("anker_an") == "1";
  neu.anker_ip = server.arg("anker_ip");
  neu.anker_ip.trim();
  if (neu.anker_an && neu.anker_ip.length() == 0) return fehler("Solarbank: IP-Adresse fehlt");
  const long ap_port = server.arg("anker_port").toInt();
  if (ap_port < 1 || ap_port > 65535) return fehler("Solarbank: Port ungültig");
  neu.anker_port = ap_port;
  const long au = server.arg("anker_unit").toInt();
  if (au < 0 || au > 247) return fehler("Solarbank: Unit-ID muss 0–247 sein");
  neu.anker_unit = au;

  neu.wlan_name = server.arg("wlan_name");
  neu.wlan_name.trim();
  if (server.arg("wlan_passwort").length() > 0) neu.wlan_passwort = server.arg("wlan_passwort");
  if (neu.wlan_name.length() == 0) neu.wlan_passwort = "";

  const String ap = server.arg("ap_passwort");
  if (ap.length() > 0) {
    if (ap.length() < 8 || ap.length() > 63) return fehler("Passwort des ESP32-WLANs: 8–63 Zeichen");
    neu.ap_passwort = ap;
  }

  neu.mqtt_an = server.arg("mqtt_an") == "1";
  neu.mqtt_server = server.arg("mqtt_server");
  neu.mqtt_server.trim();
  const long port = server.arg("mqtt_port").toInt();
  if (port < 1 || port > 65535) return fehler("MQTT-Port ungültig");
  neu.mqtt_port = port;
  neu.mqtt_benutzer = server.arg("mqtt_benutzer");
  if (server.arg("mqtt_passwort").length() > 0) neu.mqtt_passwort = server.arg("mqtt_passwort");
  if (neu.mqtt_benutzer.length() == 0) neu.mqtt_passwort = "";
  if (neu.mqtt_an && neu.mqtt_server.length() == 0) return fehler("MQTT: Adresse der openWB fehlt");
  neu.mqtt_ersatz_an = server.arg("ersatz_an") == "1";
  const long ew = server.arg("ersatz_w").toInt();
  if (ew < 0 || ew > 100000) return fehler("Ersatzwert muss 0–100000 W sein");
  neu.mqtt_ersatz_w = ew;
  neu.mqtt_speicher_an = server.arg("mqtt_speicher_an") == "1";
  neu.solarapi_an = server.arg("solarapi_an") == "1";
  neu.fronius_ip = server.arg("fronius_ip");
  neu.fronius_ip.trim();
  if (neu.solarapi_an && neu.fronius_ip.length() == 0) return fehler("Solar API: Adresse des Fronius fehlt");

  einstellungen_speichern(neu);
  server.send(200, "text/plain; charset=utf-8", "Gespeichert. Der ESP32 startet neu.");
  neustart_um_ms = millis() + 1000;
  if (neustart_um_ms == 0) neustart_um_ms = 1;
}

static void neustart_empfangen() {
  if (!angemeldet()) return;
  server.send(200, "text/plain; charset=utf-8", "Der ESP32 startet neu.");
  neustart_um_ms = millis() + 1000;
  if (neustart_um_ms == 0) neustart_um_ms = 1;
}

// ---------------------------------------------------------------------------
// GET /api/wlan_suche
// ---------------------------------------------------------------------------
static void wlan_suche() {
  const int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {         // noch keine Suche: starten
    WiFi.scanNetworks(true);
    server.send(200, "application/json", "{\"laeuft\":true}");
    return;
  }
  if (n == WIFI_SCAN_RUNNING) {
    server.send(200, "application/json", "{\"laeuft\":true}");
    return;
  }
  String j = "{\"laeuft\":false,\"netze\":[";
  for (int i = 0; i < n; i++) {
    if (i) j += ",";
    j += "{\"name\":" + json_text(WiFi.SSID(i)) + ",\"dbm\":" + String(WiFi.RSSI(i)) + "}";
  }
  j += "]}";
  WiFi.scanDelete();                    // nächste Anfrage startet eine neue Suche
  server.send(200, "application/json", j);
}

// ---------------------------------------------------------------------------
// Ereignisse, Energie, Verlauf, Diagnose-Datei
// ---------------------------------------------------------------------------
static String ereignisse_json() {
  static Ereignis liste[EREIGNIS_ANZAHL];
  const size_t n = ereignisse_lesen(liste, EREIGNIS_ANZAHL);
  const uint32_t ms = millis();
  String j;
  j.reserve(n * 110 + 16);
  j += "[";
  for (size_t i = 0; i < n; i++) {
    if (i) j += ",";
    char zeit[24];
    uhrzeit_text(liste[i].zeit, zeit, sizeof(zeit));
    j += "{\"zeit\":" + json_text(liste[i].zeit ? zeit : "") + ",\"vor_s\":" + String((ms - liste[i].ms) / 1000) +
         ",\"stoerung\":" + jn(liste[i].stoerung) + ",\"text\":" + json_text(liste[i].text) + "}";
  }
  j += "]";
  return j;
}

static void summe_json(String& j, const EnergieSumme& s) {
  j += "{\"kennung\":" + String(s.kennung) + ",\"kwh\":[";
  for (int i = 0; i < E_ANZAHL; i++) {
    if (i) j += ",";
    j += json_zahl(s.kwh[i], 2);
  }
  j += "]}";
}

static String energie_json() {
  const EnergieStand st = energie_stand();
  String j = "{\"uhrzeit\":";
  j += jn(st.uhrzeit);
  j += ",\"heute\":"; summe_json(j, st.heute);
  j += ",\"gestern\":"; summe_json(j, st.gestern);
  j += ",\"monat\":"; summe_json(j, st.monat);
  j += ",\"monate\":[";
  bool erster = true;
  for (const EnergieSumme& m : st.monate) {
    if (m.kennung == 0) continue;
    if (!erster) j += ",";
    erster = false;
    summe_json(j, m);
  }
  j += "]}";
  return j;
}

static String verlauf_json() {
  static VerlaufPunkt punkte[VERLAUF_PUNKTE];
  const size_t n = energie_verlauf(punkte, VERLAUF_PUNKTE);
  const uint32_t ms = millis();
  String j;
  j.reserve(n * 40 + 64);
  j += "{\"zeit\":" + String(uhrzeit_jetzt()) + ",\"punkt_s\":" + String(VERLAUF_PUNKT_S) + ",\"punkte\":[";
  auto w = [](int16_t v) { return v == VERLAUF_LEER ? String("null") : String(v); };
  for (size_t i = 0; i < n; i++) {
    const VerlaufPunkt& p = punkte[i];
    if (i) j += ",";
    j += "[" + String((ms - p.ms) / 1000) + "," + w(p.netz) + "," + w(p.pv_symo) + "," + w(p.solarbank) + "," +
         w(p.haus) + "," + (p.soc < 0 ? String("null") : String(p.soc)) + "]";
  }
  j += "]}";
  return j;
}

static void ereignisse_senden() { server.send(200, "application/json", ereignisse_json()); }
static void energie_senden() { server.send(200, "application/json", energie_json()); }
static void verlauf_senden() { server.send(200, "application/json", verlauf_json()); }

// Alles in einer Datei, die der Betreiber per Mail weitergeben kann.
// Enthält keine Passwörter (die Einstellungen kommen wie auf der Webseite).
static void diagnose_senden() {
  char name[64];
  const uint32_t zeit = uhrzeit_jetzt();
  if (zeit) {
    struct tm t;
    uhrzeit_lokal(zeit, t);
    strftime(name, sizeof(name), "sdm630-diagnose-%Y%m%d-%H%M.json", &t);
  } else {
    snprintf(name, sizeof(name), "sdm630-diagnose.json");
  }
  String j;
  j.reserve(24000);
  j += "{\"geraet\":\"SDM630-Fronius\",\"status\":" + status_json();
  j += ",\"einstellungen\":" + einstellungen_json();
  j += ",\"ereignisse\":" + ereignisse_json();
  j += ",\"energie\":" + energie_json();
  j += ",\"verlauf\":" + verlauf_json() + "}";
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + name + "\"");
  server.send(200, "application/json", j);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
static const char* zustand_name(UpdateZustand z) {
  switch (z) {
    case UpdateZustand::RUHE:     return "ruhe";
    case UpdateZustand::SUCHT:    return "sucht";
    case UpdateZustand::AKTUELL:  return "aktuell";
    case UpdateZustand::GEFUNDEN: return "gefunden";
    case UpdateZustand::LAEDT:    return "laedt";
    case UpdateZustand::FERTIG:   return "fertig";
    default:                      return "fehler";
  }
}

static void update_senden() {
  const UpdateStand u = update_stand();
  String j = "{\"zustand\":" + json_text(zustand_name(u.zustand));
  j += ",\"version\":" + json_text(FIRMWARE_VERSION);
  j += ",\"neue_version\":" + json_text(u.neue_version);
  j += ",\"beschreibung\":" + json_text(u.beschreibung);
  j += ",\"prozent\":" + String(u.prozent);
  j += ",\"meldung\":" + json_text(u.meldung);
  j += ",\"probezeit\":"; j += jn(u.probezeit);
  j += ",\"repo\":" + json_text(String(GITHUB_BESITZER) + "/" + GITHUB_REPO);
  j += "}";
  server.send(200, "application/json", j);
}

static void update_suchen_empfangen() {
  update_suchen();
  update_senden();
}

static void update_installieren_empfangen() {
#ifndef TEST_UPLOAD_OHNE_ANMELDUNG
  if (!angemeldet()) return;
#endif
  if (!update_installieren()) return fehler("Keine gefundene Version zum Installieren.");
  update_senden();
}

static void anmelden_empfangen() {
  if (!angemeldet()) return;
  server.send(200, "text/plain; charset=utf-8", "ok");
}

// Datei-Upload: Die Daten kommen in Teilen (upload_teil), danach die Antwort
// (upload_fertig). Ohne Anmeldung wird nichts geschrieben.
static const char* upload_fehler = nullptr;
static bool        upload_erlaubt = false;

static void upload_teil() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    upload_fehler = nullptr;
#ifdef TEST_UPLOAD_OHNE_ANMELDUNG
    upload_erlaubt = true;   // NUR für Tests am PC (Bauschalter), nie in der Anlage
#else
    upload_erlaubt = server.authenticate(WEB_BENUTZER, einstellungen().ap_passwort.c_str());
#endif
    if (!upload_erlaubt) return;
    Serial.printf("[Update] Datei %s wird hochgeladen\n", u.filename.c_str());
    upload_fehler = update_datei_anfang();
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (upload_erlaubt && upload_fehler == nullptr) upload_fehler = update_datei_teil(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END) {
    if (upload_erlaubt && upload_fehler == nullptr) upload_fehler = update_datei_ende();
    else if (upload_erlaubt) update_datei_abbrechen();
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    if (upload_erlaubt) update_datei_abbrechen();
    upload_fehler = "Hochladen abgebrochen.";
  }
}

static void upload_fertig() {
  if (!upload_erlaubt) {
    server.requestAuthentication(BASIC_AUTH, "SDM72-Fronius",
                                 "Bitte als admin mit dem Passwort des ESP32-WLANs anmelden");
    return;
  }
  if (upload_fehler != nullptr) return fehler(upload_fehler);
  server.send(200, "text/plain; charset=utf-8", "Datei geschrieben. Der ESP32 startet neu …");
}

// ---------------------------------------------------------------------------
// Die Seite
// ---------------------------------------------------------------------------
// Die Seite selbst steht in seite.h (HTML, CSS und JavaScript in einem Stück).
#include "seite.h"

static void seite_senden() {
  server.send_P(200, "text/html; charset=utf-8", SEITE);
}

void webseite_start() {
  server.on("/", HTTP_GET, seite_senden);
  server.on("/api/status", HTTP_GET, status_senden);
  server.on("/api/einstellungen", HTTP_GET, einstellungen_senden);
  server.on("/api/einstellungen", HTTP_POST, einstellungen_empfangen);
  server.on("/api/neustart", HTTP_POST, neustart_empfangen);
  server.on("/api/wlan_suche", HTTP_GET, wlan_suche);
  server.on("/api/ereignisse", HTTP_GET, ereignisse_senden);
  server.on("/api/energie", HTTP_GET, energie_senden);
  server.on("/api/verlauf", HTTP_GET, verlauf_senden);
  server.on("/api/diagnose", HTTP_GET, diagnose_senden);
  server.on("/api/update", HTTP_GET, update_senden);
  server.on("/api/update/suchen", HTTP_POST, update_suchen_empfangen);
  server.on("/api/update/installieren", HTTP_POST, update_installieren_empfangen);
  server.on("/api/update/datei", HTTP_POST, upload_fertig, upload_teil);
  server.on("/api/anmelden", HTTP_POST, anmelden_empfangen);
  server.onNotFound([]() { server.send(404, "text/plain", "nicht gefunden"); });
  server.begin();
}

void webseite_schritt() {
  server.handleClient();
  if (neustart_um_ms != 0 && (int32_t)(millis() - neustart_um_ms) >= 0) {
    Serial.println(F("[Web] Neustart"));
    delay(100);
    ESP.restart();
  }
}
