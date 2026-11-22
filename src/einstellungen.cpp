// einstellungen.cpp
// ---------------------------------------------------------------------------
// Laden und Speichern der Einstellungen im Flash (Preferences).
// ---------------------------------------------------------------------------
#include "einstellungen.h"
#include "konfiguration.h"
#include <Preferences.h>

static Einstellungen aktuell;
static const char*   BEREICH = "sdm72";

// Text lesen; fehlt der Eintrag, die Vorgabe (ohne Fehlermeldung der Bibliothek)
static String text_lesen(Preferences& p, const char* name, const char* vorgabe) {
  return p.isKey(name) ? p.getString(name, vorgabe) : String(vorgabe);
}

void einstellungen_laden() {
  Preferences p;
  p.begin(BEREICH, false);  // legt den Bereich beim ersten Start an
  aktuell.zaehler_baud    = p.getULong("z_baud", VORGABE_ZAEHLER_BAUD);
  aktuell.zaehler_adresse = p.getUChar("z_adr", VORGABE_ZAEHLER_ADRESSE);
  aktuell.fronius_baud    = p.getULong("f_baud", VORGABE_FRONIUS_BAUD);
  aktuell.fronius_format  = p.getULong("f_form", SERIAL_8N1);
  aktuell.fronius_adresse = p.getUChar("f_adr", VORGABE_FRONIUS_ADRESSE);
  aktuell.sunspec_modell  = p.getUShort("modell", VORGABE_SUNSPEC_MODELL);
  aktuell.vorzeichen      = p.getChar("vorz", VORGABE_VORZEICHEN);
  aktuell.erkannt_baud    = p.getULong("e_baud", 0);
  aktuell.erkannt_format  = p.getULong("e_form", SERIAL_8N1);
  aktuell.erkannt_adresse = p.getUChar("e_adr", 0);
  aktuell.wlan_name       = text_lesen(p, "w_name", "");
  aktuell.wlan_passwort   = text_lesen(p, "w_pw", "");
  aktuell.ap_passwort     = text_lesen(p, "ap_pw", VORGABE_AP_PASSWORT);
  aktuell.mqtt_an         = p.getBool("m_an", false);
  aktuell.mqtt_server     = text_lesen(p, "m_srv", "");
  aktuell.mqtt_port       = p.getUShort("m_port", VORGABE_MQTT_PORT);
  aktuell.mqtt_benutzer   = text_lesen(p, "m_ben", "");
  aktuell.mqtt_passwort   = text_lesen(p, "m_pw", "");
  aktuell.mqtt_ersatz_an  = p.getBool("m_ers_an", true);
  aktuell.mqtt_ersatz_w   = p.getLong("m_ers_w", VORGABE_MQTT_ERSATZ_W);
  p.end();

  // Unsinnige Werte auf die Vorgabe zurücksetzen
  if (aktuell.sunspec_modell != 213 && aktuell.sunspec_modell != 203)
    aktuell.sunspec_modell = VORGABE_SUNSPEC_MODELL;
  if (aktuell.vorzeichen != 1 && aktuell.vorzeichen != -1)
    aktuell.vorzeichen = VORGABE_VORZEICHEN;
  if (aktuell.zaehler_adresse < 1 || aktuell.zaehler_adresse > 247)
    aktuell.zaehler_adresse = VORGABE_ZAEHLER_ADRESSE;
  if (aktuell.ap_passwort.length() < 8)
    aktuell.ap_passwort = VORGABE_AP_PASSWORT;
}

const Einstellungen& einstellungen() {
  return aktuell;
}

void einstellungen_speichern(const Einstellungen& neu) {
  Preferences p;
  p.begin(BEREICH, false);
  p.putULong("z_baud", neu.zaehler_baud);
  p.putUChar("z_adr", neu.zaehler_adresse);
  p.putULong("f_baud", neu.fronius_baud);
  p.putULong("f_form", neu.fronius_format);
  p.putUChar("f_adr", neu.fronius_adresse);
  p.putUShort("modell", neu.sunspec_modell);
  p.putChar("vorz", neu.vorzeichen);
  p.putString("w_name", neu.wlan_name);
  p.putString("w_pw", neu.wlan_passwort);
  p.putString("ap_pw", neu.ap_passwort);
  p.putBool("m_an", neu.mqtt_an);
  p.putString("m_srv", neu.mqtt_server);
  p.putUShort("m_port", neu.mqtt_port);
  p.putString("m_ben", neu.mqtt_benutzer);
  p.putString("m_pw", neu.mqtt_passwort);
  p.putBool("m_ers_an", neu.mqtt_ersatz_an);
  p.putLong("m_ers_w", neu.mqtt_ersatz_w);
  p.end();
  // Die erkannten Werte bleiben unberührt.
  const uint32_t eb = aktuell.erkannt_baud, ef = aktuell.erkannt_format;
  const uint8_t ea = aktuell.erkannt_adresse;
  aktuell = neu;
  aktuell.erkannt_baud = eb;
  aktuell.erkannt_format = ef;
  aktuell.erkannt_adresse = ea;
}

void einstellungen_erkannt_speichern(uint32_t baud, uint32_t format, uint8_t adresse) {
  Preferences p;
  p.begin(BEREICH, false);
  p.putULong("e_baud", baud);
  p.putULong("e_form", format);
  p.putUChar("e_adr", adresse);
  p.end();
  aktuell.erkannt_baud = baud;
  aktuell.erkannt_format = format;
  aktuell.erkannt_adresse = adresse;
}

const char* format_name(uint32_t format) {
  switch (format) {
    case SERIAL_8N1: return "8N1";
    case SERIAL_8E1: return "8E1";
    case SERIAL_8O1: return "8O1";
    case SERIAL_8N2: return "8N2";
    default:         return "?";
  }
}
