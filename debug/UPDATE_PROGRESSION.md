# UPDATE — Physio-Progression

Stand: 2026-09-12

## Was

- Builtin `physio` trägt `progression` (Hauptteil +60 s, max 1800 s)
- Persistenz LittleFS `/progression/<id>.json`
- Nach Session-Ende: Angebot nur bei **sauber** (done, kein Deckel, Ist≈Soll)
- API: `/api/progression/get|accept|decline`
- Verlauf-Reiter: Kurve, Historie, +1 min / Ablehnen

## Nutzung

1. Physio fahren bis `done`
2. Verlauf → bei sauber „Hauptteil +1 min“
3. Nächster Start nutzt die neue Dauer

## Nicht drin

Automatische Übernahme, wochenweise Diagramme, Progression für beliebige JSON-Felder außer duration.
