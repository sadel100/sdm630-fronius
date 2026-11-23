// firmware_update.cpp
// ---------------------------------------------------------------------------
// Firmware-Update per Datei oder von GitHub (siehe firmware_update.h).
//
// Technik:
//  - Suche: HTTPS-Client aus dem ESP-IDF (esp_http_client). Die eingebaute
//    Zertifikatssammlung (esp_crt_bundle_attach) prüft, dass wirklich GitHub
//    antwortet. Kein Schlüssel: das Repository ist öffentlich.
//  - Installieren: esp_https_ota lädt die Datei und schreibt sie in den
//    zweiten Programmspeicher. GitHub leitet den Download auf einen anderen
//    Server um, dem folgt der Client von selbst.
//  - Datei-Upload: Arduino-Bibliothek Update.
//  - Am Ende prüft das ESP-IDF das ganze Programmbild (Prüfsumme), bevor es
//    beim nächsten Start benutzt wird.
// ---------------------------------------------------------------------------
#include "firmware_update.h"
#include "konfiguration.h"
#include "ereignisse.h"
#include <WiFi.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_netif.h>
#include <lwip/netdb.h>

// Der Arduino-Core fragt beim Start, ob die neue Version erst später als gut
// gilt. true: Wir markieren sie selbst nach UPDATE_GUT_NACH_MS. Stürzt sie
// vorher ab, startet der Bootloader wieder die alte Version.
extern "C" bool verifyRollbackLater() { return true; }

static constexpr int      HTTP_TIMEOUT_MS = 15000;
static constexpr size_t   JSON_MAX = 24 * 1024;      // Antwort von GitHub
static constexpr size_t   ADRESSE_MAX = 512;
static constexpr uint32_t NEUSTART_NACH_MS = 1500;
static constexpr uint8_t  BILD_KENNUNG = 0xE9;      // erstes Byte jeder ESP-Firmware
static constexpr uint16_t CHIP_ESP32 = 0;           // Chip-Kennung im Kopf (Byte 12/13)

static UpdateStand  stand;
static portMUX_TYPE sperre = portMUX_INITIALIZER_UNLOCKED;
static volatile bool aufgabe_laeuft = false;
static char         download_adresse[ADRESSE_MAX] = "";
static uint32_t     download_groesse = 0;
static uint32_t     neustart_um_ms = 0;
static bool         gut_markiert = true;
static bool         datei_erster_teil = true;

// ------------------------------------------------------------------ Stand
template <typename F>
static void aendern(F f) {
  portENTER_CRITICAL(&sperre);
  f(stand);
  portEXIT_CRITICAL(&sperre);
}

static void setze(UpdateZustand z, const char* meldung) {
  aendern([&](UpdateStand& s) {
    s.zustand = z;
    snprintf(s.meldung, sizeof(s.meldung), "%s", meldung);
  });
}

UpdateStand update_stand() {
  portENTER_CRITICAL(&sperre);
  UpdateStand s = stand;
  portEXIT_CRITICAL(&sperre);
  return s;
}

// ------------------------------------------------------------------ Versionen
// "v1.3.0", "1.3.0" -> drei Zahlen
static bool version_lesen(const char* text, int v[3]) {
  if (*text == 'v' || *text == 'V') ++text;
  return sscanf(text, "%d.%d.%d", &v[0], &v[1], &v[2]) == 3;
}

static bool ist_neuer(const int neu[3], const int alt[3]) {
  for (int i = 0; i < 3; i++) {
    if (neu[i] != alt[i]) return neu[i] > alt[i];
  }
  return false;
}

// ------------------------------------------------------------------ Start, Probezeit
void update_start() {
  // Läuft eine frisch installierte Version, die noch geprüft werden muss?
  esp_ota_img_states_t zustand;
  const esp_partition_t* laufend = esp_ota_get_running_partition();
  if (esp_ota_get_state_partition(laufend, &zustand) == ESP_OK && zustand == ESP_OTA_IMG_PENDING_VERIFY) {
    gut_markiert = false;
    aendern([](UpdateStand& s) { s.probezeit = true; });
  }
  // Versionswechsel melden (auch nach einem Rückfall auf die alte Version)
  Preferences p;
  p.begin("update", false);
  const String vorher = p.isKey("version") ? p.getString("version", "") : String("");
  if (vorher.length() > 0 && vorher != FIRMWARE_VERSION) {
    int alt[3], neu[3];
    const bool zurueck = version_lesen(vorher.c_str(), alt) && version_lesen(FIRMWARE_VERSION, neu) &&
                         ist_neuer(alt, neu);
    ereignis(zurueck, zurueck ? "Firmware zurück auf %s (vorher %s)" : "Firmware %s installiert (vorher %s)",
             FIRMWARE_VERSION, vorher.c_str());
  }
  if (vorher != FIRMWARE_VERSION) p.putString("version", FIRMWARE_VERSION);
  p.end();
}

void update_schritt() {
  if (!gut_markiert && millis() >= UPDATE_GUT_NACH_MS) {
    gut_markiert = true;
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      ereignis(false, "Neue Version läuft seit %lu s fehlerfrei und bleibt",
               (unsigned long)(UPDATE_GUT_NACH_MS / 1000));
    }
    aendern([](UpdateStand& s) { s.probezeit = false; });
  }
  if (neustart_um_ms != 0 && (int32_t)(millis() - neustart_um_ms) >= 0) {
    Serial.println(F("[Update] Neustart mit der neuen Version"));
    delay(100);
    ESP.restart();
  }
}

static void neustart_planen() {
  neustart_um_ms = millis() + NEUSTART_NACH_MS;
  if (neustart_um_ms == 0) neustart_um_ms = 1;
}

// ------------------------------------------------------------------ Namensauflösung
// Erfahrung aus dem Puffer-Projekt: Manche Router melden nach einer Weile nur
// noch IPv6-Namensserver, über die der ESP32 nicht hinauskommt. Dann den
// IPv4-Namensserver (bzw. den Router) wieder einsetzen. GitHub hat nur IPv4.
static void dns_v4_einsetzen() {
  const IPAddress dns = WiFi.dnsIP(0);
  if (dns.type() == IPv4 && (uint32_t)dns != 0) return;
  const uint32_t ziel = (uint32_t)WiFi.gatewayIP();
  if (ziel == 0) return;
  esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif == nullptr) return;
  esp_netif_dns_info_t info = {};
  info.ip.type = ESP_IPADDR_TYPE_V4;
  info.ip.u_addr.ip4.addr = ziel;
  esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &info);
}

// Namen vorab nur über IPv4 auflösen, das Ergebnis merkt sich lwIP.
static bool ipv4_vorab(const char* adresse) {
  char rechner[128];
  const char* anfang = strstr(adresse, "://");
  anfang = anfang ? anfang + 3 : adresse;
  size_t n = strcspn(anfang, "/:?");
  if (n >= sizeof(rechner)) n = sizeof(rechner) - 1;
  memcpy(rechner, anfang, n);
  rechner[n] = '\0';
  for (int versuch = 0; versuch < 3; versuch++) {
    struct addrinfo hinweise = {};
    hinweise.ai_family = AF_INET;
    hinweise.ai_socktype = SOCK_STREAM;
    struct addrinfo* ergebnis = nullptr;
    if (lwip_getaddrinfo(rechner, "443", &hinweise, &ergebnis) == 0 && ergebnis != nullptr) {
      lwip_freeaddrinfo(ergebnis);
      return true;
    }
  }
  return false;
}

// ------------------------------------------------------------------ Suchen
// Fragt GitHub nach der neuesten Release. HTTP-Status, -1 ohne Verbindung,
// -2 wenn der Name nicht aufgelöst wurde.
static int github_fragen(char* json, size_t max) {
  char adresse[160];
  snprintf(adresse, sizeof(adresse), "https://api.github.com/repos/%s/%s/releases/latest", GITHUB_BESITZER,
           GITHUB_REPO);
  dns_v4_einsetzen();
  if (!ipv4_vorab(adresse)) return -2;
  esp_http_client_config_t cfg = {};
  cfg.url = adresse;
  cfg.timeout_ms = HTTP_TIMEOUT_MS;
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.buffer_size = 2048;
  cfg.buffer_size_tx = 1024;
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  if (c == nullptr) return -1;
  esp_http_client_set_header(c, "Accept", "application/vnd.github+json");
  esp_http_client_set_header(c, "User-Agent", "sdm630-fronius");
  int status = -1;
  if (esp_http_client_open(c, 0) == ESP_OK && esp_http_client_fetch_headers(c) >= 0) {
    status = esp_http_client_get_status_code(c);
    size_t n = 0;
    while (n + 1 < max) {
      const int gelesen = esp_http_client_read(c, json + n, max - 1 - n);
      if (gelesen <= 0) break;
      n += gelesen;
    }
    json[n] = '\0';
  }
  esp_http_client_close(c);
  esp_http_client_cleanup(c);
  return status;
}

static void suchen_aufgabe(void*) {
  char* json = (char*)malloc(JSON_MAX);
  if (json == nullptr) {
    setze(UpdateZustand::FEHLER, "Zu wenig freier Speicher.");
    aufgabe_laeuft = false;
    vTaskDelete(nullptr);
    return;
  }
  const int status = github_fragen(json, JSON_MAX);
  if (status != 200) {
    char text[120];
    if (status == -2) snprintf(text, sizeof(text), "GitHub nicht gefunden (Namensauflösung im Router).");
    else if (status == -1) snprintf(text, sizeof(text), "Keine Verbindung zu GitHub (Internet?).");
    else if (status == 404) snprintf(text, sizeof(text), "Noch keine Version auf GitHub veröffentlicht.");
    else if (status == 403) snprintf(text, sizeof(text), "GitHub lehnt ab (zu viele Anfragen?). Später erneut versuchen.");
    else snprintf(text, sizeof(text), "GitHub antwortet mit Fehler %d.", status);
    setze(UpdateZustand::FEHLER, text);
    free(json);
    aufgabe_laeuft = false;
    vTaskDelete(nullptr);
    return;
  }
  JsonDocument filter;
  filter["tag_name"] = true;
  filter["body"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["size"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  JsonDocument doc;
  const DeserializationError f = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  free(json);
  if (f) {
    setze(UpdateZustand::FEHLER, "Antwort von GitHub unlesbar (zu lang?).");
    aufgabe_laeuft = false;
    vTaskDelete(nullptr);
    return;
  }
  const char* tag = doc["tag_name"] | "";
  int neu[3], alt[3];
  version_lesen(FIRMWARE_VERSION, alt);
  if (!version_lesen(tag, neu)) {
    setze(UpdateZustand::FEHLER, "Release ohne Versionsnummer (Kennzeichen v1.2.3).");
    aufgabe_laeuft = false;
    vTaskDelete(nullptr);
    return;
  }
  // Passende Datei in der Release suchen: sdm630-fronius-<Version>.bin
  char erwartet[48];
  snprintf(erwartet, sizeof(erwartet), "%s%d.%d.%d.bin", UPDATE_DATEI_ANFANG, neu[0], neu[1], neu[2]);
  const char* adresse = nullptr;
  uint32_t groesse = 0;
  for (JsonVariantConst a : doc["assets"].as<JsonArrayConst>()) {
    if (strcmp(a["name"] | "", erwartet) == 0) {
      adresse = a["browser_download_url"] | "";
      groesse = a["size"] | 0;
    }
  }
  const char* text = doc["body"] | "";
  aendern([&](UpdateStand& s) {
    snprintf(s.neue_version, sizeof(s.neue_version), "%d.%d.%d", neu[0], neu[1], neu[2]);
    snprintf(s.beschreibung, sizeof(s.beschreibung), "%s", text);
  });
  if (!ist_neuer(neu, alt)) {
    char m[120];
    snprintf(m, sizeof(m), "Version %s ist aktuell (neueste auf GitHub: %d.%d.%d).", FIRMWARE_VERSION, neu[0],
             neu[1], neu[2]);
    setze(UpdateZustand::AKTUELL, m);
  } else if (adresse == nullptr || strlen(adresse) >= sizeof(download_adresse)) {
    char m[120];
    snprintf(m, sizeof(m), "Version %d.%d.%d gefunden, aber ohne Datei %s.", neu[0], neu[1], neu[2], erwartet);
    setze(UpdateZustand::FEHLER, m);
  } else {
    snprintf(download_adresse, sizeof(download_adresse), "%s", adresse);
    download_groesse = groesse;
    char m[120];
    snprintf(m, sizeof(m), "Version %d.%d.%d ist verfügbar (%lu KB).", neu[0], neu[1], neu[2],
             (unsigned long)(groesse / 1024));
    setze(UpdateZustand::GEFUNDEN, m);
  }
  aufgabe_laeuft = false;
  vTaskDelete(nullptr);
}

bool update_suchen() {
  if (aufgabe_laeuft) return false;
  if (WiFi.status() != WL_CONNECTED) {
    setze(UpdateZustand::FEHLER, "Kein Heimnetz: Die Suche braucht Internet.");
    return false;
  }
  aufgabe_laeuft = true;
  aendern([](UpdateStand& s) {
    s.zustand = UpdateZustand::SUCHT;
    s.neue_version[0] = '\0';
    s.beschreibung[0] = '\0';
    snprintf(s.meldung, sizeof(s.meldung), "Frage GitHub …");
  });
  if (xTaskCreatePinnedToCore(suchen_aufgabe, "upd_suche", 8192, nullptr, 1, nullptr, 1) != pdPASS) {
    aufgabe_laeuft = false;
    setze(UpdateZustand::FEHLER, "Zu wenig freier Speicher.");
    return false;
  }
  return true;
}

// ------------------------------------------------------------------ Installieren (GitHub)
static void installieren_aufgabe(void*) {
  esp_http_client_config_t http = {};
  http.url = download_adresse;
  http.timeout_ms = HTTP_TIMEOUT_MS;
  http.crt_bundle_attach = esp_crt_bundle_attach;
  http.buffer_size = 4096;      // Kopfzeilen der Weiterleitung sind lang
  http.buffer_size_tx = 4096;   // die Weiterleitungsadresse auch
  http.keep_alive_enable = true;
  esp_https_ota_config_t ota = {};
  ota.http_config = &http;
  dns_v4_einsetzen();
  ipv4_vorab(download_adresse);

  esp_https_ota_handle_t h = nullptr;
  esp_err_t e = esp_https_ota_begin(&ota, &h);
  if (e == ESP_OK) {
    for (;;) {
      e = esp_https_ota_perform(h);
      if (e != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;
      const int gelesen = esp_https_ota_get_image_len_read(h);
      const int gesamt = download_groesse ? (int)download_groesse : esp_https_ota_get_image_size(h);
      if (gesamt > 0) aendern([&](UpdateStand& s) { s.prozent = (uint8_t)min(99, gelesen * 100 / gesamt); });
    }
    if (e == ESP_OK && !esp_https_ota_is_complete_data_received(h)) e = ESP_FAIL;
    if (e == ESP_OK) {
      e = esp_https_ota_finish(h);
    } else {
      esp_https_ota_abort(h);
    }
  }
  if (e == ESP_OK) {
    char m[120];
    snprintf(m, sizeof(m), "Version %s geschrieben. Der ESP32 startet neu …", stand.neue_version);
    aendern([](UpdateStand& s) { s.prozent = 100; });
    setze(UpdateZustand::FERTIG, m);
    ereignis(false, "Update von GitHub geschrieben, Neustart");
    neustart_planen();
  } else {
    char m[120];
    snprintf(m, sizeof(m), "Update fehlgeschlagen (%s). Die bisherige Version läuft weiter.", esp_err_to_name(e));
    setze(UpdateZustand::FEHLER, m);
    ereignis(true, "Update von GitHub fehlgeschlagen: %s", esp_err_to_name(e));
  }
  aufgabe_laeuft = false;
  vTaskDelete(nullptr);
}

bool update_installieren() {
  if (aufgabe_laeuft || stand.zustand != UpdateZustand::GEFUNDEN) return false;
  aufgabe_laeuft = true;
  aendern([](UpdateStand& s) {
    s.zustand = UpdateZustand::LAEDT;
    s.prozent = 0;
    snprintf(s.meldung, sizeof(s.meldung), "Lade und schreibe … Strom nicht trennen.");
  });
  if (xTaskCreatePinnedToCore(installieren_aufgabe, "upd_ota", 8192, nullptr, 1, nullptr, 1) != pdPASS) {
    aufgabe_laeuft = false;
    setze(UpdateZustand::FEHLER, "Zu wenig freier Speicher.");
    return false;
  }
  return true;
}

// ------------------------------------------------------------------ Datei-Upload
const char* update_datei_anfang() {
  if (aufgabe_laeuft || Update.isRunning()) return "Es läuft schon ein Update.";
  if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) return "Programmspeicher lässt sich nicht vorbereiten.";
  datei_erster_teil = true;
  setze(UpdateZustand::LAEDT, "Datei wird geschrieben … Strom nicht trennen.");
  return nullptr;
}

const char* update_datei_teil(const uint8_t* daten, size_t laenge) {
  if (!Update.isRunning()) return "Update nicht gestartet.";
  if (datei_erster_teil) {
    datei_erster_teil = false;
    // Kopf prüfen: ESP-Firmware für einen ESP32 (nicht S3 o. Ä.)
    if (laenge < 16 || daten[0] != BILD_KENNUNG) {
      Update.abort();
      return "Das ist keine Firmware-Datei.";
    }
    if ((daten[12] | (daten[13] << 8)) != CHIP_ESP32) {
      Update.abort();
      return "Die Firmware ist für einen anderen Chip.";
    }
  }
  if (Update.write((uint8_t*)daten, laenge) != laenge) return "Schreibfehler im Programmspeicher.";
  return nullptr;
}

const char* update_datei_ende() {
  if (!Update.isRunning()) return "Update nicht gestartet.";
  if (!Update.end(true)) {
    setze(UpdateZustand::FEHLER, "Datei fehlerhaft, die bisherige Version läuft weiter.");
    return "Datei fehlerhaft (Prüfsumme), die bisherige Version läuft weiter.";
  }
  setze(UpdateZustand::FERTIG, "Datei geschrieben. Der ESP32 startet neu …");
  ereignis(false, "Update per Datei geschrieben, Neustart");
  neustart_planen();
  return nullptr;
}

void update_datei_abbrechen() {
  if (Update.isRunning()) Update.abort();
  setze(UpdateZustand::FEHLER, "Hochladen abgebrochen, die bisherige Version läuft weiter.");
}
