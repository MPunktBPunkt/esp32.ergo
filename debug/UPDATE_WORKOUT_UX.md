# UPDATE — Workout-UX (v0.3.2-dev)

Stand: 2026-09-12.

## Befund zu den Ideen

| Idee | Status |
|------|--------|
| %FTP statt Fix-Watt | **schon da** (`ftp_pct`, Editor-Toggle „Ziel: % FTP“) |
| Machbarkeit als Kurve | **schon da** (`#woprev` Canvas + Validate) |
| Editor-Validierung | **schon da** (`/api/workout/validate` vs maxPower/maxHr/Map) |
| panic in Verlauf | **Bug**: Dock-STOP speicherte `endReason=panic` |
| Last-Run Vergleich | fehlte (nur Name, keine ID) |
| kJ/TSS in Vorschau | fehlte (workJ intern) |
| Schritt duplizieren | fehlte |
| Auto-Pause hält Schritt-Timer | fehlte |
| Tags/Favoriten, .zwo, RPE, Auto-Pause pro Workout | bewusst später |

## Neu in 0.3.2-dev

1. STOP → `endReason: "stop"` (UI: „Abbruch“; altes `panic` zeigt als „STOP“)
2. `workoutId` in SessionSummary / Persistenz / API
3. Vorschau: `workKj` + geschätztes `tss` + „Letzte Ausführung“
4. Editor: Schritt duplizieren (⧉)
5. Auto-Pause pausiert `WorkoutEngine` mit (Resume an derselben Stelle)

## Nicht umgesetzt (bewusst)

- Tags/Favoriten, .zwo-Import, RPE-Notiz
- Auto-Pause-Schwelle pro Workout (weiterhin global 10 s)
- Profil ohne FTP stärker kennzeichnen (Hinweis im Workouts-Text reicht vorerst)
