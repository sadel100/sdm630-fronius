"""
solarbank_simulator.py
---------------------------------------------------------------------------
Spielt eine Anker SOLIX Solarbank Max AC per Modbus TCP (nur zum Testen am PC).
Beantwortet nur Funktionscode 04 (Input-Register) mit der Belegung aus der
offiziellen Anker-Integration. Jede andere Anfrage (auch jeder Schreibversuch)
wird mit einer Modbus-Ausnahme abgelehnt und auf der Konsole gemeldet.

Start:   py tools/solarbank_simulator.py [port]      (Standard 502)
Dann am ESP32 die IP-Adresse dieses PCs als Solarbank eintragen.
Die Windows-Firewall muss Python im privaten Netz zulassen.
"""
import math
import socket
import struct
import sys
import threading
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 502
START = time.time()


def register():
    """Aktuelle Registerwerte (Adresse -> uint16)."""
    t = time.time() - START
    batterie = int(900 * math.sin(t / 30))          # + Entladen, − Laden
    soc = 50 + int(20 * math.sin(t / 120))
    r = {}

    def i32(adr, wert):
        wert &= 0xFFFFFFFF
        r[adr], r[adr + 1] = wert >> 16, wert & 0xFFFF

    r[10001] = 2 if batterie > 0 else 1
    i32(10002, 1200 + int(200 * math.sin(t / 50)))   # PV
    i32(10004, 0)
    i32(10008, batterie)
    i32(10010, 350)                                  # Last
    i32(10012, -800)                                 # Netz laut Solarbank
    r[10014] = soc
    i32(10018, 12345)                                # PV gesamt (0,1 kWh)
    i32(10208, max(batterie, 0))                     # AC-Abgabe
    i32(10250, 50)                                   # 5,0 kWh
    i32(10262, 4123)                                 # 412,3 kWh geladen
    i32(10264, 3889)                                 # 388,9 kWh entladen
    for i, wert in enumerate(struct.unpack(">5H", b"A17E2\0\0\0\0\0")):
        r[32768 + i] = wert
    return r


def antwort(anfrage):
    tid, pid, laenge, unit, fc = struct.unpack(">HHHBB", anfrage[:8])
    if fc != 4:
        print(f"  ABGELEHNT: Funktionscode {fc} (nur 04 erlaubt)", flush=True)
        pdu = struct.pack(">BB", fc | 0x80, 1)
    else:
        start, anzahl = struct.unpack(">HH", anfrage[8:12])
        regs = register()
        if anzahl < 1 or anzahl > 125 or any(a not in regs and not (10000 <= a <= 10300) for a in range(start, start + anzahl)):
            print(f"  ABGELEHNT: Register {start} + {anzahl}", flush=True)
            pdu = struct.pack(">BB", 0x84, 2)
        else:
            werte = [regs.get(a, 0) for a in range(start, start + anzahl)]
            pdu = struct.pack(">BB", 4, anzahl * 2) + struct.pack(f">{anzahl}H", *werte)
            print(f"  FC04 Unit {unit} ab {start}, {anzahl} Register", flush=True)
    return struct.pack(">HHHB", tid, pid, len(pdu) + 1, unit) + pdu


def kunde(verbindung, adresse):
    print(f"Verbindung von {adresse[0]}", flush=True)
    with verbindung:
        puffer = b""
        while True:
            daten = verbindung.recv(512)
            if not daten:
                break
            puffer += daten
            while len(puffer) >= 7:
                laenge = struct.unpack(">H", puffer[4:6])[0]
                if len(puffer) < 6 + laenge:
                    break
                anfrage, puffer = puffer[:6 + laenge], puffer[6 + laenge:]
                verbindung.sendall(antwort(anfrage))
    print(f"Verbindung von {adresse[0]} beendet", flush=True)


def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", PORT))
    s.listen(4)
    print(f"Solarbank-Simulator auf Port {PORT}", flush=True)
    while True:
        verbindung, adresse = s.accept()
        threading.Thread(target=kunde, args=(verbindung, adresse), daemon=True).start()


if __name__ == "__main__":
    main()
