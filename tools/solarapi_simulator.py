"""
Spielt die Fronius Solar API eines Datamanagers (nur zum Testen am PC).

    py tools/solarapi_simulator.py [Port]      (Standard: Port 80)

Antwortet auf GET /solar_api/v1/GetPowerFlowRealtimeData.fcgi mit einer
PV-Leistung, die langsam schwankt, und einem steigenden Gesamtzähler.
Alles andere wird mit 404 beantwortet und gemeldet.
Die Windows-Firewall muss Python im privaten Netz zulassen.
"""
import json
import math
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 80
START = time.time()
GESAMT_WH = 23_456_700.0   # Gesamtzähler beim Start
TAG_WH = 8_000.0
letzte_zeit = time.time()


def pv_leistung():
    # schwankt zwischen etwa 1,5 und 4,5 kW (Periode 4 Minuten)
    return 3000 + 1500 * math.sin((time.time() - START) / 240 * 2 * math.pi)


class Anfrage(BaseHTTPRequestHandler):
    def do_GET(self):
        global GESAMT_WH, TAG_WH, letzte_zeit
        if not self.path.startswith("/solar_api/v1/GetPowerFlowRealtimeData.fcgi"):
            print("abgelehnt:", self.path, flush=True)
            self.send_error(404)
            return
        jetzt = time.time()
        pv = pv_leistung()
        # Zähler aufsummieren (W * s / 3600 = Wh)
        GESAMT_WH += pv * (jetzt - letzte_zeit) / 3600
        TAG_WH += pv * (jetzt - letzte_zeit) / 3600
        letzte_zeit = jetzt
        antwort = {
            "Body": {"Data": {
                "Inverters": {"1": {"DT": 123, "P": round(pv, 1)}},
                "Site": {"Mode": "meter", "Meter_Location": "grid", "P_Akku": None,
                         "P_Grid": -1200.0, "P_Load": round(-pv + 1200, 1), "P_PV": round(pv, 1),
                         "E_Day": round(TAG_WH), "E_Year": None, "E_Total": round(GESAMT_WH),
                         "rel_Autonomy": 100.0, "rel_SelfConsumption": 40.0},
                "Version": "12"}},
            "Head": {"RequestArguments": {}, "Status": {"Code": 0, "Reason": "", "UserMessage": ""},
                     "Timestamp": time.strftime("%Y-%m-%dT%H:%M:%S+01:00")},
        }
        daten = json.dumps(antwort).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(daten)))
        self.end_headers()
        self.wfile.write(daten)
        print(f"PV {pv:6.0f} W, gesamt {GESAMT_WH / 1000:.3f} kWh", flush=True)

    def log_message(self, *args):
        pass


print(f"Solar-API-Simulator auf Port {PORT}", flush=True)
ThreadingHTTPServer(("0.0.0.0", PORT), Anfrage).serve_forever()
