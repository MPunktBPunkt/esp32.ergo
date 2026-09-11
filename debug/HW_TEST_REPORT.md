# HW-Testbericht — esp32.ergo auf `.88` (2026-09-11)

Build-Instanz. Firmware: **`ergo 0.1.0-dev`** (BLE-Update `b101125` + Folge-OTAs).  
Gerät: ESP32-S3 **192.168.178.88**, MAC `68B6B329339C`.  
Bike: Hammer **TC174** `c2:32:a5:1e:bf:b5`.

Session beendet: kein Fahrer mehr zum Treten. Offene Punkte unten.

---

## Build (Host)

| Suite | Ergebnis |
|-------|----------|
| `pio test -e native` | **79/79** (codec 22, limiter 21, powermap 19, sweep 17) |
| `pio run -e ergo` | **OK** — RAM 18.7 % (61432), Flash **64.9 %** (1276469) |
| UI `GET /` | 33977 Bytes, 10 Reiter |
| Flash-Budget | unter 85 %-Schwelle aus `UPDATE_BLE.md` |

---

## OTA / Shell

| Check | Ergebnis |
|-------|----------|
| OTA Shell→BLE-Bin | OK |
| Ergo→Ergo Roundtrip | OK (`/ota-upload` Recovery) |
| `codecSelfTest` | `ok` |
| Hub-Heartbeat | `hubOk=true`, `ios.ergo_state` wechselt mit Link |
| SSE `/events` | Status-JSON |
| Control/Calib ohne Bike | `no-link` / HTTP 409 |

---

## BLE + Caps (TC174)

| Check | Ergebnis |
|-------|----------|
| Connect | `READY`, Name TC174, erinnert |
| Caps | `levels=16`, `strategy=EMULATE`, `powerTrusted=false` |
| Ranges | `resistanceHex=0A00A0000A00`, `powerRangeHex=fehlt` |
| Control Point | Notify (nicht Indicate), `controlGranted=true` |
| IBD-Notifies | ~2 Hz, `stale=false` solange Link steht |
| `caps.wide` | **`false`** (Doku/Checkliste erwartete sint16/`wide`) — Writes trotzdem Success |

---

## Steuerung

| Kommando | Ergebnis |
|----------|----------|
| `POST /api/control/power?watt=100` | **denied** — „Geraet meldet kein Wattziel“ (gewollt) |
| `POST /api/control/level?tenths=…` | ok; Rampe eine Stufe / ~2 s, sonst `deferred` |
| Bike-Antwort Stufe | `SetTargetResistance` / **Success** |
| `POST /api/control/stop` | sofort, Schattenstufe → 1.0, `StopPause` Success |
| `request` / `start` | Success |

Leerer Sattel: Stufenwechsel und Stop ohne Fahrer verifiziert.  
Unter Last: Stufenwechsel Success; systematischer Watt-vs-Stufe-Vergleich **nicht** abgeschlossen (siehe Live-Daten).

---

## Live-Daten unter Tritt

| Phase | Beobachtung |
|-------|-------------|
| Mit Kurbelimpuls | **12 W / 21 rpm**; Distanz stieg. Nach Stop kurz **73–93 W / 51–62 rpm** |
| Zwischenfenster mit 0 W | Distanz+elapsed eingefroren, Notifies weiter ~2 Hz |

**Kontext (User):** Auf dem Rad saß ein Kind, das **nicht ständig getreten** hat.
Die Null-Phasen sind deshalb **kein Decoder-/Firmware-Fehler**, sondern fehlender
Kurbelimpuls. Sobald getreten wurde, lief IBD (Leistung, Kadenz, Distanz) wie erwartet.

Stufen×Watt-Kurve und Sweep mit gehaltener Kadenz bleiben offen (Session beendet).

---

## Kalibrierung / Sweep

| Check | Ergebnis |
|-------|----------|
| Ohne Bike `sweep/start` | 409 `kein Bike verbunden` |
| Kurz-Sweep leer `coarse=1&settleS=3&windowS=5` | Start OK → Punkt „Kadenz zu niedrig“ → **ABORTED** + Stop |
| Map nach Connect | `levels=16`, Zellen leer |
| Test 1/2 mit Fahrer (60/80 rpm) | **nicht gefahren** |

---

## Nicht getestet

- Bike aus/an → LOST / Reconnect / READY
- Pulsgurt (`0x180D`)
- Sweep Test 1 & 2 mit gehaltener Kadenz
- Wirkung der Stufe unter stabiler Last (Watt steigt mit Stufe?)
- Nachtests 3–6 (`0x05`-Wirkung, Simulation `0x11`, Dual-Link Dauer, Crash unter Last)
- Steuermodi `OFF` / `MANUAL_ERG` / `HR_HOLD` (bewusst noch nicht im Code)

---

## Gerätezustand am Ende der Session

```text
fwType=ergo  version=0.1.0-dev
bike=READY (TC174)  levelTenths=10 (Stufe 1.0)
powerW=0  (kein Fahrer)
hubOk=true
```

Rollback-Probe falls nötig:  
`nodes/esp32.ftmsprobe/dist/ftmsprobe.0.1.4.esp32s3.bin`

---

## Für die Entwurfs-Instanz

1. `caps.wide=false` am Varon vs. sint16-Annahme klären (wirkt Success allein?).
2. Nach Stop: UI/Doku klarer machen, dass `Start/Resume` + erneuter Tritt nötig sein können.
3. Nächster HW-Block mit Fahrer: stabile Kadenz → Stufe 4 vs. 8 Wattmittel → dann Sweep 60/80 rpm.
4. Reconnect- und HR-Tests separat.

Details/Checkliste auch in [UPDATE_BLE.md](UPDATE_BLE.md).
