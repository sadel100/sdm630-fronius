// energie.cpp
// ---------------------------------------------------------------------------
// Leistungsfluss, Tagesverlauf und Tages-/Monatswerte (siehe energie.h).
// Alles läuft in loop() (auch die Webseite), deshalb ohne Sperre.
// ---------------------------------------------------------------------------
#include "energie.h"
#include "messwerte.h"
#include "anker.h"
#include "solarapi.h"
#include "uhrzeit.h"
#include "ereignisse.h"
#include <Preferences.h>

static const char* BEREICH = "energie";

// ---------------------------------------------------------------------------
// Leistungsfluss
// ---------------------------------------------------------------------------
Fluss energie_fluss() {
  Fluss f;
  const Messwerte m = messwerte_lesen();
  if (messwerte_aktuell(m, CACHE_MAX_ALTER_MS)) f.netz_w = m.leistung_w;

  f.solarapi_aktiv = solarapi_statistik().aktiv;
  if (f.solarapi_aktiv) {
    const SolarApiWerte s = solarapi_werte();
    if (solarapi_aktuell(s)) f.pv_symo_w = isnan(s.pv_w) ? 0.0f : s.pv_w;  // nachts null
  }

  f.solarbank_aktiv = anker_statistik().aktiv;
  if (f.solarbank_aktiv) {
    const AnkerWerte a = anker_werte();
    if (anker_aktuell(a)) {
      f.solarbank_laden_w = -a.batterie_w;
      f.solarbank_pv_w = a.pv_w;
      f.solarbank_w = a.pv_w + a.batterie_w;
      f.soc = a.soc;
    }
  }

  // Haus nur, wenn alle eingerichteten Quellen Werte haben
  const bool pv_ok = !f.solarapi_aktiv || !isnan(f.pv_symo_w);
  const bool bank_ok = !f.solarbank_aktiv || !isnan(f.solarbank_w);
  if (!isnan(f.netz_w) && pv_ok && bank_ok) {
    f.haus_w = f.netz_w + (f.solarapi_aktiv ? f.pv_symo_w : 0.0f) +
               (f.solarbank_aktiv ? f.solarbank_w : 0.0f);
  }
  return f;
}

// ---------------------------------------------------------------------------
// Tagesverlauf
// ---------------------------------------------------------------------------
static VerlaufPunkt verlauf[VERLAUF_PUNKTE];
static size_t       v_naechster = 0, v_anzahl = 0;

struct Mittel {
  float summe = 0;
  uint16_t n = 0;
  void dazu(float v) { if (!isnan(v)) { summe += v; n++; } }
  int16_t wert() const {
    if (n == 0) return VERLAUF_LEER;
    float m = summe / n;
    if (m > 32000) m = 32000;
    if (m < -32000) m = -32000;
    return (int16_t)lroundf(m);
  }
};
static Mittel   mi_netz, mi_pv, mi_bank, mi_haus, mi_soc;
static uint32_t abschnitt_start_ms = 0;
static uint32_t abschnitt_nummer = 0;   // bei bekannter Uhrzeit: Zeit / 300 s

static void punkt_speichern() {
  VerlaufPunkt p;
  p.ms = millis();
  p.netz = mi_netz.wert();
  p.pv_symo = mi_pv.wert();
  p.solarbank = mi_bank.wert();
  p.haus = mi_haus.wert();
  const int16_t s = mi_soc.wert();
  p.soc = s == VERLAUF_LEER ? -1 : (int8_t)s;
  verlauf[v_naechster] = p;
  v_naechster = (v_naechster + 1) % VERLAUF_PUNKTE;
  if (v_anzahl < VERLAUF_PUNKTE) v_anzahl++;
  mi_netz = mi_pv = mi_bank = mi_haus = mi_soc = Mittel();
}

static void verlauf_messen(const Fluss& f) {
  // Abschnitt zu Ende? Mit Uhrzeit an der Uhr ausgerichtet (…:00, …:05),
  // sonst alle 5 Minuten ab dem Start.
  const uint32_t zeit = uhrzeit_jetzt();
  bool ende;
  if (zeit != 0) {
    const uint32_t nr = zeit / VERLAUF_PUNKT_S;
    ende = abschnitt_nummer != 0 && nr != abschnitt_nummer;
    abschnitt_nummer = nr;
  } else {
    ende = (uint32_t)(millis() - abschnitt_start_ms) >= VERLAUF_PUNKT_S * 1000UL;
  }
  if (ende) {
    punkt_speichern();
    abschnitt_start_ms = millis();
  }
  mi_netz.dazu(f.netz_w);
  mi_pv.dazu(f.pv_symo_w);
  mi_bank.dazu(f.solarbank_laden_w);
  mi_haus.dazu(f.haus_w);
  mi_soc.dazu(f.soc);
}

size_t energie_verlauf(VerlaufPunkt* ziel, size_t max) {
  const size_t n = v_anzahl < max ? v_anzahl : max;
  for (size_t i = 0; i < n; i++) {
    ziel[i] = verlauf[(v_naechster + VERLAUF_PUNKTE - n + i) % VERLAUF_PUNKTE];
  }
  return n;
}

// ---------------------------------------------------------------------------
// Tages- und Monatswerte
// ---------------------------------------------------------------------------
// Gespeichert:
//   tag      JJJJMMTT des laufenden Tages     t_start  Zählerstände zu Tagesbeginn
//   monat    JJJJMM des laufenden Monats      m_start  Zählerstände zu Monatsbeginn
//   haus_t   Hausverbrauch heute (Wh)         haus_m   Hausverbrauch des Monats bis gestern (Wh)
//   gestern  EnergieSumme des Vortags         monate   abgeschlossene Monate
struct Speicher {
  uint32_t tag = 0;
  uint32_t monat = 0;
  float    t_start[E_ANZAHL];
  float    m_start[E_ANZAHL];
  double   haus_t_wh = 0;
  double   haus_m_wh = 0;
  EnergieSumme gestern;
  EnergieSumme monate[MONATE_ANZAHL];
};
static Speicher sp;
static uint32_t letzter_takt_ms = 0;
static uint32_t letztes_speichern_ms = 0;

static void leer(float* v) { for (int i = 0; i < E_ANZAHL; i++) v[i] = NAN; }
static void leer(EnergieSumme& s) { s.kennung = 0; leer(s.kwh); }

// Aktuelle Zählerstände in kWh (NAN = nicht verfügbar). Der Hausverbrauch
// hat keinen Zählerstand (bleibt NAN).
static void zaehlerstaende(float* v) {
  leer(v);
  const Messwerte m = messwerte_lesen();
  if (messwerte_aktuell(m, CACHE_MAX_ALTER_MS)) {
    v[E_BEZUG] = m.bezug_kwh;
    v[E_EINSPEISUNG] = m.einspeisung_kwh;
  }
  const SolarApiWerte s = solarapi_werte();
  if (solarapi_statistik().aktiv && solarapi_aktuell(s) && !isnan(s.gesamt_wh) && s.gesamt_wh > 0) {
    v[E_PV_SYMO] = s.gesamt_wh / 1000.0;
  }
  const AnkerWerte a = anker_werte();
  if (anker_statistik().aktiv && anker_aktuell(a)) {
    v[E_GELADEN] = a.lade_kwh;
    v[E_ENTLADEN] = a.entlade_kwh;
  }
}

static void speichern_alles() {
  Preferences p;
  p.begin(BEREICH, false);
  p.putULong("tag", sp.tag);
  p.putULong("monat", sp.monat);
  p.putBytes("t_start", sp.t_start, sizeof(sp.t_start));
  p.putBytes("m_start", sp.m_start, sizeof(sp.m_start));
  p.putDouble("haus_t", sp.haus_t_wh);
  p.putDouble("haus_m", sp.haus_m_wh);
  p.putBytes("gestern", &sp.gestern, sizeof(sp.gestern));
  p.putBytes("monate", sp.monate, sizeof(sp.monate));
  p.end();
  letztes_speichern_ms = millis();
}

static void haus_speichern() {
  Preferences p;
  p.begin(BEREICH, false);
  p.putDouble("haus_t", sp.haus_t_wh);
  p.putBytes("t_start", sp.t_start, sizeof(sp.t_start));   // nachgetragene Startwerte
  p.putBytes("m_start", sp.m_start, sizeof(sp.m_start));
  p.end();
  letztes_speichern_ms = millis();
}

void energie_start() {
  leer(sp.t_start);
  leer(sp.m_start);
  leer(sp.gestern);
  for (auto& m : sp.monate) leer(m);
  Preferences p;
  p.begin(BEREICH, false);
  sp.tag = p.getULong("tag", 0);
  sp.monat = p.getULong("monat", 0);
  if (p.isKey("t_start")) p.getBytes("t_start", sp.t_start, sizeof(sp.t_start));
  if (p.isKey("m_start")) p.getBytes("m_start", sp.m_start, sizeof(sp.m_start));
  sp.haus_t_wh = p.isKey("haus_t") ? p.getDouble("haus_t", 0) : 0;
  sp.haus_m_wh = p.isKey("haus_m") ? p.getDouble("haus_m", 0) : 0;
  if (p.isKey("gestern") && p.getBytesLength("gestern") == sizeof(sp.gestern)) p.getBytes("gestern", &sp.gestern, sizeof(sp.gestern));
  if (p.isKey("monate") && p.getBytesLength("monate") == sizeof(sp.monate)) p.getBytes("monate", sp.monate, sizeof(sp.monate));
  p.end();
  abschnitt_start_ms = millis();
}

// Differenz zweier Zählerstände (NAN, wenn einer fehlt oder der Zähler zurückläuft)
static float differenz(float jetzt, float start) {
  if (isnan(jetzt) || isnan(start)) return NAN;
  const float d = jetzt - start;
  return d < 0 ? NAN : d;
}

static EnergieSumme summe_heute(const float* jetzt) {
  EnergieSumme s;
  s.kennung = sp.tag;
  for (int i = 0; i < E_ANZAHL; i++) s.kwh[i] = differenz(jetzt[i], sp.t_start[i]);
  s.kwh[E_HAUS] = sp.haus_t_wh / 1000.0;
  return s;
}

static EnergieSumme summe_monat(const float* jetzt) {
  EnergieSumme s;
  s.kennung = sp.monat;
  for (int i = 0; i < E_ANZAHL; i++) s.kwh[i] = differenz(jetzt[i], sp.m_start[i]);
  s.kwh[E_HAUS] = (sp.haus_m_wh + sp.haus_t_wh) / 1000.0;
  return s;
}

static void tageswechsel(uint32_t heute, const float* jetzt) {
  const uint32_t monat_neu = heute / 100;
  if (sp.tag != 0) {
    sp.gestern = summe_heute(jetzt);
    sp.haus_m_wh += sp.haus_t_wh;
  }
  if (sp.monat != monat_neu) {
    if (sp.monat != 0) {
      // Abgeschlossenen Monat vorn einreihen
      for (size_t i = MONATE_ANZAHL - 1; i > 0; i--) sp.monate[i] = sp.monate[i - 1];
      sp.monate[0] = summe_monat(jetzt);
      sp.monate[0].kwh[E_HAUS] = sp.haus_m_wh / 1000.0;   // haus_t ist schon enthalten
    }
    sp.monat = monat_neu;
    memcpy(sp.m_start, jetzt, sizeof(sp.m_start));
    sp.haus_m_wh = 0;
  }
  sp.tag = heute;
  memcpy(sp.t_start, jetzt, sizeof(sp.t_start));
  sp.haus_t_wh = 0;
  speichern_alles();
}

void energie_schritt() {
  if ((uint32_t)(millis() - letzter_takt_ms) < ENERGIE_TAKT_MS) return;
  const uint32_t dauer_ms = letzter_takt_ms == 0 ? 0 : millis() - letzter_takt_ms;
  letzter_takt_ms = millis();

  const Fluss f = energie_fluss();
  verlauf_messen(f);

  const uint32_t zeit = uhrzeit_jetzt();
  if (zeit == 0) return;   // ohne Uhrzeit keine Tageswerte

  float jetzt[E_ANZAHL];
  zaehlerstaende(jetzt);

  struct tm t;
  uhrzeit_lokal(zeit, t);
  const uint32_t heute = (t.tm_year + 1900) * 10000UL + (t.tm_mon + 1) * 100UL + t.tm_mday;
  if (heute != sp.tag) {
    // Kurz nach dem Start auf die ersten Zählerwerte warten (höchstens 2 min)
    if (isnan(jetzt[E_BEZUG]) && millis() < 120000UL) return;
    tageswechsel(heute, jetzt);
    return;
  }

  // Erst später verfügbare Zähler (z. B. Solarbank nach WLAN-Ausfall) ab jetzt zählen
  for (int i = 0; i < E_ANZAHL; i++) {
    if (i == E_HAUS) continue;
    if (isnan(sp.t_start[i]) && !isnan(jetzt[i])) sp.t_start[i] = jetzt[i];
    if (isnan(sp.m_start[i]) && !isnan(jetzt[i])) sp.m_start[i] = jetzt[i];
  }

  // Hausverbrauch aufsummieren (nur Verbrauch, Werte unter 0 wären Messfehler)
  if (!isnan(f.haus_w) && dauer_ms > 0 && dauer_ms < 60000) {
    sp.haus_t_wh += (f.haus_w > 0 ? f.haus_w : 0) * dauer_ms / 3600000.0;
  }
  if ((uint32_t)(millis() - letztes_speichern_ms) >= HAUS_SPEICHERN_MS) haus_speichern();
}

EnergieStand energie_stand() {
  EnergieStand s;
  s.uhrzeit = uhrzeit_bekannt();
  float jetzt[E_ANZAHL];
  zaehlerstaende(jetzt);
  if (s.uhrzeit) {
    s.heute = summe_heute(jetzt);
    s.monat = summe_monat(jetzt);
  } else {
    leer(s.heute);
    leer(s.monat);
  }
  s.gestern = sp.gestern;
  memcpy(s.monate, sp.monate, sizeof(s.monate));
  return s;
}
