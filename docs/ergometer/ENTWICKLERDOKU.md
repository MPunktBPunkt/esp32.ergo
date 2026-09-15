# Entwicklerdokumentation — esp32.ergo

**Stand:** 2026-09-15 · gültig für Firmware **0.3.2x**  
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
| Rohmessungen der Sonde | Sibling-Clone `../nodes/esp32.ftmsprobe/docs/ergometer/` (siehe §16) |

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
| Runner / Bericht | `../nodes/esp32.ftmsprobe/docs/ergometer/scan-20260910/` |
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

Kurz: Generic Access / Attribute / Device Info, Vendor `1850`, FTMS `1826` mit
`2ACE`, `2AD9`, `2AD6`, `2ACC`, `2AD2`. **Vollständige Charakteristikliste:**
[`GERAETEPROFIL.md` §2](GERAETEPROFIL.md).

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

**Befund für Reha / Soft-Ceiling — harte Regel ab 0.3.24.** Ein systematischer
Offset von etwa 25 Schlägen ist für einen Pulsdeckel **kein Vorsichtshinweis,
sondern Ausschlussgrund**. `hrUsableForControl` lässt nur `Strap` und `Relay`
zu; `Machine` und `None` speisen `HR_HOLD` und Reha nicht. Bike-HR bleibt für
Anzeige, Aufzeichnung und Zonen erlaubt.

**Firmware-Verhalten.** Eintritt in `HR_HOLD` / Reha ohne vertrauenswürdige Quelle
→ HTTP 409 (`hr_source_not_trusted`). Läuft die Quelle währenddessen auf
`Machine` zurück, gilt das als **Pulsverlust** (`hrFresh=false` an die Regler),
nicht als stiller Ersatz. Workout-Schritte mit `hrSoft`/`hrMax` laufen auf Watt
weiter; der Deckel bleibt unbewaffnet (`workout.hrCapArmed=false`).

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
| `tools/nachtest_watch.sh` | paralleler JSONL-Tick (`/api/status` inkl. Probe/Journal/Bridge) für Offline-Auswertung |
| `tools/hand-proof.sh` | Handkurbel-Beweis Stufe 1 vs. 16 ohne Fahrer (Ring an, Profil, Request/Start) |
| `tools/hw_smoke.sh` | HTTP-Smoke nach OTA: Status, Builtins, Validate — **ohne** Bike-Link |
| `GET /api/debug/export` | Ring als JSONL |
| Heatmap Kalibrierung | Sweep-Zellen (Rand) vs. passiv; Live-Cursor Stufe×Kadenz (0.3.23) |

**Partitionierung.** `board_build.partitions = min_spiffs.csv` wählt die
Arduino-ESP32-Tabelle mit großer App-OTA und kleinem SPIFFS: App-Partition
**1 966 080 Byte** (`0x1E0000`, `app0`/`app1`) — genau die Zahl, die in
Build-Protokollen als Nenner der Flash-Auslastung steht. SPIFFS bleibt
128 KiB (`0x20000`) für Workouts/Sessions; die UI liegt gzip in PROGMEM.

---

## 7. Kalibrierdaten (aktueller Stand)

**Vollständige Heatmap-Tabelle (2026-09-15, passiv + Sweep):**
[`KALIBRIERUNG.md`](KALIBRIERUNG.md) · Watt-Raster
[`kalibrierung-map-20260915.json`](kalibrierung-map-20260915.json) · letzter
API-Dump mit Sample-Zählern
[`kalibrierung-map-20260913.json`](kalibrierung-map-20260913.json) · Screenshot
[`screenshots/kennflaeche.png`](screenshots/kennflaeche.png).

Persistente Map: NVS Slot 0 / MAC `c2:32:a5:1e:bf:b5`.  
Snapshot laut [`KALIBRIERUNG.md`](KALIBRIERUNG.md): **16/16 Stufen**,
**8/8 Bänder**, **99/128 Rasterzellen** belegt, **753 Stützstellen**
(11 Sweep). Die Stützstellen sind Einzelmessungen (Sweep-Fenster und passive
Samples), die in die Rasterzellen einfließen — keine zweite Zählung derselben
Größe. Stufe 4–11 ist über alle Bänder voll; Stufe 12–16 bleibt in den hohen
Kadenzbändern dünn. DeviceStore-`ceilingW` in der UI weiter **224**;
heißeste Zelle **339 W** @ Stufe 16 / 100–110 rpm.

`ceilingW ≈ 226` in [`debug/HW_TEST2_80RPM.md`](../../debug/HW_TEST2_80RPM.md)
ist eine **Momentaufnahme** direkt nach dem 80-rpm-Lauf; die gespeicherte
Karten-Decke bleibt **224** aus dem Geräteprofil, unabhängig von der
339-W-Zelle in der Heatmap.

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

## 9. Persistenz

Fünf NVS-Namespaces und LittleFS tragen den Zustand. Ein Formatwechsel ohne
Migration macht Profile oder die Kennfläche lautlos unbrauchbar — die Fläche
ist ein Messergebnis, das Stunden auf dem Rad gekostet hat.

### NVS

| Namespace | Inhalt | Hinweise |
|-----------|--------|----------|
| `esphub` | Gerätename, `hub_host`, `hub_port` | **Familienweit** geteilt mit Sonde/heartrate; Firmwarewechsel auf demselben Chip behält die Felder (`ConfigStore`) |
| `ergo` | Knotenspezifische Config, versioniert über `cfg_ver` | Bike-/HR-MAC, Bridge, Simulation, Dev-UI, Watchdog, NTP, … Fehlende Schlüssel behalten Defaults |
| `ergodev` | `DeviceStore`: Profile je MAC (Format, `powerTrusted`, Ceiling, Slot) | Serialisierung mit Magic `'ERDV'` (`0x45524456`), Version **1** |
| `ergomap` | `PowerMap`-Bytes je Slot (`m0`…`m3`, Legacy-Key `pmap`) | siehe Magic/Version unten |
| `ergoprofs` | Nutzerprofile (`ProfileStore`) | Magic `'ERGP'` (`0x45524750`), Version **2** |

**PowerMap-Blob.** Header 8 Byte, Zellen je 9 Byte, little-endian byteweise
(kein `memcpy` der Struktur). Magic-Bytes `'E''M'` (`0x45 0x4D`), Version
**1** (`PowerMap.cpp`). Falsches Magic, falsche Version oder unpassende Länge
→ `load()` lässt die Fläche **unverändert leer** — Upgrade-Bruch ohne sichtbaren
Fehler, außer Coverage 0. Nach dem Laden prüft die UI auf `mapReady` /
Zellenzahl.

### LittleFS

Beim Start: `/workouts`, `/sessions`, `/progression` (mkdir falls nötig).

| Pfad | Zweck |
|------|--------|
| `/workouts/<id>.json` | hochgeladene / editierte Workouts |
| `/workouts/meta.json` | Tags und Metadaten |
| `/sessions/log.jsonl` | Session-Archiv (Anhängen) |
| `/sessions/last.json` | letzte Zusammenfassung |
| `/progression/*`, `/progression/ftp_career.json` | Physio-Steigerung, FTP-Verlauf |

Volllaufen: Writes scheitern mit HTTP 500/503; Builtins bleiben im Flash. Die
App-Partition ist groß (`min_spiffs`, siehe §6.4), SPIFFS bewusst klein —
Workouts und Sessions müssen schlank bleiben.

---

## 10. Statusobjekt (`/api/status`)

`buildStatusJson` in `App.cpp` ist die Schnittstelle für UI, Hub-Debug und
`nachtest_watch.sh`. Hier nur Felder, die man ohne Quellcode nicht errät
(kommentierte Teilmenge, keine vollständige Schema-Liste):

```jsonc
{
  "mode": "MANUAL_ERG",          // ControlMode-Name
  "hrSource": "strap",           // strap | relay | machine | none
  "hrUsable": true,              // Strap/Relay → HR_HOLD/Reha erlaubt
  "heartRate": 132,              // effectiveHr() zur aktiven Quelle
  "bikeLink": true,              // OTA/deploy verweigert, solange true
  "erg": {
    "targetW": 120,
    "smoothedW": 118.4,
    "ceiling": false,            // true = Ziel oberhalb Kennfläche (UI: unerreichbar)
    "mapReady": true
  },
  "reha": {
    "capArmed": true             // false = Deckel unbewaffnet (keine Gurtquelle)
  },
  "workout": {
    "hrCapArmed": false          // Soft/Hard-HR nur bei hrUsable
  },
  "bridge": {
    "clientRole": "observer",    // none | connected | observer | controller
    "resistIgnored": false,      // Resistance kurz nach Power verworfen (ERG-Spam-Schutz)
    "levelWantTenths": -1,       // gewünschte App-Stufe, solange Bridge LEVEL fährt
    "exclusive": false,          // App ist Controller → Coach-Last gesperrt
    "driving": false
  },
  "calib": {
    "passive": {
      "accept": true,            // true = nächster passiver Sample würde lernen
      "reason": "learning"       // sweep | no_live | rpm_low | settle | …
    }
  },
  "debug": {
    "journal": {
      "head": {
        "contradictory": false   // Success-Quittung + NO_EFFECT-Messung
      }
    }
  }
}
```

**Hinweise.** Das Hub-IO `target_reachable` aus dem Pflichtenheft ist im
Status-JSON als Negation von `erg.ceiling` zu lesen (Decke = Ziel nicht
erreichbar). `clientRole` kommt aus `FtmsServer::clientRole()`.
`passive.accept` ist ein Live-Hinweis in der Heatmap, kein Persistenz-Flag.
`contradictory` sitzt am Journal-Eintrag (`ControlJournal::contradictory()`).
`hrUsable` spiegelt `hrUsableForControl(resolveHrSource())`. `reha.capArmed` /
`workout.hrCapArmed` sagen der UI, ob der Pulsdeckel bewaffnet ist — ohne
Eigenlogik.

---

## 11. Sicherheit

Was schiefgehen kann, und was dann passiert — zusammenhängend, weil Widerstand
unter dem Fuß steht.

**Not-Stop.** `POST /api/control/stop` und der fixe UI-Button senden `08 01`
durch den Limiter ohne Whitelist-Prüfung. Auch bei Bridge-Lock bleibt STOP
erlaubt; Coach-Last ist gesperrt, Not-Stop nicht. OTA und Hub-Shell lehnen ab,
solange `bikeLink` oder eine Session aktiv ist — ein Flash unter Last wäre ein
Reset ohne `08 01`.

**Pulsverlust.** Je Profil `freeze` / `reduce` / `stop` (`HrLossPolicy`).
`HrController` und `RehaController` werten dasselbe Politikfeld aus: Stop →
`ftms.stop` + Sessionende; Freeze → keine neuen Stufenwrites; Reduce → Wattziel
absenken. Timeout-Schwelle typisch 8 s ohne frischen Puls.

**Vertrauenswürdige Pulsquellen.** Für `HR_HOLD` und Reha gelten nur `Strap`
und `Relay` (`hrUsableForControl` in `BleTypes.h`). `Machine` (Bike-`2AD2` /
5-kHz-GymLink) und `None` sind ausgeschlossen — Dual-Link: Bike ≈ Strap+25 bpm.
Fällt die Quelle während der Fahrt auf `Machine` zurück, ist das **kein** stiller
Ersatz, sondern Pulsverlust: dieselbe `HrLossPolicy` wie beim Gurtabriss, inkl.
UI-Warnung und Session-Eintrag. Eintritt ohne Gurt/Relay → HTTP 409
`hr_source_not_trusted`. Workout-Wattziele laufen weiter; ein Schritt-Pulsdeckel
bleibt unbewaffnet.

**Bike-Verlust unter Last.** Verbindungsabbruch pausiert die Session-Rechnung;
die Stufe am Bike bleibt, was sie war — das Gerät meldet sie nicht zurück. Nach
Reconnect: Stufe neu setzen, Schattenwert nicht blind übernehmen. Ob das Bike
bei Client-Absturz die Last hält, entscheidet **Nachtest 6** (noch offen).

**Hub-Watchdog.** `watchdogS` in der Config startet den Knoten neu, wenn der
Hub schweigt. Unter Bike-Link ist das gefährlich, solange Test 6 nicht zeigt,
dass ein Abbruch lastfrei endet. Bis dahin: Watchdog unter Last nicht als
Sicherheitsnetz behandeln; gewollte Neustarts senden vorher `08 01`.

**Bewusste Bypässe.** `force=1` an der Probe-/Raw-Route setzt kurz
`allowUntrustedPower` und lässt ein `0x05` Wattziel durch den Limiter (Nachtest
3). Danach muss der Flag zurück. Produktiver Pfad bleibt: kein `0x05` ans Varon.

**Rampen.** Aufwärts max. eine Gerätestufe pro `rampMs` (Default 2 s). Lastabbau
sofort. Bridge-LEVEL (manuelle App-Gänge) setzt `setRampOverrideMs(500)` —
noch Rampe, kein Sprung, aber schneller als Coach-ERG. Nach Erreichen der
Wunschstufe fällt der Override wieder weg.

---

## 12. Regelung

Vier Arduino-freie Regler; Zeit und Messwerte kommen als Parameter. Hosttests
decken die Kerne ab (`pio test -e native`).

### PowerController

**Eingänge:** Zielwatt, aktuelle Kadenz und Leistung aus `2AD2` (`fresh`),
`PowerMap`. **Ausgänge:** gewünschte Stufe in Zehnteln, Flags `ceiling` /
`mapReady`, geglättete Leistung. **Zustand:** Integral auf dem Wattfehler,
geglättetes Ist, letzte Stufe, Retarget-Schwelle. **Grenzen:** Periodik
(~6 s), Deadband, I-Limit, Slew max. eine Stufe pro Zyklus; kleine Ziel-Updates
(&lt; ~20 W, Bridge-Spam) lösen keinen Sofort-Sprung aus. **Alternative, die
verworfen wurde:** direkt Puls→Stufe oder reines I-ohne Map — beides ignoriert
die Kadenzachse der Fläche; die Bauform ist Vorsteuerung aus der Map plus
langsamer Rückführung.

### HrController

**Eingänge:** Zielpuls, frischer HR (`hrFresh` — App setzt `false`, wenn die
Quelle nicht `hrUsableForControl` ist), optionale harte Profil-HRmax,
`HrLossPolicy`. **Ausgänge:** Wattziel für den inneren `PowerController`,
`lost`, geglätteter Puls. **Zustand:** Integral BPM→Watt, Soft-Armzeit,
Verlust-Timer. **Grenzen:** Deadband ±3 bpm, min/max Watt, `lostAfterMs`,
stärkerer Gain über Cap. Unzulässige Quelle = Verlustpfad, kein zweiter Semantik.
**Alternative:** direkter Puls→Stufe — scheitert an Kadenzwechseln bei fester
Stufe; die Kaskade hält den inneren Kreis auf einer stabilen Größe.

### WorkoutEngine

**Eingänge:** Schrittliste (Builtin oder geladen), FTP für `ftp_pct`, Zeit.
**Ausgänge:** `desiredW`, optionale `hrSoft`/`hrMax` je Schritt, Restzeiten,
State Idle/Running/Paused/Done. **Zustand:** Schrittindex, Pause-Akkumulator,
Session-Arm. **Grenzen:** max. 16 Schritte; `selfPaced` ohne Wattziel.
**Alternative:** alles in der UI steuern — abgelehnt, weil der Ablauf
hosttestbar und ohne JSON im Kern bleiben soll; Parse liegt in `WorkoutJson`.

### RehaController

**Eingänge:** festes Sollwatt, Soft-/Hard-Pulsgrenzen, Dauer, frischer HR
(gleiche `hrFresh`-/Quellen-Gate wie `HrController`), Verlustpolitik.
**Ausgänge:** `effectiveW` (abgesenkt unter Deckel), `capActive`,
Interventionszähler, `finished`/`lost`. **Zustand:** effektive Leistung, Timer,
Cap-Flag. **Grenzen:** Soft-Cut- und Hard-Cut-Gain, langsame Rückkehr
(`restorePerS`), min. Watt. Quelle unzulässig → `lost`, `capActive` fällt; kein
Eingriffszähler (Verlust, nicht Intervention). **Alternative:** denselben Pfad
wie `HR_HOLD` (Zielpuls) — bewusst getrennt: Reha hält die Leistung und schneidet
nur von oben; `HR_HOLD` regelt beidseitig auf einen Zielpuls.

---

## 13. Glossar

| Begriff | Bedeutung hier |
|---------|----------------|
| **W/rpm** | Watt pro Kadenz — normierte Größe im Steuer-Journal (Wirkung der Stufe, nicht der Trittfrequenz) |
| **Kennfläche** | Raster Stufe × Kadenzband → geschätzte Watt (`PowerMap`) |
| **Schattenwert** | vom ESP gesetzte Stufe; das Bike meldet keine Resistance in `2AD2` |
| **Stützstelle** | einzelne Messung (Sweep-Fenster oder passives Sample), die eine Zelle füllt |
| **Kadenzband** | 10-rpm-Spalte der Fläche (40–50 … 110–120) |
| **Ceiling** | Zielwatt oberhalb der höchsten bekannten Stufe / gemessenen Decke |
| **Slew** | Begrenzung der Stufenänderung pro Regelzyklus (ERG) bzw. Ramp-Zeit (Limiter) |
| **Takeover** | Bridge: App übernimmt Laststeuerung (Controller); Coach-Last gesperrt |
| **Observer / Controller** | Bridge-Rollen: nur lesen bzw. Lastkommandos gesendet |
| **unjudged** | Journal-Urteil verweigert (Kadenz zu niedrig oder weggelaufen) |
| **contradictory** | Success-Quittung und NO_EFFECT-Messung zugleich |

---

## 14. Harte Regeln (nicht verhandelbar)

1. `/ota-upload` und Hub-Shell bleiben in jeder Bin.  
2. Kein `[env]` in `platformio.ini` — `[common]` + native Positivliste.  
3. Hosttestbare Kerne Arduino-frei halten.  
4. FTMS-Writes nur durch den Limiter.  
5. Kein Steuerweg `0x05` ans Varon.  
6. Bridge-`0x05` von der App → PowerController → Stufen.  
7. Erfolgsquittung ≠ Wirkung.

---

## 15. Offen

1. Nachtest 6 — Verhalten bei Client-Abbruch unter hoher Stufe  
2. Formale Bridge-Abnahme (MyWhoosh ERG + Exclusive/Observer)  
3. Optional: Stufe 12 @ 80 rpm nachholen; `allowSimulation` aus, wenn unnötig  
4. Formale ERG/HR/Reha-Fahrer-Abnahme (Mechanik vorhanden)

---

## 16. Quellenindex

Pfade zu Scan-Daten der Sonde sind relativ zum **Sibling-Clone** im esphub-
Monorepo: von `esp32.ergo/` aus `../nodes/esp32.ftmsprobe/...`. Ein Checkout
nur des Ergo-Repos hat diese Pfade nicht; dann gilt das GitHub-Repo der Sonde.

| Thema | Pfad |
|-------|------|
| Lebender Stand | [`STATE.md`](../../STATE.md) |
| Geräteprofil | [`GERAETEPROFIL.md`](GERAETEPROFIL.md) |
| Nachtest-Plan / Ergebnisse | [`NACHTESTS.md`](NACHTESTS.md) |
| Bridge | [`BRIDGE.md`](BRIDGE.md) |
| WebUI | [`WEBINTERFACE.md`](WEBINTERFACE.md) |
| Probe-Scan 2026-09-10 | `../nodes/esp32.ftmsprobe/docs/ergometer/scan-20260910/` |
| Probe DATEN/IBD | `../nodes/esp32.ftmsprobe/docs/ergometer/DATEN.md` |
| Kalibrierung 60/80 | `debug/HW_TEST1_60RPM.md`, `HW_TEST2_*.md` |
| Nachtest-Abend | `debug/UPDATE_NACHTEST_RESULTS.md` |
| Caps-Fix | `debug/UPDATE_CAPS_FIX.md` |
| Dev-UI / Ride-UI | `CHANGELOG.md` (0.3.18–0.3.21) |
