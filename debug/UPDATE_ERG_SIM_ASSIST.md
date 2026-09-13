# UPDATE — ERG Sim-Assist (0.3.13-dev)

Stand: **2026-09-13**. Nutzt Nachtest-4-Befund (`0x11` wirkt ab ~3 %).

## Was

Wenn **ERG / HR / REHA / WORKOUT** an der **Stufendecke** hängt und das
Wattziel nicht erreicht wird, legt der ESP optional eine Steigung über `0x11`
darauf — grob kalibriert (~8 W je 1 %, max 6 %, Mindest-Defizit 15 W,
wirksames Minimum 2 %).

### Config
- `allowSimulation` muss an sein
- `ergSimAssist` (Default **aus**) — Einstellungen-Checkbox

### Status
`erg.simAssist`, `erg.simAssistGradePct`

### Loop
Assist nur außerhalb von `SIM`-Modus; STOP/Moduswechsel setzt Grade 0.

## Tests
`pio test -e native -f test_simassist`
