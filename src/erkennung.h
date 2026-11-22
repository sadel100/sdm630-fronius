// erkennung.h
// ---------------------------------------------------------------------------
// Findet heraus, mit welcher Baudrate, welchem Format (z. B. 8N1) und an
// welche Busadresse der Fronius Datamanager seine Anfragen schickt.
//
// Der ESP32 hört dafür auf Bus B nur zu (sendet nichts) und probiert der
// Reihe nach mehrere Baudraten und Formate. Gültig ist eine Einstellung, wenn
// mehrere vollständige Modbus-Anfragen mit richtiger Prüfsumme (CRC) ankommen.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

struct ErkennungStand {
  bool     laeuft   = false;
  bool     fertig   = false;
  uint32_t baud     = 0;    // gerade geprüft bzw. gefunden
  uint32_t format   = 0;
  uint8_t  adresse  = 0;    // gefunden
  uint32_t durchlaeufe = 0; // wie oft alle Möglichkeiten probiert wurden
  uint32_t bytes_gesehen = 0;  // irgendetwas auf der Leitung?
};

// Hört so lange zu, bis eine Einstellung gefunden ist (blockiert, deshalb in
// einem eigenen Task aufrufen). `fest_baud`/`fest_adresse` != 0 schränken die
// Suche ein.
void erkennung_ausfuehren(uint32_t fest_baud, uint32_t fest_format, uint8_t fest_adresse);

ErkennungStand erkennung_stand();

// Modbus-CRC16 (auch für Tests der Rahmen nützlich)
uint16_t modbus_crc(const uint8_t* daten, size_t laenge);
