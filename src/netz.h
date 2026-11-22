// netz.h
// ---------------------------------------------------------------------------
// WLAN: Der ESP32 öffnet immer ein eigenes WLAN (Notzugang, Adresse
// 192.168.4.1) und verbindet sich zusätzlich mit dem Heimnetz, wenn dafür
// Name und Passwort eingestellt sind.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

void netz_start();

bool   netz_heimnetz_verbunden();
String netz_heimnetz_ip();
int    netz_empfang_dbm();   // nur gültig, wenn verbunden
String netz_ap_ip();
