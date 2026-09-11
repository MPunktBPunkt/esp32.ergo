# MANUAL_ERG / PowerController

Stand: **2026-09-11** Nacht. Nach Test 1 + Test 2 leicht.

## Was

- `src/control/PowerController.{h,cpp}` — Vorsteuerung `PowerMap::bestLevel` +
  langsamer I-Anteil auf dem Leistungsfehler (Pflichtenheft §6)
- `ControlMode::ManualErg` freigeschaltet
- App-Loop schreibt Stufen nur über Limiter/`FtmsClient`
- API: `POST /api/control/mode?mode=erg&watt=80`, `POST /api/control/power?watt=80`
  (`raw=1` = altes FTMS-`0x05` für Nachtest 3)
- Ride-UI: Button ERG + Zielwatt
- Hosttests: `test_powerctl` (5), `test_mode` erweitert

## Nutzung

1. Profil **standard**, Kennfläche aus Test 1/2 vorhanden
2. Ride → **ERG** oder Zielwatt setzen (z. B. 80 W)
3. Gleichmäßig treten — alle ~6 s ggf. Stufenwechsel
4. `ceiling` in Status/UI, wenn Ziel über der Map liegt

## Noch nicht

HR_HOLD, Reha-Programm, feine UI (Ist-vs-Ziel-Chart).
