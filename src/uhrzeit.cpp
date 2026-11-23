// uhrzeit.cpp
// ---------------------------------------------------------------------------
// Uhrzeit (siehe uhrzeit.h). Der ESP32 stellt seine Uhr selbst nach, sobald
// das Heimnetz verbunden ist, und gleicht sie danach regelmäßig ab.
// ---------------------------------------------------------------------------
#include "uhrzeit.h"
#include "konfiguration.h"

// Vor 2024 kann die Uhr nicht stimmen: dann ist sie noch nicht gestellt.
static constexpr uint32_t GUELTIG_AB = 1704067200UL;  // 01.01.2024

void uhrzeit_start() {
  configTzTime(ZEITZONE, ZEITSERVER_1, ZEITSERVER_2, ZEITSERVER_3);
}

bool uhrzeit_bekannt() {
  return (uint32_t)time(nullptr) >= GUELTIG_AB;
}

uint32_t uhrzeit_jetzt() {
  const uint32_t t = (uint32_t)time(nullptr);
  return t >= GUELTIG_AB ? t : 0;
}

void uhrzeit_lokal(uint32_t zeit, struct tm& t) {
  const time_t z = (time_t)zeit;
  localtime_r(&z, &t);
}

void uhrzeit_text(uint32_t zeit, char* ziel, size_t groesse) {
  if (zeit == 0) {
    snprintf(ziel, groesse, "-");
    return;
  }
  struct tm t;
  uhrzeit_lokal(zeit, t);
  strftime(ziel, groesse, "%d.%m.%Y %H:%M:%S", &t);
}
