# Stand

**Dies ist der Einstiegspunkt.** Technische Gesamtschau:
[docs/ergometer/ENTWICKLERDOKU.md](docs/ergometer/ENTWICKLERDOKU.md).
Versionsgeschichte: [CHANGELOG.md](CHANGELOG.md).

Stand: **2026-09-14** · Firmware **0.3.23-dev**

---

## 0. Handoff

| Lesen | Inhalt |
|-------|--------|
| [`docs/ergometer/ENTWICKLERDOKU.md`](docs/ergometer/ENTWICKLERDOKU.md) | Technik, Persistenz, Sicherheit, Regelung |
| [`docs/ergometer/KALIBRIERUNG.md`](docs/ergometer/KALIBRIERUNG.md) | aktuelle Kennfläche |
| [`docs/ergometer/BRIDGE.md`](docs/ergometer/BRIDGE.md) | Bridge-Betriebsregeln |
| [`docs/ergometer/NACHTESTS.md`](docs/ergometer/NACHTESTS.md) | fünf Ergebnisse, Test 6 offen |
| diese Datei §4–§5 | was offen ist |
| [CHANGELOG.md](CHANGELOG.md) | was in welcher Version steckt |

### Offen

1. **Nachtest 6** — Crash unter Last (Stufe ~10, hart Reset ohne STOP)
2. **Bridge-Abnahme** MyWhoosh (ERG + Exclusive/Observer)
3. Optional: Kadenzband 100–110 / 110–120 dichter; `allowSimulation` aus
4. Formale ERG/HR/Reha-Fahrer-Abnahme

`debug/` hält Primärmessungen und Archiv — siehe [`debug/README.md`](debug/README.md).

---

## 1. Was läuft, und wo

| | |
|---|---|
| Gerät | ESP32-S3 auf `192.168.178.88`, MAC `68B6B329339C` |
| Bike | Hammer Varon XTR II, BLE-Name `TC174`, MAC `c2:32:a5:1e:bf:b5` |
| Hub | `192.168.178.113:8093`, `fwType: ergo` |
| Rollback-Bin | `../nodes/esp32.ftmsprobe/dist/ftmsprobe.0.1.4.esp32s3.bin` (Monorepo) |
| Build-Host | Debian, `pio` unter `/home/martin/.venvs/pio/bin/pio` |
| Aktuelle Bin | `dist/ergo.0.3.23-dev.esp32s3.bin` |
| Flash | Bin 1 472 992 B / App-Partition 1 966 080 B (`min_spiffs`) ≈ **74,9 %** (0.3.23-dev, 2026-09-13) |

Die Entwurfs-Instanz auf Windows hat **nur git** — kein PlatformIO, keinen
Compiler. **Firmware- und UI-Build brauchen Python** (`tools/pio_pack_ui.py` →
`pack_ui.py`). Bauen/Flashen: Build-Instanz. Messen: Mensch am Rad.

Auf Hardware bewährt: WLAN, Web-UI, OTA beide Richtungen, Hub-Heartbeat,
BLE-Central zwei Rollen, FTMS-Client mit Capability-Ableitung, Handsteuerung
über Limiter, Sweep-Abbruch ohne Kadenz, Bridge Exclusive/Takeover (Live),
Dual-HR + Heartrate-Relay mit MyWhoosh.

## 2. Hardware-Stand (Kurz)

**Stufenwirkung (2026-09-12):** `0x04` sint16 wirkt. Journal WORKS, **0 Widersprüche**.
Details: [debug/RELEASE_v0.1.0.md](debug/RELEASE_v0.1.0.md). Caps-Fix-Lektion:
[debug/UPDATE_CAPS_FIX.md](debug/UPDATE_CAPS_FIX.md) — **Erfolgsquittung beweist nichts.**

**Sweeps:** ~170 W @ Stufe 16 / 60 rpm ([HW_TEST1_60RPM.md](debug/HW_TEST1_60RPM.md));
~245 W @ 80 rpm ([HW_TEST2_80RPM.md](debug/HW_TEST2_80RPM.md)). Nachtests 3–5:
[NACHTESTS.md](docs/ergometer/NACHTESTS.md), [HW_NACHTEST_20260913.md](debug/HW_NACHTEST_20260913.md),
[UPDATE_NACHTEST_RESULTS.md](debug/UPDATE_NACHTEST_RESULTS.md).

UI-Flashbudget: gzip-PROGMEM (~33 kB statt ~110 kB). LittleFS nur noch, wenn UI
ohne Firmware-OTA austauschbar sein soll.

## 3. Nächste Schritte

1. Bridge-Abnahme MyWhoosh (ERG + Exclusive/Observer)
2. Nachtest 6 Crash unter Last (Stufe ~10, ESP-Reset **ohne** STOP; Ring vorher exportieren)
3. `allowSimulation` wieder aus, wenn nicht dauerhaft nötig
4. Formale ERG/HR/Reha-Abnahme mit Fahrer

**Befund HR-Quellen:** Bike-HR − Strap ≈ +25 bpm. Firmware **verweigert** HR_HOLD/Reha
**nicht** bei `hrSource=machine` — für Reha-Deckel problematisch; siehe
ENTWICKLERDOKU Sicherheit/Puls.

## 4. Harte Regeln

1. **`/ota-upload` und die WiFi/Hub-Shell bleiben.** `tools/deploy.sh` prüft Strings.
2. **Kein `[env]`-Block** in `platformio.ini`. `[common]` plus `${common.x}`.
   `build_src_filter` von `env:native` ist eine **Positivliste**.
3. **Arduino-frei und hosttestbar** bleiben die Bausteine in der Positivliste
   von `platformio.ini` `[env:native]` (aktuell 24 Translation Units).
4. **Jeder FTMS-Write nur durch den Limiter.** Bridge-`0x05` der App → PowerController,
   nie als `0x05` ans Bike.
5. **Kein Steuerweg über `0x05`** Set Target Power am Varon.
6. **Hub-Watchdog:** bei Bike-Link kein Blind-Restart unter Last — erst `08 01`.
   Ob das reicht, entscheidet Nachtest 6.
7. Im NimBLE-Callback wird **nicht** gelesen, gerechnet oder geurteilt.

## 5. Abnahmekriterien v0.1 — Stand

Liste: [PFLICHTENHEFT.md](docs/ergometer/PFLICHTENHEFT.md) §12 — **24** Kriterien
(Labels 1…22 plus 6a und 6b).

| Nr. | Stand |
|---|---|
| 1–4 | erfüllt (4 = CI curated fixtures, nicht „274 Pakete“) |
| 5 | **erfüllt** (Test 1: linear bis ~170 W @ Stufe 16; Journal WORKS) |
| 6 | **erfüllt für Architektur** (60 rpm voll + 80 rpm leicht); dichtere Map optional |
| 6a | **erfüllt** (Host + Live-Export + CI `--verify-curated`) |
| 6b | gebaut; Rampen-Stub auf Hardware (kein volles MAP) |
| 7 | Mechanik da (`MANUAL_ERG`); Fahrer-Abnahme offen |
| 8 | Ceiling-Flag + UI |
| 9–12 | nicht angefangen / nicht formal abgenommen |
| 13 | erfüllt (gewollter Neustart sendet `08 01`) |
| 14–17 | nicht angefangen / nicht formal abgenommen |
| 18 | Profile + Sessions + Workouts + Progression auf Hardware genutzt |
| 19–22 | nicht angefangen / nicht formal abgenommen |

## 6. Welches Dokument beantwortet was

| Datei | Rolle |
|---|---|
| **diese Datei** | Stand, Offen, harte Regeln |
| [CHANGELOG.md](CHANGELOG.md) | Versionsgeschichte |
| [PFLICHTENHEFT.md](docs/ergometer/PFLICHTENHEFT.md) | Konzept (was werden soll) |
| [ENTWICKLERDOKU.md](docs/ergometer/ENTWICKLERDOKU.md) | gebautes System |
| [GERAETEPROFIL.md](docs/ergometer/GERAETEPROFIL.md) | gemessenes Bike-Verhalten |
| [KALIBRIERUNG.md](docs/ergometer/KALIBRIERUNG.md) | Kennfläche |
| [BRIDGE.md](docs/ergometer/BRIDGE.md) | Bridge-Regeln |
| [NACHTESTS.md](docs/ergometer/NACHTESTS.md) | Nachtest-Ergebnisse (Test 6 offen) |
| [WEBINTERFACE.md](docs/ergometer/WEBINTERFACE.md) | UI-Designkonzept |
| [BEDIENUNG.md](docs/ergometer/BEDIENUNG.md) | kurze Bedienung |
| [debug/RELEASE_v0.1.0.md](debug/RELEASE_v0.1.0.md) | Release + Messwerte v0.1.0 |
| [debug/UPDATE_CAPS_FIX.md](debug/UPDATE_CAPS_FIX.md) | teuerste Lektion |
| [debug/HW_TEST1_60RPM.md](debug/HW_TEST1_60RPM.md) u. a. | Primärmessungen |

## 7. Offene Entscheidungen

- ~~Reihenfolge v0.2 gegen v0.3~~ — in der Praxis entschieden (Coach-Features in 0.1,
  Bridge als 0.3.x).
- ~~UI nach LittleFS?~~ — Budget vorerst durch gzip erledigt.
- **Gerätename** in NVS `esphub` oft noch Altlast — kosmetisch.
- **mDNS** vom Build-Host nicht auflösbar.

## 8. Git

Von der Entwurfs-Instanz aus wird **nicht** committet und **nicht** gepusht,
außer der Mensch sagt es ausdrücklich. Repo-lokal `user.email` =
`martin@bchmnn.de`, sonst landet die Firmenadresse in der GPL-Historie.
