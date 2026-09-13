# UPDATE — Ghost (Bestleistung)

Stand: **2026-09-13**, Firmware **0.3.8-dev**.

## Was

Minimaler Ghost-Vergleich gegen die beste Session im Archiv-Ring (max. 12):

- `SessionStore::bestFor(workoutId, profileId, mode, out)` — Score `workKj`
- Bei Session-Start: Cache; bei Ende: löschen
- Status/SSE: `ghost.{exists,workKj,avgPowerW,durationS,workoutName,…}`
- Ride-UI: Zeile `#rghost` — `Ghost: Ø X W · Y kJ · Z min`

## Grenzen

- Nur Ring (12 neueste), keine all-time FS-Historie
- Keine Chart-Geisterlinie, kein Live-Delta
- `endedUnix` weiter ungenutzt

## Tests
`pio test -e native -f test_session`
