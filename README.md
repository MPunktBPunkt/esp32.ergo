# esp32.ergo

![Version](https://img.shields.io/badge/version-0.1.0-green)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Build](https://github.com/MPunktBPunkt/esp32.ergo/actions/workflows/build.yml/badge.svg)](https://github.com/MPunktBPunkt/esp32.ergo/actions/workflows/build.yml)
[![Donate](https://img.shields.io/badge/Donate-PayPal-00457C.svg?logo=paypal)](https://www.paypal.com/donate/?business=martin%40bchmnn.de&currency_code=EUR)

> **Trainingsrechner und BLE-Steuerung für das Ergometer Hammer Varon XTR II** — ERG-Emulation über die Widerstandsstufe, Pulsführung, Trainingszonen, Profile und Web-UI. Anbindung an [iobroker.esp-hub](https://github.com/MPunktBPunkt/iobroker.esp-hub).

> [!NOTE]
> **v0.1.0 (Coach) ist released.** Stand und Testergebnisse:
> [`debug/RELEASE_v0.1.0.md`](debug/RELEASE_v0.1.0.md). Weiterarbeit beginnt bei
> [`STATE.md`](STATE.md). Offen u. a. TestRunner, Flash-/UI-Budget, Bridge (v0.3).

> [!TIP]
> Stufenwirkung ist auf Hardware belegt (Journal WORKS, 0 Widersprüche). Caps-Fix
> und die Regel „Erfolgsquittung beweist nichts“ bleiben Pflichtlektüre:
> [`debug/UPDATE_CAPS_FIX.md`](debug/UPDATE_CAPS_FIX.md).

---

## Überblick

`esp32.ergo` ist ein **eigenständiger Trainings-Node** zwischen Ergometer und Fahrer. Der ESP verbindet sich als BLE Central mit dem Bike, holt sich den Puls aus einer von drei Quellen und stellt den Widerstand — die Web-UI ist während der Fahrt die **einzige** Anzeige, weil das Konsolendisplay bei bestehendem BLE-Link abschaltet.

| Das ist es | Das ist es nicht |
|------------|------------------|
| Trainingsrechner mit ERG-Emulation | Ersatz für MyWhoosh oder Zwift |
| Zonen, Profile, Workouts, Tests | Medizinisches Messgerät |
| Pulsgeführtes Training inkl. Reha-Deckel | Trainingsplan-Generator |
| Hub-Telemetrie + Session-Archiv | Cloud-Sync ohne Hub |

```
Varon XTR II ──BLE Central──┐
                            ├──▶ ESP32-S3 ──WiFi──▶ WebUI/SSE + ESP-Hub
HR-Relay / Polar H9 ────────┘         │
                                      └──BLE Peripheral──▶ MyWhoosh (Bridge, v0.3)
```

Der Puls kommt wahlweise direkt vom Gurt, vom [HR-Relay](https://github.com/MPunktBPunkt/esp32.heartrate) oder aus dem `0x2AD2`-Feld des Bikes, das der Polar H9 über 5 kHz GymLink speist. Die Relay-Variante ist die interessante: der H9 lässt nur **einen** BLE-Client zu, das Relay hält ihn und bedient MyWhoosh und den Ergo-Node gleichzeitig.

---

## Was das Gerät kann — und was nicht

Vier Befunde aus dem Sondenlauf mit [esp32.ftmsprobe](https://github.com/MPunktBPunkt/esp32.ftmsprobe) tragen die gesamte Architektur:

- **Standard-FTMS mit offenem Control Point.** Kein Hersteller-Protokoll, kein Reverse Engineering nötig.
- **Kein Set Target Power.** `0x2ACC` Target-Bit 3 ist gelöscht, `0x2AD8` fehlt ganz. Jede Wattsteuerung ist deshalb **Emulation über die Widerstandsstufe**.
- **16 Stufen**, 1,0 bis 16,0 in Zehnteln, als `04 <sint16 LE>`. Die 1-Byte-Form wird quittiert, wirkt aber nicht.
- **Eine Erfolgsquittung beweist nichts.** Das Bike antwortet auch auf `05 64 00` mit `80 05 01` Success, obwohl es das Feature nicht meldet.

Stufen-Sweep und Kadenzabhängigkeit fährt die geführte Kalibrierung selbst
(inkl. Verwerfung weggelaufener Kadenz). Auf Hardware: linear bis ~170 W @ Stufe 16;
Kadenztest leicht bestätigt.

**Nichts davon steht als Konstante im Code.** `ftms::Capabilities` leitet zur Verbindungszeit aus `0x2ACC`, `0x2AD6`, `0x2AD8` und den beobachteten `0x2AD2`-Flags ab, was das angeschlossene Gerät kann, und wählt daraus die Steuerstrategie: Wattziel direkt, Emulation über die Stufe, oder nur Dashboard. Ein anderes Ergometer ist damit ein Scan, ein Connect und ein Kalibrierlauf — kein Firmwarethema. Dasselbe gilt für den Pulsgurt: die BLE-Quellen sind reines `0x180D` und herstellerunabhängig.

---

## Features

### v0.1 — Coach *(released)*

- **BLE Central (NimBLE):** Scan, Connect, Remember / Forget für Bike und Gurt
- **Steuermodi:** `OFF`, `MANUAL_LEVEL`, `MANUAL_ERG` (emuliert), `HR_HOLD`, `REHA`, `WORKOUT`
- **Profile** (Mehrbenutzer), harte Grenzen, Zonen mit Ambient/Hysterese
- **Reha:** festes Watt + Pulsdeckel; **Progression** nach sauberer Physio-Einheit
- **Limiter** als einziger Schreibpfad; Steuer-Journal; Kalibrierung / Kennfläche
- **Web-UI:** Ride (Tablet), Bibliothek, Schritt-Editor, Tests-Stubs, Verlauf, Debug
- Session-Archiv, Hub-Heartbeat `fwType: ergo`, OTA in beide Richtungen

### v0.2 — Trainingslehre

Echter TestRunner (MAP / 20 min / Recovery), Interval-/Rampen-Editor, Ghost,
LittleFS-UI falls Flash-Budget es erzwingt.

### v0.3 — Bridge

FTMS-Peripheral mit aufgewertetem Feature-Satz: MyWhoosh verbindet sich mit dem ESP32 statt mit dem Bike und bekommt ein echtes Wattziel, das die Firmware in Stufen übersetzt.

---

## Hardware

| Board | PlatformIO-Env | Hinweis |
|-------|----------------|---------|
| ESP32-S3 | `ergo` | zwei BLE-Links im Coach, drei in der Bridge |

Kein D1 Mini: der Coach braucht zwei gleichzeitige Verbindungen, die Bridge drei. Kein PSRAM — `heartrate-s3` fährt drei Links plus WiFi und SSE ohne.

---

## Quickstart

```bash
pio run -e ergo --target upload
pio device monitor
```

1. Hotspot **`ESP-Ergo-Setup`** → WLAN + Hub-IP (Port `8093`)
2. Browser: `http://<ESP-IP>/` → Reiter **Geräte** → Suchen → Zeile mit `FTMS`-Marke antippen
3. Gerät erscheint im [ESP-Hub](https://github.com/MPunktBPunkt/iobroker.esp-hub) mit `fwType: ergo`

Die Rolle wird aus dem Advertising abgeleitet: was `0x1826` bewirbt, wird als Bike verbunden, was nur `0x180D` hat, als Pulsgurt. Beides wird gemerkt und nach einem Verbindungsverlust mit ansteigendem Abstand (2, 5, 10, 20, 30 s) neu versucht.

Ohne Kabel ausrollen:

```bash
tools/deploy.sh --ota 192.168.178.88
tools/deploy.sh --hub 192.168.178.113:8093 --mac 68B6B329339C
```

Das Skript baut, benennt die Bin nach dem Familienschema und **weigert sich, eine Bin auszurollen, in der `/ota-upload` nicht vorkommt.** Genau dieser Fall lag bis 0.1.0-dev vor: die Firmware baute, hatte aber keinen Rückweg — ein OTA-Flash auf ein Gerät ohne Kabel wäre ein Totalverlust gewesen.

| | |
|--|--|
| mDNS | `ergo-XXXXXX.local` |
| OTA | `POST /ota-upload` (multipart `firmware`) |
| Bin-Schema | `ergo.<semver>.esp32s3.bin` |
| WLAN zurücksetzen | BOOT / GPIO0 ca. 3 s halten |

---

## Libraries

| Library | Autor | Version |
|---------|-------|---------|
| WiFiManager | tzapu | ≥ 2.0.17 |
| ArduinoJson | bblanchon | ≥ 7.2 |
| NimBLE-Arduino | h2zero | ≥ 1.4.3 |

Platform: `espressif32@6.4.0`, Framework Arduino. NimBLE ist auf 1.4.x gepinnt — gleiche API wie `esp32.heartrate` und `esp32.ftmsprobe`, damit Code zwischen den dreien wandern kann.

---

## Hub-IO-Werte

Heartbeat-Feld `fwType`: **`ergo`**

| Key / Feld | Bedeutung |
|------------|-----------|
| `ergo_state` / `control_mode` | BLE-Zustand, aktiver Steuermodus |
| `profile` | aktives Nutzerprofil |
| `power` / `power_target` | Ist- und Zielleistung in W |
| `level` / `level_target` | Widerstandsstufe — **Schattenwert**, siehe unten |
| `cadence` / `speed` / `distance` | rpm, km/h, m |
| `heart_rate` / `hr_source` / `hr_zone` | BPM, Quelle, Zone 1–5 |
| `work_kj` / `calories` | kJ, kcal |
| `np` / `if` / `tss` | Normalized Power, Intensity Factor, Training Stress Score |
| `workout_name` / `workout_step` / `workout_remaining` | laufendes Programm |
| `target_reachable` | 0, wenn das Wattziel über der Stufendecke liegt |

`level` ist der Schattenwert des ESP, keine Rückmeldung des Bikes: `0x2AD2` liefert bei diesem Gerät kein Resistance-Level-Feld.

---

## API (Auswahl)

Die Oberfläche hat die zehn Reiter aus [WEBINTERFACE.md](docs/ergometer/WEBINTERFACE.md) §7 — **Ride, Workouts, Tests, Verlauf, Profile, Geräte, Kalibrierung, Debug, Einstellungen, OTA**. Sechs davon tragen Inhalt, vier sind Platzhalter mit Zielversion. Ohne JavaScript zeigt die Seite alle Abschnitte untereinander und das OTA-Formular sendet native; diese Seite ist der Rückweg nach einem Fehlflash und darf nicht an einem Skriptfehler hängen.

Erreichbar sind bisher die Shell (`/`, `/ota`, `/ota-upload`, `/api/status`, `/api/config/get` `/save`, `/api/system/restart`, `/events`), die BLE-Endpunkte (`/api/ble/scan/start` `/stop`, `/api/ble/devices`, `/api/ble/connect` `/disconnect` `/forget` `/reconnect`), die Handsteuerung (`/api/control/request` `/reset` `/start` `/stop` `/level` `/power`) die Kalibrierung (`/api/calib/sweep/start` `/stop`, `/api/calib/map`, `/api/calib/clear`) und der Debug-Modus (`/api/debug/ring`, `/api/debug/clear`, `/api/debug/export`).

| Endpoint | Funktion |
|----------|----------|
| `GET /api/status` | Gesamtstatus, Live-Werte, Session |
| `GET /api/history` | Chart-Historie |
| `GET /api/ble/devices` · `POST /api/ble/scan/start` `/stop` | Scan |
| `POST /api/ble/connect` `/disconnect` `/remember` `/forget` | Verbindung |
| `POST /api/control/mode` | `off` / `level` / `erg` / `hr` / `workout` / `sim` |
| `POST /api/control/target` · `POST /api/control/stop` | Zielwert, Not-Stop |
| `POST /api/calib/sweep/start` `/stop` | Geführter Stufen-Sweep |
| `GET /api/calib/map` · `POST /api/calib/clear` | Kennfläche Stufe × Kadenz → Watt |
| `GET/POST /api/profile/list` `/get` `/put` `/select` | Profile |
| `GET/POST /api/workout/list` `/load` `/start` `/pause` `/skip` | Programme |
| `POST /api/workout/put` · `GET /api/workout/download` `/validate` | Editor (v0.2) |
| `GET/POST /api/test/list` `/start` `/result` `/accept-ftp` | Geführte Tests (v0.2) |
| `GET/POST /api/calib/…` | Kennfläche, Sweep |
| `POST /api/debug/ring` `/clear` | Mitschnitt ein/aus, Ausdünnung, leeren |
| `GET /api/debug/export` | NDJSON-Rohbytes im Sondenformat — direkt als Fixture verwertbar |
| `GET/POST /api/config/get` `/save` | Config |
| `/events` | SSE (Live-Updates) |

---

## Build und Tests

```bash
pio run -e ergo              # Firmware für den S3
pio test -e native           # Codec und Limiter auf dem Host
```

Zwei Hostsuiten: `test_codec` prüft den FTMS-Decoder gegen die aufgezeichneten Pakete, `test_limiter` die Sicherheitsschicht — Whitelist, Klemmen, Rasterung, Rampe und Deadman.

Beide Bausteine sind bewusst frei von Arduino, NimBLE und Zustand; der Limiter bekommt sogar die Zeit als Parameter statt `millis()` zu lesen. Bei einer Komponente, die verhindern soll, dass ein Ergometer unter einem Menschen stehen bleibt, ist ein Test der echten Logik kein Luxus.

> In `platformio.ini` gibt es bewusst **keine** `[env]`-Sektion: PlatformIO vererbt sie an jedes Environment, womit `env:native` das `framework = arduino` samt Boardpflicht mitbekäme. Gemeinsame Werte stehen in `[common]` und werden explizit referenziert. Die Sollwerte der Fixtures stammen aus `tools/ftms.py` der Sonde, also aus einer unabhängigen zweiten Implementierung — sonst prüfte der Test sich selbst.

Vollständigen Fixture-Satz erzeugen:

```bash
# Stichprobe aus Debug-Export (Abnahme 6a) — CI und lokal:
python tools/make-fixtures.py --verify-curated

# Optional voller Laborlauf (schreibt Temp-Header; fixtures_ibd.h nur mit --write-curated):
python tools/make-fixtures.py --scan ../nodes/esp32.ftmsprobe/docs/ergometer/scan-20260910
```

`test/test_codec/fixtures_synth.h` wird davon **nicht** überschrieben. Es deckt die Feldkombinationen ab, die der Varon nie sendet: über 832 aufgezeichnete Pakete hinweg schickt das Gerät ausschließlich `flags = 0x0B54` mit 19 Byte. Ohne die konstruierten Pakete hätte man einen Varon-Decoder statt eines FTMS-Decoders.

---

## Docs

Die Planungsunterlagen liegen unter [`docs/ergometer/`](docs/ergometer/):

| Dokument | Inhalt |
|----------|--------|
| [PFLICHTENHEFT.md](docs/ergometer/PFLICHTENHEFT.md) | Zielbild, Steuerung, Regelung, Versionen — Revision 4 |
| [WEBINTERFACE.md](docs/ergometer/WEBINTERFACE.md) | Designkonzept der Web-UI: Zonen, Profile, Editor, Tests |
| [GERAETEPROFIL.md](docs/ergometer/GERAETEPROFIL.md) | was am Gerät gemessen wurde, inkl. Abweichungen vom Standard |
| [NACHTESTS.md](docs/ergometer/NACHTESTS.md) | sechs offene Messungen mit Kommandos und Entscheidungslogik |
| [BLE-SCAN.md](docs/ergometer/BLE-SCAN.md) | Vorgehen beim Erkunden eines unbekannten Geräts |

---

## Verwandte Projekte

| Projekt | Rolle |
|---------|-------|
| [esp32.ftmsprobe](https://github.com/MPunktBPunkt/esp32.ftmsprobe) | Laborsonde; `BleProbe` wird hier zu `BleCentral` + `FtmsClient`, `tools/ftms.py` ist die Referenz für `FtmsCodec` |
| [esp32.heartrate](https://github.com/MPunktBPunkt/esp32.heartrate) | HR-Relay als Pulsquelle; `HrServer` ist die Vorlage für den `FtmsServer` der Bridge |
| [iobroker.esp-hub](https://github.com/MPunktBPunkt/iobroker.esp-hub) | Registrierung, IO-Werte, OTA-Verteilung |

---

## Lizenz & Support

GNU General Public License v3.0 © MPunktBPunkt — siehe [LICENSE](LICENSE).

Wenn dir das Projekt hilft, freue ich mich über einen Kaffee:

[![Donate](https://img.shields.io/badge/Donate-PayPal-00457C.svg?logo=paypal)](https://www.paypal.com/donate/?business=martin%40bchmnn.de&currency_code=EUR)
