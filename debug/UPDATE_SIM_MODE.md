# UPDATE — SIM-Steuermodus (0.3.12-dev)

Stand: **2026-09-13**. Nachtest 4 hat `0x11` belegt (wirkt ab ~3 %).

## Was

### ControlMode::Sim
- Freigeschaltet (war reserviert)
- Ziel: `gradeTargetHundredth` (0,01 %-Einheiten)
- Session aktiv; STOP / OFF setzt Grade 0

### API
- `POST /api/control/mode?mode=sim&grade=3`
- `POST /api/control/sim?grade=3` — setzt Grade, wechselt standardmäßig in SIM
  (`mode=0` = nur Write, für Nachtest-Probe)
- Status: `gradeTargetPct`, `gradeTargetHundredth`, `allowSimulation`

### Loop
- `loopSim`: schmutzig oder alle 8 s erneut `setSimulation` (Wind 0, Crr/Cw Default)

### UI
- Ride: Button **SIM**, Steigungsfeld, „Steigung setzen“
- Braucht `allowSimulation` (Einstellungen)

## Gate
Weiterhin Config `allowSimulation` + Limiter — kein Dauer-ON ohne Absicht.

## Tests
`pio test -e native -f test_mode`
