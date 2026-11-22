// zaehler.cpp
// ---------------------------------------------------------------------------
// Liest den Eastron SDM72D-M (Modbus RTU, Funktionscode 04 = Input-Register).
//
// Registerbelegung (Adressen nullbasiert, je Wert ein float32 = 2 Register,
// höherwertiges Wort zuerst):
//
//   Pflicht, laut Handbuch SDM72D-M V1.5:
//     0x0048  Bezug gesamt (kWh)          0x004A  Einspeisung gesamt (kWh)
//     0x0500  Bezugsleistung gesamt (W)   0x0502  Einspeiseleistung gesamt (W)
//   Zusatz, NICHT im Handbuch, nur aus der Bibliothek nmakel/sdm_modbus:
//     0x0000..0x0004  Spannung L1..L3 (V)
//     0x0006..0x000A  Strom L1..L3 (A)
//     0x000C..0x0010  Leistung L1..L3 (W)
//     0x003E          Leistungsfaktor gesamt
//     0x0046          Frequenz (Hz)
//
// Die Leistung wird als Bezug minus Einspeisung gebildet (0x0500 − 0x0502).
// So hängt das Vorzeichen nicht davon ab, wie der Zähler "Total system power"
// (0x0034) darstellt.
//
// Lehnt der Zähler ein Zusatzregister mit einer Modbus-Ausnahme ab, wird es
// bis zum nächsten Neustart nicht mehr abgefragt. Bei einer Zeitüberschreitung
// wird es weiter versucht.
// ---------------------------------------------------------------------------
#include "zaehler.h"
#include "messwerte.h"
#include "konfiguration.h"
#include "einstellungen.h"
#include <ModbusClientRTU.h>
#include <string.h>

// Registeradressen (benannt, keine Zahlen im Ablauf)
static constexpr uint16_t REG_BEZUG_KWH      = 0x0048;  // + 0x004A Einspeisung
static constexpr uint16_t REG_BEZUG_W        = 0x0500;  // + 0x0502 Einspeisung
static constexpr uint16_t REG_PHASEN_START   = 0x0000;  // 9 Werte: U1-3, I1-3, P1-3
static constexpr uint16_t REG_PHASEN_ANZAHL  = 18;
static constexpr uint16_t REG_PF             = 0x003E;
static constexpr uint16_t REG_FREQUENZ       = 0x0046;

static ModbusClientRTU   client(ZAEHLER_PIN_DE);
static ZaehlerStatistik  statistik;
static portMUX_TYPE      statistik_sperre = portMUX_INITIALIZER_UNLOCKED;
static uint32_t          token = 0;

// float32 aus der Antwort holen. `index` = Nummer des Werts (0, 1, 2 …).
// Nutzdaten beginnen in einer FC-04-Antwort bei Byte 3.
static float float_aus_antwort(const ModbusMessage& antwort, uint16_t index) {
  const uint16_t pos = 3 + index * 4;
  uint32_t roh = ((uint32_t)antwort[pos] << 24) | ((uint32_t)antwort[pos + 1] << 16) |
                 ((uint32_t)antwort[pos + 2] << 8) | (uint32_t)antwort[pos + 3];
  float wert;
  memcpy(&wert, &roh, sizeof(wert));
  return wert;
}

// Fehler merken (für die Debug-Ausgabe)
static void fehler_merken(const char* was, Error fehler) {
  ModbusError text(fehler);
  portENTER_CRITICAL(&statistik_sperre);
  snprintf(statistik.letzter_fehler, sizeof(statistik.letzter_fehler), "%s: %s", was,
           (const char*)text);
  portEXIT_CRITICAL(&statistik_sperre);
}

// Eine Anfrage stellen und auf die Antwort warten. Liefert true bei Erfolg.
// `abgelehnt` wird true, wenn der Zähler mit einer Modbus-Ausnahme antwortet
// (Register gibt es nicht), nicht bei Zeitüberschreitung.
static bool lesen(const char* was, uint16_t adresse, uint16_t anzahl, ModbusMessage& antwort,
                  bool& abgelehnt) {
  abgelehnt = false;
  antwort = client.syncRequest(++token, einstellungen().zaehler_adresse, READ_INPUT_REGISTER, adresse, anzahl);
  const Error fehler = antwort.getError();
  vTaskDelay(pdMS_TO_TICKS(ZAEHLER_PAUSE_MS));
  if (fehler != SUCCESS) {
    fehler_merken(was, fehler);
    abgelehnt = (fehler == ILLEGAL_FUNCTION || fehler == ILLEGAL_DATA_ADDRESS ||
                 fehler == ILLEGAL_DATA_VALUE);
    return false;
  }
  // Länge prüfen: Kopf (3 Bytes) + 2 Bytes je Register + CRC wird von eModbus entfernt
  if (antwort.size() < (size_t)(3 + anzahl * 2)) {
    fehler_merken(was, PACKET_LENGTH_ERROR);
    return false;
  }
  return true;
}

// Eine ganze Abfragerunde. Pflichtwerte zuerst, dann die Zusatzwerte.
static void runde(Messwerte& m) {
  ModbusMessage antwort;
  bool abgelehnt;
  bool pflicht_ok = true;

  float bezug_kwh = 0, einsp_kwh = 0, bezug_w = 0, einsp_w = 0;
  if (lesen("Energie", REG_BEZUG_KWH, 4, antwort, abgelehnt)) {
    bezug_kwh = float_aus_antwort(antwort, 0);
    einsp_kwh = float_aus_antwort(antwort, 1);
  } else {
    pflicht_ok = false;
  }
  if (lesen("Leistung", REG_BEZUG_W, 4, antwort, abgelehnt)) {
    bezug_w = float_aus_antwort(antwort, 0);
    einsp_w = float_aus_antwort(antwort, 1);
  } else {
    pflicht_ok = false;
  }

  // Zusatzwerte. Fehlen sie in dieser Runde, bleiben die alten Werte stehen,
  // gelten aber als ungültig.
  ZaehlerStatistik st = zaehler_statistik();
  if (st.phasen_vorhanden) {
    if (lesen("Phasen", REG_PHASEN_START, REG_PHASEN_ANZAHL, antwort, abgelehnt)) {
      for (int i = 0; i < 3; i++) {
        m.spannung_v[i]       = float_aus_antwort(antwort, i);
        m.strom_a[i]          = float_aus_antwort(antwort, 3 + i);
        m.leistung_phase_w[i] = float_aus_antwort(antwort, 6 + i);
      }
      m.phasen_gueltig = true;
    } else {
      m.phasen_gueltig = false;
      if (abgelehnt) st.phasen_vorhanden = false;
    }
  }
  if (st.pf_vorhanden) {
    if (lesen("PF", REG_PF, 2, antwort, abgelehnt)) {
      m.leistungsfaktor = float_aus_antwort(antwort, 0);
      m.pf_gueltig = true;
    } else {
      m.pf_gueltig = false;
      if (abgelehnt) st.pf_vorhanden = false;
    }
  }
  if (st.frequenz_vorhanden) {
    if (lesen("Frequenz", REG_FREQUENZ, 2, antwort, abgelehnt)) {
      m.frequenz_hz = float_aus_antwort(antwort, 0);
      m.frequenz_gueltig = true;
    } else {
      m.frequenz_gueltig = false;
      if (abgelehnt) st.frequenz_vorhanden = false;
    }
  }

  if (pflicht_ok) {
    m.bezug_kwh       = bezug_kwh;
    m.einspeisung_kwh = einsp_kwh;
    m.leistung_w      = bezug_w - einsp_w;
    m.gueltig         = true;
    m.zeitstempel_ms  = millis();
  }

  portENTER_CRITICAL(&statistik_sperre);
  statistik.phasen_vorhanden   = st.phasen_vorhanden;
  statistik.pf_vorhanden       = st.pf_vorhanden;
  statistik.frequenz_vorhanden = st.frequenz_vorhanden;
  if (pflicht_ok) statistik.runden_ok++; else statistik.runden_fehler++;
  portEXIT_CRITICAL(&statistik_sperre);
}

// Der Abfrage-Task: Runde, Werte in den Zwischenspeicher, warten.
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
  Serial1.begin(einstellungen().zaehler_baud, ZAEHLER_FORMAT, ZAEHLER_PIN_RX, ZAEHLER_PIN_TX);
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
