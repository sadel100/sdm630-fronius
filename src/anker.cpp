// anker.cpp
// ---------------------------------------------------------------------------
// Abfrage der Anker SOLIX Solarbank Max AC (siehe anker.h).
//
// Register laut offizieller Anker-Integration (Input-Register, FC 04,
// INT32/UINT32 mit höherwertigem Wort zuerst):
//   10001  Batteriestatus (UINT16: 1 lädt, 2 entlädt)
//   10002  PV-Leistung (INT32, W)            10004  PV fremd (INT32, W)
//   10008  Batterieleistung (INT32, W: + Entladen, − Laden)
//   10010  Lastleistung (INT32, W)           10012  Netzleistung (INT32, W: + Bezug)
//   10014  Ladestand (UINT16, %)
//   10208  AC-Ausgangsleistung (INT32, W)
//   10250  Nennkapazität (UINT32, 0,1 kWh)
//   10262  geladen gesamt (UINT32, 0,1 kWh)  10264  entladen gesamt (UINT32, 0,1 kWh)
//   32768  Modellkennung (5 Register Text)
// Zwei Anfragen je Runde (10000–10019 und 10208–10265), alle 5 s.
// ---------------------------------------------------------------------------
#include "anker.h"
#include "einstellungen.h"
#include "netz.h"
#include "konfiguration.h"
#include <WiFi.h>
#include <ModbusClientTCP.h>

static WiFiClient       netz_client;
static ModbusClientTCP  client(netz_client);
static AnkerWerte       werte;
static AnkerStatistik   statistik;
static portMUX_TYPE     sperre = portMUX_INITIALIZER_UNLOCKED;
static uint32_t         token = 0;

static void fehler_merken(const char* was, Error fehler) {
  ModbusError text(fehler);
  portENTER_CRITICAL(&sperre);
  snprintf(statistik.letzter_fehler, sizeof(statistik.letzter_fehler), "%s: %s", was,
           (const char*)text);
  statistik.runden_fehler++;
  portEXIT_CRITICAL(&sperre);
}

// Liest `anzahl` Input-Register ab `adresse`; die Werte stehen danach in `ziel`.
static bool lesen(const char* was, uint16_t adresse, uint16_t anzahl, uint16_t* ziel) {
  ModbusMessage antwort = client.syncRequest(++token, einstellungen().anker_unit,
                                             READ_INPUT_REGISTER, adresse, anzahl);
  const Error fehler = antwort.getError();
  if (fehler != SUCCESS) {
    fehler_merken(was, fehler);
    return false;
  }
  if (antwort.size() < (size_t)(3 + anzahl * 2)) {
    fehler_merken(was, PACKET_LENGTH_ERROR);
    return false;
  }
  for (uint16_t i = 0; i < anzahl; i++) {
    ziel[i] = ((uint16_t)antwort[3 + 2 * i] << 8) | antwort[4 + 2 * i];
  }
  return true;
}

static int32_t i32(const uint16_t* r, uint16_t i) {
  return (int32_t)(((uint32_t)r[i] << 16) | r[i + 1]);
}
static uint32_t u32(const uint16_t* r, uint16_t i) {
  return ((uint32_t)r[i] << 16) | r[i + 1];
}

static void runde() {
  static constexpr uint16_t A_START = 10000, A_ANZAHL = 20;
  static constexpr uint16_t B_START = 10208, B_ANZAHL = 58;
  uint16_t a[A_ANZAHL], b[B_ANZAHL];
  if (!lesen("Werte 10000", A_START, A_ANZAHL, a)) return;
  if (!lesen("Werte 10208", B_START, B_ANZAHL, b)) return;

  AnkerWerte w;
  w.status         = a[10001 - A_START];
  w.pv_w           = i32(a, 10002 - A_START) + i32(a, 10004 - A_START);
  w.batterie_w     = i32(a, 10008 - A_START);
  w.last_w         = i32(a, 10010 - A_START);
  w.netz_w         = i32(a, 10012 - A_START);
  w.soc            = a[10014 - A_START];
  w.ac_w           = i32(b, 10208 - B_START);
  w.kapazitaet_kwh = u32(b, 10250 - B_START) / 10.0f;
  w.lade_kwh       = u32(b, 10262 - B_START) / 10.0f;
  w.entlade_kwh    = u32(b, 10264 - B_START) / 10.0f;
  w.gueltig        = true;
  w.zeitstempel_ms = millis();

  portENTER_CRITICAL(&sperre);
  werte = w;
  statistik.runden_ok++;
  portEXIT_CRITICAL(&sperre);
}

// Modellkennung einmal lesen (Text, zwei Zeichen je Register)
static void modell_lesen() {
  uint16_t r[5];
  if (!lesen("Modell", 32768, 5, r)) return;
  char text[11];
  for (int i = 0; i < 5; i++) {
    text[2 * i] = (char)(r[i] >> 8);
    text[2 * i + 1] = (char)(r[i] & 0xFF);
  }
  text[10] = 0;
  for (int i = 0; i < 10; i++) if (text[i] != 0 && (text[i] < 32 || text[i] > 126)) text[i] = '?';
  portENTER_CRITICAL(&sperre);
  strncpy(statistik.modell, text, sizeof(statistik.modell) - 1);
  portEXIT_CRITICAL(&sperre);
}

static void anker_task(void*) {
  const Einstellungen& e = einstellungen();
  IPAddress ziel;
  bool ziel_ok = false;
  bool modell_gelesen = false;
  TickType_t letzter_start = xTaskGetTickCount();
  for (;;) {
    if (netz_heimnetz_verbunden()) {
      if (!ziel_ok) {
        // IP-Adresse oder Name (Name wird über den Router aufgelöst)
        ziel_ok = ziel.fromString(e.anker_ip) || WiFi.hostByName(e.anker_ip.c_str(), ziel) == 1;
        if (ziel_ok) {
          client.setTarget(ziel, e.anker_port, ANKER_TIMEOUT_MS);
          Serial.printf("[Anker] lese %s:%u, Unit %u\n", ziel.toString().c_str(), e.anker_port,
                        e.anker_unit);
        } else {
          portENTER_CRITICAL(&sperre);
          snprintf(statistik.letzter_fehler, sizeof(statistik.letzter_fehler), "Adresse unbekannt");
          statistik.runden_fehler++;
          portEXIT_CRITICAL(&sperre);
        }
      }
      if (ziel_ok) {
        if (!modell_gelesen) {
          modell_lesen();
          modell_gelesen = statistik.modell[0] != 0;
        }
        runde();
      }
    }
    vTaskDelayUntil(&letzter_start, pdMS_TO_TICKS(ANKER_ABFRAGE_MS));
  }
}

void anker_start() {
  const Einstellungen& e = einstellungen();
  statistik.aktiv = e.anker_an && e.anker_ip.length() > 0;
  if (!statistik.aktiv) return;
  client.setTimeout(ANKER_TIMEOUT_MS);
  client.begin(1);
  xTaskCreatePinnedToCore(anker_task, "anker", 4096, nullptr, 1, nullptr, 1);
}

AnkerWerte anker_werte() {
  portENTER_CRITICAL(&sperre);
  AnkerWerte kopie = werte;
  portEXIT_CRITICAL(&sperre);
  return kopie;
}

AnkerStatistik anker_statistik() {
  portENTER_CRITICAL(&sperre);
  AnkerStatistik kopie = statistik;
  portEXIT_CRITICAL(&sperre);
  return kopie;
}

bool anker_aktuell(const AnkerWerte& w) {
  return w.gueltig && (uint32_t)(millis() - w.zeitstempel_ms) <= ANKER_MAX_ALTER_MS;
}
