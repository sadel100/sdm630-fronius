// messwerte.cpp
// ---------------------------------------------------------------------------
// Zwischenspeicher für die Zählerwerte, geschützt durch einen Mutex.
// ---------------------------------------------------------------------------
#include "messwerte.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static Messwerte         speicher;
static SemaphoreHandle_t sperre = nullptr;

void messwerte_start() {
  sperre = xSemaphoreCreateMutex();
}

void messwerte_schreiben(const Messwerte& neu) {
  xSemaphoreTake(sperre, portMAX_DELAY);
  speicher = neu;
  xSemaphoreGive(sperre);
}

Messwerte messwerte_lesen() {
  xSemaphoreTake(sperre, portMAX_DELAY);
  Messwerte kopie = speicher;
  xSemaphoreGive(sperre);
  return kopie;
}

bool messwerte_aktuell(const Messwerte& m, uint32_t max_alter_ms) {
  if (!m.gueltig) return false;
  return (uint32_t)(millis() - m.zeitstempel_ms) <= max_alter_ms;
}
