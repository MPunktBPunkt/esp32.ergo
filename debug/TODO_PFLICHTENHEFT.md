# ANWEISUNG: Pflichtenheft weiter einarbeiten

**Status Connectivity (erledigt):** `.88` läuft `ergo 0.1.0-dev`, OTA Probe→Ergo und Ergo→Ergo ok,
`codecSelfTest=ok`, Hub-Heartbeat ok. Details: `UPDATE_CONNECTIVITY.md`.

**Ja — die andere Instanz soll jetzt das Pflichtenheft (Rev. 4) umsetzen.**  
Nächster Baustein laut Architektur und `UPDATE_CONNECTIVITY.md`: **BleCentral + FtmsClient**, Limiter davor.

---

## Harte Regeln (nicht brechen)

1. **`/ota-upload` und WiFi/Hub-Shell bleiben.** Jede neue Bin muss remote flashbar bleiben.
   `tools/deploy.sh` prüft die Strings — das so lassen.
2. **Kein `[env]`-Block** in `platformio.ini` — sonst sterben Native-Tests.
3. **Codec + Limiter bleiben Arduino-/NimBLE-frei** und hosttestbar (`pio test -e native`).
4. **Jeder FTMS-Write nur durch den Limiter** — kein Bypass aus BleCentral/UI.
5. **Kein Steuerweg über `0x05` Set Target Power** am Varon (Feature fehlt; Success lügt).
   ERG = Emulation über Resistance (`wide`/sint16), siehe Limiter-Tests.
6. **Nicht flashen ohne Build-Instanz / ohne Review**, wenn die Shell-Routen angefasst werden.
7. Hub-Watchdog: sobald ein Bike-Link steht, **kein Blind-Restart** unter Last
   (erst Stop `08 01` — Nachtest 6 / Pflichtenheft).

---

## Was als Nächstes aus dem Pflichtenheft

Priorität Coach v0.1 (nicht die ganze Web-UI auf einmal):

| Schritt | Ziel |
|---------|------|
| 1 | `BleCentral`: Scan, Connect, Remember/Forget — Rollen Bike + optional HR |
| 2 | `FtmsClient`: GATT `0x1826`, Features/Ranges → `ftms::Capabilities`, Notify `0x2AD2`/`0x2AD9` |
| 3 | Limiter verdrahten als einzigen Schreibpfad |
| 4 | Live-Werte in `/api/status` + SSE; Hub-`ios` über `SHELL` hinaus |
| 5 | Steuermodi laut Pflichtenheft: `OFF`, `MANUAL_LEVEL`, … — erst wenn Write-Pfad steht |

Vorlage BLE-Central: `esp32.ftmsprobe` (`BleProbe` → hier `BleCentral`/`FtmsClient`).  
HR-Muster: `esp32.heartrate`.

Docs laut README (noch privat / beim Release spiegeln):

- `PFLICHTENHEFT.md` Rev. 4
- `GERAETEPROFIL.md`, `NACHTESTS.md`, `WEBINTERFACE.md`

Pfad-Hinweis im Repo: README/`platformio.ini` erwähnen `private/docs/ergometer/` bzw. `docs/ergometer/`.
Wenn die Dateien nur lokal bei der Entwurfs-Instanz liegen: **hierher nach `docs/` kopieren**
oder den kanonischen Pfad in der nächsten Notiz festhalten.

---

## Definition of Done für diesen Abschnitt

- Native-Tests weiterhin grün (Codec + Limiter; neue BLE-Logik nur hosttestbar wo sinnvoll)
- `pio run -e ergo` grün, Flash ~sinnvoll (< Partition)
- Shell-Routen (`/`, `/api/status`, `/ota-upload`, Hub) unverändert nutzbar
- Kurznotiz in `debug/UPDATE_BLE.md` (oder ähnlich) für die Build-Instanz:
  was gebaut werden / geflasht werden soll, bekannte Risiken (Bike unter Last)

## Nicht in diesem Schritt

- Workout-Editor, geführte Tests, Bridge/Peripheral (v0.2/v0.3)
- Kalibrier-Sweep auf dem Bike ohne ausdrückliche HW-Session
- Gerätename umbenennen (kosmetisch; NVS `esphub` hält noch `FtmsProbe-S3`)

---

## Für die Build-Instanz danach

1. `git pull`
2. `pio test -e native && pio run -e ergo`
3. Nur nach Review: `./tools/deploy.sh --ota 192.168.178.88`
4. Rollback: `nodes/esp32.ftmsprobe/dist/ftmsprobe.0.1.4.esp32s3.bin`
