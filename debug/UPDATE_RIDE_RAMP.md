# UPDATE — Ride-Kurve + Rampen-Editor

Stand: 2026-09-12

## Ride UX

- Workout-/Test-Start → automatisch Reiter **Ride**
- Während `WORKOUT`: Programm-Kurve mit Playhead (aktueller Schritt)
- Plan kommt von Download+Validate der Workout-ID

## Rampen-Autorenschaft

- JSON: `type:"ramp"` mit `duration_s` und
  `target.ftp_pct_from`/`ftp_pct_to` oder `power_from`/`power_to`
- Expandiert zu 2–10 Steady-Scheiben (~30 s)
- Editor: **+ Rampe** (Von/Bis, Dauer)

## Grenzen

- Keine echte kontinuierliche Watt-Interpolation im Engine-Tick
- Ride-Kurve braucht bekannte Workout-ID nach Start
