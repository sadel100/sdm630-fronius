// zaehler.cpp
// ---------------------------------------------------------------------------
// Liest den Netzzähler Eastron SDM630-Modbus V2 (Modbus RTU, Funktionscode 04).
// Jeder Wert ist ein float32 = 2 Register, höherwertiges Wort zuerst.
//
// Gelesen werden drei Bereiche (siehe messwerte.h):
//   A  0x0000–0x004F  Pflicht, in zwei Anfragen à 40 Register:
//        0x0000/02/04  Spannung L1–L3 (V)      0x0006/08/0A  Strom L1–L3 (A)
//        0x000C/0E/10  Wirkleistung L1–L3 (W)  0x001E/20/22  Leistungsfaktor L1–L3
//        0x0034        Gesamtleistung (W, + Bezug / − Einspeisung)
//        0x003E        Leistungsfaktor gesamt   0x0046  Frequenz (Hz)
//        0x0048        Bezug (kWh)              0x004A  Einspeisung (kWh)
//   B  0x00C8–0x00EF  Zusatz (Außenleiterspannungen, N-Strom, Oberschwingungen)
//   C  0x0156–0x0187  Zusatz (Summen- und Phasenzählerstände)
// Lehnt der Zähler B oder C mit einer Modbus-Ausnahme ab, wird der Bereich bis
// zum Neustart nicht mehr gefragt. Die Rohregister gehen unverändert an den
// Fronius, wenn er die Eastron-Register abfragt.
// ---------------------------------------------------------------------------
#include "zaehler.h"
#include "messwerte.h"
#include "konfiguration.h"
#include "einstellungen.h"
#include <ModbusClientRTU.h>
#include <string.h>

// Je Anfrage höchstens 40 Register (20 Werte), wie es Eastron empfiehlt
static constexpr uint16_t ANFRAGE_MAX = 40;

static ModbusClientRTU   client(ZAEHLER_PIN_DE);
static ZaehlerStatistik  statistik;
static portMUX_TYPE      statistik_sperre = portMUX_INITIALIZER_UNLOCKED;
static uint32_t          token = 0;

// float32 aus einem Rohregister-Feld (Index = Registernummer im Feld)
static float float_aus(const uint16_t* roh, uint16_t index) {
  const uint32_t r = ((uint32_t)roh[index] << 16) | roh[index + 1];
  float wert;
  memcpy(&wert, &r, sizeof(wert));
  return wert;
}

static void fehler_merken(const char* was, Error fehler) {
  ModbusError text(fehler);
  portENTER_CRITICAL(&statistik_sperre);
  snprintf(statistik.letzter_fehler, sizeof(statistik.letzter_fehler), "%s: %s", was,
           (const char*)text);
  portEXIT_CRITICAL(&statistik_sperre);
}

// Liest `anzahl` Register ab `adresse` nach `ziel` (in Stücken zu ANFRAGE_MAX).
// `abgelehnt` wird true, wenn der Zähler mit einer Modbus-Ausnahme antwortet.
static bool bereich_lesen(const char* was, uint16_t adresse, uint16_t anzahl, uint16_t* ziel,
                          bool& abgelehnt) {
  abgelehnt = false;
  for (uint16_t ofs = 0; ofs < anzahl; ofs += ANFRAGE_MAX) {
    const uint16_t n = min<uint16_t>(ANFRAGE_MAX, anzahl - ofs);
    ModbusMessage antwort = client.syncRequest(++token, einstellungen().zaehler_adresse,
                                               READ_INPUT_REGISTER, adresse + ofs, n);
    const Error fehler = antwort.getError();
    vTaskDelay(pdMS_TO_TICKS(ZAEHLER_PAUSE_MS));
    if (fehler != SUCCESS) {
      fehler_merken(was, fehler);
      abgelehnt = (fehler == ILLEGAL_FUNCTION || fehler == ILLEGAL_DATA_ADDRESS ||
                   fehler == ILLEGAL_DATA_VALUE);
      return false;
    }
    if (antwort.size() < (size_t)(3 + n * 2)) {
      fehler_merken(was, PACKET_LENGTH_ERROR);
      return false;
    }
    for (uint16_t i = 0; i < n; i++) {
      ziel[ofs + i] = ((uint16_t)antwort[3 + 2 * i] << 8) | antwort[4 + 2 * i];
    }
  }
  return true;
}

// Eine Abfragerunde
static void runde(Messwerte& m) {
  bool abgelehnt;
  static uint16_t a[ROH_A_ANZAHL];
  const bool pflicht_ok = bereich_lesen("Grundwerte", ROH_A_START, ROH_A_ANZAHL, a, abgelehnt);

  // Antwortet der Zähler gar nicht, die Zusatzbereiche gar nicht erst versuchen
  ZaehlerStatistik st = zaehler_statistik();
  if (pflicht_ok && st.block_b_vorhanden) {
    static uint16_t b[ROH_B_ANZAHL];
    m.roh_b_ok = bereich_lesen("Zusatz C8", ROH_B_START, ROH_B_ANZAHL, b, abgelehnt);
    if (m.roh_b_ok) memcpy(m.roh_b, b, sizeof(b));
    else if (abgelehnt) st.block_b_vorhanden = false;
  }
  if (pflicht_ok && st.block_c_vorhanden) {
    static uint16_t c[ROH_C_ANZAHL];
    m.roh_c_ok = bereich_lesen("Zusatz 156", ROH_C_START, ROH_C_ANZAHL, c, abgelehnt);
    if (m.roh_c_ok) memcpy(m.roh_c, c, sizeof(c));
    else if (abgelehnt) st.block_c_vorhanden = false;
  }

  if (pflicht_ok) {
    memcpy(m.roh_a, a, sizeof(a));
    for (int p = 0; p < 3; p++) {
      m.spannung_v[p]       = float_aus(a, 0x0000 + 2 * p);
      m.strom_a[p]          = float_aus(a, 0x0006 + 2 * p);
      m.leistung_phase_w[p] = float_aus(a, 0x000C + 2 * p);
      m.pf_phase[p]         = float_aus(a, 0x001E + 2 * p);
    }
    m.leistung_w      = float_aus(a, 0x0034);
    m.leistungsfaktor = float_aus(a, 0x003E);
    m.frequenz_hz     = float_aus(a, 0x0046);
    m.bezug_kwh       = float_aus(a, 0x0048);
    m.einspeisung_kwh = float_aus(a, 0x004A);
    m.phasen_gueltig = m.frequenz_gueltig = m.pf_gueltig = true;
    m.gueltig        = true;
    m.zeitstempel_ms = millis();
  }

  portENTER_CRITICAL(&statistik_sperre);
  statistik.block_b_vorhanden = st.block_b_vorhanden;
  statistik.block_c_vorhanden = st.block_c_vorhanden;
  if (pflicht_ok) statistik.runden_ok++; else statistik.runden_fehler++;
  portEXIT_CRITICAL(&statistik_sperre);
}

static void zaehler_task(void*) {
  Messwerte m;
  TickType_t letzter_start = xTaskGetTickCount();
  for (;;) {
    runde(m);
    messwerte_schreiben(m);
    vTaskDelayUntil(&letzter_start, pdMS_TO_TICKS(ZAEHLER_ABFRAGE_MS));
  }
}

void zaehler_start() {
  RTUutils::prepareHardwareSerial(Serial1);
  Serial1.begin(einstellungen().zaehler_baud, einstellungen().zaehler_format, ZAEHLER_PIN_RX, ZAEHLER_PIN_TX);
  client.setTimeout(ZAEHLER_TIMEOUT_MS);
  client.begin(Serial1);
  xTaskCreatePinnedToCore(zaehler_task, "zaehler", 4096, nullptr, 2, nullptr, 1);
}

ZaehlerStatistik zaehler_statistik() {
  portENTER_CRITICAL(&statistik_sperre);
  ZaehlerStatistik kopie = statistik;
  portEXIT_CRITICAL(&statistik_sperre);
  return kopie;
}
