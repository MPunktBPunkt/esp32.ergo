# Changelog

Absteigend nach Version. Kurze Einträge aus den ehemaligen `debug/UPDATE_*.md`-
Notizen; Belege und Messwerte bleiben in den Primärprotokollen unter `debug/`.

## 0.3.23-dev — 2026-09-13

Kennfläche: Live-Cursor Stufe×Kadenz-Zelle. Status: `calib.passive.bandIdx`.
Bin: `dist/ergo.0.3.23-dev.esp32s3.bin`. Flash-Bin 1 472 992 B / App-Partition
1 966 080 B ≈ **74,9 %**.

## 0.3.22-dev — 2026-09-13

Calib-UI: Kadenzband-Spalten mit Bereichen; Live-Hinweis `calib.passive`.

## 0.3.21-dev — 2026-09-13

Ride: Alltags-Labels für Modi; Start/Freigabe nach STOP; Nachtest-Hierarchie
im Calib-Tab.

## 0.3.19-dev / 0.3.18-dev — 2026-09-13

Betrieb vs. Entwickler-UI (`showDevUi` NVS). Calib/Debug/Quirks hinter Flag.
0.3.19: Zone-Render darf `devui`-Klasse nicht löschen.

## 0.3.17-dev — 2026-09-13

Bridge-LEVEL: Ramp-Override **500 ms**/Stufe; Coach/ERG bleiben bei **2000 ms**.
Siehe `docs/ergometer/BRIDGE.md` §4.

## 0.3.16-dev — 2026-09-13

Resistance &lt; **3 s** nach SetPower = ERG-Spam (`resistIgnored`); danach
Takeover → `MANUAL_LEVEL`. Details: `BRIDGE.md` §4.

## 0.3.15-dev — 2026-09-13

ERG-Slew: `maxStepTenths=10` (±1 Stufe/Zyklus), `retargetW=20 W` (kein I-Reset
bei kleinen MyWhoosh-Retargets), `periodMs≈6000`.

## 0.3.14-dev — 2026-09-13

Observer vs. Controller; Exclusive-Lock (Coach 409 bei App-Last); STOP gibt frei.

## 0.3.13-dev — 2026-09-13

Optional `0x11`-Sim-Assist am Stufendach (`ergSimAssist`, default aus).

## 0.3.12-dev — 2026-09-13

`ControlMode::Sim`, gated durch `allowSimulation`.

## 0.3.11-dev — 2026-09-13

DeviceStore: bis 4 Geräte, NVS `ergodev`, Map-Slots; `/api/devices`, `/api/device`.

## 0.3.8-dev — 2026-09-12

Ghost vs. beste der letzten 12 Sessions (`workKj`).

## 0.3.7-dev — 2026-09-12

Workout-Tags (max 4×16); per-Workout `autoPauseS`.

## 0.3.6-dev — 2026-09-12

FTP-Karriere (8 Stufen); Profile 4→6; Hub-OTA deferred bei Bike/Session.

## 0.3.5-dev — 2026-09-12

Sechs %FTP-Builtins (SS/TH/FTP/O-U/VO₂).

## 0.3.4-dev — 2026-09-12

`.zwo`-Import → Workout-JSON (`/api/workout/import`).

## 0.3.3-dev — 2026-09-12

Favorites, Goal, RPE/Note-Annotate.

## 0.3.2-dev — 2026-09-12

STOP → `endReason:stop`; Preview kJ/TSS; Auto-Pause pausiert Engine.

## 0.3.1-dev — 2026-09-12

Bridge-MVP-Erweiterung: Difficulty 50–150, HR soft/max auf Bridge-ERG;
CPS/CSC (`CyclingCodec`); `0x11` dekodiert, NotSupported bis `allowSimulation`.

## 0.3.0-dev — 2026-09-12

FTMS-Peripheral-Bridge (Wattziel für Apps → Stufen am Varon).

## 0.1.2-dev — 2026-09-12

Gzip-UI (`web/index.html` → `UiPagesGz.h`); TestRunner MAP/20 min/Recovery.
Flash-Budget über Kompression statt LittleFS-Auslagerung gelöst.

## 0.1.0 — 2026-09-12

Coach-Release: LEVEL belegt (0 Widersprüche im Journal). Profile, Workouts,
Reha, Progression, Session-Archiv, Limiter, Kennfläche. Messprotokoll:
`debug/RELEASE_v0.1.0.md`. Flash damals ~75 %; LittleFS-Empfehlung später durch
gzip ersetzt.

### Coach-Vorläufer (2026-09-11/12, vor Bridge)

- ProfileStore + Persistenz NVS `ergoprofs`; Profile-UI; Caps-Fix
- `HrController` / `RehaController`; SessionTracker + SessionStore
- WorkoutEngine, Editor, Zones, Tablet-Ride, Interval/Ramp
- Control-UI: Profilpflicht vor Last (409)
