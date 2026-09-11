# REHA / Physio — festes Watt + Pulsdeckel

Stand: **2026-09-12**. Nach HR_HOLD.

## Was

- `src/control/RehaController.{h,cpp}` — Außenkreis: gewünschte Watt bleiben,
  Soft-/Hard-Pulsdeckel senkt nur ab (kein Zielpuls wie HR_HOLD)
- `ControlMode::Reha` (`reha` / `physio`)
- App: `loopReha` → effektive Watt → `PowerController` → Limiter
- Dauer optional (Default 600 s); danach OFF + FTMS stop
- Pulsverlust wie Profil `onHrLoss` (Reha-Vorlage: Stop)
- API: `POST /api/control/mode?mode=reha&watt=60&hrMax=120&hrSoft=115&durationS=600`
  und `POST /api/control/reha?...`
- Status: `reha.{desiredW,effectiveW,hrMax,hrSoft,capActive,interventions,…}`
- Ride-UI: Button REHA, Watt / Deckel / Dauer, Hinweis „Deckel greift“
- Hosttests: `test_reha` (7), `test_mode` erweitert

## Nutzung

1. Profil (z. B. **reha** oder **standard**), Kennfläche da, Pulsquelle live
2. Ride → **REHA** (Defaults 60 W / 120 bpm / 10 min)
3. Soft-Band greift vor dem Hard-Deckel; Eingriffe werden gezählt

## Noch nicht

Mehrschritt-Workout-JSON, Progression, volle Reha-Ride-Gestaltung.
