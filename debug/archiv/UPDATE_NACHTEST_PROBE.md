# UPDATE — Nachtest-Probe (0.3.9-dev)

Stand: **2026-09-13**.

## Was

### Simulation freigeben (Test 4)
- Config `allowSimulation` (NVS, Einstellungen-Checkbox)
- Limiter + Bridge übernehmen den Flag nach Speichern
- `POST /api/control/sim?grade=1` (Prozent; Draht = ×100)

### Probe-Daten
- Status-Feld `probe`: watt, rpm, hrBike, hrStrap, hrEff, level, marks, allowSimulation, ringOn
- `POST /api/probe/arm` — Ring an (ibdEvery=1), Journal leeren, Mark „arm“
- `POST /api/probe/mark?label=` — Snapshot
- `POST /api/probe/clear`
- raw `0x05` und sim antworten mit `probe` + `debug.journal`

### UI
Kalibrierung → Karte „Nachtests 3–6“: Probe schärfen, Mark, Test 3, Sim 1/3/6 %.

### Watch
```bash
bash tools/nachtest_watch.sh 192.168.178.88
```

## Ablauf
1. OTA 0.3.9-dev  
2. Profil standard, Bike verbinden  
3. Probe schärfen  
4. Watch starten  
5. Tests fahren
