# Ersteinrichtung: Zähler-Box „SDM72 → Fronius“

Diese kleine Box (ein ESP32) liest den neuen Zähler **Eastron SDM72D-M** an deinem Netzanschluss
aus und gibt die Werte weiter:

- an deinen **Fronius Symo** (für Solar.web und die Verbrauchsanzeige),
- an deine **openWB** (damit sie das Auto mit Überschuss laden kann),
- an eine **Webseite**, die du mit Handy oder PC öffnen kannst.

Der bisherige Zähler am S0-Eingang des Fronius (SDM72DR) wird dafür nicht mehr gebraucht.
Deine **Anker Solarbank** bleibt, wie sie ist. Sie regelt weiter mit ihrem eigenen Anker Smart Meter.

Die Box ist schon fertig programmiert. Du musst nichts flashen.

---

## Was du brauchst

- die Box (ESP32 mit zwei kleinen RS485-Platinen)
- ein USB-Netzteil (5 V, mindestens 1 A) und ein USB-Kabel
- zweiadrige Leitung (am besten verdrillt, z. B. eine Ader-Paar aus einem Netzwerkkabel)
  - vom Zähler zur Box
  - von der Box zum Fronius Datamanager
- ein Handy oder einen Laptop mit WLAN
- das Passwort deines WLANs
- die IP-Adresse deiner openWB (steht im Router, z. B. in der Fritzbox unter „Heimnetz“)

> ⚠️ **Sicherheit:** Der Zähler SDM72D-M sitzt im Zählerschrank an 230/400 V. Den Einbau des
> Zählers macht **nur eine Elektrofachkraft**. Die Leitungen zur Box führen nur Kleinspannung,
> sie dürfen aber nicht zusammen mit 230-V-Leitungen an offenen Klemmen hängen.

---

## Schritt 1: Box einschalten und verbinden

1. Box mit dem USB-Netzteil verbinden. Noch keine Leitungen anschließen.
2. Am Handy die WLAN-Liste öffnen. Nach etwa 10 Sekunden erscheint das WLAN **„SDM72-Fronius“**.
3. Damit verbinden. Passwort: **sdm72fronius**
   (Das Handy meldet vielleicht „kein Internet“. Einfach verbunden bleiben.)
4. Im Browser die Adresse **http://192.168.4.1** öffnen.

Du siehst jetzt die Seite der Box mit den vier Reitern **Übersicht, Diagnose, Einstellungen,
So funktioniert's**. Oben stehen vier farbige Punkte:

| Punkt | Grün heißt | Rot/Orange heißt |
|---|---|---|
| Zähler | Zähler antwortet | Zähler antwortet nicht |
| Fronius | Fronius fragt ab | Box sucht noch / keine Anfragen |
| Heimnetz | Box ist in deinem WLAN | nicht verbunden |
| openWB | Werte gehen an die openWB | keine Verbindung |

Jetzt ist noch alles rot oder orange. Das ist richtig so.

---

## Schritt 2: Eigenes Passwort und dein WLAN

Reiter **Einstellungen**:

1. **Heimnetz (WLAN):** auf „WLANs suchen“ tippen, in der Liste darunter dein WLAN auswählen,
   dein WLAN-Passwort eintragen.
2. **Zugang zu diesem Gerät:** ein neues Passwort ausdenken (mindestens 8 Zeichen) und eintragen.
   **Gut aufschreiben!** Es gilt danach für das WLAN „SDM72-Fronius“ und zum Speichern auf der Seite.
3. Ganz unten **„Speichern und neu starten“**.
4. Der Browser fragt nach einer Anmeldung: Benutzer **admin**, Passwort **sdm72fronius**
   (beim ersten Mal noch das alte Passwort).
5. Die Box startet neu (etwa 10 Sekunden).

Ab jetzt erreichst du die Seite auch in deinem normalen WLAN unter
**http://sdm72-fronius.local**. Klappt das nicht (manche Android-Handys kennen solche Namen nicht),
steht die IP-Adresse der Box im Router, Gerätename „sdm72-fronius“.

---

## Schritt 3: Zähler anschließen

1. Box vom Strom trennen.
2. Leitung vom Zähler an die Klemmen **„Zähler“** der Box: **A an A, B an B**
   (am SDM72D-M heißen die Klemmen ebenfalls A und B), GND, falls vorhanden.
3. Box wieder einschalten, Seite öffnen.

**Prüfen (Reiter Übersicht):**
- Der Punkt **Zähler** ist grün.
- Oben steht die Leistung am Netzanschluss:
  - **orange und positiv** = du beziehst gerade Strom aus dem Netz,
  - **grün und negativ** = die Anlage speist gerade ein.
- Vergleiche die Zählerstände (Bezug und Einspeisung in kWh) mit der Anzeige am Zähler.
  Sie müssen übereinstimmen.

**Zähler bleibt rot?**
- A und B vertauschen (das ist der häufigste Fehler).
- Im Setup-Menü des Zählers Baudrate und Adresse nachsehen (meist 9600 und 1).
  Unter **Einstellungen → Zähler SDM72D-M** genauso eintragen und speichern.

---

## Schritt 4: openWB einrichten

**Auf der Seite der Box** (Reiter Einstellungen → openWB):
1. Haken bei **„Werte an die openWB senden“**.
2. **Adresse der openWB** eintragen (IP aus dem Router, z. B. 192.168.178.20).
3. Port **1883**, Benutzer und Passwort leer lassen.
4. Haken bei **„Bei Zählerausfall Ersatzwert senden“** drin lassen (10 000 W).
5. Speichern.

**In der openWB** (Software 1.9):
1. Einstellungen → Modulkonfiguration → **EVU**.
2. Als Modul **„MQTT“** auswählen und speichern.
   (Der bisherige EVU-Zähler der openWB wird damit ersetzt.)

**Prüfen:**
- Auf der Seite der Box ist der Punkt **openWB** grün.
- Auf der Startseite der openWB steht bei **EVU** dieselbe Leistung wie auf der Box
  (Bezug positiv, Einspeisung negativ).

Warum der Ersatzwert? Fällt der Zähler aus, würde die openWB sonst mit dem letzten Wert
weiterladen. Mit dem hohen Ersatzwert sieht sie „kein Überschuss“ und hört auf, aus PV zu laden.

---

## Schritt 5: Fronius einrichten

**Im Fronius Datamanager** (Weboberfläche des Wechselrichters, Menüpunkt „Zähler“):
1. Den bisherigen **S0-Zähler** entfernen bzw. abwählen.
2. Als Zähler **„Fronius Smart Meter“** wählen, Einbauort **Einspeisepunkt**. Speichern.

**Leitung anschließen:**
1. Box vom Strom trennen.
2. Leitung von den Zähler-Klemmen des Datamanagers (Modbus/RS485, meist mit „D+ / D−“
   oder „A / B“ beschriftet) an die Klemmen **„Fronius“** der Box.
3. Box einschalten.

**Was jetzt passiert:** Die Box hört zuerst nur zu und findet selbst heraus, wie der Fronius
fragt. Das dauert ein paar Sekunden bis etwa eine Minute. Unter **Diagnose** siehst du
„Erkennung läuft“ und danach „fragt ab“.

**Prüfen:**
- Der Punkt **Fronius** ist grün.
- Im Datamanager wird der Zähler als erkannt angezeigt (kein Zählerfehler).
- In **Solar.web** stimmen nach ein paar Minuten Einspeisung und Verbrauch.
  Zeigt Solar.web Einspeisung als Bezug: **Einstellungen → Fronius → Vorzeichen umdrehen**.

**Fronius bleibt orange?**

| Diagnose zeigt | Was tun |
|---|---|
| „Auf der Leitung zum Fronius kommt nichts an“ | Leitung prüfen, A und B tauschen, im Datamanager „Fronius Smart Meter“ eingestellt? |
| viele Zeilen **„abgelehnt“** in der Tabelle | Foto von der Tabelle machen und weitergeben, dann kann die Box angepasst werden |
| „fragt, keine Antwort“ | Der Zähler antwortet nicht (siehe Schritt 3) |
| Datamanager nimmt den Zähler nicht an | Einstellungen → Fronius → SunSpec-Modell auf **203** stellen, speichern, ausprobieren |

> Das ist der einzige Teil, der vorher nicht getestet werden konnte. Wenn hier etwas nicht klappt,
> bitte **Fotos vom Reiter „Diagnose“** (ganz nach unten scrollen) schicken.

---

## Wenn alles grün ist

Fertig. Die Box braucht keine Pflege. Nach einem Stromausfall startet sie von selbst und behält
alle Einstellungen.

**Gut zu wissen:**
- Die Seite kannst du jederzeit unter **http://sdm72-fronius.local** ansehen.
- Unter **„So funktioniert's“** auf der Seite steht noch einmal, was die Box macht und was bei
  Störungen passiert.
- Kommst du nicht mehr an die Seite: mit dem Handy ins WLAN **„SDM72-Fronius“** (dein eigenes
  Passwort aus Schritt 2), dann **http://192.168.4.1**.
- Passwort vergessen: Die Box muss am PC neu programmiert werden.

## Was bei Störungen passiert

| Störung | Fronius | openWB |
|---|---|---|
| Zähler antwortet nicht | bekommt keine Antwort und meldet Zählerfehler | bekommt 10 000 W Bezug, hört auf, aus PV zu laden |
| WLAN weg | läuft normal weiter | bekommt keine Werte (letzter Wert bleibt stehen) |
| Box ohne Strom | meldet Zählerfehler | bekommt keine Werte (letzter Wert bleibt stehen) |

Bei den letzten beiden Fällen kann die openWB mit einem alten Wert weiterladen, bis die Box wieder
läuft. Wenn das Auto dann aus dem Netz lädt, kurz in der openWB nachsehen.
