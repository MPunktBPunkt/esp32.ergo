# UPDATE — Geführte Tests (UI)

Stand: 2026-09-12

## Was

Reiter **Tests** statt v0.2-Platzhalter (WEBINTERFACE §6, ohne TestRunner):

- Karten: Rampe, 20 Minuten, Recovery
- Bei Profilziel `reha` ausgeblendet
- Vorschau + Machbarkeit über `/api/workout/validate` (Start gesperrt bei Warnungen)
- Builtins `test_ramp` / `test_20min` / `test_recovery` (WorkoutEngine, max. 8 Schritte)
- Ergebnis: FTP-Vorschlag, Übernahme nur nach Bestätigung (`ftpOrigin: test`)
- Grobe Testhistorie aus Session-Archiv

## Grenzen

- Kein echtes 1-Min-MAP / kein Self-paced-20-Min / keine HR-Serie für Recovery-Note
- Rampe endet nach 8 Stufen (200 W) oder per Stop
- Volle `/api/test/*` + TestRunner später

## Nutzung

1. Profil ohne Reha-Ziel, Kennfläche geladen
2. Tests → Prüfen → ggf. Zeitfaktor → Start
3. Stop / Ende → Ergebnis → optional FTP übernehmen
