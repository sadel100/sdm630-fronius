// erkennung.cpp
// ---------------------------------------------------------------------------
// Erkennung von Baudrate, Format und Busadresse auf Bus B (siehe erkennung.h).
//
// So wird ein Modbus-RTU-Rahmen erkannt: Zwischen zwei Rahmen ist die Leitung
// mindestens 3,5 Zeichen lang still. Alle Bytes, die ohne längere Pause
// ankommen, gehören zu einem Rahmen. Eine Leseanfrage (Funktionscode 03 oder
// 04) ist genau 8 Bytes lang: Adresse, Funktion, Start (2), Anzahl (2), CRC (2).
// ---------------------------------------------------------------------------
#include "erkennung.h"
#include "konfiguration.h"
#include "einstellungen.h"

static ErkennungStand stand;
static portMUX_TYPE   sperre = portMUX_INITIALIZER_UNLOCKED;

struct Moeglichkeit {
  uint32_t baud;
  uint32_t format;
};

// Reihenfolge: das Wahrscheinlichste zuerst
static const Moeglichkeit MOEGLICHKEITEN[] = {
  {9600, SERIAL_8N1},  {19200, SERIAL_8N1}, {9600, SERIAL_8E1},  {19200, SERIAL_8E1},
  {38400, SERIAL_8N1}, {4800, SERIAL_8N1},  {2400, SERIAL_8N1},  {115200, SERIAL_8N1},
  {9600, SERIAL_8N2},  {9600, SERIAL_8O1},
};

uint16_t modbus_crc(const uint8_t* daten, size_t laenge) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < laenge; i++) {
    crc ^= daten[i];
    for (int b = 0; b < 8; b++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc;
}

static void stand_setzen(const ErkennungStand& s) {
  portENTER_CRITICAL(&sperre);
  stand = s;
  portEXIT_CRITICAL(&sperre);
}

ErkennungStand erkennung_stand() {
  portENTER_CRITICAL(&sperre);
  ErkennungStand kopie = stand;
  portEXIT_CRITICAL(&sperre);
  return kopie;
}

// Prüft, ob ein Rahmen eine gültige Leseanfrage ist. Liefert die Adresse oder 0.
static uint8_t anfrage_pruefen(const uint8_t* r, size_t n) {
  if (n != 8) return 0;
  if (r[0] < 1 || r[0] > 247) return 0;
  if (r[1] != 0x03 && r[1] != 0x04) return 0;
  const uint16_t crc = modbus_crc(r, 6);
  if (r[6] != (crc & 0xFF) || r[7] != (crc >> 8)) return 0;
  return r[0];
}

// Hört eine Weile mit einer Einstellung zu. Liefert die häufigste Adresse,
// wenn genug gültige Anfragen kamen, sonst 0.
static uint8_t lauschen(const Moeglichkeit& m, uint8_t fest_adresse, ErkennungStand& s) {
  // begin() stellt einen laufenden UART direkt um (ein end() davor meldet im Core 3.0 einen Treiberfehler)
  Serial2.begin(m.baud, m.format, FRONIUS_PIN_RX, FRONIUS_PIN_TX);
  // DE fest auf "Empfang": der ESP32 sendet während der Erkennung nie.
  pinMode(FRONIUS_PIN_DE, OUTPUT);
  digitalWrite(FRONIUS_PIN_DE, LOW);
  while (Serial2.available()) Serial2.read();

  // Pause, die einen Rahmen beendet: 3,5 Zeichen à 11 Bit, mindestens 1,75 ms
  const uint32_t pause_us = max<uint32_t>(1750, (uint32_t)(3.5 * 11 * 1000000.0 / m.baud));

  uint8_t  rahmen[64];
  size_t   laenge = 0;
  uint32_t letztes_byte_us = micros();
  uint16_t treffer[248] = {0};
  uint16_t gueltig = 0;

  const uint32_t start = millis();
  while ((uint32_t)(millis() - start) < ERKENNUNG_LAUSCHEN_MS) {
    while (Serial2.available()) {
      const int b = Serial2.read();
      if (b < 0) break;
      s.bytes_gesehen++;
      if (laenge < sizeof(rahmen)) rahmen[laenge] = (uint8_t)b;
      laenge++;
      letztes_byte_us = micros();
    }
    if (laenge > 0 && (uint32_t)(micros() - letztes_byte_us) > pause_us) {
      const uint8_t adr = anfrage_pruefen(rahmen, laenge);
      if (adr != 0 && (fest_adresse == 0 || adr == fest_adresse)) {
        treffer[adr]++;
        gueltig++;
      }
      laenge = 0;
    }
    vTaskDelay(1);
  }

  if (gueltig < ERKENNUNG_MIN_ANFRAGEN) return 0;
  uint8_t beste = 0;
  for (int a = 1; a <= 247; a++) {
    if (treffer[a] > (beste ? treffer[beste] : 0)) beste = a;
  }
  return beste;
}

void erkennung_ausfuehren(uint32_t fest_baud, uint32_t fest_format, uint8_t fest_adresse) {
  ErkennungStand s;
  s.laeuft = true;
  stand_setzen(s);

  for (;;) {
    for (const Moeglichkeit& m : MOEGLICHKEITEN) {
      Moeglichkeit probe = m;
      if (fest_baud != 0) probe = {fest_baud, fest_format};
      s.baud = probe.baud;
      s.format = probe.format;
      stand_setzen(s);
      Serial.printf("[Erkennung] hoere zu: %lu Baud %s\n", (unsigned long)probe.baud,
                    format_name(probe.format));
      const uint8_t adr = lauschen(probe, fest_adresse, s);
      if (adr != 0) {
        s.adresse = adr;
        s.fertig = true;
        s.laeuft = false;
        stand_setzen(s);
        Serial2.end();
        Serial.printf("[Erkennung] gefunden: %lu Baud %s, Adresse %u\n",
                      (unsigned long)probe.baud, format_name(probe.format), adr);
        einstellungen_erkannt_speichern(probe.baud, probe.format, adr);
        return;
      }
      if (fest_baud != 0) break;  // nur die feste Einstellung prüfen
    }
    s.durchlaeufe++;
    stand_setzen(s);
  }
}
