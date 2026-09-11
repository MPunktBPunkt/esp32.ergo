# WorkoutEngine — Physio 3 Schritte

Stand: **2026-09-12**. Nach Ride-UI-Politur.

## Was

- `src/control/WorkoutEngine.{h,cpp}` — steady-Schritte, Pause/Skip, FTP-%
- Eingebaut: **Physio Grundlage** (40 W / 60 W / 35 W, Pulsdeckel 120/115)
- `ControlMode::Workout`; Deckel weiter über `RehaController`
- API: `POST /api/workout/start|pause|resume|skip|stop` (`scale=` kürzt Zeiten)
- Ride-UI: Button **PHYSIO**, Pause / Weiter / Skip
- Hosttests: `test_workout` (6)

## Nutzung

1. Profil + Kennfläche, Bike verbunden
2. Ride → **PHYSIO** (volle Dauer ~14 min) oder `?scale=0.1` zum Testen
3. Pulsdeckel greift je Schritt; Fortschritt unter den Messwerten

## Noch nicht

JSON/LittleFS-Bibliothek, Editor, Progression, Interval/Ramp.
