# MANUAL_ERG / PowerController

Stand: **2026-09-13** (Slew: [UPDATE_ERG_SLEW.md](UPDATE_ERG_SLEW.md)).

## Was

- `src/control/PowerController.{h,cpp}` — Vorsteuerung `PowerMap::bestLevel` +
  langsamer I-Anteil auf dem Leistungsfehler (Pflichtenheft §6)
- Ab 0.3.15: max. 1 Stufe/Zyklus + Retarget-Hysterese gegen Bridge-Spam
- `ControlMode::ManualErg` freigeschaltet
- App-Loop schreibt Stufen nur über Limiter/`FtmsClient`
- API: `POST /api/control/mode?mode=erg&watt=80`, `POST /api/control/power?watt=80`
  (`raw=1` = altes FTMS-`0x05` für Nachtest 3)
- Ride-UI: Button ERG + Zielwatt
- Hosttests: `test_powerctl`

## Nutzung

1. Profil **standard**, Kennfläche aus Test 1/2 vorhanden
2. Ride → **ERG** oder Zielwatt setzen (z. B. 80 W)
3. Gleichmäßig treten — alle ~6 s höchstens ±1 Stufe Richtung Map
4. `ceiling` in Status/UI, wenn Ziel über der Map liegt
