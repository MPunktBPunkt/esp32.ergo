# Entwicklerdokumentation — esp32.ergo

**Stand:** 2026-09-13 · Firmware **0.3.21-dev**  
**Gerät:** ESP32-S3 `192.168.178.88` · Bike Hammer Varon XTR II (BLE `TC174`) · Polar H9  

Dies ist die **technische Gesamtschau**: Technologien, Probe-Weg, was Bike und
Gurt wirklich senden, welche Debug-Befunde die Architektur erzwungen haben, und
die gemessenen Kalibrierdaten. Der lebende Betriebsstand bleibt
[`STATE.md`](../../STATE.md). Detailprotokolle einzelner Arbeitsschritte liegen
unter `debug/` und werden nicht nachgeführt.

---

## 1. Zweck und Lesereihenfolge

| Wenn du … | lies zuerst |
|-----------|-------------|
| am Gerät weiterarbeitest | [`STATE.md`](../../STATE.md) |
| Architektur / Grenzen brauchst | diese Datei + [`GERAETEPROFIL.md`](GERAETEPROFIL.md) |
| UI / Profile / Workouts planst | [`WEBINTERFACE.md`](WEBINTERFACE.md), [`PFLICHTENHEFT.md`](PFLICHTENHEFT.md) |
| Bridge / MyWhoosh | [`BRIDGE.md`](BRIDGE.md) |
| Rohmessungen der Sonde | `nodes/esp32.ftmsprobe/docs/ergometer/` |

---

## 2. Technologien und Topologie

### Stack (Ergo-Firmware)

| Schicht | Technologie |
|---------|-------------|
| MCU | ESP32-S3 (Arduino / PlatformIO, NimBLE) |
| WLAN | WiFiManager, mDNS, NTP, Hub-Heartbeat (`fwType: ergo`) |
| BLE Central | Bike (FTMS) + optional HR (`0x180D`) |
| BLE Peripheral | FTMS-Bridge (+ CPS/CSC für Apps) |
| Persistenz | NVS (Config, DeviceStore, PowerMap je Slot), LittleFS (Workouts/Sessions) |
| UI | eine gzip-PROGMEM-Seite (`web/index.html` → `UiPagesGz.h`), SSE `/events` |
| Hosttests | `pio test -e native` — Codec, Limiter, PowerMap, … ohne Arduino |

### Topologie (Familie)

```
Polar H9 ──BLE──▶ ESP heartrate (Relay)  ──BLE──▶ MyWhoosh / ergo
                     └── 5-kHz GymLink ──▶ Bike-Konsole (HR in 2AD2)

Varon TC174 ──BLE──▶ ESP ergo  ──BLE Peripheral──▶ MyWhoosh (FTMS)
                        └── WiFi ──▶ WebUI + Hub :8093
```

Ein Bike erlaubt **nur einen Central**. MyWhoosh und die Coach-UI teilen sich
die Steuerung über die Bridge (Observer vs. Controller), nicht über einen
zweiten Direct-Link zum Varon.

---

## 3. Der Weg: FTMS-Probe → Entscheidungen → Ergo

### 3.1 Warum eine Sonde

Vor `esp32.ergo` brauchte es ein Werkzeug, das **Rohbytes** und **Wirkung**
trennt: Erfolgsquittung am Control Point ≠ Last am Rad. Die Sonde
[`esp32.ftmsprobe`](https://github.com/MPunktBPunkt/esp32.ftmsprobe) ist genau
das: BLE-Central, Guard (Whitelist, Deadman), Ring-Log, automatisierter Runner.

### 3.2 Laborlauf 2026-09-10

| Artefakt | Ort |
|----------|-----|
| Runner / Bericht | `nodes/esp32.ftmsprobe/docs/ergometer/scan-20260910/` |
| GATT, Reads, Effects | `gatt.json`, `reads.json`, `effects.json`, `ergebnis.json` |
| Rohstreams | `bike-data.jsonl`, `controlpoint.jsonl`, `probe-log.jsonl` |
| Verdichtung | `ERGEBNISBERICHT.md`, `DATEN.md` im Probe-Repo |
| Kanon in ergo | [`GERAETEPROFIL.md`](GERAETEPROFIL.md) |

Ablauf (vereinfacht):

1. Scan / Connect per **MAC** (`c2:32:a5:1e:bf:b5`) — Advertising während Link oft leer  
2. GATT walk + Read Feature (`2ACC`), Resistance Range (`2AD6`), IBD Subscribe (`2AD2`)  
3. Control Point: Request Control → Start → Resistance / Power / Stop  
4. Wirkung nur mit Trittfrequenz bewerten (sonst Artefakte)  
5. Ergebnisse → Geräteprofil + Nachtest-Plan ([`NACHTESTS.md`](NACHTESTS.md))

### 3.3 Was in die Firmware wanderte

| Probe-Idee | Ergo-Baustein |
|------------|---------------|
| `BleProbe` Multi-Role | `BleCentral` (Bike + HR) |
| IBD-Decode | `FtmsCodec` / `FtmsClient` |
| Guard / Deadman | `Limiter` (einziges Write-Tor) |
| Wirkung vs. Quittung | `ControlJournal` (WORKS / NO_EFFECT / Widerspruch) |
| Sweep von Hand | `SweepRunner` + UI Kalibrierung |
| Rohmitschnitt | `DebugRing` + Probe-API (`/api/probe/arm`) |

---

## 4. Was das Hammer-Ergometer sendet

### 4.1 Identität

| Feld | Wert |
|------|------|
| BLE-Name | `TC174` (kein „Hammer/Varon“ im BLE) |
| MAC | `c2:32:a5:1e:bf:b5` (Public) |
| Manufacturer | `ASR` |
| FW / SW | `6.1.2` / `6.3.0` |

### 4.2 GATT (gemessen)

```
1800 Generic Access          2A00 2A01 2AC9
1801 Generic Attribute       2A05 2B29 2B2A
180A Device Information      …
1850 Vendor                  2C00 2C01   (für uns irrelevant)
1826 FTMS                    2ACE(N) 2AD9(W+N) 2AD6(R) 2ACC(R) 2AD2(N)
```

**Nicht vorhanden am Bike:** `2AD8` Power Range, CPS `0x1818`, CSC `0x1816`,
FitShow. CPS/CSC existieren nur auf der **Bridge** (für Apps).

### 4.3 Spec-Abweichungen

1. Control Point `2AD9`: **Notify**, nicht Indicate (CCCD `0x0001`)  
2. Kein Power Target / kein `2AD8`  
3. IBD `2AD2` enthält **kein** Resistance-Level — Stufe nicht rücklesbar  
4. Cross Trainer Data `2ACE` streamt redundant parallel

### 4.4 Feature & Bereich

- `2ACC` = `A646000004200000` → Resistance + Simulation; **kein** Power Target  
- `2AD6` = `0A00A0000A00` → Stufen **1,0 … 16,0** Schritt **1,0** (Zehntel: 10…160)

### 4.5 Indoor Bike Data (`2AD2`)

Flags auf diesem Bike **immer** `0x0B54`, Paket **19 Byte**, ~1 Hz beim Treten:

| Feld | Einheit | Roh |
|------|---------|-----|
| Speed | km/h | uint16 / 100 |
| Cadence | rpm | uint16 / 2 |
| Distance | m | uint24 |
| Power | W | sint16 |
| Energy total | kcal | uint16 |
| Heart Rate | bpm | uint8 (Bike-Empfänger / GymLink) |
| Elapsed | s | uint16 |

### 4.6 Control Point — was wirkt

| Bytes | Bedeutung | Quittung | Wirkung |
|-------|-----------|----------|---------|
| `00` | Request Control | Success | nötig vor Writes |
| `07` | Start/Resume | Success | Freigabe |
| `04 64 00` | Resistance sint16 Stufe 10,0 | Success | **WORKS** |
| `04 0A` / `04 64` | uint8 / 2-Byte-Form | Success | **keine Last** |
| `05 64 00` | Target Power 100 W | Success | **NO_EFFECT** (Draht 2026-09-13) |
| `08 01` | Stop | Success | Last weg |
| `11 …` Grade ~1 % | Simulation | Success | oft **NO_EFFECT** |
| `11 …` Grade ≥~3 % | Simulation | Success | **WORKS** |

**Leitregel:** Eine Erfolgsquittung beweist nichts. Wirkung nur über Watt/Kadenz
(Journal) belegen.

Encoding Stufe: `04 <lo> <hi>` sint16 LE in **Zehnteln**  
(Stufe 10,0 → `04 64 00`).

---

## 5. Was der Polar H9 sendet

| Feld | Wert |
|------|------|
| Scan-Name | z. B. `Polar H9 1DF6CA` |
| MAC (Labor) | `24:ac:ac:1d:f6:ca` |
| Service | `0x180D` Heart Rate |
| Characteristic | `0x2A37` Heart Rate Measurement (Notify) |
| Optional | Battery `0x180F` |

### Payload (typisch)

- Flags + bpm (8- oder 16-bit)  
- optional Energy Expended  
- optional **RR-Intervalle** (Flag `0x10`, Einheit 1/1024 s)  
- Sensor Contact beim H9 oft **nicht** gesetzt → UI `n/a`, nicht „kein Kontakt“

### Parallelkanäle

Der H9 kann BLE + ANT+ + **5-kHz GymLink** gleichzeitig. GymLink speist den
Bike-Empfänger → HR-Feld in `2AD2` **ohne** dass ergo den Gurt gekoppelt hat.

### Dual-Link (Nachtest 2026-09-13)

- Beide Links READY unter Last  
- `hrSource=strap` führend  
- Mittel **Bike-HR − Strap ≈ +25 bpm** (GymLink/Konsole träge/ungenau)  
- H9: **nur ein BLE-Central** → Relay-Architektur (`esp32.heartrate`)

---

## 6. Debugdaten, die Entscheidungen erzwungen haben

### 6.1 Caps-Bug (historisch, teuer)

Ableitung „Werte passen in uint8“ → Writes `04 xx` statt `04 xx xx`.  
Bike: Success, **keine Last**. Fix: Format aus Messung/Profil (`sint16`), nicht
aus Wertebereich. Siehe `debug/UPDATE_CAPS_FIX.md`.

### 6.2 Journal statt Bauchgefühl

`ControlJournal` bewertet **Watt pro Kadenz** vor/nach dem Write:

| Urteil | Bedeutung |
|--------|-----------|
| WORKS | Laständerung plausibel |
| NO_EFFECT | Success, aber W/rpm unverändert |
| Widerspruch | Success + NO_EFFECT-Flag / contradictory |

Erstlauf-Sonde: Wirkungsmessungen wertlos, weil Kadenz 0 bzw. weggelaufen —
deshalb Sweep mit festem Fenster und Verwerfung.

### 6.3 Architektur-Konsequenzen (Kanon)

1. **Jeder FTMS-Write nur durch den Limiter**  
2. **Kein produktives `0x05` ans Varon** — App-ERG → `PowerController` → Stufen  
3. Kennlinie = **Fläche Stufe × Kadenz** (`PowerMap`), keine reine Tabelle  
4. Simulation `0x11` optional ab ~3 % Grade (Bridge / Assist), nicht Ersatz für Stufen  
5. OTA / Neustart: bei Bike-Link zuerst Stop (`08 01`); Hub-Watchdog unter Last heikel bis Test 6  
6. Connect per gemerkter MAC; Advertising nicht vertrauen  

### 6.4 Hilfreiche Werkzeuge heute

| Werkzeug | Nutzen |
|----------|--------|
| Debug-Tab / Steuer-Journal | WORKS vs. Lüge live |
| `POST /api/probe/arm` | dichter Ring + leeres Journal |
| `POST /api/probe/mark` | Snapshot Watt/rpm/HR/Stufe |
| `tools/nachtest_watch.sh` | JSONL-Ticks für Offline-Auswertung |
| `GET /api/debug/export` | Ring als JSONL |
| Heatmap Kalibrierung | Sweep-Zellen (Rand) vs. passiv; Live-Cursor Stufe×Kadenz (0.3.23) |

---

## 7. Kalibrierdaten (aktueller Stand)

**Vollständige Heatmap-Tabelle (Abend 2026-09-13, inkl. passiv ~40 rpm):**
[`KALIBRIERUNG.md`](KALIBRIERUNG.md) · Roh-JSON
[`kalibrierung-map-20260913.json`](kalibrierung-map-20260913.json).

Persistente Map: NVS Slot 0 / MAC `c2:32:a5:1e:bf:b5`.  
Snapshot: **16/16 Stufen**, **7/8 Bänder** (110–120 leer), **ceilingW ≈ 224**,
~480 Stützstellen. Band 40–50 durch passives Lernen voll; 100–110 nur dünn.

### 7.1 Test 1 — 60 rpm (2026-09-11)

Quelle: [`debug/HW_TEST1_60RPM.md`](../../debug/HW_TEST1_60RPM.md)

| Stufe | Mittel-W | Mittel-rpm | gültig |
|------:|---------:|-----------:|:------:|
| 1,0 | 26,0 | 61,6 | nein |
| 2,0 | 30,7 | 61,9 | ja |
| 4,0 | 50,0 | 60,0 | ja |
| 6,0 | 69,3 | 59,6 | ja |
| 8,0 | 89,4 | 59,7 | ja |
| 10,0 | 109,7 | 59,8 | ja |
| 12,0 | 129,4 | 60,0 | ja |
| 14,0 | 150,1 | 60,0 | ja |
| **16,0** | **170,2** | **60,1** | ja |

≈ **+20 W** je zwei Stufen. Journal: **8× WORKS**, 0 Widersprüche.  
Band @ 60 rpm: **130–200 W** an der Decke — Spitzen brauchen höhere Kadenz und/oder `0x11`.

### 7.2 Test 2 leicht — 80 rpm Stufe 4+8 (2026-09-11)

Quelle: [`debug/HW_TEST2_LIGHT.md`](../../debug/HW_TEST2_LIGHT.md)

| Stufe | W | rpm | gültig |
|------:|--:|----:|:------:|
| 4,0 | 67,5 | 79,5 | nein (Kadenz) |
| 8,0 | **122,7** | 79,4 | ja |

Stufe 8 vs. 60 rpm: **+37 %** → Fläche, keine Tabelle.

### 7.3 Test 2 grob — 80 rpm 4/8/12/16 (2026-09-13)

Quelle: [`debug/HW_TEST2_80RPM.md`](../../debug/HW_TEST2_80RPM.md)

| Stufe | Mittel-W | Mittel-rpm | gültig |
|------:|---------:|-----------:|:------:|
| 4,0 | **72,7** | 84,7 | ja |
| 8,0 | **122,9** | 79,4 | ja |
| 12,0 | 180,8 | 80,6 | **nein** |
| **16,0** | **244,6** | **81,6** | ja |

Δ vs. 60 rpm: Stufe 4 **+46 %**, 8 **+38 %**, 16 **+44 %**.

### 7.4 Wie sich die Kennfläche weiter füllt

| Quelle | Regel |
|--------|--------|
| Sweep | 20 s Settle + 40 s Mittel; nur gültige Punkte; ersetzt Passives in der Zelle |
| Passiv | Stufe ≥20 s stabil, rpm ≥40, alle 5 s; Sweep-Zellen nur ~1/32 nachziehen |
| Raster | 16 Stufen × 8 Kadenzbänder (40–120 rpm) |

### 7.5 Weitere Hardware-Nachtests (Kurz)

Quellen: [`debug/HW_NACHTEST_20260913.md`](../../debug/HW_NACHTEST_20260913.md),
[`debug/UPDATE_NACHTEST_RESULTS.md`](../../debug/UPDATE_NACHTEST_RESULTS.md)

| Test | Ergebnis |
|------|----------|
| 3 · `0x05` Draht (`force=1`) | Success, **NO_EFFECT** (96→101 W @ ~81 rpm, Stufe 6) |
| 4 · `0x11` | 1 % NO_EFFECT; 3 %/6 % WORKS (~+25 % W/rpm) |
| 5 · Dual-Link | ok; Bike-HR ≈ Strap+25 |
| Reconnect | ok (kurzer WLAN-Blip) |
| 6 · Crash unter Last | **offen** |
| Bridge-Abnahme MyWhoosh | **offen** (Mechanik 0.3.14–17 da) |

---

## 8. Software-Architektur (Kurzkarte)

```
App
 ├─ BleCentral ── FtmsClient ── Limiter ──▶ Bike CP
 │              └─ HrClient  ──▶ Strap
 ├─ ControlMode + PowerController / Hr / Reha / Workout / BridgeAssist
 ├─ PowerMap ← SweepRunner + passives Lernen
 ├─ FtmsServer (Bridge) ← App-Watt → PowerController (nie 0x05 ans Bike)
 ├─ ConfigStore / DeviceStore / ProfileStore / Session*
 └─ Web: Status, Control, Calib, Probe, OTA
```

Geräteprofil Labor-Varon (Defaults ab 0.3.20 auch nach NVS-Load):

- `resistanceFormat = sint16`  
- `powerTrusted = 0`  
- `requestControlOnReconnect = true`

---

## 9. Harte Regeln (nicht verhandelbar)

1. `/ota-upload` und Hub-Shell bleiben in jeder Bin.  
2. Kein `[env]` in `platformio.ini` — `[common]` + native Positivliste.  
3. Hosttestbare Kerne Arduino-frei halten.  
4. FTMS-Writes nur durch den Limiter.  
5. Kein Steuerweg `0x05` ans Varon.  
6. Bridge-`0x05` von der App → PowerController → Stufen.  
7. Erfolgsquittung ≠ Wirkung.

---

## 10. Offen

1. Nachtest 6 — Verhalten bei Client-Abbruch unter hoher Stufe  
2. Formale Bridge-Abnahme (MyWhoosh ERG + Exclusive/Observer)  
3. Optional: Stufe 12 @ 80 rpm nachholen; `allowSimulation` aus, wenn unnötig  
4. Formale ERG/HR/Reha-Fahrer-Abnahme (Mechanik vorhanden)

---

## 11. Quellenindex

| Thema | Pfad |
|-------|------|
| Lebender Stand | [`STATE.md`](../../STATE.md) |
| Geräteprofil | [`GERAETEPROFIL.md`](GERAETEPROFIL.md) |
| Nachtest-Plan | [`NACHTESTS.md`](NACHTESTS.md) |
| Bridge | [`BRIDGE.md`](BRIDGE.md) |
| WebUI | [`WEBINTERFACE.md`](WEBINTERFACE.md) |
| Probe-Scan 2026-09-10 | `nodes/esp32.ftmsprobe/docs/ergometer/scan-20260910/` |
| Probe DATEN/IBD | `nodes/esp32.ftmsprobe/docs/ergometer/DATEN.md` |
| Kalibrierung 60/80 | `debug/HW_TEST1_60RPM.md`, `HW_TEST2_*.md` |
| Nachtest-Abend | `debug/UPDATE_NACHTEST_RESULTS.md` |
| Caps-Fix | `debug/UPDATE_CAPS_FIX.md` |
| Dev-UI / Ride-UI | `debug/UPDATE_DEV_UI.md`, `UPDATE_UI_RIDE.md` |
