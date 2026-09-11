# HR_HOLD / HrController

Stand: **2026-09-11** Nacht. Nach MANUAL_ERG.

## Was

- `src/control/HrController.{h,cpp}` — Außenkreis Puls→Watt (Totband, I-Anteil,
  hard maxHr, Verlust-Timeout). Innenkreis bleibt `PowerController`.
- `ControlMode::HrHold` freigeschaltet (`allowsErg` + `allowsHrHold`)
- App: `loopHr` setzt Wattziel; `loopErg` schreibt Stufen auch im HR-Modus
- Pulsverlust laut Profil `onHrLoss`: Freeze / Reduce / Stop
- API: `POST /api/control/mode?mode=hr&hr=130`, `POST /api/control/hr?bpm=130`
- Status: `hrHold.{targetBpm,powerTargetW,lost,smoothedHr,onHrLoss}`
- Ride-UI: Button HR, Zielpuls, Hinweis bei Pulsverlust
- Hosttests: `test_hrctl` (5), `test_mode` erweitert

## Nutzung

1. Profil aktiv, Kennfläche vorhanden, Pulsquelle (Gurt/Bike) live
2. Ride → **HR** oder `mode=hr&hr=130`
3. Gleichmäßig treten — Wattziel wandert mit dem Pulsfehler
4. Bei Verlust: UI-Hinweis; Politik aus Profil

## Noch nicht

Reha-Programm (festes Watt + HR-Deckel), feine Tuning-Defaults am Fahrer.
