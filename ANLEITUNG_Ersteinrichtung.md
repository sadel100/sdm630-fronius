# Ersteinrichtung: Zähler-Box „SDM630 → Fronius“

Diese kleine Box (ein ESP32) liest

- den neuen Netzzähler **Eastron SDM630** an deinem Netzanschluss und
- deine **Anker Solarbank Max AC** über das WLAN

und gibt die Werte weiter:

- an deinen **Fronius Symo** als zwei Zähler: Netzanschluss und Solarbank (für Solar.web),
- an deine **openWB** (Netzanschluss und Speicher, damit sie das Auto mit Überschuss laden kann),
- an eine **Webseite**, die du mit Handy oder PC öffnen kannst.

Der bisherige Zähler am S0-Eingang des Fronius (SDM72DR) wird dafür nicht mehr gebraucht.
Deine Solarbank regelt weiter selbst mit ihrem Anker Smart Meter. Die Box liest sie nur aus und
verändert an ihr nichts.

Die Box ist schon fertig programmiert. Du musst nichts flashen.

---

## Was du brauchst

- die Box (ESP32 mit zwei kleinen RS485-Platinen)
- ein 5-V-Hutschienennetzteil
- zweiadrige Leitung (am besten verdrillt, z. B. ein Aderpaar aus einem Netzwerkkabel)
  - vom SDM630 zur Box
  - von der Box zum Fronius Datamanager
- ein Handy oder einen Laptop mit WLAN
- das Passwort deines WLANs
- die IP-Adressen deiner **openWB**, deiner **Solarbank** und deines **Fronius** (stehen im Router,
  z. B. in der Fritzbox unter „Heimnetz“; am besten dort allen dreien eine feste Adresse geben)

> ⚠️ **Sicherheit:** Der SDM630 sitzt im Zählerschrank an 230/400 V. Den Einbau des Zählers
> macht **nur eine Elektrofachkraft**. Die Leitungen zur Box führen nur Kleinspannung,
> sie dürfen aber nicht zusammen mit 230-V-Leitungen an offenen Klemmen hängen.

---

## Schritt 1: Box einschalten und verbinden

1. Box mit dem Netzteil verbinden. Noch keine Leitungen anschließen.
2. Am Handy die WLAN-Liste öffnen. Nach etwa 10 Sekunden erscheint das WLAN **„SDM630-Fronius“**.
3. Damit verbinden. Passwort: **sdm630fronius**
   (Das Handy meldet vielleicht „kein Internet“. Einfach verbunden bleiben.)
4. Im Browser die Adresse **http://192.168.4.1** öffnen.

Du siehst die Seite der Box mit den fünf Reitern **Übersicht, Verlauf, Diagnose, Einstellungen,
So funktioniert's**. Oben stehen farbige Punkte:

| Punkt | Grün heißt | Rot/Orange heißt |
|---|---|---|
| Netzzähler | SDM630 antwortet | antwortet nicht |
| Fronius | Fronius fragt ab | Box sucht noch / keine Anfragen |
| Solarbank | Solarbank antwortet | antwortet nicht |
| Heimnetz | Box ist in deinem WLAN | nicht verbunden |
| openWB | Werte gehen an die openWB | keine Verbindung |

Jetzt ist noch alles rot, orange oder grau. Das ist richtig so.

---

## Schritt 2: Eigenes Passwort und dein WLAN

Reiter **Einstellungen**:

1. **Heimnetz (WLAN):** auf „WLANs suchen“ tippen, in der Liste darunter dein WLAN auswählen,
   dein WLAN-Passwort eintragen.
2. **Zugang zu diesem Gerät:** ein neues Passwort ausdenken (mindestens 8 Zeichen) und eintragen.
   **Gut aufschreiben!** Es gilt danach für das Geräte-WLAN und zum Speichern auf der Seite.
3. Ganz unten **„Speichern und neu starten“**.
4. Der Browser fragt nach einer Anmeldung: Benutzer **admin**, Passwort **sdm630fronius**
   (beim ersten Mal noch das alte Passwort).
5. Die Box startet neu (etwa 10 Sekunden).

Ab jetzt erreichst du die Seite auch in deinem normalen WLAN unter
**http://sdm630-fronius.local**. Klappt das nicht (manche Android-Handys kennen solche Namen nicht),
steht die IP-Adresse der Box im Router, Gerätename „sdm630-fronius“.

---

## Schritt 3: Netzzähler SDM630 anschließen

1. Am SDM630 im Setup-Menü nachsehen: **Adresse** (meist 1), **Baudrate** (meist 9600) und
   **Parität** (None oder Even). Aufschreiben.
2. Box vom Strom trennen.
3. Leitung vom SDM630 an die Klemmen **„Zähler“** der Box: **A an A, B an B**, GND, falls vorhanden.
4. Box einschalten, Seite öffnen. Unter **Einstellungen → Netzzähler SDM630** Baudrate, Format
   (8N1 = keine Parität, 8E1 = Even) und Adresse wie am Zähler einstellen, speichern.

**Prüfen (Reiter Übersicht):**
- Der Punkt **Netzzähler** ist grün.
- Oben steht die Leistung am Netzanschluss: **orange und positiv** = Bezug aus dem Netz,
  **grün und negativ** = Einspeisung.
- Die Zählerstände (Bezug und Einspeisung in kWh) stimmen mit der Anzeige am SDM630 überein.

**Bleibt er rot?** A und B vertauschen (häufigster Fehler), dann Parität und Adresse prüfen.

---

## Schritt 4: Solarbank einrichten

**In der Anker-App:** Gerät antippen → Zahnrad → **„Three-Party Control Settings“** →
**„Modbus TCP“** einschalten. Dort steht auch die IP-Adresse der Solarbank.

**Auf der Seite der Box** (Einstellungen → Anker Solarbank):
1. Haken bei **„Solarbank abfragen“**.
2. **IP-Adresse** der Solarbank eintragen. Port **502** und Unit-ID **1** so lassen.
3. Speichern.

**Prüfen:** Der Punkt **Solarbank** ist grün, in der Übersicht stehen Ladestand und
„lädt mit …“ bzw. „entlädt mit …“. Vergleiche mit der Anker-App.

---

## Schritt 5: openWB einrichten

**Auf der Seite der Box** (Einstellungen → openWB):
1. Haken bei **„Werte an die openWB senden“** und **„Solarbank als Speicher an die openWB senden“**.
2. **Adresse der openWB** eintragen. Port **1883**, Benutzer und Passwort leer lassen.
3. Haken bei **„Bei Zählerausfall Ersatzwert senden“** drin lassen (10 000 W).
4. Speichern.

**In der openWB** (Software 1.9), Einstellungen → Modulkonfiguration:
1. **EVU:** Modul **„MQTT“** wählen und speichern.
2. **Speicher:** Modul **„MQTT“** wählen und speichern.

**Prüfen:** Auf der Box ist der Punkt **openWB** grün. Auf der Startseite der openWB steht bei
**EVU** dieselbe Leistung wie auf der Box, beim **Speicher** Ladestand und Leistung der Solarbank
(Laden positiv).

Warum der Ersatzwert? Fällt der Netzzähler aus, würde die openWB sonst mit dem letzten Wert
weiterladen. Mit dem hohen Ersatzwert sieht sie „kein Überschuss“ und hört auf, aus PV zu laden.

---

## Schritt 6: Fronius einrichten

**Im Fronius Datamanager** (Weboberfläche des Wechselrichters, Einstellungen → Zähler):
1. Den bisherigen **S0-Zähler** entfernen bzw. abwählen.
2. **Primärzähler:** „Fronius Smart Meter“, Einbauort **Einspeisepunkt**. Speichern.
3. **Sekundärzähler:** „Fronius Smart Meter“ hinzufügen, Bezeichnung z. B. „Solarbank“,
   Modbus-Adresse **2**. Speichern.

**Leitung anschließen:**
1. Box vom Strom trennen.
2. Leitung von den Zähler-Klemmen des Datamanagers (Modbus/RS485, meist „D+ / D−“) an die
   Klemmen **„Fronius“** der Box.
3. Box einschalten.

**Was jetzt passiert:** Die Box hört zuerst nur zu und findet selbst heraus, wie der Fronius
fragt. Das dauert ein paar Sekunden bis etwa eine Minute. Unter **Diagnose** siehst du
„Erkennung läuft“ und danach in der Tabelle die Anfragen an **Netzzähler** und **Solarbank**.

**Prüfen:**
- Der Punkt **Fronius** ist grün, unter Diagnose sind bei beiden Zählern Anfragen „beantwortet“.
- Im Datamanager werden beide Zähler ohne Fehler angezeigt.
- In **Solar.web** stimmen nach ein paar Minuten Einspeisung und Verbrauch. Die Solarbank erscheint
  dort als eigene Kurve (Laden positiv). Zeigt Solar.web Einspeisung als Bezug:
  **Einstellungen → Fronius → Vorzeichen umdrehen**.

**PV-Leistung und Hausverbrauch (Solar API):** Damit die Box auch weiß, was der Symo gerade erzeugt,
liest sie seine Leistung über das WLAN (nur lesen).
1. Im Datamanager die **Solar API** einschalten (Einstellungen → Kommunikation bzw. „Solar API“).
2. Auf der Box unter **Einstellungen → Fronius**: Haken bei **„PV-Leistung über die Solar API lesen“**,
   die **Adresse des Fronius** eintragen, speichern.
3. Prüfen: Auf der Übersicht zeigt der **Energiefluss** die PV-Leistung und den Hausverbrauch.

**Klappt es nicht?**

| Diagnose zeigt | Was tun |
|---|---|
| „Auf der Leitung zum Fronius kommt nichts an“ | Leitung prüfen, A und B tauschen, im Datamanager „Fronius Smart Meter“ eingestellt? |
| Solarbank: „noch keine Anfrage an Adresse 2“ | Sekundärzähler im Datamanager angelegt? Adresse dort und auf der Box gleich? |
| viele Zeilen **„abgelehnt“** in der Tabelle | Foto von der Tabelle machen und weitergeben, dann kann die Box angepasst werden |
| „fragt, bekommt aber keine Antwort“ | Netzzähler bzw. Solarbank antwortet nicht (Schritt 3 bzw. 4) |
| Datamanager nimmt die Zähler nicht an | Einstellungen → Fronius → SunSpec-Modell auf **203**, speichern, ausprobieren |
| Solar API „antwortet nicht“ | Solar API im Datamanager eingeschaltet? Adresse des Fronius richtig? |

> Das ist der Teil, der vorher nicht getestet werden konnte. Wenn hier etwas nicht klappt,
> bitte **Fotos vom Reiter „Diagnose“** (ganz nach unten scrollen) schicken.

---

## Wenn alles grün ist

Fertig. Die Box braucht keine Pflege. Nach einem Stromausfall startet sie von selbst und behält
alle Einstellungen.

**Gut zu wissen:**
- Die Seite kannst du jederzeit unter **http://sdm630-fronius.local** ansehen.
- Unter **Verlauf** siehst du die letzten 24 Stunden als Kurve und die Energie von heute, gestern,
  diesem Monat und den letzten 12 Monaten. Die Tageswerte beginnen um Mitternacht.
- Unter **Diagnose → Ereignisse** steht, wann etwas ausgefallen und wiedergekommen ist.
- **Etwas stimmt nicht?** Unter **Diagnose** ganz unten auf **„Diagnose-Datei herunterladen“** tippen
  und die Datei per Mail schicken. Passwörter stehen nicht darin.
- Unter **„So funktioniert's“** steht noch einmal, was die Box macht und was bei Störungen passiert.
- Kommst du nicht mehr an die Seite: mit dem Handy ins WLAN **„SDM630-Fronius“** (dein eigenes
  Passwort aus Schritt 2), dann **http://192.168.4.1**.
- Passwort vergessen: Die Box muss am PC neu programmiert werden.

## Neue Version einspielen (Update)

Unter **Einstellungen → Firmware-Update**, ganz unten:

- **Von GitHub:** „Nach Updates suchen“. Gibt es eine neue Version, steht dort, was sich ändert.
  „Installieren“ antippen, mit admin und deinem Passwort anmelden. Die Box lädt die neue Version
  und startet neu (etwa 1 Minute). Dafür braucht die Box Internet.
- **Aus einer Datei:** Hast du eine Datei `sdm630-fronius-….bin` bekommen: „Datei auswählen“,
  „Datei hochladen und installieren“. Geht auch ohne Internet.

Während des Updates den Strom nicht trennen. Deine Einstellungen bleiben erhalten. Läuft die neue
Version nicht, startet die Box nach einem Absturz von selbst wieder mit der alten Version.

## Was bei Störungen passiert

| Störung | Fronius | openWB |
|---|---|---|
| Netzzähler antwortet nicht | bekommt für den Netzzähler keine Antwort, meldet Zählerfehler | EVU: 10 000 W Bezug, hört auf, aus PV zu laden |
| Solarbank antwortet nicht | bekommt für den zweiten Zähler keine Antwort | Speicher: 0 W |
| WLAN weg | Netzzähler läuft weiter, Solarbank-Zähler antwortet nicht mehr | bekommt keine Werte (letzter Wert bleibt stehen) |
| Box ohne Strom | meldet Zählerfehler | bekommt keine Werte (letzter Wert bleibt stehen) |

Bei den letzten beiden Fällen kann die openWB mit einem alten Wert weiterladen, bis die Box wieder
läuft. Wenn das Auto dann aus dem Netz lädt, kurz in der openWB nachsehen.
