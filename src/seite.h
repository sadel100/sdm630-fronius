// seite.h
// ---------------------------------------------------------------------------
// Die Webseite des ESP32: HTML, CSS und JavaScript in einem Stück.
// Fünf Reiter: Übersicht, Verlauf, Diagnose, Einstellungen, So funktioniert's.
// Die Seite holt alle 2 s /api/status und zeigt die Werte an; Einstellungen
// kommen von /api/einstellungen und gehen per POST dorthin zurück.
// Verlauf und Energiewerte (/api/verlauf, /api/energie) und die Ereignisse
// (/api/ereignisse) werden nur geholt, solange ihr Reiter offen ist.
// Wird nur von webseite.cpp eingebunden.
// ---------------------------------------------------------------------------
#pragma once
#include <Arduino.h>

static const char SEITE[] PROGMEM = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SDM630 → Fronius</title>
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
@media(min-width:700px){.doppelt{grid-column:span 2}}
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
.soc-zeile{display:flex;gap:14px;align-items:center}
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
.fluss{width:100%;max-width:560px;display:block;margin:0 auto;color:var(--text)}
.fluss text{fill:currentColor;font:13px system-ui,sans-serif;text-anchor:middle}.fluss .wert{font-size:16px;font-weight:700}
.fluss .t2{fill:var(--grau);font-size:11.5px}
.fluss .knoten{fill:var(--feld);stroke:var(--linie);stroke-width:1.5}.fluss .haus{fill:var(--blau-h);stroke:var(--blau)}
.fl{stroke-width:4;fill:none;stroke-linecap:round;stroke-dasharray:2 10;animation:lauf .9s linear infinite}
.fl.rueck{animation-direction:reverse}.fl.ruhe{animation:none;stroke:var(--linie);stroke-dasharray:none;stroke-width:3}
@keyframes lauf{to{stroke-dashoffset:-12}}
@media(prefers-reduced-motion:reduce){.fl{animation:none}}
.diagramm{width:100%;display:block;color:var(--grau);touch-action:pan-y}
.diagramm text{fill:currentColor;font:11px system-ui,sans-serif}.diagramm .gitter{stroke:var(--linie);stroke-width:1}
.diagramm .null{stroke:var(--grau);stroke-width:1;opacity:.6}.diagramm .kurve{fill:none;stroke-width:2;stroke-linejoin:round}
.diagramm .zeiger{stroke:var(--grau);stroke-width:1;stroke-dasharray:3 3}
.schalter{display:flex;flex-wrap:wrap;gap:6px 14px;margin:4px 0 8px}
.schalter label{display:flex;align-items:center;gap:6px;margin:0;color:var(--text);font-size:13.5px}
.schalter input{width:16px;height:16px}.schalter i{display:inline-block;width:14px;height:3px;border-radius:2px}
#v_info{min-height:20px;font-size:13px;font-variant-numeric:tabular-nums}
td.z,th.z{text-align:right;font-variant-numeric:tabular-nums}
tr.stoer td{color:var(--rot)}
.knopf{display:inline-block;padding:9px 14px;border-radius:8px;background:var(--feld);color:var(--text);border:1px solid var(--linie);text-decoration:none;font-weight:500}
.fortschritt{height:10px;background:var(--feld);border:1px solid var(--linie);border-radius:99px;overflow:hidden;margin:10px 0}
.fortschritt div{height:100%;width:0;background:var(--blau);transition:width .3s}
pre.text{white-space:pre-wrap;font:13px system-ui,sans-serif;background:var(--feld);border-radius:8px;padding:10px;margin:8px 0;max-height:220px;overflow:auto}
</style></head><body>
<div class="kopf"><div class="kopf-innen">
<div class="titel"><b>SDM630 → Fronius</b><span id="k_ip"></span></div>
<div class="pillen">
<span class="pille" id="p_zaehler"><i></i>Zähler</span>
<span class="pille" id="p_fronius"><i></i>Fronius</span>
<span class="pille" id="p_anker"><i></i>Solarbank</span>
<span class="pille" id="p_wlan"><i></i>Heimnetz</span>
<span class="pille" id="p_mqtt"><i></i>openWB</span></div>
<nav><a href="#uebersicht">Übersicht</a><a href="#verlauf">Verlauf</a><a href="#diagnose">Diagnose</a><a href="#einstellungen">Einstellungen</a><a href="#erklaerung">So funktioniert's</a></nav>
</div></div>
<main>

<!-- ============================ ÜBERSICHT ============================ -->
<section class="seite" id="s_uebersicht"><div class="raster">
<div class="karte breit" id="probezeit" style="display:none"><div class="hinweisbox info" style="margin:0">Neue Firmware läuft in der Probezeit (1 Minute). Stürzt sie in dieser Zeit ab, startet automatisch wieder die alte Version.</div></div>
<div class="karte breit"><h2>Energiefluss</h2>
<svg class="fluss" viewBox="0 0 560 262" role="img" aria-label="Energiefluss zwischen PV, Netz, Solarbank und Haus">
<path id="fl_pv" class="fl ruhe" d="M280 88V118"/><path id="fl_netz" class="fl ruhe" d="M150 170H215"/><path id="fl_bank" class="fl ruhe" d="M410 170H345"/>
<rect class="knoten" x="200" y="8" width="160" height="80" rx="12"/><text x="280" y="32">PV Symo</text><text class="wert" id="f_pv" x="280" y="56">–</text><text class="t2" id="f_pv2" x="280" y="76"></text>
<rect class="knoten" x="10" y="130" width="140" height="80" rx="12"/><text x="80" y="154">Netz</text><text class="wert" id="f_netz" x="80" y="178">–</text><text class="t2" id="f_netz2" x="80" y="198"></text>
<rect class="knoten haus" x="215" y="118" width="130" height="104" rx="12"/><text x="280" y="146">Haus</text><text class="wert" id="f_haus" x="280" y="172">–</text><text class="t2" id="f_haus2" x="280" y="194">Verbrauch</text>
<rect class="knoten" x="410" y="130" width="140" height="80" rx="12"/><text x="480" y="154">Solarbank</text><text class="wert" id="f_bank" x="480" y="178">–</text><text class="t2" id="f_bank2" x="480" y="198"></text>
<text class="t2" id="f_hinweis" x="280" y="250"></text>
</svg></div>
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

<div class="karte"><h2>Solarbank</h2>
<div class="soc-zeile"><div class="gross zahl" id="a_soc">–</div><div><div class="richtung" id="a_richtung">&nbsp;</div><div class="klein" id="a_kap"></div></div></div>
<div class="balken"><div class="wert" id="a_balken" style="left:0"></div></div>
<div class="zeile"><span>PV an der Solarbank</span><span class="zahl" id="a_pv">–</span></div>
<div class="zeile"><span>Abgabe ins Hausnetz</span><span class="zahl" id="a_ac">–</span></div>
<div class="zeile"><span>Geladen / entladen gesamt</span><span class="zahl" id="a_energie">–</span></div>
<p class="klein" id="a_alter"></p></div>

<div class="karte"><h2>Weitergabe</h2>
<div class="ziel"><span class="punkt" id="z_fronius"></span><div><b>Fronius: Netzzähler</b><span class="klein" id="z_fronius_t">–</span></div></div>
<div class="ziel"><span class="punkt" id="z_zweit"></span><div><b>Fronius: Solarbank (zweiter Zähler)</b><span class="klein" id="z_zweit_t">–</span></div></div>
<div class="ziel"><span class="punkt" id="z_mqtt"></span><div><b>openWB (MQTT)</b><span class="klein" id="z_mqtt_t">–</span></div></div>
<div class="ziel"><span class="punkt" id="z_wlan"></span><div><b>Heimnetz</b><span class="klein" id="z_wlan_t">–</span></div></div></div>
</div></section>

<!-- ============================ VERLAUF ============================ -->
<section class="seite" id="s_verlauf"><div class="raster">
<div class="karte breit"><h2>Letzte 24 Stunden (Mittelwerte je 5 Minuten)</h2>
<div class="schalter">
<label><input type="checkbox" data-k="1" checked><i style="background:#ea580c"></i>Netz</label>
<label><input type="checkbox" data-k="2" checked><i style="background:#ca8a04"></i>PV Symo</label>
<label><input type="checkbox" data-k="4" checked><i style="background:#2563eb"></i>Haus</label>
<label><input type="checkbox" data-k="3" checked><i style="background:#8b5cf6"></i>Solarbank (+ lädt)</label>
<label><input type="checkbox" data-k="5" checked><i style="background:#64748b"></i>Ladestand %</label></div>
<svg class="diagramm" id="diagramm" viewBox="0 0 760 280" preserveAspectRatio="none"></svg>
<div id="v_info" class="klein"></div>
<p class="klein" id="v_hinweis">Der Verlauf liegt nur im Arbeitsspeicher und beginnt nach jedem Neustart neu.</p></div>

<div class="karte breit"><h2>Energie in kWh</h2>
<div id="e_hinweis"></div>
<div class="tabelle"><table><thead><tr><th></th><th class="z">Heute</th><th class="z" id="e_gestern_kopf">Gestern</th><th class="z" id="e_monat_kopf">Dieser Monat</th></tr></thead>
<tbody id="e_tabelle"></tbody></table></div>
<p class="klein">Bezug und Einspeisung vom SDM630, PV vom Symo (Solar API), Solarbank aus ihren eigenen Zählern (Auflösung 0,1 kWh).
Der Hausverbrauch hat keinen eigenen Zähler: Er wird aus der Leistung alle 10 s aufsummiert, nur solange alle Werte da sind.</p></div>

<div class="karte breit"><h2>Vergangene Monate</h2>
<div class="tabelle"><table><thead><tr><th>Monat</th><th class="z">Bezug</th><th class="z">Einspeisung</th><th class="z">PV Symo</th><th class="z">geladen</th><th class="z">entladen</th><th class="z">Haus</th></tr></thead>
<tbody id="e_monate"><tr><td colspan="7" class="grau">noch kein Monat abgeschlossen</td></tr></tbody></table></div></div>
</div></section>

<!-- ============================ DIAGNOSE ============================ -->
<section class="seite" id="s_diagnose"><div class="raster">
<div class="karte"><h2>Netzzähler SDM630 (Bus A)</h2>
<div class="zeile"><span>Zustand</span><span id="d_zstatus">–</span></div>
<div class="zeile"><span>Busparameter</span><span id="d_zbus">–</span></div>
<div class="zeile"><span>Alter der Werte</span><span class="zahl" id="d_zalter">–</span></div>
<div class="zeile"><span>Runden gut / fehlerhaft</span><span class="zahl" id="d_zrunden">–</span></div>
<div class="zeile"><span>Letzter Fehler</span><span id="d_zfehler">–</span></div>
<div class="zeile"><span>Zusatzregister (C8, 156)</span><span id="d_zphasen">–</span></div></div>

<div class="karte doppelt"><h2>Fronius Datamanager (Bus B)</h2>
<div class="zeile"><span>Zustand</span><span id="d_fstatus">–</span></div>
<div class="zeile"><span>Baudrate</span><span id="d_fbus">–</span></div>
<div class="tabelle"><table><thead><tr><th></th><th>Netzzähler</th><th>Solarbank</th></tr></thead><tbody>
<tr><td>Adresse</td><td id="d_fa0">–</td><td id="d_fa1">–</td></tr>
<tr><td>Fragt nach</td><td id="d_fart0">–</td><td id="d_fart1">–</td></tr>
<tr><td>Beantwortet</td><td id="d_fok0">–</td><td id="d_fok1">–</td></tr>
<tr><td>Abgel. / ohne Antw.</td><td id="d_fnok0">–</td><td id="d_fnok1">–</td></tr>
<tr><td>Letzte Anfrage</td><td id="d_fletzt0">–</td><td id="d_fletzt1">–</td></tr>
<tr><td>Gemeldet</td><td id="d_fw0">–</td><td id="d_fw1">–</td></tr></tbody></table></div>
<div id="d_fhinweis"></div></div>

<div class="karte"><h2>Anker Solarbank (Modbus TCP)</h2>
<div class="zeile"><span>Zustand</span><span id="d_astatus">–</span></div>
<div class="zeile"><span>Adresse</span><span id="d_aip">–</span></div>
<div class="zeile"><span>Kennung</span><span id="d_amodell">–</span></div>
<div class="zeile"><span>Runden gut / fehlerhaft</span><span class="zahl" id="d_arunden">–</span></div>
<div class="zeile"><span>Letzter Fehler</span><span id="d_afehler">–</span></div>
<div class="zeile"><span>Batterie (Register 10008)</span><span class="zahl" id="d_abatt">–</span></div>
<div class="zeile"><span>Netz / Last laut Solarbank</span><span class="zahl" id="d_anetz">–</span></div>
<p class="klein">Der ESP32 liest nur (Funktionscode 04) und schreibt nie in die Solarbank.</p></div>

<div class="karte"><h2>Fronius Solar API (PV-Leistung)</h2>
<div class="zeile"><span>Zustand</span><span id="d_sstatus">–</span></div>
<div class="zeile"><span>Adresse</span><span id="d_sip">–</span></div>
<div class="zeile"><span>PV jetzt / heute</span><span class="zahl" id="d_spv">–</span></div>
<div class="zeile"><span>Netz laut Fronius</span><span class="zahl" id="d_snetz">–</span></div>
<div class="zeile"><span>Runden gut / fehlerhaft</span><span class="zahl" id="d_srunden">–</span></div>
<div class="zeile"><span>Letzter Fehler</span><span id="d_sfehler">–</span></div>
<p class="klein">„Netz laut Fronius“ ist der Wert, den der Datamanager von diesem Gerät bekommt. Er sollte zum SDM630 passen.</p></div>

<div class="karte"><h2>Netzwerk und System</h2>
<div class="zeile"><span>Heimnetz</span><span id="d_wlan">–</span></div>
<div class="zeile"><span>Eigenes WLAN</span><span id="d_ap">–</span></div>
<div class="zeile"><span>MQTT openWB</span><span id="d_mqtt">–</span></div>
<div class="zeile"><span>Gesendet / Ersatzwert / Speicher</span><span class="zahl" id="d_mzahl">–</span></div>
<div class="zeile"><span>Laufzeit</span><span class="zahl" id="d_lauf">–</span></div>
<div class="zeile"><span>Freier Speicher</span><span class="zahl" id="d_speicher">–</span></div>
<div class="zeile"><span>Version</span><span id="d_version">–</span></div>
<div class="zeile"><span>Uhrzeit</span><span id="d_zeit">–</span></div></div>

<div class="karte breit"><h2>Ereignisse (neueste oben)</h2>
<p class="klein">Ausfälle stehen hier erst, wenn sie länger als 10 s dauern. Die Liste liegt im Arbeitsspeicher: Nach einem Neustart steht als ältester Eintrag (ganz unten), warum neu gestartet wurde.</p>
<div class="tabelle"><table><thead><tr><th>Zeit</th><th>Ereignis</th></tr></thead>
<tbody id="ereignisse"><tr><td colspan="2" class="grau">lädt …</td></tr></tbody></table></div>
<p style="margin-top:12px"><a class="knopf" href="/api/diagnose" download>Diagnose-Datei herunterladen</a>
<span class="klein">&nbsp;Alle Werte, Einstellungen (ohne Passwörter), Ereignisse und Verlauf in einer Datei. Die kann man per Mail weitergeben.</span></p></div>

<div class="karte breit"><h2>Anfragen des Fronius (neueste oben)</h2>
<p class="klein">FC 3 = Holding-Register, FC 4 = Input-Register. „abgelehnt“ heißt: Der Fronius fragt Register, die dieses Gerät nicht kennt. Diese Zeilen bitte notieren.</p>
<div class="tabelle"><table><thead><tr><th>vor</th><th>Zähler</th><th>FC</th><th>Register</th><th>Anzahl</th><th>Bereich</th><th>Ergebnis</th></tr></thead>
<tbody id="prot"><tr><td colspan="7" class="grau">noch keine Anfragen</td></tr></tbody></table></div></div>
</div></section>

<!-- ============================ EINSTELLUNGEN ============================ -->
<section class="seite" id="s_einstellungen"><form id="form">
<div class="hinweisbox info">Änderungen gelten nach dem Speichern; das Gerät startet dabei neu (etwa 10 s).
Beim Speichern fragt der Browser nach Benutzer <b>admin</b> und dem Passwort des Geräte-WLANs.</div>

<fieldset><legend>Heimnetz (WLAN)</legend>
<p class="erkl">Damit die openWB die Werte bekommt und du die Seite im Heimnetz unter <b>http://sdm630-fronius.local</b> erreichst.</p>
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
<label class="haken"><input name="mqtt_speicher_an" type="checkbox" value="1"> Solarbank als Speicher an die openWB senden</label>
<p class="erkl">openWB/set/houseBattery/… (Leistung, Ladestand, Energie). In der openWB als Speichermodul „MQTT“ einstellen.</p>
<label class="haken"><input name="ersatz_an" type="checkbox" value="1"> Bei Zählerausfall Ersatzwert senden</label>
<div class="felder"><div><label>Ersatzwert Netzbezug in W</label><input name="ersatz_w" type="number" min="0" max="100000"></div></div>
<p class="erkl">Die openWB 1.9 merkt nicht, wenn keine Werte mehr kommen, und würde mit dem letzten Wert weiterladen. Ein hoher Bezug beendet das Überschussladen sicher.</p></fieldset>

<fieldset><legend>Anker Solarbank (Modbus TCP)</legend>
<p class="erkl">In der Anker-App einschalten: Gerät → Zahnrad → „Three-Party Control Settings“ → „Modbus TCP“. Dort steht auch die IP-Adresse. Am besten im Router eine feste Adresse vergeben.</p>
<label class="haken"><input name="anker_an" type="checkbox" value="1"> Solarbank abfragen</label>
<div class="felder"><div><label>IP-Adresse der Solarbank</label><input name="anker_ip" placeholder="z. B. 192.168.178.30"></div>
<div><label>Port</label><input name="anker_port" type="number" min="1" max="65535"></div>
<div><label>Unit-ID</label><input name="anker_unit" type="number" min="0" max="247"></div></div>
<p class="erkl">Port 502 und Unit-ID 1 sind die üblichen Werte. Der ESP32 liest nur, er verändert an der Solarbank nichts.</p></fieldset>

<fieldset><legend>Fronius (Bus B)</legend>
<p class="erkl">Normalerweise alles auf „automatisch“ lassen: Das Gerät hört zu und erkennt Baudrate und Adresse selbst.</p>
<div class="felder"><div><label>Baudrate</label><select name="fronius_baud"><option value="0">automatisch erkennen</option>
<option>9600:8N1</option><option>19200:8N1</option><option>9600:8E1</option><option>19200:8E1</option><option>38400:8N1</option><option>4800:8N1</option><option>2400:8N1</option><option>115200:8N1</option><option>9600:8N2</option><option>9600:8O1</option></select></div>
<div><label>Busadresse Netzzähler (0 = automatisch)</label><input name="fronius_adresse" type="number" min="0" max="247"></div>
<div><label>Busadresse Solarbank-Zähler (0 = aus)</label><input name="zweit_adresse" type="number" min="0" max="247"></div>
<div><label>SunSpec-Modell</label><select name="modell"><option value="213">213 – Gleitkomma (Standard)</option><option value="203">203 – Ganzzahl</option></select></div>
<div><label>Vorzeichen an den Fronius</label><select name="vorzeichen"><option value="1">+ Bezug / − Einspeisung (Standard)</option><option value="-1">umgedreht</option></select></div></div>
<p class="erkl">Zeigt Solar.web Einspeisung als Bezug, das Vorzeichen umdrehen. Nimmt der Datamanager den Zähler nicht an, Modell 203 versuchen.
Die Solarbank wird im Datamanager als <b>Sekundärzähler</b> mit dieser Adresse (meist 2) angelegt. Laden zählt dort wie ein Verbraucher (positiv), Entladen negativ.</p>
<label class="haken"><input name="solarapi_an" type="checkbox" value="1"> PV-Leistung über die Solar API lesen (für Energiefluss und Hausverbrauch)</label>
<div class="felder"><div><label>Adresse des Fronius im Heimnetz</label><input name="fronius_ip" placeholder="z. B. 192.168.178.40"></div></div>
<p class="erkl">Nur lesen, über das WLAN. Im Datamanager muss die „Solar API“ eingeschaltet sein (Einstellungen → Kommunikation bzw. Solar API).</p></fieldset>

<fieldset><legend>Netzzähler SDM630 (Bus A)</legend>
<p class="erkl">Muss zu den Einstellungen im Setup-Menü des Zählers passen (Baudrate, Parität, Adresse).</p>
<div class="felder"><div><label>Baudrate und Format</label><select name="zaehler_baud"><option>9600:8N1</option><option>9600:8E1</option><option>9600:8O1</option><option>9600:8N2</option>
<option>19200:8N1</option><option>19200:8E1</option><option>38400:8N1</option><option>38400:8E1</option><option>4800:8N1</option><option>2400:8N1</option></select></div>
<div><label>Busadresse (1–247)</label><input name="zaehler_adresse" type="number" min="1" max="247"></div></div></fieldset>

<fieldset><legend>Zugang zu diesem Gerät</legend>
<div class="felder"><div><label>Neues Passwort für das Geräte-WLAN und diese Seite</label>
<input name="ap_passwort" type="password" autocomplete="new-password" placeholder="leer = unverändert"></div></div>
<p class="erkl">8 bis 63 Zeichen. Das Passwort gilt danach für das Geräte-WLAN und für das Speichern hier (Benutzer admin). Gut aufheben.</p></fieldset>

<div class="knopfleiste"><button type="submit">Speichern und neu starten</button>
<button type="button" class="zweit" id="neustart">Nur neu starten</button><span id="meldung"></span></div>
</form>

<fieldset id="update"><legend>Firmware-Update</legend>
<div class="zeile"><span>Installiert</span><span id="u_version">–</span></div>
<div id="u_probe"></div>
<h3>Von GitHub</h3>
<p class="erkl">Sucht die neueste Version im Repository <b id="u_repo"></b>. Installiert wird nur, wenn du auf „Installieren“ tippst.</p>
<button type="button" class="zweit" id="u_suchen">Nach Updates suchen</button>
<p id="u_meldung"></p><pre class="text" id="u_text" style="display:none"></pre>
<button type="button" id="u_install" style="display:none">Installieren</button>
<div class="fortschritt" id="u_balken" style="display:none"><div></div></div>
<h3>Aus einer Datei</h3>
<p class="erkl">Geht auch ohne Internet: Datei <b>sdm630-fronius-&lt;Version&gt;.bin</b> auswählen. Strom während des Schreibens nicht trennen (etwa 30 s).
Läuft die neue Version nicht, startet nach einem Absturz wieder die alte.</p>
<input type="file" id="u_datei" accept=".bin"><br><br>
<button type="button" id="u_hoch">Datei hochladen und installieren</button>
<p id="u_dmeldung"></p></fieldset>
</section>

<!-- ============================ ERKLÄRUNG ============================ -->
<section class="seite" id="s_erklaerung"><div class="raster">
<div class="karte breit"><h2>Was dieses Gerät macht</h2>
<svg class="bild" viewBox="0 0 760 290" role="img" aria-label="Aufbau: Netzzähler, Solarbank, ESP32, Fronius, openWB, Browser">
<rect class="box" x="10" y="40" width="130" height="60" rx="10"/><text x="75" y="65" text-anchor="middle">Netzanschluss</text><text class="t2" x="75" y="83" text-anchor="middle">Hausanschluss</text>
<path class="lin" d="M140 70h40"/>
<rect class="box" x="180" y="40" width="130" height="60" rx="10"/><text x="245" y="65" text-anchor="middle">SDM630</text><text class="t2" x="245" y="83" text-anchor="middle">Netzzähler</text>
<path class="lin" d="M310 70h40 v55 h20"/><text class="t2" x="330" y="62" text-anchor="middle">RS485 A</text>
<rect class="box" x="180" y="190" width="130" height="60" rx="10"/><text x="245" y="215" text-anchor="middle">Anker Solarbank</text><text class="t2" x="245" y="233" text-anchor="middle">Max AC</text>
<path class="funk" d="M310 220h40 v-55 h20"/><text class="t2" x="330" y="245" text-anchor="middle">WLAN, Modbus TCP</text>
<rect class="esp" x="370" y="105" width="120" height="80" rx="10"/><text x="430" y="140" text-anchor="middle">ESP32</text><text class="t2" x="430" y="158" text-anchor="middle">dieses Gerät</text>
<path class="lin" d="M430 105V50H580"/><text class="t2" x="510" y="43" text-anchor="middle">RS485 B</text>
<rect class="box" x="580" y="20" width="170" height="60" rx="10"/><text x="665" y="45" text-anchor="middle">Fronius Symo</text><text class="t2" x="665" y="63" text-anchor="middle">2 Zähler: Netz, Solarbank</text>
<path class="funk" d="M490 145h90"/><text class="t2" x="535" y="138" text-anchor="middle">WLAN</text>
<rect class="box" x="580" y="115" width="170" height="60" rx="10"/><text x="665" y="140" text-anchor="middle">openWB</text><text class="t2" x="665" y="158" text-anchor="middle">Netz (EVU) und Speicher</text>
<path class="funk" d="M490 170h40 v70 h50"/>
<rect class="box" x="580" y="210" width="170" height="60" rx="10"/><text x="665" y="235" text-anchor="middle">Handy / PC</text><text class="t2" x="665" y="253" text-anchor="middle">diese Webseite</text>
</svg>
<p>Der Zähler <b>SDM630</b> sitzt am Netzanschluss und misst, wie viel Strom gerade aus dem Netz kommt (Bezug) oder hineingeht (Einspeisung).
Die <b>Anker Solarbank</b> meldet über das WLAN ihren Ladestand und wie viel sie gerade lädt oder entlädt.
Dieses Gerät sammelt beides und gibt es weiter: an den <b>Fronius</b> als zwei Zähler (Netzzähler und Solarbank),
an die <b>openWB</b> (Netzanschluss und Speicher, für das Laden mit Überschuss) und an diese <b>Webseite</b>.</p>
<p>Die Solarbank regelt weiter selbst mit ihrem eigenen Anker Smart Meter. Dieses Gerät liest sie nur aus und verändert an ihr nichts.</p></div>

<div class="karte"><h2>Vorzeichen</h2>
<p><b>Netzanschluss:</b> <b class="warn">positiv</b> = Bezug aus dem Netz, <b class="ok">negativ</b> = Einspeisung (Überschuss).</p>
<p><b>Solarbank:</b> <b class="ok">Laden</b> zählt am Fronius und in der openWB positiv, <b class="warn">Entladen</b> negativ.</p>
<p class="klein">Die Anker-App zeigt die Batterieleistung andersherum (Entladen positiv). Gemeint ist dasselbe.</p></div>

<div class="karte"><h2>Der Fronius</h2>
<p>Der Datamanager hält dieses Gerät für zwei „Fronius Smart Meter“: den Primärzähler am Einspeisepunkt (Netz) und einen
Sekundärzähler (Solarbank). Der Sekundärzähler erscheint in Solar.web als eigene Lastkurve. Einen Ladestand kann ein Zähler nicht melden,
der steht hier und in der openWB.</p>
<p>Nach dem Start hört das Gerät erst zu, erkennt Baudrate und Adressen und beantwortet dann zwei Varianten gleichzeitig:
<b>SunSpec</b> und die Register eines <b>Eastron</b>-Zählers. Unter „Diagnose“ siehst du, was der Fronius wirklich fragt.</p></div>

<div class="karte"><h2>Energiefluss und Hausverbrauch</h2>
<p>Das Gerät kennt drei Leistungen: <b>Netz</b> (SDM630), <b>PV des Symo</b> (Solar API des Datamanagers) und die <b>Solarbank</b>
(was sie ins Haus abgibt oder zum Laden daraus nimmt, gerechnet als ihre PV-Leistung plus Batterieleistung).
Der <b>Hausverbrauch</b> ist die Summe: Netz + PV Symo + Solarbank. Verluste in der Solarbank sind nicht berücksichtigt.</p>
<p class="klein">Die Tageswerte beginnen um Mitternacht. Dafür braucht das Gerät die Uhrzeit aus dem Internet.</p></div>

<div class="karte"><h2>Updates</h2>
<p>Neue Versionen kommen über <b>Einstellungen → Firmware-Update</b>, von GitHub oder als Datei. Die neue Version muss eine Minute
fehlerfrei laufen, sonst startet wieder die alte. Die Einstellungen bleiben bei einem Update erhalten.</p></div>

<div class="karte"><h2>Bei Störungen</h2>
<p><b>Netzzähler antwortet nicht:</b> Der Fronius bekommt für den Netzzähler keine Antwort und meldet einen Zählerfehler, statt mit alten Werten zu rechnen.
Die openWB bekommt einen hohen Ersatzwert für den Bezug (Standard 10 000 W) und beendet das Überschussladen.</p>
<p><b>Solarbank antwortet nicht:</b> Der Fronius bekommt für den zweiten Zähler keine Antwort. Die openWB bekommt für den Speicher 0 W.</p>
<p><b>WLAN weg:</b> Netzzähler und Fronius laufen weiter. Solarbank und openWB bekommen nichts, bis das WLAN wieder da ist.</p>
<p><b>Stromausfall:</b> Alle Einstellungen bleiben gespeichert.</p></div>

<div class="karte breit"><h2>Erste Hilfe</h2><div class="tabelle"><table class="faq">
<thead><tr><th>Was du siehst</th><th>Mögliche Ursache</th><th>Was du tun kannst</th></tr></thead><tbody>
<tr><td>Zähler „antwortet nicht“</td><td>Leitung A/B vertauscht oder lose, falsche Baudrate, Parität oder Adresse</td><td>A und B tauschen; im Setup-Menü des SDM630 Baudrate, Parität und Adresse nachsehen und unter Einstellungen eintragen</td></tr>
<tr><td>Solarbank „antwortet nicht“</td><td>Modbus TCP in der Anker-App aus, falsche IP-Adresse, WLAN getrennt</td><td>In der Anker-App „Modbus TCP“ einschalten, IP-Adresse dort ablesen und eintragen</td></tr>
<tr><td>Fronius „sucht Baudrate … nichts empfangen“</td><td>Leitung zum Datamanager, A/B vertauscht, oder Datamanager steht nicht auf „Fronius Smart Meter“</td><td>Verdrahtung prüfen; im Datamanager den Zähler einstellen</td></tr>
<tr><td>Fronius fragt die Solarbank nicht</td><td>Sekundärzähler im Datamanager nicht angelegt oder andere Adresse</td><td>Im Datamanager Sekundärzähler „Fronius Smart Meter“ mit Adresse 2 anlegen</td></tr>
<tr><td>Fronius: viele Anfragen „abgelehnt“</td><td>Der Fronius fragt Register, die das Gerät nicht kennt</td><td>Unter Diagnose die Zeilen notieren (Foto) und weitergeben</td></tr>
<tr><td>Solar.web zeigt Einspeisung als Bezug</td><td>Vorzeichen</td><td>Einstellungen → Fronius → Vorzeichen umdrehen</td></tr>
<tr><td>openWB „keine Verbindung“</td><td>Falsche Adresse der openWB, Heimnetz getrennt</td><td>Adresse der openWB im Router nachsehen und eintragen</td></tr>
<tr><td>Seite im Heimnetz nicht erreichbar</td><td>WLAN-Passwort falsch, Gerät außer Reichweite</td><td>Mit dem Handy ins Geräte-WLAN, dann http://192.168.4.1</td></tr>
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
let offen='uebersicht';
function reiter(){const n=(location.hash||'#uebersicht').slice(1);offen=n;
 document.querySelectorAll('.seite').forEach(s=>s.classList.toggle('aktiv',s.id=='s_'+n));
 document.querySelectorAll('nav a').forEach(a=>a.classList.toggle('aktiv',a.getAttribute('href')=='#'+n));
 if(!document.querySelector('.seite.aktiv')){$('s_uebersicht').classList.add('aktiv');offen='uebersicht'}
 if(offen=='verlauf'){verlauf();energie()}if(offen=='diagnose')ereignisse();if(offen=='einstellungen')updateStand()}
addEventListener('hashchange',reiter);reiter();

// Zustand eines Fronius-Zählers: 'ok', 'warn', '' (aus)
function fZustand(f,s){if(!f.bereit)return['warn','Erkennung läuft'];if(!s.adresse)return['','aus'];
 const n=s.beantwortet+s.abgelehnt+s.ohne_antwort;
 if(s.seit_s<0||n==0)return['warn','noch keine Anfrage an Adresse '+s.adresse];
 if(s.seit_s>15)return['warn','keine Anfrage seit '+s.seit_s+' s'];
 return s.beantwortet?['ok','fragt ab']:['warn','fragt, bekommt aber keine Antwort']}

// ---------- Status ----------
let fehlversuche=0;
async function status(){
 let d;try{d=await (await fetch('/api/status',{cache:'no-store'})).json();fehlversuche=0}
 catch(e){if(++fehlversuche>1){pille('p_zaehler','fehler','keine Verbindung zum Gerät')}return}
 const z=d.zaehler,f=d.fronius,w=d.wlan,m=d.mqtt,a=d.anker;const aktuell=z.gueltig&&z.aktuell;
 const fz=[fZustand(f,f.zaehler[0]),fZustand(f,f.zaehler[1])];

 // Kopf
 setz('k_ip',w.verbunden?w.ip:w.ap_ip);
 pille('p_zaehler',aktuell?'ok':'fehler',aktuell?'Netzzähler':'Netzzähler antwortet nicht');
 pille('p_fronius',!f.bereit?'warn':fz[0][0],!f.bereit?'Fronius: Suche':(fz[0][0]=='ok'?'Fronius':'Fronius: '+fz[0][1]));
 pille('p_anker',!a.aktiv?'':(a.aktuell?'ok':'fehler'),!a.aktiv?'Solarbank aus':(a.aktuell?'Solarbank':'Solarbank antwortet nicht'));
 pille('p_wlan',w.verbunden?'ok':(w.name?'fehler':''),w.verbunden?'Heimnetz':(w.name?'Heimnetz getrennt':'kein Heimnetz'));
 pille('p_mqtt',!m.aktiv?'':(m.verbunden?'ok':'fehler'),!m.aktiv?'openWB aus':(m.verbunden?'openWB':'openWB getrennt'));

 // Übersicht: Netz
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
 setz('phinweis','');
 setz('bezug',z.gueltig?tausender(z.bezug_kwh):'–');setz('einsp',z.gueltig?tausender(z.einspeisung_kwh):'–');

 // Übersicht: Solarbank
 if(!a.aktiv){setz('a_soc','–','gross zahl grau');setz('a_richtung','nicht eingerichtet','richtung grau');setz('a_kap','');
  $('a_balken').style.width='0';['a_pv','a_ac','a_energie'].forEach(i=>setz(i,'–'));setz('a_alter','Einstellungen → Anker Solarbank')}
 else if(!a.gueltig||!a.aktuell){setz('a_soc',a.gueltig?a.soc+' %':'–','gross zahl grau');
  setz('a_richtung','antwortet nicht','richtung fehler');setz('a_kap',a.fehler||'');$('a_balken').style.width=a.gueltig?a.soc+'%':'0';
  $('a_balken').style.background='var(--grau)';['a_pv','a_ac','a_energie'].forEach(i=>setz(i,'–'));setz('a_alter',a.gueltig?'letzter Wert vor '+a.alter_s+' s':'')}
 else{const lad=a.batterie_w<0,ent=a.batterie_w>0;
  setz('a_soc',a.soc+' %','gross zahl');
  setz('a_richtung',lad?'lädt mit '+kw(-a.batterie_w):(ent?'entlädt mit '+kw(a.batterie_w):'Ruhe'),'richtung '+(lad?'ok':(ent?'warn':'grau')));
  setz('a_kap',a.kapazitaet_kwh?'Kapazität '+komma(a.kapazitaet_kwh,1)+' kWh':'');
  const bb=$('a_balken');bb.style.width=a.soc+'%';bb.style.background=a.soc<15?'var(--rot)':'var(--blau)';
  setz('a_pv',kw(a.pv_w));setz('a_ac',kw(a.ac_w));
  setz('a_energie',tausender(a.lade_kwh)+' / '+tausender(a.entlade_kwh)+' kWh');setz('a_alter','Stand vor '+a.alter_s+' s')}

 // Übersicht: Weitergabe
 punkt('z_fronius',fz[0][0]);
 setz('z_fronius_t',!f.bereit?'Erkennung läuft: hört auf '+f.erkennung.baud+' Baud '+f.erkennung.format:
  fz[0][1]+(fz[0][0]=='ok'?', meldet '+(aktuell?kw(f.an_fronius_w):'–'):'')+' · Adresse '+f.zaehler[0].adresse);
 punkt('z_zweit',fz[1][0]);
 setz('z_zweit_t',!a.aktiv?'aus (Solarbank nicht eingerichtet)':(!f.bereit?'Erkennung läuft':
  fz[1][1]+(fz[1][0]=='ok'?', meldet '+(a.aktuell?kw(f.zweit_w):'–'):'')+(f.zaehler[1].adresse?' · Adresse '+f.zaehler[1].adresse:'')));
 punkt('z_mqtt',!m.aktiv?'':(m.verbunden?(aktuell?'ok':'warn'):'fehler'));
 setz('z_mqtt_t',!m.aktiv?'ausgeschaltet':(m.verbunden?(aktuell?'Netz jede Sekunde':'Netz: Ersatzwert (Zählerausfall)')+
  (m.speicher_an?(a.aktuell?' · Speicher':' · Speicher: 0 W (Solarbank fehlt)'):''):'keine Verbindung zu '+m.server));
 punkt('z_wlan',w.verbunden?'ok':(w.name?'fehler':''));
 setz('z_wlan_t',w.verbunden?w.name+' · '+w.ip+' · '+w.dbm+' dBm':(w.name?w.name+': nicht verbunden':'nicht eingerichtet'));

 // Diagnose: Netzzähler
 setz('d_zstatus',aktuell?'antwortet':'antwortet nicht',aktuell?'ok':'fehler');
 setz('d_zbus',z.baud+' Baud '+z.format+', Adresse '+z.adresse);
 setz('d_zalter',z.gueltig?komma(z.alter_ms/1000,1)+' s':'–');
 setz('d_zrunden',z.runden_ok+' / '+z.runden_fehler);setz('d_zfehler',z.fehler||'–');
 setz('d_zphasen',z.zusatz?'ja':(aktuell?'nicht alle':'–'));

 // Diagnose: Fronius
 setz('d_fstatus',!f.bereit?'Erkennung läuft':fz[0][1],!f.bereit?'warn':fz[0][0]);
 setz('d_fbus',f.bereit?f.baud+' Baud '+f.format+(f.automatik?' (erkannt)':' (fest)'):
  'prüft '+f.erkennung.baud+' '+f.erkennung.format+' · Durchlauf '+(f.erkennung.durchlaeufe+1));
 for(let i=0;i<2;i++){const s=f.zaehler[i];
  setz('d_fa'+i,s.adresse?String(s.adresse):'aus');
  const art=[];if(s.sunspec)art.push('SunSpec '+f.modell+' ('+s.sunspec+')');if(s.eastron)art.push('Eastron ('+s.eastron+')');
  setz('d_fart'+i,art.length?art.join(', '):'–');setz('d_fok'+i,String(s.beantwortet));
  setz('d_fnok'+i,s.abgelehnt+' / '+s.ohne_antwort);setz('d_fletzt'+i,s.seit_s>=0?'vor '+s.seit_s+' s':'–')}
 setz('d_fw0',aktuell?kw(f.an_fronius_w):'–');setz('d_fw1',a.aktiv&&a.aktuell?kw(f.zweit_w):'–');
 let hin='';const abgelehnt=f.zaehler[0].abgelehnt+f.zaehler[1].abgelehnt;
 if(!f.bereit)hin=f.erkennung.bytes?'<div class="hinweisbox warn">Auf der Leitung kommt etwas an ('+f.erkennung.bytes+' Bytes), aber noch keine gültige Anfrage. Die Suche probiert weitere Einstellungen.</div>':
  '<div class="hinweisbox warn">Auf der Leitung zum Fronius kommt nichts an. Verdrahtung (A/B) prüfen und im Datamanager „Fronius Smart Meter“ einstellen.</div>';
 else if(abgelehnt>0)hin='<div class="hinweisbox warn">Der Fronius fragt Register, die das Gerät nicht kennt. Die Zeilen „abgelehnt“ in der Tabelle unten bitte notieren.</div>';
 $('d_fhinweis').innerHTML=hin;

 // Diagnose: Solarbank
 setz('d_astatus',!a.aktiv?'nicht eingerichtet':(a.aktuell?'antwortet':'antwortet nicht'),!a.aktiv?'grau':(a.aktuell?'ok':'fehler'));
 setz('d_aip',a.ip?a.ip:'–');setz('d_amodell',a.modell||'–');
 setz('d_arunden',a.runden_ok+' / '+a.runden_fehler);setz('d_afehler',a.fehler||'–');
 setz('d_abatt',a.gueltig?a.batterie_w+' W ('+(a.batterie_w>0?'entlädt':(a.batterie_w<0?'lädt':'Ruhe'))+')':'–');
 setz('d_anetz',a.gueltig?kw(a.netz_w)+' / '+kw(a.last_w):'–');

 // Diagnose: Netzwerk
 setz('d_wlan',w.name?(w.verbunden?w.name+', '+w.ip+' ('+w.dbm+' dBm)':w.name+': nicht verbunden'):'nicht eingerichtet',w.verbunden?'ok':(w.name?'fehler':'grau'));
 setz('d_ap',w.ap_name+', '+w.ap_ip);
 setz('d_mqtt',!m.aktiv?'aus':(m.verbunden?'verbunden mit '+m.server:'keine Verbindung zu '+m.server+' (Code '+m.code+')'),!m.aktiv?'grau':(m.verbunden?'ok':'fehler'));
 setz('d_mzahl',m.gesendet+' / '+m.ersatz+' / '+m.speicher_gesendet);setz('d_lauf',dauer(d.laufzeit_s));
 setz('d_speicher',Math.round(d.speicher_frei/1024)+' KB');setz('d_version',d.version);
 setz('fuss','SDM630 → Fronius · Version '+d.version+' · läuft seit '+dauer(d.laufzeit_s));
 
 // Übersicht: Energiefluss
 fluss(d.fluss);$('probezeit').style.display=d.probezeit?'block':'none';
 // Diagnose: Solar API
 const sa=d.solarapi;
 setz('d_sstatus',!sa.aktiv?'nicht eingerichtet':(sa.aktuell?'antwortet':'antwortet nicht'),!sa.aktiv?'grau':(sa.aktuell?'ok':'fehler'));
 setz('d_sip',sa.ip||'–');setz('d_spv',sa.aktuell?kw(sa.pv_w==null?0:sa.pv_w)+' / '+(sa.tag_kwh==null?'–':komma(sa.tag_kwh,2)+' kWh'):'–');
 setz('d_snetz',sa.aktuell?kw(sa.netz_w):'–');setz('d_srunden',sa.runden_ok+' / '+sa.runden_fehler);setz('d_sfehler',sa.fehler||'–');
 setz('d_zeit',d.zeit?d.zeit_text:'noch nicht gestellt (braucht Internet)',d.zeit?'':'warn');
 const tb=$('prot');
 if(!f.protokoll.length){tb.innerHTML='<tr><td colspan="7" class="grau">noch keine Anfragen</td></tr>'}
 else{tb.innerHTML='';for(const p of f.protokoll){const tr=document.createElement('tr');
  [komma(p.vor_s,1)+' s',p.z==0?'Netz':'Solarbank',p.fc,p.start+'–'+(p.start+p.anzahl-1),p.anzahl,p.bereich,p.ergebnis].forEach(t=>{const td=document.createElement('td');td.textContent=t;tr.appendChild(td)});
  if(p.ergebnis!='beantwortet')tr.className='warn';tb.appendChild(tr)}}
}

// ---------- Energiefluss ----------
function linie(id,w,vor){const el=$(id);const an=w!=null&&Math.abs(w)>=20;
 el.setAttribute('class','fl'+(an?(vor?'':' rueck'):' ruhe'));return an}
function fluss(f){
 // PV
 if(!f.pv_aktiv){setz('f_pv','–');setz('f_pv2','Solar API aus');linie('fl_pv',null,true)}
 else{setz('f_pv',f.pv==null?'–':kw(f.pv));setz('f_pv2',f.pv==null?'keine Antwort':'');
  if(linie('fl_pv',f.pv,true))$('fl_pv').style.stroke='#ca8a04'}
 // Netz
 setz('f_netz',f.netz==null?'–':kw(Math.abs(f.netz)));
 setz('f_netz2',f.netz==null?'Zähler fehlt':(f.netz<-20?'Einspeisung':(f.netz>20?'Bezug':'')));
 if(linie('fl_netz',f.netz,f.netz>0))$('fl_netz').style.stroke=f.netz>0?'var(--orange)':'var(--gruen)';
 // Solarbank
 if(!f.bank_aktiv){setz('f_bank','–');setz('f_bank2','nicht eingerichtet');linie('fl_bank',null,true)}
 else if(f.bank==null){setz('f_bank','–');setz('f_bank2','antwortet nicht');linie('fl_bank',null,true)}
 else{setz('f_bank',f.soc==null?'–':f.soc+' %');
  setz('f_bank2',f.bank_laden>20?'lädt '+kw(f.bank_laden):(f.bank_laden<-20?'entlädt '+kw(-f.bank_laden):'Ruhe'));
  if(linie('fl_bank',f.bank,f.bank>0))$('fl_bank').style.stroke='var(--blau)'}
 // Haus
 setz('f_haus',f.haus==null?'–':kw(f.haus));
 setz('f_hinweis',f.haus==null?(f.netz==null?'Ohne Netzzähler kein Hausverbrauch':'Hausverbrauch braucht alle eingerichteten Werte'):
  (!f.pv_aktiv?'Ohne Solar API fehlt die PV des Symo im Hausverbrauch':''))}

// ---------- Verlauf ----------
const FARBE={1:'#ea580c',2:'#ca8a04',3:'#8b5cf6',4:'#2563eb',5:'#64748b'};
const NAME={1:'Netz',2:'PV',3:'Solarbank',4:'Haus',5:'Ladestand'};
let vdaten=null;
async function verlauf(){try{vdaten=await (await fetch('/api/verlauf',{cache:'no-store'})).json();zeichnen()}catch(e){}}
function zeichnen(){const sv=$('diagramm');if(!vdaten)return;
 const W=760,H=280,L=46,R=36,O=10,U=24,B=W-L-R,HH=H-O-U;
 const jetzt=vdaten.zeit?vdaten.zeit*1000:Date.now(),von=jetzt-86400000;
 const an=[...document.querySelectorAll('.schalter input')].filter(c=>c.checked).map(c=>+c.dataset.k);
 const pk=vdaten.punkte.map(p=>({t:jetzt-p[0]*1000,v:p}));
 let lo=0,hi=1000;for(const p of pk)for(const k of an)if(k!=5&&p.v[k]!=null){lo=Math.min(lo,p.v[k]);hi=Math.max(hi,p.v[k])}
 const schritt=[500,1000,2000,5000,10000].find(s=>(hi-lo)/s<=6)||20000;lo=Math.floor(lo/schritt)*schritt;hi=Math.ceil(hi/schritt)*schritt;
 const x=t=>L+(t-von)/86400000*B,y=v=>O+(hi-v)/(hi-lo)*HH,ys=v=>O+(100-v)/100*HH;
 let g='';
 for(let v=lo;v<=hi;v+=schritt){g+='<line class="'+(v==0?'null':'gitter')+'" x1="'+L+'" x2="'+(W-R)+'" y1="'+y(v)+'" y2="'+y(v)+'"/>'+
  '<text x="'+(L-6)+'" y="'+(y(v)+4)+'" text-anchor="end">'+(v/1000).toLocaleString('de-DE')+'</text>'}
 g+='<text x="'+(L-6)+'" y="'+(O+HH+18)+'" text-anchor="end">kW</text>';
 if(an.includes(5))for(const v of [0,50,100])g+='<text x="'+(W-R+6)+'" y="'+(ys(v)+4)+'">'+v+'%</text>';
 const d0=new Date(von);d0.setMinutes(0,0,0);
 for(let t=d0.getTime()+3600000;t<=jetzt;t+=3600000){const h=new Date(t).getHours();if(h%3)continue;
  g+='<line class="gitter" x1="'+x(t)+'" x2="'+x(t)+'" y1="'+O+'" y2="'+(O+HH)+'"/><text x="'+x(t)+'" y="'+(H-6)+'" text-anchor="middle">'+
  (vdaten.zeit?String(h).padStart(2,'0')+':00':'')+'</text>'}
 if(!vdaten.zeit)g+='<text x="'+(L+B/2)+'" y="'+(H-6)+'" text-anchor="middle">letzte 24 h (Uhrzeit unbekannt)</text>';
 for(const k of an){let d='',luecke=true,vorher=null;
  for(const p of pk){const v=p.v[k];if(v==null||(vorher&&p.t-vorher>vdaten.punkt_s*2500)){luecke=true;if(v==null){vorher=null;continue}}
   d+=(luecke?'M':'L')+x(p.t).toFixed(1)+' '+(k==5?ys(v):y(v)).toFixed(1);luecke=false;vorher=p.t}
  if(d)g+='<path class="kurve" d="'+d+'" stroke="'+FARBE[k]+'"'+(k==5?' stroke-dasharray="5 4"':'')+'/>'}
 g+='<line id="zeiger" class="zeiger" y1="'+O+'" y2="'+(O+HH)+'" style="display:none"/>';
 sv.innerHTML=g;
 if(!pk.length)setz('v_info','Noch keine Werte. Der erste Punkt kommt nach 5 Minuten.');else setz('v_info','Finger oder Maus auf das Bild: Werte zu dieser Zeit.');
 sv.onpointermove=ev=>{const r=sv.getBoundingClientRect();const t=von+((ev.clientX-r.left)/r.width*W-L)/B*86400000;
  let best=null;for(const p of pk)if(!best||Math.abs(p.t-t)<Math.abs(best.t-t))best=p;
  if(!best||Math.abs(best.t-t)>900000)return;const z=$('zeiger');z.style.display='';z.setAttribute('x1',x(best.t));z.setAttribute('x2',x(best.t));
  const zeit=vdaten.zeit?new Date(best.t).toLocaleTimeString('de-DE',{hour:'2-digit',minute:'2-digit'}):'vor '+Math.round(best.v[0]/60)+' min';
  setz('v_info',zeit+' · '+an.map(k=>NAME[k]+' '+(best.v[k]==null?'–':(k==5?best.v[k]+' %':kw(best.v[k])))).join(' · '))}}
document.querySelectorAll('.schalter input').forEach(c=>c.addEventListener('change',zeichnen));

const ZEILEN=['Bezug aus dem Netz','Einspeisung ins Netz','PV Symo erzeugt','Solarbank geladen','Solarbank entladen','Hausverbrauch'];
const kwh=v=>v==null?'–':v.toLocaleString('de-DE',{minimumFractionDigits:1,maximumFractionDigits:1});
function tagName(k){if(!k)return'';const s=String(k);return s.slice(6,8)+'.'+s.slice(4,6)+'.'}
function monatName(k){if(!k)return'';const s=String(k);return new Date(+s.slice(0,4),+s.slice(4,6)-1,1).toLocaleDateString('de-DE',{month:'long',year:'numeric'})}
async function energie(){let e;try{e=await (await fetch('/api/energie',{cache:'no-store'})).json()}catch(x){return}
 $('e_hinweis').innerHTML=e.uhrzeit?'':'<div class="hinweisbox warn">Die Uhrzeit ist noch nicht bekannt (Heimnetz mit Internet nötig). Bis dahin werden keine Tageswerte gezählt.</div>';
 setz('e_gestern_kopf',e.gestern.kennung?'Gestern ('+tagName(e.gestern.kennung)+')':'Gestern');
 setz('e_monat_kopf',e.monat.kennung?monatName(e.monat.kennung):'Dieser Monat');
 $('e_tabelle').innerHTML=ZEILEN.map((n,i)=>'<tr><td>'+n+'</td><td class="z">'+kwh(e.heute.kwh[i])+'</td><td class="z">'+
  (e.gestern.kennung?kwh(e.gestern.kwh[i]):'–')+'</td><td class="z">'+kwh(e.monat.kwh[i])+'</td></tr>').join('');
 $('e_monate').innerHTML=e.monate.length?e.monate.map(m=>'<tr><td>'+monatName(m.kennung)+'</td>'+m.kwh.map(v=>'<td class="z">'+kwh(v)+'</td>').join('')+'</tr>').join(''):
  '<tr><td colspan="7" class="grau">noch kein Monat abgeschlossen</td></tr>'}

// ---------- Ereignisse ----------
async function ereignisse(){let l;try{l=await (await fetch('/api/ereignisse',{cache:'no-store'})).json()}catch(e){return}
 const tb=$('ereignisse');tb.innerHTML='';
 if(!l.length){tb.innerHTML='<tr><td colspan="2" class="grau">keine Ereignisse</td></tr>';return}
 for(const e of l){const tr=document.createElement('tr');if(e.stoerung)tr.className='stoer';
  const a=document.createElement('td');a.textContent=e.zeit||('vor '+dauer(e.vor_s));
  const b=document.createElement('td');b.textContent=e.text;b.style.whiteSpace='normal';tr.append(a,b);tb.appendChild(tr)}}

// ---------- Update ----------
let uTimer=null;
async function updateStand(){let u;try{u=await (await fetch('/api/update',{cache:'no-store'})).json()}catch(e){return null}
 setz('u_version',u.version);setz('u_repo',u.repo);
 $('u_probe').innerHTML=u.probezeit?'<div class="hinweisbox info">Diese Version läuft noch in der Probezeit (1 Minute nach dem Start).</div>':'';
 const kl=u.zustand=='fehler'?'fehler':(u.zustand=='gefunden'||u.zustand=='fertig'?'ok':'');
 setz('u_meldung',u.meldung,kl);
 const t=$('u_text');t.style.display=u.beschreibung&&(u.zustand=='gefunden'||u.zustand=='aktuell')?'block':'none';t.textContent=u.beschreibung;
 const k=$('u_install');k.style.display=u.zustand=='gefunden'?'':'none';k.textContent='Version '+u.neue_version+' installieren';
 const b=$('u_balken');b.style.display=u.zustand=='laedt'||u.zustand=='fertig'?'block':'none';b.firstElementChild.style.width=u.prozent+'%';
 $('u_suchen').disabled=u.zustand=='sucht'||u.zustand=='laedt';
 if((u.zustand=='sucht'||u.zustand=='laedt')&&!uTimer)uTimer=setInterval(updateStand,1000);
 if(u.zustand!='sucht'&&u.zustand!='laedt'&&uTimer){clearInterval(uTimer);uTimer=null}
 if(u.zustand=='fertig')nachNeustart();
 return u}
$('u_suchen').addEventListener('click',async()=>{try{await fetch('/api/update/suchen',{method:'POST'})}catch(e){}updateStand()});
$('u_install').addEventListener('click',async()=>{if(!confirm('Neue Version jetzt installieren? Das Gerät startet danach neu.'))return;
 try{const r=await fetch('/api/update/installieren',{method:'POST'});if(!r.ok){setz('u_meldung',r.status==401?'Anmeldung fehlgeschlagen.':await r.text(),'fehler');return}}
 catch(e){setz('u_meldung','Keine Verbindung zum Gerät.','fehler');return}updateStand()});
$('u_hoch').addEventListener('click',async()=>{const f=$('u_datei').files[0];
 if(!f){setz('u_dmeldung','Bitte zuerst eine Datei auswählen.','fehler');return}
 if(!/\.bin$/i.test(f.name)){setz('u_dmeldung','Das ist keine .bin-Datei.','fehler');return}
 if(!confirm('Datei '+f.name+' installieren? Das Gerät startet danach neu.'))return;
 // Erst anmelden (der Browser fragt nach dem Passwort), dann die Datei senden
 try{const r=await fetch('/api/anmelden',{method:'POST'});if(!r.ok){setz('u_dmeldung','Anmeldung fehlgeschlagen.','fehler');return}}
 catch(e){setz('u_dmeldung','Keine Verbindung zum Gerät.','fehler');return}
 const fd=new FormData();fd.append('datei',f,f.name);const x=new XMLHttpRequest();const b=$('u_balken');b.style.display='block';
 x.upload.onprogress=ev=>{if(ev.lengthComputable){b.firstElementChild.style.width=Math.round(ev.loaded*100/ev.total)+'%';
  setz('u_dmeldung','Sende … '+Math.round(ev.loaded*100/ev.total)+' %','')}};
 x.onload=()=>{setz('u_dmeldung',x.responseText,x.status==200?'ok':'fehler');if(x.status==200)nachNeustart();else b.style.display='none'};
 x.onerror=()=>{setz('u_dmeldung','Verbindung abgebrochen. Die bisherige Version läuft weiter.','fehler');b.style.display='none'};
 x.open('POST','/api/update/datei');x.send(fd)});

// ---------- Einstellungen ----------
const FELDER=['zaehler_baud','zaehler_adresse','fronius_baud','fronius_adresse','zweit_adresse','modell','vorzeichen','wlan_name',
 'anker_ip','anker_port','anker_unit','mqtt_server','mqtt_port','mqtt_benutzer','ersatz_w','fronius_ip'];
const HAKEN=['mqtt_an','ersatz_an','anker_an','mqtt_speicher_an','solarapi_an'];
async function laden(){try{const d=await (await fetch('/api/einstellungen',{cache:'no-store'})).json();const f=$('form');
 for(const k of FELDER)if(f[k])f[k].value=d[k];
 for(const k of HAKEN)f[k].checked=d[k];
 setz('wpw',d.wlan_passwort_gesetzt?'Ein Passwort ist gespeichert (verborgen). Leer lassen, um es zu behalten.':'Noch kein Passwort gespeichert.');
 setz('mpw',d.mqtt_passwort_gesetzt?'Ein Passwort ist gespeichert (verborgen). Leer lassen, um es zu behalten.':'')}catch(e){}}
function nachNeustart(){let n=0;const t=setInterval(async()=>{n++;
 try{const r=await fetch('/api/status',{cache:'no-store'});if(r.ok){clearInterval(t);location.reload()}}catch(e){}
 if(n>30){clearInterval(t);setz('meldung','Gerät nicht wieder erreichbar. Neue Adresse? Ggf. über das Geräte-WLAN und http://192.168.4.1 öffnen.','fehler')}},2000)}
$('form').addEventListener('submit',async ev=>{ev.preventDefault();const f=ev.target;const daten=new URLSearchParams(new FormData(f));
 for(const k of HAKEN)if(!f[k].checked)daten.set(k,'0');
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
setInterval(()=>{if(offen=='verlauf'){verlauf();energie()}},60000);
setInterval(()=>{if(offen=='diagnose')ereignisse()},10000);
</script></body></html>)HTML";
