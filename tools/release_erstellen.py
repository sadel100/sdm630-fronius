"""
Baut die Firmware für eine Veröffentlichung auf GitHub.

    py tools/release_erstellen.py

1. Liest die Versionsnummer aus include/konfiguration.h (FIRMWARE_VERSION).
2. Baut ohne Testschalter (PLATFORMIO_BUILD_FLAGS wird geleert).
3. Kopiert das Ergebnis nach release/sdm630-fronius-<Version>.bin
   (der Ordner release/ ist nicht im Git).

Danach auf GitHub (Repository sadel100/sdm630-fronius, öffentlich) eine
Release mit dem Kennzeichen v<Version> anlegen und die Datei anhängen.
Der Text der Release erscheint auf der Webseite der Box beim Suchen.
Hochgeladen wird NICHT von diesem Skript.
"""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

PROJEKT = Path(__file__).resolve().parent.parent
PIO = Path(os.environ["USERPROFILE"]) / ".platformio" / "penv" / "Scripts" / "pio.exe"


def version_lesen():
    text = (PROJEKT / "include" / "konfiguration.h").read_text(encoding="utf-8")
    # die Zeile ohne TEST_VERSION
    treffer = re.findall(r'FIRMWARE_VERSION = "(\d+\.\d+\.\d+)"', text)
    if len(treffer) != 1:
        sys.exit("Versionsnummer in konfiguration.h nicht eindeutig gefunden.")
    return treffer[0]


def main():
    version = version_lesen()
    umgebung = dict(os.environ)
    umgebung.pop("PLATFORMIO_BUILD_FLAGS", None)   # keine Testschalter
    print(f"Baue Version {version} …", flush=True)
    ergebnis = subprocess.run([str(PIO), "run", "-d", str(PROJEKT)], env=umgebung)
    if ergebnis.returncode != 0:
        sys.exit("Bauen fehlgeschlagen.")
    quelle = PROJEKT / ".pio" / "build" / "esp32" / "firmware.bin"
    ziel_ordner = PROJEKT / "release"
    ziel_ordner.mkdir(exist_ok=True)
    ziel = ziel_ordner / f"sdm630-fronius-{version}.bin"
    shutil.copyfile(quelle, ziel)
    print(f"\nFertig: {ziel} ({ziel.stat().st_size // 1024} KB)")
    print(f"Auf GitHub: Release mit Kennzeichen v{version} anlegen und diese Datei anhängen.")


if __name__ == "__main__":
    main()
