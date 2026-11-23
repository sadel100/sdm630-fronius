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
//
// Eastron-Abbild (Funktionscode 04), Belegung wie SDM630:
//   Netzzähler: die Rohregister des echten SDM630 (0x0000–0x004F, 0x00C8–0x00EF,
//               0x0156–0x0187), alles andere bis 0x0187 ist 0.
//   Solarbank:  aus den Anker-Werten gerechnet (Leistung, Zählerstände;
//               Spannung und Frequenz vom Netzzähler, Leistung auf L1).
//   Beide:      0x0500/0x0502 Bezugs-/Einspeiseleistung (wie SDM72).
// ---------------------------------------------------------------------------
#include "fronius.h"
#include "messwerte.h"
#include "anker.h"
#include "einstellungen.h"
#include "erkennung.h"
#include "konfiguration.h"
#include <ModbusServerRTU.h>
#include <string.h>

static constexpr uint16_t LAENGE_MODELL1 = 65;
static constexpr uint16_t OFS_MODELL1    = 2;
static constexpr uint16_t OFS_ZAEHLER    = OFS_MODELL1 + 2 + LAENGE_MODELL1;  // = 69
static constexpr uint16_t OFS_DATEN      = OFS_ZAEHLER + 2;                    // = 71
static constexpr uint16_t SUNSPEC_MAX    = OFS_DATEN + 124 + 2;

static constexpr uint16_t EASTRON_A_START  = 0x0000;
static constexpr uint16_t EASTRON_A_ANZAHL = 0x0188;
static constexpr uint16_t EASTRON_B_START  = 0x0500;
static constexpr uint16_t EASTRON_B_ANZAHL = 4;

static uint16_t laenge_zaehlermodell() { return einstellungen().sunspec_modell == 213 ? 124 : 105; }
static uint16_t sunspec_groesse() { return OFS_DATEN + laenge_zaehlermodell() + 2; }

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
    default:                    return "keine Antwort (Daten veraltet)";
  }
}

// ---------------------------------------------------------------------------
// Ein Zähler, wie er dem Fronius gezeigt wird (Vorzeichen: + Bezug/Laden)
// ---------------------------------------------------------------------------
struct ZaehlerBild {
  bool  aktuell = false;
  bool  phasen = false;
  float leistung = 0, phase_w[3] = {0, 0, 0};
  float spannung[3] = {0, 0, 0}, strom[3] = {0, 0, 0};
  float pf = 0, hz = 0;
  float import_kwh = 0, export_kwh = 0;
  const char* serie = "";
};

static ZaehlerBild bild_netz(const Messwerte& m) {
  ZaehlerBild b;
  b.aktuell = messwerte_aktuell(m, CACHE_MAX_ALTER_MS);
  b.phasen = m.phasen_gueltig;
  b.leistung = m.leistung_w;
  for (int p = 0; p < 3; p++) {
    b.phase_w[p] = m.leistung_phase_w[p];
    b.spannung[p] = m.spannung_v[p];
    b.strom[p] = m.strom_a[p];
  }
  b.pf = m.pf_gueltig ? m.leistungsfaktor : 0;
  b.hz = m.frequenz_gueltig ? m.frequenz_hz : 0;
  b.import_kwh = m.bezug_kwh;
  b.export_kwh = m.einspeisung_kwh;
  b.serie = SUNSPEC_SERIE;
  return b;
}

// Solarbank: Laden = Bezug (positiv), Entladen = negativ. Leistung auf L1,
// Spannung und Frequenz vom Netzzähler (gleiches Netz), Strom daraus gerechnet.
static ZaehlerBild bild_solarbank(const AnkerWerte& a, const Messwerte& m) {
  ZaehlerBild b;
  b.aktuell = anker_aktuell(a);
  b.leistung = -(float)a.batterie_w;
  b.phase_w[0] = b.leistung;
  if (messwerte_aktuell(m, CACHE_MAX_ALTER_MS) && m.phasen_gueltig) {
    b.phasen = true;
    for (int p = 0; p < 3; p++) b.spannung[p] = m.spannung_v[p];
    if (m.spannung_v[0] > 100) b.strom[0] = fabsf(b.leistung) / m.spannung_v[0];
    b.hz = m.frequenz_gueltig ? m.frequenz_hz : 0;
  }
  b.import_kwh = a.lade_kwh;
  b.export_kwh = a.entlade_kwh;
  b.serie = SUNSPEC_SERIE_ZWEIT;
  return b;
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
  abbild[ofs]     = (uint16_t)(roh >> 16);
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
// SunSpec
// ---------------------------------------------------------------------------
static constexpr int SF_STROM    = -2;
static constexpr int SF_SPANNUNG = -1;
static constexpr int SF_FREQUENZ = -2;
static constexpr int SF_PF       = LEISTUNGSFAKTOR_IN_PROZENT ? -1 : -3;

static void modell_213(uint16_t* d, const ZaehlerBild& b) {
  const float vz = einstellungen().vorzeichen;
  const float pf = b.pf * (LEISTUNGSFAKTOR_IN_PROZENT ? 100.0f : 1.0f);
  float_setzen(d, 0, b.strom[0] + b.strom[1] + b.strom[2]);              // A
  for (int p = 0; p < 3; p++) float_setzen(d, 2 + 2 * p, b.strom[p]);    // AphA–C
  float_setzen(d, 8, (b.spannung[0] + b.spannung[1] + b.spannung[2]) / 3.0f);  // PhV
  for (int p = 0; p < 3; p++) float_setzen(d, 10 + 2 * p, b.spannung[p]);
  float_setzen(d, 24, b.hz);                                              // Hz
  float_setzen(d, 26, vz * b.leistung);                                   // W
  for (int p = 0; p < 3; p++) float_setzen(d, 28 + 2 * p, vz * b.phase_w[p]);
  float_setzen(d, 50, pf);                                                // PF
  float_setzen(d, 58, b.export_kwh * 1000.0f);                            // TotWhExp
  float_setzen(d, 66, b.import_kwh * 1000.0f);                            // TotWhImp
}

static void modell_203(uint16_t* d, const ZaehlerBild& b) {
  const float vz = einstellungen().vorzeichen;
  const float pf = b.pf * (LEISTUNGSFAKTOR_IN_PROZENT ? 100.0f : 1.0f);
  float groesste = fabsf(b.leistung);
  for (int p = 0; p < 3; p++) groesste = fmaxf(groesste, fabsf(b.phase_w[p]));
  int sf_w = 0;
  while (sf_w < 3 && groesste / powf(10.0f, (float)sf_w) > 32767.0f) sf_w++;

  d[0] = (uint16_t)skaliert(b.strom[0] + b.strom[1] + b.strom[2], SF_STROM);
  for (int p = 0; p < 3; p++) d[1 + p] = (uint16_t)skaliert(b.strom[p], SF_STROM);
  d[4] = (uint16_t)(int16_t)SF_STROM;
  d[5] = (uint16_t)skaliert((b.spannung[0] + b.spannung[1] + b.spannung[2]) / 3.0f, SF_SPANNUNG);
  for (int p = 0; p < 3; p++) d[6 + p] = (uint16_t)skaliert(b.spannung[p], SF_SPANNUNG);
  d[13] = (uint16_t)(int16_t)SF_SPANNUNG;
  d[14] = (uint16_t)skaliert(b.hz, SF_FREQUENZ);
  d[15] = (uint16_t)(int16_t)SF_FREQUENZ;
  d[16] = (uint16_t)skaliert(vz * b.leistung, sf_w);
  for (int p = 0; p < 3; p++) d[17 + p] = (uint16_t)skaliert(vz * b.phase_w[p], sf_w);
  d[20] = (uint16_t)(int16_t)sf_w;
  d[31] = (uint16_t)skaliert(pf, SF_PF);
  d[35] = (uint16_t)(int16_t)SF_PF;
  u32_setzen(d, 36, wh(b.export_kwh));   // TotWhExp
  u32_setzen(d, 44, wh(b.import_kwh));   // TotWhImp
  d[52] = 0;                              // TotWh_SF = 0 (Wh)
}

static void sunspec_bauen(uint16_t* abbild, const ZaehlerBild& b) {
  memset(abbild, 0, SUNSPEC_MAX * sizeof(uint16_t));
  abbild[0] = 0x5375;
  abbild[1] = 0x6E53;
  abbild[OFS_MODELL1]     = 1;
  abbild[OFS_MODELL1 + 1] = LAENGE_MODELL1;
  text_setzen(abbild, 4, 16, SUNSPEC_HERSTELLER);
  text_setzen(abbild, 20, 16, SUNSPEC_GERAET);
  text_setzen(abbild, 36, 8, SUNSPEC_OPTIONEN);
  text_setzen(abbild, 44, 8, SUNSPEC_VERSION);
  text_setzen(abbild, 52, 16, b.serie);
  abbild[68] = SUNSPEC_GERAETEADRESSE;
  abbild[OFS_ZAEHLER]     = einstellungen().sunspec_modell;
  abbild[OFS_ZAEHLER + 1] = laenge_zaehlermodell();
  if (einstellungen().sunspec_modell == 213) modell_213(abbild + OFS_DATEN, b);
  else                                       modell_203(abbild + OFS_DATEN, b);
  const uint16_t ende = OFS_DATEN + laenge_zaehlermodell();
  abbild[ende]     = 0xFFFF;
  abbild[ende + 1] = 0;
}

// ---------------------------------------------------------------------------
// Eastron
// ---------------------------------------------------------------------------
// Gerechnetes Abbild (für die Solarbank und als Ersatz, wenn Rohregister fehlen)
static void eastron_rechnen(uint16_t* a, const ZaehlerBild& b) {
  float u = 0, i = 0;
  for (int p = 0; p < 3; p++) {
    float_setzen(a, 0x0000 + 2 * p, b.spannung[p]);
    float_setzen(a, 0x0006 + 2 * p, b.strom[p]);
    float_setzen(a, 0x000C + 2 * p, b.phase_w[p]);
    u += b.spannung[p];
    i += b.strom[p];
  }
  float_setzen(a, 0x002A, u / 3.0f);
  float_setzen(a, 0x002E, i / 3.0f);
  float_setzen(a, 0x0030, i);
  float_setzen(a, 0x0034, b.leistung);
  float_setzen(a, 0x003E, b.pf);
  float_setzen(a, 0x0046, b.hz);
  float_setzen(a, 0x0048, b.import_kwh);
  float_setzen(a, 0x004A, b.export_kwh);
  float_setzen(a, 0x0156, b.import_kwh + b.export_kwh);
}

static void eastron_bauen(uint16_t* a, uint16_t* bb, const ZaehlerBild& b, const Messwerte* roh) {
  memset(a, 0, EASTRON_A_ANZAHL * sizeof(uint16_t));
  eastron_rechnen(a, b);
  if (roh != nullptr) {   // echter SDM630: Rohregister darüberlegen
    memcpy(a + ROH_A_START, roh->roh_a, sizeof(roh->roh_a));
    if (roh->roh_b_ok) memcpy(a + ROH_B_START, roh->roh_b, sizeof(roh->roh_b));
    if (roh->roh_c_ok) memcpy(a + ROH_C_START, roh->roh_c, sizeof(roh->roh_c));
  }
  float_setzen(bb, 0, b.leistung > 0 ? b.leistung : 0);
  float_setzen(bb, 2, b.leistung < 0 ? -b.leistung : 0);
}

// ---------------------------------------------------------------------------
// Protokoll und Statistik
// ---------------------------------------------------------------------------
static void melden(int z, uint8_t fc, uint16_t start, uint16_t anzahl, Bereich bereich, Ergebnis e) {
  const FroniusAnfrage a{millis(), (uint8_t)z, fc, start, anzahl, bereich, e};
  portENTER_CRITICAL(&sperre);
  FroniusZaehlerStatistik& s = statistik.zaehler[z];
  s.letzte_anfrage_ms = a.zeit_ms;
  statistik.letzte_anfrage_ms = a.zeit_ms;
  if (e == Ergebnis::BEANTWORTET) s.beantwortet++;
  else if (e == Ergebnis::ABGELEHNT) s.abgelehnt++;
  else s.ohne_antwort++;
  if (bereich == Bereich::SUNSPEC) s.sunspec_anfragen++;
  if (bereich == Bereich::EASTRON) s.eastron_anfragen++;
  protokoll[protokoll_pos] = a;
  protokoll_pos = (protokoll_pos + 1) % PROTOKOLL_EINTRAEGE;
  if (protokoll_anzahl < PROTOKOLL_EINTRAEGE) protokoll_anzahl++;
  portEXIT_CRITICAL(&sperre);
  if (meldungen) xQueueSend(meldungen, &a, 0);
}

// ---------------------------------------------------------------------------
// Anfragen beantworten (läuft im Task des eModbus-Servers)
// ---------------------------------------------------------------------------
static ModbusMessage antworten(const ModbusMessage& anfrage, int z) {
  uint16_t start = 0, anzahl = 0;
  anfrage.get(2, start);
  anfrage.get(4, anzahl);
  const uint8_t fc = anfrage.getFunctionCode();
  const uint32_t ende = (uint32_t)start + anzahl;
  ModbusMessage antwort;

  static uint16_t sunspec[SUNSPEC_MAX];
  static uint16_t eastron_a[EASTRON_A_ANZAHL];
  static uint16_t eastron_b[EASTRON_B_ANZAHL];
  Bereich bereich = Bereich::UNBEKANNT;
  const uint16_t* quelle = nullptr;

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
    melden(z, fc, start, anzahl, bereich, Ergebnis::ABGELEHNT);
    antwort.setError(anfrage.getServerID(), fc, ILLEGAL_DATA_ADDRESS);
    return antwort;
  }

  static Messwerte m;   // groß, deshalb nicht auf dem Stapel (nur dieser Task)
  m = messwerte_lesen();
  const ZaehlerBild b = (z == ZAEHLER_HAUPT) ? bild_netz(m) : bild_solarbank(anker_werte(), m);
  if (!b.aktuell) {   // veraltete Daten: absichtlich keine Antwort
    melden(z, fc, start, anzahl, bereich, Ergebnis::OHNE_ANTWORT);
    return NIL_RESPONSE;
  }
  if (bereich == Bereich::SUNSPEC) sunspec_bauen(sunspec, b);
  else eastron_bauen(eastron_a, eastron_b, b, z == ZAEHLER_HAUPT ? &m : nullptr);

  antwort.add(anfrage.getServerID(), fc, (uint8_t)(anzahl * 2));
  for (uint16_t i = 0; i < anzahl; i++) antwort.add(quelle[i]);
  melden(z, fc, start, anzahl, bereich, Ergebnis::BEANTWORTET);
  return antwort;
}

static ModbusMessage lesen_haupt(ModbusMessage anfrage) { return antworten(anfrage, ZAEHLER_HAUPT); }
static ModbusMessage lesen_zweit(ModbusMessage anfrage) { return antworten(anfrage, ZAEHLER_ZWEIT); }

// ---------------------------------------------------------------------------
// Start: erst Erkennung, dann Server
// ---------------------------------------------------------------------------
static void fronius_task(void*) {
  const Einstellungen& e = einstellungen();
  uint32_t baud = e.fronius_baud, format = e.fronius_format;
  uint8_t adresse = e.fronius_adresse;

  if (baud == 0 || adresse == 0) {
    erkennung_ausfuehren(baud, format, adresse);   // blockiert, bis etwas gefunden ist
    const ErkennungStand s = erkennung_stand();
    baud = s.baud;
    format = s.format;
    adresse = s.adresse;
  }
  uint8_t zweit = e.zweit_adresse;
  if (zweit == adresse || !(e.anker_an && e.anker_ip.length() > 0)) zweit = 0;

  portENTER_CRITICAL(&sperre);
  statistik.baud = baud;
  statistik.format = format;
  statistik.zaehler[ZAEHLER_HAUPT].adresse = adresse;
  statistik.zaehler[ZAEHLER_ZWEIT].adresse = zweit;
  statistik.zustand = FroniusZustand::BEREIT;
  statistik.letzte_anfrage_ms = millis();
  portEXIT_CRITICAL(&sperre);

  Serial2.end();
  RTUutils::prepareHardwareSerial(Serial2);
  Serial2.begin(baud, format, FRONIUS_PIN_RX, FRONIUS_PIN_TX);
  server.registerWorker(adresse, READ_HOLD_REGISTER, &lesen_haupt);
  server.registerWorker(adresse, READ_INPUT_REGISTER, &lesen_haupt);
  if (zweit != 0) {
    server.registerWorker(zweit, READ_HOLD_REGISTER, &lesen_zweit);
    server.registerWorker(zweit, READ_INPUT_REGISTER, &lesen_zweit);
  }
  server.begin(Serial2, 0);
  Serial.printf("[Fronius] bereit: %lu Baud %s, Netzzaehler Adresse %u, Solarbank %s%u\n",
                (unsigned long)baud, format_name(format), adresse, zweit ? "Adresse " : "aus ", zweit);
  vTaskDelete(nullptr);
}

void fronius_start() {
  meldungen = xQueueCreate(32, sizeof(FroniusAnfrage));
  xTaskCreatePinnedToCore(fronius_task, "fronius", 6144, nullptr, 2, nullptr, 0);
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

float fronius_zweit_leistung_w() {
  return -(float)anker_werte().batterie_w * einstellungen().vorzeichen;
}
