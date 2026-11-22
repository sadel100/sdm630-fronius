// fronius.cpp
// ---------------------------------------------------------------------------
// Zähler-Nachbildung für den Fronius Datamanager (siehe fronius.h).
//
// SunSpec-Abbild ab SUNSPEC_BASIS (Modbus-Adresse nullbasiert, 40000 = Register 40001):
//   +0     "SunS"  (0x5375 0x6E53)
//   +2     Modell 1 (Common Block): ID 1, Länge 65
//   +4     Hersteller (16 Reg.), Gerät (16), Optionen (8), Version (8),
//          Seriennummer (16), Geräteadresse DA (1)
//   +69    Zählermodell: ID 213 (Länge 124) oder 203 (Länge 105)
//   +71    Daten des Zählermodells
//   +71+L  Endkennung 0xFFFF, Länge 0
// Für Modell 213 liegt die Leistung W bei 40097 und das Ende bei 40195. Das
// deckt sich mit einer mitgeschnittenen Abfrage eines Fronius-Wechselrichters
// (Photovoltaikforum, Modbus TCP).
//
// Eastron-Abbild (Funktionscode 04, float32, höherwertiges Wort zuerst),
// Belegung wie SDM72D-M bzw. SDM630, Leistung + Bezug / − Einspeisung:
//   0x0000–0x0004 Spannung L1–L3     0x0006–0x000A Strom L1–L3
//   0x000C–0x0010 Leistung L1–L3     0x001E–0x0022 Leistungsfaktor L1–L3
//   0x002A mittlere Spannung         0x002E mittlerer Strom   0x0030 Summe Strom
//   0x0034 Gesamtleistung            0x003E Leistungsfaktor   0x0046 Frequenz
//   0x0048 Bezug kWh                 0x004A Einspeisung kWh   0x0156 Summe kWh
//   0x0500 Bezugsleistung W          0x0502 Einspeiseleistung W
// Alles andere in den Bereichen 0x0000–0x0187 und 0x0500–0x0503 ist 0.
//
// Werte, die der Zähler nicht liefert, stehen als 0 in beiden Abbildern.
// ---------------------------------------------------------------------------
#include "fronius.h"
#include "messwerte.h"
#include "einstellungen.h"
#include "erkennung.h"
#include "konfiguration.h"
#include <ModbusServerRTU.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Größen der Abbilder
// ---------------------------------------------------------------------------
static constexpr uint16_t LAENGE_MODELL1 = 65;
static constexpr uint16_t OFS_MODELL1    = 2;
static constexpr uint16_t OFS_ZAEHLER    = OFS_MODELL1 + 2 + LAENGE_MODELL1;  // = 69
static constexpr uint16_t OFS_DATEN      = OFS_ZAEHLER + 2;                    // = 71
static constexpr uint16_t SUNSPEC_MAX    = OFS_DATEN + 124 + 2;                // größtes Modell

static constexpr uint16_t EASTRON_A_START  = 0x0000;
static constexpr uint16_t EASTRON_A_ANZAHL = 0x0188;
static constexpr uint16_t EASTRON_B_START  = 0x0500;
static constexpr uint16_t EASTRON_B_ANZAHL = 4;

static uint16_t laenge_zaehlermodell() {
  return einstellungen().sunspec_modell == 213 ? 124 : 105;
}
static uint16_t sunspec_groesse() {
  return OFS_DATEN + laenge_zaehlermodell() + 2;
}

static ModbusServerRTU   server(FRONIUS_SERVER_TIMEOUT_MS, FRONIUS_PIN_DE);
static FroniusStatistik  statistik;
static portMUX_TYPE      sperre = portMUX_INITIALIZER_UNLOCKED;
static FroniusAnfrage    protokoll[PROTOKOLL_EINTRAEGE];
static size_t            protokoll_pos = 0, protokoll_anzahl = 0;
static QueueHandle_t     meldungen = nullptr;

const char* bereich_name(Bereich b) {
  switch (b) {
    case Bereich::SUNSPEC: return "SunSpec";
    case Bereich::EASTRON: return "Eastron";
    default:               return "unbekannt";
  }
}
const char* ergebnis_name(Ergebnis e) {
  switch (e) {
    case Ergebnis::BEANTWORTET: return "beantwortet";
    case Ergebnis::ABGELEHNT:   return "abgelehnt";
    default:                    return "keine Antwort (Zählerdaten veraltet)";
  }
}

// ---------------------------------------------------------------------------
// Hilfsfunktionen zum Füllen der Abbilder
// ---------------------------------------------------------------------------
static void text_setzen(uint16_t* abbild, uint16_t ofs, uint16_t register_anzahl, const char* text) {
  const size_t laenge = strlen(text);
  for (uint16_t i = 0; i < register_anzahl; i++) {
    const size_t p = i * 2;
    const uint8_t hoch  = p < laenge ? (uint8_t)text[p] : 0;
    const uint8_t niedr = p + 1 < laenge ? (uint8_t)text[p + 1] : 0;
    abbild[ofs + i] = ((uint16_t)hoch << 8) | niedr;
  }
}

static void float_setzen(uint16_t* abbild, uint16_t ofs, float wert) {
  uint32_t roh;
  memcpy(&roh, &wert, sizeof(roh));
  abbild[ofs]     = (uint16_t)(roh >> 16);  // höherwertiges Wort zuerst
  abbild[ofs + 1] = (uint16_t)(roh & 0xFFFF);
}

static void u32_setzen(uint16_t* abbild, uint16_t ofs, uint32_t wert) {
  abbild[ofs]     = (uint16_t)(wert >> 16);
  abbild[ofs + 1] = (uint16_t)(wert & 0xFFFF);
}

static int16_t skaliert(float wert, int sf) {
  const float v = wert / powf(10.0f, (float)sf);
  if (v > 32767.0f) return 32767;
  if (v < -32767.0f) return -32767;
  return (int16_t)lroundf(v);
}

static uint32_t wh(float kwh) {
  if (kwh <= 0) return 0;
  return (uint32_t)llroundf(kwh * 1000.0f);
}

// ---------------------------------------------------------------------------
// Gemeinsame Werte für die SunSpec-Modelle (mit eingestelltem Vorzeichen)
// ---------------------------------------------------------------------------
struct Zielwerte {
  float strom_summe, strom[3];
  float spannung_mittel, spannung[3];
  float frequenz;
  float leistung, leistung_phase[3];
  float pf;
};

static Zielwerte zielwerte(const Messwerte& m) {
  const float vz = einstellungen().vorzeichen;
  Zielwerte z = {};
  if (m.phasen_gueltig) {
    float u_summe = 0;
    for (int i = 0; i < 3; i++) {
      z.strom[i]          = m.strom_a[i];
      z.spannung[i]       = m.spannung_v[i];
      z.leistung_phase[i] = vz * m.leistung_phase_w[i];
      z.strom_summe      += m.strom_a[i];
      u_summe            += m.spannung_v[i];
    }
    z.spannung_mittel = u_summe / 3.0f;
  }
  z.frequenz = m.frequenz_gueltig ? m.frequenz_hz : 0;
  z.leistung = vz * m.leistung_w;
  z.pf = m.pf_gueltig ? m.leistungsfaktor * (LEISTUNGSFAKTOR_IN_PROZENT ? 100.0f : 1.0f) : 0;
  return z;
}

// Modell 213: alles float32. Offsets relativ zum Datenbeginn.
static void modell_213(uint16_t* d, const Messwerte& m) {
  const Zielwerte z = zielwerte(m);
  float_setzen(d, 0, z.strom_summe);        // A
  float_setzen(d, 2, z.strom[0]);           // AphA
  float_setzen(d, 4, z.strom[1]);           // AphB
  float_setzen(d, 6, z.strom[2]);           // AphC
  float_setzen(d, 8, z.spannung_mittel);    // PhV
  float_setzen(d, 10, z.spannung[0]);       // PhVphA
  float_setzen(d, 12, z.spannung[1]);       // PhVphB
  float_setzen(d, 14, z.spannung[2]);       // PhVphC
  // 16..22 Leiter-Leiter-Spannungen: liefert der SDM72 nicht, bleiben 0
  float_setzen(d, 24, z.frequenz);          // Hz
  float_setzen(d, 26, z.leistung);          // W
  float_setzen(d, 28, z.leistung_phase[0]); // WphA
  float_setzen(d, 30, z.leistung_phase[1]); // WphB
  float_setzen(d, 32, z.leistung_phase[2]); // WphC
  // 34..48 Schein- und Blindleistung: 0
  float_setzen(d, 50, z.pf);                // PF
  // 52..56 PF je Phase: 0
  float_setzen(d, 58, m.einspeisung_kwh * 1000.0f);  // TotWhExp
  float_setzen(d, 66, m.bezug_kwh * 1000.0f);        // TotWhImp
  // übrige Zählerstände und Ereignisse: 0
}

// Modell 203: Ganzzahlen mit Skalierungsfaktoren (SF).
static constexpr int SF_STROM    = -2;  // 0,01 A
static constexpr int SF_SPANNUNG = -1;  // 0,1 V
static constexpr int SF_FREQUENZ = -2;  // 0,01 Hz
static constexpr int SF_PF       = LEISTUNGSFAKTOR_IN_PROZENT ? -1 : -3;

static void modell_203(uint16_t* d, const Messwerte& m) {
  const Zielwerte z = zielwerte(m);

  // Leistung: kleinster Faktor, bei dem alle Werte in int16 passen
  float groesste = fabsf(z.leistung);
  for (int i = 0; i < 3; i++) groesste = fmaxf(groesste, fabsf(z.leistung_phase[i]));
  int sf_w = 0;
  while (sf_w < 3 && groesste / powf(10.0f, (float)sf_w) > 32767.0f) sf_w++;

  d[0] = (uint16_t)skaliert(z.strom_summe, SF_STROM);
  for (int i = 0; i < 3; i++) d[1 + i] = (uint16_t)skaliert(z.strom[i], SF_STROM);
  d[4] = (uint16_t)(int16_t)SF_STROM;
  d[5] = (uint16_t)skaliert(z.spannung_mittel, SF_SPANNUNG);
  for (int i = 0; i < 3; i++) d[6 + i] = (uint16_t)skaliert(z.spannung[i], SF_SPANNUNG);
  d[13] = (uint16_t)(int16_t)SF_SPANNUNG;
  d[14] = (uint16_t)skaliert(z.frequenz, SF_FREQUENZ);
  d[15] = (uint16_t)(int16_t)SF_FREQUENZ;
  d[16] = (uint16_t)skaliert(z.leistung, sf_w);
  for (int i = 0; i < 3; i++) d[17 + i] = (uint16_t)skaliert(z.leistung_phase[i], sf_w);
  d[20] = (uint16_t)(int16_t)sf_w;
  d[31] = (uint16_t)skaliert(z.pf, SF_PF);
  d[35] = (uint16_t)(int16_t)SF_PF;
  u32_setzen(d, 36, wh(m.einspeisung_kwh));  // TotWhExp
  u32_setzen(d, 44, wh(m.bezug_kwh));        // TotWhImp
  d[52] = 0;                                  // TotWh_SF = 0 (Wh)
}

static void sunspec_bauen(uint16_t* abbild, const Messwerte& m) {
  memset(abbild, 0, SUNSPEC_MAX * sizeof(uint16_t));
  abbild[0] = 0x5375;  // "Su"
  abbild[1] = 0x6E53;  // "nS"
  abbild[OFS_MODELL1]     = 1;
  abbild[OFS_MODELL1 + 1] = LAENGE_MODELL1;
  text_setzen(abbild, 4, 16, SUNSPEC_HERSTELLER);
  text_setzen(abbild, 20, 16, SUNSPEC_GERAET);
  text_setzen(abbild, 36, 8, SUNSPEC_OPTIONEN);
  text_setzen(abbild, 44, 8, SUNSPEC_VERSION);
  text_setzen(abbild, 52, 16, SUNSPEC_SERIE);
  abbild[68] = SUNSPEC_GERAETEADRESSE;
  abbild[OFS_ZAEHLER]     = einstellungen().sunspec_modell;
  abbild[OFS_ZAEHLER + 1] = laenge_zaehlermodell();
  if (einstellungen().sunspec_modell == 213) modell_213(abbild + OFS_DATEN, m);
  else                                       modell_203(abbild + OFS_DATEN, m);
  const uint16_t ende = OFS_DATEN + laenge_zaehlermodell();
  abbild[ende]     = 0xFFFF;
  abbild[ende + 1] = 0;
}

// Eastron-Abbild: Bereich A (0x0000–0x0187) und B (0x0500–0x0503) hintereinander
static void eastron_bauen(uint16_t* a, uint16_t* b, const Messwerte& m) {
  memset(a, 0, EASTRON_A_ANZAHL * sizeof(uint16_t));
  memset(b, 0, EASTRON_B_ANZAHL * sizeof(uint16_t));
  if (m.phasen_gueltig) {
    float u = 0, i_summe = 0;
    for (int p = 0; p < 3; p++) {
      float_setzen(a, 0x0000 + 2 * p, m.spannung_v[p]);
      float_setzen(a, 0x0006 + 2 * p, m.strom_a[p]);
      float_setzen(a, 0x000C + 2 * p, m.leistung_phase_w[p]);
      u += m.spannung_v[p];
      i_summe += m.strom_a[p];
    }
    float_setzen(a, 0x002A, u / 3.0f);
    float_setzen(a, 0x002E, i_summe / 3.0f);
    float_setzen(a, 0x0030, i_summe);
  }
  float_setzen(a, 0x0034, m.leistung_w);
  if (m.pf_gueltig) {
    float_setzen(a, 0x003E, m.leistungsfaktor);
    for (int p = 0; p < 3; p++) float_setzen(a, 0x001E + 2 * p, m.leistungsfaktor);
  }
  if (m.frequenz_gueltig) float_setzen(a, 0x0046, m.frequenz_hz);
  float_setzen(a, 0x0048, m.bezug_kwh);
  float_setzen(a, 0x004A, m.einspeisung_kwh);
  float_setzen(a, 0x0156, m.bezug_kwh + m.einspeisung_kwh);
  float_setzen(b, 0, m.leistung_w > 0 ? m.leistung_w : 0);    // Bezugsleistung
  float_setzen(b, 2, m.leistung_w < 0 ? -m.leistung_w : 0);   // Einspeiseleistung
}

// ---------------------------------------------------------------------------
// Protokoll und Statistik
// ---------------------------------------------------------------------------
static void melden(uint8_t fc, uint16_t start, uint16_t anzahl, Bereich bereich, Ergebnis e) {
  const FroniusAnfrage a{millis(), fc, start, anzahl, bereich, e};
  portENTER_CRITICAL(&sperre);
  statistik.letzte_anfrage_ms = a.zeit_ms;
  if (e == Ergebnis::BEANTWORTET) {
    statistik.beantwortet++;
    statistik.schon_beantwortet = true;
  } else if (e == Ergebnis::ABGELEHNT) {
    statistik.abgelehnt++;
  } else {
    statistik.ohne_antwort++;
  }
  if (bereich == Bereich::SUNSPEC) statistik.sunspec_anfragen++;
  if (bereich == Bereich::EASTRON) statistik.eastron_anfragen++;
  protokoll[protokoll_pos] = a;
  protokoll_pos = (protokoll_pos + 1) % PROTOKOLL_EINTRAEGE;
  if (protokoll_anzahl < PROTOKOLL_EINTRAEGE) protokoll_anzahl++;
  portEXIT_CRITICAL(&sperre);
  if (meldungen) xQueueSend(meldungen, &a, 0);  // Schlange voll: Meldung entfällt
}

// ---------------------------------------------------------------------------
// Anfragen beantworten (läuft im Task des eModbus-Servers)
// ---------------------------------------------------------------------------
static ModbusMessage register_lesen(ModbusMessage anfrage) {
  uint16_t start = 0, anzahl = 0;
  anfrage.get(2, start);
  anfrage.get(4, anzahl);
  const uint8_t fc = anfrage.getFunctionCode();
  const uint32_t ende = (uint32_t)start + anzahl;
  ModbusMessage antwort;

  // Welcher Bereich?
  Bereich bereich = Bereich::UNBEKANNT;
  const uint16_t* quelle = nullptr;
  static uint16_t sunspec[SUNSPEC_MAX];
  static uint16_t eastron_a[EASTRON_A_ANZAHL];
  static uint16_t eastron_b[EASTRON_B_ANZAHL];

  if (anzahl >= 1 && anzahl <= 125) {
    if (start >= SUNSPEC_BASIS && ende <= (uint32_t)SUNSPEC_BASIS + sunspec_groesse()) {
      bereich = Bereich::SUNSPEC;
      quelle = sunspec + (start - SUNSPEC_BASIS);
    } else if (fc == READ_INPUT_REGISTER && ende <= (uint32_t)EASTRON_A_START + EASTRON_A_ANZAHL) {
      bereich = Bereich::EASTRON;
      quelle = eastron_a + (start - EASTRON_A_START);
    } else if (fc == READ_INPUT_REGISTER && start >= EASTRON_B_START &&
               ende <= (uint32_t)EASTRON_B_START + EASTRON_B_ANZAHL) {
      bereich = Bereich::EASTRON;
      quelle = eastron_b + (start - EASTRON_B_START);
    }
  }
  if (quelle == nullptr) {
    melden(fc, start, anzahl, bereich, Ergebnis::ABGELEHNT);
    antwort.setError(anfrage.getServerID(), fc, ILLEGAL_DATA_ADDRESS);
    return antwort;
  }

  // Veraltete Zählerdaten: absichtlich keine Antwort.
  const Messwerte m = messwerte_lesen();
  if (!messwerte_aktuell(m, CACHE_MAX_ALTER_MS)) {
    melden(fc, start, anzahl, bereich, Ergebnis::OHNE_ANTWORT);
    return NIL_RESPONSE;
  }

  if (bereich == Bereich::SUNSPEC) sunspec_bauen(sunspec, m);
  else                             eastron_bauen(eastron_a, eastron_b, m);

  antwort.add(anfrage.getServerID(), fc, (uint8_t)(anzahl * 2));
  for (uint16_t i = 0; i < anzahl; i++) antwort.add(quelle[i]);
  melden(fc, start, anzahl, bereich, Ergebnis::BEANTWORTET);
  return antwort;
}

// ---------------------------------------------------------------------------
// Start: erst Erkennung, dann Server
// ---------------------------------------------------------------------------
static void fronius_task(void*) {
  const Einstellungen& e = einstellungen();
  uint32_t baud = e.fronius_baud, format = e.fronius_format;
  uint8_t adresse = e.fronius_adresse;

  if (baud == 0 || adresse == 0) {
    erkennung_ausfuehren(baud, format, adresse);  // blockiert, bis etwas gefunden ist
    const ErkennungStand s = erkennung_stand();
    baud = s.baud;
    format = s.format;
    adresse = s.adresse;
  }

  portENTER_CRITICAL(&sperre);
  statistik.baud = baud;
  statistik.format = format;
  statistik.adresse = adresse;
  statistik.zustand = FroniusZustand::BEREIT;
  statistik.letzte_anfrage_ms = millis();
  portEXIT_CRITICAL(&sperre);

  Serial2.end();
  RTUutils::prepareHardwareSerial(Serial2);
  Serial2.begin(baud, format, FRONIUS_PIN_RX, FRONIUS_PIN_TX);
  server.registerWorker(adresse, READ_HOLD_REGISTER, &register_lesen);
  server.registerWorker(adresse, READ_INPUT_REGISTER, &register_lesen);
  server.begin(Serial2, 0);
  Serial.printf("[Fronius] Server bereit: %lu Baud %s, Adresse %u\n", (unsigned long)baud,
                format_name(format), adresse);
  vTaskDelete(nullptr);
}

void fronius_start() {
  meldungen = xQueueCreate(32, sizeof(FroniusAnfrage));
  xTaskCreatePinnedToCore(fronius_task, "fronius", 4096, nullptr, 2, nullptr, 0);
}

FroniusStatistik fronius_statistik() {
  portENTER_CRITICAL(&sperre);
  FroniusStatistik kopie = statistik;
  portEXIT_CRITICAL(&sperre);
  return kopie;
}

size_t fronius_protokoll(FroniusAnfrage* ziel, size_t max_anzahl) {
  portENTER_CRITICAL(&sperre);
  size_t n = protokoll_anzahl < max_anzahl ? protokoll_anzahl : max_anzahl;
  for (size_t i = 0; i < n; i++) {
    const size_t idx = (protokoll_pos + PROTOKOLL_EINTRAEGE - 1 - i) % PROTOKOLL_EINTRAEGE;
    ziel[i] = protokoll[idx];
  }
  portEXIT_CRITICAL(&sperre);
  return n;
}

bool fronius_naechste_meldung(FroniusAnfrage& a) {
  return meldungen && xQueueReceive(meldungen, &a, 0) == pdTRUE;
}
