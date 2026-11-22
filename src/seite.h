// seite.h
// ---------------------------------------------------------------------------
// Die Webseite des ESP32: HTML, CSS und JavaScript in einem Stück.
// Vier Reiter: Übersicht, Diagnose, Einstellungen, So funktioniert's.
// Die Seite holt alle 2 s /api/status und zeigt die Werte an; Einstellungen
// kommen von /api/einstellungen und gehen per POST dorthin zurück.
// Wird nur von webseite.cpp eingebunden.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

static const char SEITE[] PROGMEM = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SDM72 → Fronius</title>
<style>
:root{--bg:#eef1f5;--karte:#fff;--text:#1c2430;--grau:#6a7482;--linie:#dfe3e9;--feld:#f6f7f9;
--gruen:#15803d;--gruen-h:#dcfce7;--orange:#c2410c;--orange-h:#ffedd5;--rot:#b91c1c;--rot-h:#fee2e2;--blau:#1d4ed8;--blau-h:#dbeafe;--schatten:0 1px 2px rgba(0,0,0,.06)}
@media(prefers-color-scheme:dark){:root{--bg:#0f141b;--karte:#18202a;--text:#e8edf3;--grau:#8f9aa8;--linie:#2a3441;--feld:#121820;
--gruen:#4ade80;--gruen-h:#123222;--orange:#fb923c;--orange-h:#3a2312;--rot:#f87171;--rot-h:#3b1717;--blau:#60a5fa;--blau-h:#15253f;--schatten:none}}
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}
body{margin:0;font:15px/1.45 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;background:var(--bg);color:var(--text)}
.kopf{background:var(--karte);border-bottom:1px solid var(--linie);position:sticky;top:0;z-index:5}
.kopf-innen{max-width:1100px;margin:0 auto;padding:10px 16px 0}
.titel{display:flex;align-items:baseline;gap:10px;flex-wrap:wrap}
.titel b{font-size:18px}.titel span{color:var(--grau);font-size:13px}
.pillen{display:flex;gap:6px;flex-wrap:wrap;margin:8px 0}
.pille{display:inline-flex;align-items:center;gap:6px;font-size:12.5px;padding:3px 9px;border-radius:99px;background:var(--feld);border:1px solid var(--linie);white-space:nowrap}
.pille i{width:8px;height:8px;border-radius:50%;background:var(--grau)}
.pille.ok i{background:var(--gruen)}.pille.warn i{background:var(--orange)}.pille.fehler i{background:var(--rot)}
nav{display:flex;gap:2px;overflow-x:auto;scrollbar-width:none}nav::-webkit-scrollbar{display:none}
nav a{padding:9px 12px;color:var(--grau);text-decoration:none;border-bottom:2px solid transparent;white-space:nowrap;font-weight:500}
nav a.aktiv{color:var(--blau);border-color:var(--blau)}
main{max-width:1100px;margin:0 auto;padding:16px}
.seite{display:none}.seite.aktiv{display:block}
.raster{display:grid;gap:14px;grid-template-columns:repeat(auto-fit,minmax(300px,1fr))}
.karte{background:var(--karte);border:1px solid var(--linie);border-radius:12px;padding:16px;box-shadow:var(--schatten);min-width:0}
.breit{grid-column:1/-1}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.04em;color:var(--grau);margin:0 0 10px;font-weight:600}
h3{font-size:16px;margin:18px 0 6px}h3:first-child{margin-top:0}
p{margin:6px 0}.klein{font-size:13px;color:var(--grau)}
.zahl{font-variant-numeric:tabular-nums}
.gross{font-size:52px;font-weight:700;line-height:1;letter-spacing:-.02em}
.richtung{font-size:15px;font-weight:600;margin-top:6px}
.ok{color:var(--gruen)}.warn{color:var(--orange)}.fehler{color:var(--rot)}.grau{color:var(--grau)}
.balken{position:relative;height:12px;background:var(--feld);border:1px solid var(--linie);border-radius:99px;margin:16px 0 4px;overflow:hidden}
.balken .mitte{position:absolute;left:50%;top:0;bottom:0;width:2px;background:var(--grau);opacity:.5}
.balken .wert{position:absolute;top:0;bottom:0;border-radius:99px;transition:all .4s}
.skala{display:flex;justify-content:space-between;font-size:11.5px;color:var(--grau)}
.zeile{display:flex;justify-content:space-between;gap:12px;padding:6px 0;border-bottom:1px solid var(--linie)}
.zeile:last-child{border-bottom:0}.zeile>span:first-child{color:var(--grau)}.zeile>span:last-child{text-align:right}
.phase{display:grid;grid-template-columns:28px 1fr 74px;gap:10px;align-items:center;padding:7px 0;border-bottom:1px solid var(--linie)}
.phase:last-of-type{border-bottom:0}.phase b{font-size:13px;color:var(--grau)}
.phase .mini{height:8px;background:var(--feld);border-radius:99px;position:relative;overflow:hidden}
.phase .mini div{position:absolute;top:0;bottom:0;border-radius:99px}
.phase .rechts{text-align:right;font-variant-numeric:tabular-nums}
.phase small{display:block;color:var(--grau);font-size:12px}
.stand{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.stand div{background:var(--feld);border-radius:10px;padding:12px}
.stand b{display:block;font-size:22px;font-variant-numeric:tabular-nums}
.ziel{display:flex;gap:12px;align-items:flex-start;padding:10px 0;border-bottom:1px solid var(--linie)}
.ziel:last-child{border-bottom:0}
.ziel .punkt{width:10px;height:10px;border-radius:50%;margin-top:6px;flex:none;background:var(--grau)}
.ziel .punkt.ok{background:var(--gruen)}.ziel .punkt.warn{background:var(--orange)}.ziel .punkt.fehler{background:var(--rot)}
.ziel b{display:block}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{text-align:left;padding:6px;border-bottom:1px solid var(--linie);white-space:nowrap}
th{color:var(--grau);font-weight:600}tr.warn td{color:var(--orange)}
.tabelle{overflow-x:auto;max-height:420px;overflow-y:auto}
.hinweisbox{border-radius:10px;padding:10px 12px;margin:8px 0;font-size:14px}
.hinweisbox.info{background:var(--blau-h)}.hinweisbox.warn{background:var(--orange-h)}.hinweisbox.fehler{background:var(--rot-h)}
fieldset{border:1px solid var(--linie);border-radius:12px;padding:14px 16px 16px;margin:0 0 14px;background:var(--karte)}
legend{font-weight:600;padding:0 6px}
.felder{display:grid;gap:4px 16px;grid-template-columns:repeat(auto-fit,minmax(240px,1fr))}
label{display:block;font-size:13px;color:var(--grau);margin-top:10px}
input,select{width:100%;padding:9px 10px;border-radius:8px;border:1px solid var(--linie);background:var(--feld);color:var(--text);font:inherit}
input:focus,select:focus{outline:2px solid var(--blau);outline-offset:-1px}
.haken{display:flex;gap:8px;align-items:center;color:var(--text);font-size:15px;margin-top:12px}
.haken input{width:18px;height:18px}
.erkl{font-size:13px;color:var(--grau);margin:6px 0 0}
button{padding:10px 16px;border:0;border-radius:8px;background:var(--blau);color:#fff;font:inherit;font-weight:600;cursor:pointer}
button.zweit{background:var(--feld);color:var(--text);border:1px solid var(--linie);font-weight:500}
.knopfleiste{position:sticky;bottom:0;background:var(--bg);padding:12px 0;display:flex;gap:10px;flex-wrap:wrap;align-items:center}
#meldung{font-size:14px}
.bild{width:100%;max-width:760px;display:block;margin:6px auto 4px;color:var(--text)}
.bild .box{fill:var(--feld);stroke:var(--linie);stroke-width:1.5}
.bild .esp{fill:var(--blau-h);stroke:var(--blau);stroke-width:1.5}
.bild text{fill:currentColor;font:13px system-ui,sans-serif}.bild .t2{fill:var(--grau);font-size:11.5px}
.bild .lin{stroke:var(--grau);stroke-width:2;fill:none}.bild .funk{stroke:var(--blau);stroke-width:2;fill:none;stroke-dasharray:5 4}
.faq td{white-space:normal;vertical-align:top}
@media(max-width:600px){.kopf{position:static}.gross{font-size:44px}main{padding:12px}}
footer{max-width:1100px;margin:0 auto;padding:0 16px 24px;color:var(--grau);font-size:12px}
</style></head><body>
<div class="kopf"><div class="kopf-innen">
<div class="titel"><b>SDM72 → Fronius</b><span id="k_ip"></span></div>
<div class="pillen">
<span class="pille" id="p_zaehler"><i></i>Zähler</span>
<span class="pille" id="p_fronius"><i></i>Fronius</span>
<span class="pille" id="p_wlan"><i></i>Heimnetz</span>
<span class="pille" id="p_mqtt"><i></i>openWB</span></div>
<nav><a href="#uebersicht">Übersicht</a><a href="#diagnose">Diagnose</a><a href="#einstellungen">Einstellungen</a><a href="#erklaerung">So funktioniert's</a></nav>
</div></div>
<main>

<!-- ============================ ÜBERSICHT ============================ -->
<section class="seite" id="s_uebersicht"><div class="raster">
<div class="karte"><h2>Netzanschluss jetzt</h2>
<div class="gross zahl" id="leistung">–</div><div class="richtung" id="richtung">&nbsp;</div>
<div class="balken"><div class="mitte"></div><div class="wert" id="balken"></div></div>
<div class="skala"><span>← Einspeisung</span><span>0</span><span>Bezug →</span></div>
<p class="klein" id="alter"></p></div>

<div class="karte"><h2>Phasen</h2><div id="phasen"></div>
<div class="zeile"><span>Frequenz</span><span class="zahl" id="hz">–</span></div>
<div class="zeile"><span>Leistungsfaktor</span><span class="zahl" id="pf">–</span></div>
<p class="klein" id="phinweis"></p></div>

<div class="karte"><h2>Zählerstände</h2><div class="stand">
<div><span class="klein">Bezug aus dem Netz</span><b id="bezug">–</b><span class="klein">kWh</span></div>
<div><span class="klein">Einspeisung ins Netz</span><b id="einsp">–</b><span class="klein">kWh</span></div></div></div>

<div class="karte"><h2>Weitergabe</h2>
<div class="ziel"><span class="punkt" id="z_fronius"></span><div><b>Fronius Datamanager</b><span class="klein" id="z_fronius_t">–</span></div></div>
<div class="ziel"><span class="punkt" id="z_mqtt"></span><div><b>openWB (MQTT)</b><span class="klein" id="z_mqtt_t">–</span></div></div>
<div class="ziel"><span class="punkt" id="z_wlan"></span><div><b>Heimnetz</b><span class="klein" id="z_wlan_t">–</span></div></div></div>
</div></section>

<!-- ============================ DIAGNOSE ============================ -->
<section class="seite" id="s_diagnose"><div class="raster">
<div class="karte"><h2>Zähler SDM72D-M (Bus A)</h2>
<div class="zeile"><span>Zustand</span><span id="d_zstatus">–</span></div>
<div class="zeile"><span>Busparameter</span><span id="d_zbus">–</span></div>
<div class="zeile"><span>Alter der Werte</span><span class="zahl" id="d_zalter">–</span></div>
<div class="zeile"><span>Runden gut / fehlerhaft</span><span class="zahl" id="d_zrunden">–</span></div>
<div class="zeile"><span>Letzter Fehler</span><span id="d_zfehler">–</span></div>
<div class="zeile"><span>Phasenwerte vorhanden</span><span id="d_zphasen">–</span></div></div>

<div class="karte"><h2>Fronius Datamanager (Bus B)</h2>
<div class="zeile"><span>Zustand</span><span id="d_fstatus">–</span></div>
<div class="zeile"><span>Baudrate / Adresse</span><span id="d_fbus">–</span></div>
<div class="zeile"><span>Fragt nach</span><span id="d_fart">–</span></div>
<div class="zeile"><span>Beantwortet</span><span class="zahl" id="d_fok">–</span></div>
<div class="zeile"><span>Abgelehnt / ohne Antwort</span><span class="zahl" id="d_fnok">–</span></div>
<div class="zeile"><span>Letzte Anfrage vor</span><span class="zahl" id="d_fletzt">–</span></div>
<div class="zeile"><span>Gemeldete Leistung</span><span class="zahl" id="d_fw">–</span></div>
<div id="d_fhinweis"></div></div>

<div class="karte"><h2>Netzwerk und System</h2>
<div class="zeile"><span>Heimnetz</span><span id="d_wlan">–</span></div>
<div class="zeile"><span>Eigenes WLAN</span><span id="d_ap">–</span></div>
<div class="zeile"><span>MQTT openWB</span><span id="d_mqtt">–</span></div>
<div class="zeile"><span>Gesendet / Ersatzwert</span><span class="zahl" id="d_mzahl">–</span></div>
<div class="zeile"><span>Laufzeit</span><span class="zahl" id="d_lauf">–</span></div>
<div class="zeile"><span>Freier Speicher</span><span class="zahl" id="d_speicher">–</span></div>
<div class="zeile"><span>Version</span><span id="d_version">–</span></div></div>

<div class="karte breit"><h2>Anfragen des Fronius (neueste oben)</h2>
<p class="klein">FC 3 = Holding-Register, FC 4 = Input-Register. „abgelehnt“ heißt: Der Fronius fragt Register, die dieses Gerät nicht kennt. Diese Zeilen bitte notieren.</p>
<div class="tabelle"><table><thead><tr><th>vor</th><th>FC</th><th>Register</th><th>Anzahl</th><th>Bereich</th><th>Ergebnis</th></tr></thead>
<tbody id="prot"><tr><td colspan="6" class="grau">noch keine Anfragen</td></tr></tbody></table></div></div>
</div></section>

<!-- ============================ EINSTELLUNGEN ============================ -->
<section class="seite" id="s_einstellungen"><form id="form">
<div class="hinweisbox info">Änderungen gelten nach dem Speichern; das Gerät startet dabei neu (etwa 10 s).
Beim Speichern fragt der Browser nach Benutzer <b>admin</b> und dem Passwort des Geräte-WLANs.</div>

<fieldset><legend>Heimnetz (WLAN)</legend>
<p class="erkl">Damit die openWB die Werte bekommt und du die Seite im Heimnetz unter <b>http://sdm72-fronius.local</b> erreichst.</p>
<div class="felder"><div><label>WLAN-Name</label><input name="wlan_name" autocomplete="off">
<button type="button" class="zweit" id="suche" style="margin-top:8px">WLANs suchen</button>
<select id="netze" style="display:none;margin-top:8px"></select></div>
<div><label>WLAN-Passwort</label><input name="wlan_passwort" type="password" autocomplete="new-password">
<p class="erkl" id="wpw"></p></div></div></fieldset>

<fieldset><legend>openWB (MQTT)</legend>
<p class="erkl">Die openWB 1.9 bekommt jede Sekunde die Werte am Netzanschluss. In der openWB als EVU-Modul „MQTT“ einstellen.</p>
<label class="haken"><input name="mqtt_an" type="checkbox" value="1"> Werte an die openWB senden</label>
<div class="felder"><div><label>Adresse der openWB (IP oder Name)</label><input name="mqtt_server" placeholder="z. B. 192.168.178.20"></div>
<div><label>MQTT-Port</label><input name="mqtt_port" type="number" min="1" max="65535"></div>
<div><label>MQTT-Benutzer (meist leer)</label><input name="mqtt_benutzer" autocomplete="off"></div>
<div><label>MQTT-Passwort (meist leer)</label><input name="mqtt_passwort" type="password" autocomplete="new-password"><p class="erkl" id="mpw"></p></div></div>
<label class="haken"><input name="ersatz_an" type="checkbox" value="1"> Bei Zählerausfall Ersatzwert senden</label>
<div class="felder"><div><label>Ersatzwert Netzbezug in W</label><input name="ersatz_w" type="number" min="0" max="100000"></div></div>
<p class="erkl">Die openWB 1.9 merkt nicht, wenn keine Werte mehr kommen, und würde mit dem letzten Wert weiterladen. Ein hoher Bezug beendet das Überschussladen sicher.</p></fieldset>

<fieldset><legend>Fronius (Bus B)</legend>
<p class="erkl">Normalerweise alles auf „automatisch“ lassen: Das Gerät hört zu und erkennt Baudrate und Adresse selbst.</p>
<div class="felder"><div><label>Baudrate</label><select name="fronius_baud"><option value="0">automatisch erkennen</option>
<option>9600:8N1</option><option>19200:8N1</option><option>9600:8E1</option><option>19200:8E1</option><option>38400:8N1</option><option>4800:8N1</option><option>2400:8N1</option><option>115200:8N1</option><option>9600:8N2</option><option>9600:8O1</option></select></div>
<div><label>Busadresse (0 = automatisch)</label><input name="fronius_adresse" type="number" min="0" max="247"></div>
<div><label>SunSpec-Modell</label><select name="modell"><option value="213">213 – Gleitkomma (Standard)</option><option value="203">203 – Ganzzahl</option></select></div>
<div><label>Vorzeichen an den Fronius</label><select name="vorzeichen"><option value="1">+ Bezug / − Einspeisung (Standard)</option><option value="-1">umgedreht</option></select></div></div>
<p class="erkl">Zeigt Solar.web Einspeisung als Bezug, das Vorzeichen umdrehen. Nimmt der Datamanager den Zähler nicht an, Modell 203 versuchen.</p></fieldset>

<fieldset><legend>Zähler SDM72D-M (Bus A)</legend>
<p class="erkl">Muss zu den Einstellungen im Setup-Menü des Zählers passen.</p>
<div class="felder"><div><label>Baudrate</label><select name="zaehler_baud"><option>2400</option><option>4800</option><option>9600</option><option>19200</option><option>38400</option></select></div>
<div><label>Busadresse (1–247)</label><input name="zaehler_adresse" type="number" min="1" max="247"></div></div></fieldset>

<fieldset><legend>Zugang zu diesem Gerät</legend>
<div class="felder"><div><label>Neues Passwort für das Geräte-WLAN „SDM72-Fronius“ und diese Seite</label>
<input name="ap_passwort" type="password" autocomplete="new-password" placeholder="leer = unverändert"></div></div>
<p class="erkl">8 bis 63 Zeichen. Das Passwort gilt danach für das WLAN „SDM72-Fronius“ und für das Speichern hier (Benutzer admin). Gut aufheben.</p></fieldset>

<div class="knopfleiste"><button type="submit">Speichern und neu starten</button>
<button type="button" class="zweit" id="neustart">Nur neu starten</button><span id="meldung"></span></div>
</form></section>

<!-- ============================ ERKLÄRUNG ============================ -->
<section class="seite" id="s_erklaerung"><div class="raster">
<div class="karte breit"><h2>Was dieses Gerät macht</h2>
<svg class="bild" viewBox="0 0 760 250" role="img" aria-label="Aufbau: Zähler, ESP32, Fronius, openWB, Browser">
<rect class="box" x="10" y="95" width="120" height="60" rx="10"/><text x="70" y="120" text-anchor="middle">Netzanschluss</text><text class="t2" x="70" y="138" text-anchor="middle">Hausanschluss</text>
<path class="lin" d="M130 125h40"/>
<rect class="box" x="170" y="95" width="130" height="60" rx="10"/><text x="235" y="120" text-anchor="middle">SDM72D-M</text><text class="t2" x="235" y="138" text-anchor="middle">misst Bezug/Einspeisung</text>
<path class="lin" d="M300 125h60"/><text class="t2" x="330" y="116" text-anchor="middle">RS485 A</text>
<rect class="esp" x="360" y="85" width="130" height="80" rx="10"/><text x="425" y="118" text-anchor="middle">ESP32</text><text class="t2" x="425" y="136" text-anchor="middle">dieses Gerät</text>
<path class="lin" d="M425 85V50H580"/><text class="t2" x="505" y="43" text-anchor="middle">RS485 B</text>
<rect class="box" x="580" y="20" width="170" height="60" rx="10"/><text x="665" y="45" text-anchor="middle">Fronius Symo</text><text class="t2" x="665" y="63" text-anchor="middle">Datamanager, Solar.web</text>
<path class="funk" d="M490 125h90"/><text class="t2" x="535" y="118" text-anchor="middle">WLAN</text>
<rect class="box" x="580" y="95" width="170" height="60" rx="10"/><text x="665" y="120" text-anchor="middle">openWB</text><text class="t2" x="665" y="138" text-anchor="middle">Überschussladen</text>
<path class="funk" d="M490 145h40 v65 h50"/>
<rect class="box" x="580" y="180" width="170" height="60" rx="10"/><text x="665" y="205" text-anchor="middle">Handy / PC</text><text class="t2" x="665" y="223" text-anchor="middle">diese Webseite</text>
</svg>
<p>Der Zähler <b>SDM72D-M</b> sitzt am Netzanschluss und misst, wie viel Strom gerade aus dem Netz kommt (Bezug) oder hineingeht (Einspeisung).
Dieses Gerät fragt ihn jede Sekunde ab und gibt die Werte an drei Stellen weiter: an den <b>Fronius</b> (für Solar.web und die Verbrauchsanzeige),
an die <b>openWB</b> (damit sie das Auto mit Überschuss laden kann) und an diese <b>Webseite</b>.</p>
<p>Die Anker Solarbank regelt unabhängig davon mit ihrem eigenen Anker Smart Meter.</p></div>

<div class="karte"><h2>Vorzeichen</h2>
<p><b class="warn">Positiv</b> = Bezug: Das Haus braucht mehr, als die Anlage gerade erzeugt.</p>
<p><b class="ok">Negativ</b> = Einspeisung: Überschuss fließt ins Netz. Das ist der Strom, den die openWB ins Auto laden kann.</p>
<p class="klein">So gibt das Gerät die Werte auch an den Fronius und die openWB weiter.</p></div>

<div class="karte"><h2>Der Fronius</h2>
<p>Der Datamanager hält dieses Gerät für einen „Fronius Smart Meter“. Welche Sprache er dabei spricht, war vorher nicht bekannt.
Deshalb hört das Gerät nach dem Start erst zu, erkennt Baudrate und Adresse selbst und beantwortet dann zwei Varianten gleichzeitig:
<b>SunSpec</b> und die Register eines <b>Eastron</b>-Zählers. Unter „Diagnose“ siehst du, was der Fronius wirklich fragt.</p></div>

<div class="karte"><h2>Bei Störungen</h2>
<p><b>Zähler antwortet nicht:</b> Das Gerät antwortet dem Fronius absichtlich nicht mehr. Der Fronius meldet dann einen Zählerfehler, statt mit alten Werten zu rechnen.</p>
<p><b>openWB:</b> Sie bekommt dann einen hohen Ersatzwert für den Bezug (Standard 10 000 W) und beendet das Überschussladen.</p>
<p><b>WLAN weg:</b> Fronius und Zähler laufen weiter, nur die openWB bekommt nichts mehr. Das Gerät verbindet sich von selbst wieder.</p>
<p><b>Stromausfall:</b> Alle Einstellungen bleiben gespeichert.</p></div>

<div class="karte breit"><h2>Erste Hilfe</h2><div class="tabelle"><table class="faq">
<thead><tr><th>Was du siehst</th><th>Mögliche Ursache</th><th>Was du tun kannst</th></tr></thead><tbody>
<tr><td>Zähler „antwortet nicht“</td><td>Leitung A/B vertauscht oder lose, falsche Baudrate oder Adresse</td><td>A und B tauschen; im Setup-Menü des Zählers Baudrate/Adresse nachsehen und unter Einstellungen eintragen</td></tr>
<tr><td>Fronius „sucht Baudrate … nichts empfangen“</td><td>Leitung zum Datamanager, A/B vertauscht, oder Datamanager steht nicht auf „Fronius Smart Meter“</td><td>Verdrahtung prüfen; im Datamanager den Zähler einstellen</td></tr>
<tr><td>Fronius: viele Anfragen „abgelehnt“</td><td>Der Fronius fragt Register, die das Gerät nicht kennt</td><td>Unter Diagnose die Zeilen notieren (Foto) und weitergeben</td></tr>
<tr><td>Solar.web zeigt Einspeisung als Bezug</td><td>Vorzeichen</td><td>Einstellungen → Fronius → Vorzeichen umdrehen</td></tr>
<tr><td>openWB „keine Verbindung“</td><td>Falsche Adresse der openWB, Heimnetz getrennt</td><td>Adresse der openWB im Router nachsehen und eintragen</td></tr>
<tr><td>Seite im Heimnetz nicht erreichbar</td><td>WLAN-Passwort falsch, Gerät außer Reichweite</td><td>Mit dem Handy ins WLAN „SDM72-Fronius“, dann http://192.168.4.1</td></tr>
</tbody></table></div></div>
</div></section>
</main>
<footer id="fuss"></footer>
<script>
const $=id=>document.getElementById(id);
const komma=(v,s)=>v.toFixed(s).replace('.',',');
const kw=w=>w==null?'–':(Math.abs(w)>=1000?komma(w/1000,2)+' kW':Math.round(w)+' W');
const tausender=v=>v.toLocaleString('de-DE',{minimumFractionDigits:1,maximumFractionDigits:1});
function setz(id,text,klasse){const el=$(id);if(!el)return;el.textContent=text;if(klasse!==undefined)el.className=klasse}
function pille(id,klasse,text){const el=$(id);el.className='pille '+klasse;el.lastChild.textContent=text}
function punkt(id,klasse){$(id).className='punkt '+klasse}
function dauer(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return (d?d+' d ':'')+h+' h '+m+' min'}

// ---------- Reiter ----------
function reiter(){const n=(location.hash||'#uebersicht').slice(1);
 document.querySelectorAll('.seite').forEach(s=>s.classList.toggle('aktiv',s.id=='s_'+n));
 document.querySelectorAll('nav a').forEach(a=>a.classList.toggle('aktiv',a.getAttribute('href')=='#'+n));
 if(!document.querySelector('.seite.aktiv')){$('s_uebersicht').classList.add('aktiv')}}
addEventListener('hashchange',reiter);reiter();

// ---------- Status ----------
let fehlversuche=0;
async function status(){
 let d;try{d=await (await fetch('/api/status',{cache:'no-store'})).json();fehlversuche=0}
 catch(e){if(++fehlversuche>1){pille('p_zaehler','fehler','keine Verbindung zum Gerät')}return}
 const z=d.zaehler,f=d.fronius,w=d.wlan,m=d.mqtt;const aktuell=z.gueltig&&z.aktuell;
 const fAnfragen=f.beantwortet+f.abgelehnt+f.ohne_antwort;
 const fAktiv=f.bereit&&fAnfragen>0&&f.seit_letzter_s<10;

 // Kopf
 setz('k_ip',w.verbunden?w.ip:w.ap_ip);
 pille('p_zaehler',aktuell?'ok':'fehler',aktuell?'Zähler':'Zähler antwortet nicht');
 pille('p_fronius',!f.bereit?'warn':(fAktiv?(f.beantwortet?'ok':'warn'):'warn'),!f.bereit?'Fronius: Suche':(fAktiv?'Fronius':'Fronius: keine Anfragen'));
 pille('p_wlan',w.verbunden?'ok':(w.name?'fehler':''),w.verbunden?'Heimnetz':(w.name?'Heimnetz getrennt':'kein Heimnetz'));
 pille('p_mqtt',!m.aktiv?'':(m.verbunden?'ok':'fehler'),!m.aktiv?'openWB aus':(m.verbunden?'openWB':'openWB getrennt'));

 // Übersicht
 if(aktuell){setz('leistung',kw(z.leistung_w),'gross zahl '+(z.leistung_w<0?'ok':'warn'));
  setz('richtung',z.leistung_w<0?'Einspeisung ins Netz':'Bezug aus dem Netz','richtung '+(z.leistung_w<0?'ok':'warn'));
  const anteil=Math.min(Math.abs(z.leistung_w)/10000,1)*50;const b=$('balken');
  b.style.background=z.leistung_w<0?'var(--gruen)':'var(--orange)';
  b.style.left=(z.leistung_w<0?50-anteil:50)+'%';b.style.width=anteil+'%';
  setz('alter','Stand vor '+komma(z.alter_ms/1000,1)+' s · Balken bis 10 kW')}
 else{setz('leistung','–','gross zahl grau');setz('richtung',z.gueltig?'Werte veraltet – Zähler antwortet nicht':'noch keine Werte vom Zähler','richtung fehler');
  $('balken').style.width='0';setz('alter','')}
 let ph='';for(let p=0;p<3;p++){const x=z.phasen&&aktuell?z.phasen[p]:null;
  const ant=x?Math.min(Math.abs(x.p)/4000,1)*100:0;const farbe=x&&x.p<0?'var(--gruen)':'var(--orange)';
  ph+='<div class="phase"><b>L'+(p+1)+'</b><div><div class="mini"><div style="left:0;width:'+ant+'%;background:'+farbe+'"></div></div>'+
   '<small>'+(x?komma(x.u,1)+' V · '+komma(x.i,2)+' A':'–')+'</small></div><div class="rechts">'+(x?kw(x.p):'–')+'</div></div>'}
 $('phasen').innerHTML=ph;
 setz('hz',z.hz==null||!aktuell?'–':komma(z.hz,2)+' Hz');setz('pf',z.pf==null||!aktuell?'–':komma(z.pf,3));
 setz('phinweis',z.phasen_vorhanden?'':'Der Zähler liefert keine Phasenwerte.');
 setz('bezug',z.gueltig?tausender(z.bezug_kwh):'–');setz('einsp',z.gueltig?tausender(z.einspeisung_kwh):'–');

 punkt('z_fronius',!f.bereit?'warn':(fAktiv&&f.beantwortet?'ok':'warn'));
 setz('z_fronius_t',!f.bereit?'Erkennung läuft: hört auf '+f.erkennung.baud+' Baud '+f.erkennung.format:
  (fAktiv?(f.beantwortet?'fragt ab, bekommt '+(aktuell?kw(f.an_fronius_w):'–')+' gemeldet':'fragt, bekommt aber keine Antwort'):'keine Anfragen seit '+f.seit_letzter_s+' s'));
 punkt('z_mqtt',!m.aktiv?'':(m.verbunden?(aktuell?'ok':'warn'):'fehler'));
 setz('z_mqtt_t',!m.aktiv?'ausgeschaltet':(m.verbunden?(aktuell?'sendet jede Sekunde':'sendet Ersatzwert (Zählerausfall)'):'keine Verbindung zu '+m.server));
 punkt('z_wlan',w.verbunden?'ok':(w.name?'fehler':''));
 setz('z_wlan_t',w.verbunden?w.name+' · '+w.ip+' · '+w.dbm+' dBm':(w.name?w.name+': nicht verbunden':'nicht eingerichtet'));

 // Diagnose
 setz('d_zstatus',aktuell?'antwortet':'antwortet nicht',aktuell?'ok':'fehler');
 setz('d_zbus',z.baud+' Baud 8N1, Adresse '+z.adresse);
 setz('d_zalter',z.gueltig?komma(z.alter_ms/1000,1)+' s':'–');
 setz('d_zrunden',z.runden_ok+' / '+z.runden_fehler);setz('d_zfehler',z.fehler||'–');
 setz('d_zphasen',z.phasen_vorhanden?'ja':'nein (Zähler lehnt ab)');
 setz('d_fstatus',!f.bereit?'Erkennung läuft':(fAktiv?(f.beantwortet?'fragt ab':'fragt, keine Antwort'):'keine Anfragen'),!f.bereit?'warn':(fAktiv&&f.beantwortet?'ok':'warn'));
 setz('d_fbus',f.bereit?f.baud+' Baud '+f.format+', Adresse '+f.adresse+(f.automatik?' (erkannt)':' (fest)'):
  'prüft '+f.erkennung.baud+' '+f.erkennung.format+' · Durchlauf '+(f.erkennung.durchlaeufe+1));
 const art=[];if(f.sunspec)art.push('SunSpec '+f.modell+' ('+f.sunspec+')');if(f.eastron)art.push('Eastron ('+f.eastron+')');
 setz('d_fart',art.length?art.join(', '):'–');
 setz('d_fok',String(f.beantwortet));setz('d_fnok',f.abgelehnt+' / '+f.ohne_antwort);
 setz('d_fletzt',f.bereit&&fAnfragen?f.seit_letzter_s+' s':'–');setz('d_fw',aktuell?kw(f.an_fronius_w):'–');
 let hin='';
 if(!f.bereit)hin=f.erkennung.bytes?'<div class="hinweisbox warn">Auf der Leitung kommt etwas an ('+f.erkennung.bytes+' Bytes), aber noch keine gültige Anfrage. Die Suche probiert weitere Einstellungen.</div>':
  '<div class="hinweisbox warn">Auf der Leitung zum Fronius kommt nichts an. Verdrahtung (A/B) prüfen und im Datamanager „Fronius Smart Meter“ einstellen.</div>';
 else if(f.abgelehnt>0)hin='<div class="hinweisbox warn">Der Fronius fragt Register, die das Gerät nicht kennt. Die Zeilen „abgelehnt“ in der Tabelle unten bitte notieren.</div>';
 $('d_fhinweis').innerHTML=hin;
 setz('d_wlan',w.name?(w.verbunden?w.name+', '+w.ip+' ('+w.dbm+' dBm)':w.name+': nicht verbunden'):'nicht eingerichtet',w.verbunden?'ok':(w.name?'fehler':'grau'));
 setz('d_ap',w.ap_name+', '+w.ap_ip);
 setz('d_mqtt',!m.aktiv?'aus':(m.verbunden?'verbunden mit '+m.server:'keine Verbindung zu '+m.server+' (Code '+m.code+')'),!m.aktiv?'grau':(m.verbunden?'ok':'fehler'));
 setz('d_mzahl',m.gesendet+' / '+m.ersatz);setz('d_lauf',dauer(d.laufzeit_s));
 setz('d_speicher',Math.round(d.speicher_frei/1024)+' KB');setz('d_version',d.version);
 setz('fuss','SDM72 → Fronius · Version '+d.version+' · läuft seit '+dauer(d.laufzeit_s));
 const tb=$('prot');
 if(!f.protokoll.length){tb.innerHTML='<tr><td colspan="6" class="grau">noch keine Anfragen</td></tr>'}
 else{tb.innerHTML='';for(const a of f.protokoll){const tr=document.createElement('tr');
  [komma(a.vor_s,1)+' s',a.fc,a.start+'–'+(a.start+a.anzahl-1),a.anzahl,a.bereich,a.ergebnis].forEach(t=>{const td=document.createElement('td');td.textContent=t;tr.appendChild(td)});
  if(a.ergebnis!='beantwortet')tr.className='warn';tb.appendChild(tr)}}
}

// ---------- Einstellungen ----------
async function laden(){try{const d=await (await fetch('/api/einstellungen',{cache:'no-store'})).json();const f=$('form');
 for(const k of['zaehler_baud','zaehler_adresse','fronius_baud','fronius_adresse','modell','vorzeichen','wlan_name','mqtt_server','mqtt_port','mqtt_benutzer','ersatz_w'])if(f[k])f[k].value=d[k];
 f.mqtt_an.checked=d.mqtt_an;f.ersatz_an.checked=d.ersatz_an;
 setz('wpw',d.wlan_passwort_gesetzt?'Ein Passwort ist gespeichert (verborgen). Leer lassen, um es zu behalten.':'Noch kein Passwort gespeichert.');
 setz('mpw',d.mqtt_passwort_gesetzt?'Ein Passwort ist gespeichert (verborgen). Leer lassen, um es zu behalten.':'')}catch(e){}}
function nachNeustart(){let n=0;const t=setInterval(async()=>{n++;
 try{const r=await fetch('/api/status',{cache:'no-store'});if(r.ok){clearInterval(t);location.reload()}}catch(e){}
 if(n>30){clearInterval(t);setz('meldung','Gerät nicht wieder erreichbar. Neue Adresse? Ggf. über das WLAN „SDM72-Fronius“ und http://192.168.4.1 öffnen.','fehler')}},2000)}
$('form').addEventListener('submit',async ev=>{ev.preventDefault();const f=ev.target;const daten=new URLSearchParams(new FormData(f));
 if(!f.mqtt_an.checked)daten.set('mqtt_an','0');if(!f.ersatz_an.checked)daten.set('ersatz_an','0');
 setz('meldung','speichere …','');
 try{const r=await fetch('/api/einstellungen',{method:'POST',body:daten});const t=await r.text();
  setz('meldung',r.ok?t+' Bitte etwa 10 s warten …':(r.status==401?'Anmeldung fehlgeschlagen.':t),r.ok?'ok':'fehler');
  f.wlan_passwort.value='';f.ap_passwort.value='';f.mqtt_passwort.value='';if(r.ok)nachNeustart()}
 catch(e){setz('meldung','Keine Verbindung zum Gerät.','fehler')}});
$('neustart').addEventListener('click',async()=>{try{const r=await fetch('/api/neustart',{method:'POST'});
 setz('meldung',r.ok?'Startet neu … bitte etwa 10 s warten.':'Anmeldung fehlgeschlagen.',r.ok?'ok':'fehler');if(r.ok)nachNeustart()}catch(e){}});
$('suche').addEventListener('click',async()=>{const k=$('suche');k.disabled=true;k.textContent='suche …';
 try{for(let i=0;i<20;i++){const d=await (await fetch('/api/wlan_suche',{cache:'no-store'})).json();
  if(!d.laeuft){const sel=$('netze');sel.innerHTML='';const kopf=document.createElement('option');kopf.value='';
   kopf.textContent=d.netze.length+' WLANs gefunden – hier auswählen';sel.appendChild(kopf);const gesehen=new Set();
   for(const n of d.netze.sort((a,b)=>b.dbm-a.dbm)){if(!n.name||gesehen.has(n.name))continue;gesehen.add(n.name);
    const o=document.createElement('option');o.value=n.name;o.textContent=n.name+' ('+n.dbm+' dBm)';sel.appendChild(o)}
   sel.style.display='block';break}
  await new Promise(r=>setTimeout(r,1000))}}catch(e){}
 k.disabled=false;k.textContent='WLANs neu suchen'});
$('netze').addEventListener('change',ev=>{if(ev.target.value){$('form').wlan_name.value=ev.target.value;$('form').wlan_passwort.focus()}});
laden();status();setInterval(status,2000);
</script></body></html>)HTML";
