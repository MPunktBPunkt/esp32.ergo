# Profilpflicht, Ride-Steuerung, Profil-Editor

Stand: **2026-09-11** (Weiterarbeit ohne Fahrer).

## Was

1. **Profilpflicht** vor Last: `/api/control/mode` (außer OFF), `/level`, `/power`
   antworten mit 409 ohne aktives Profil (Abnahmekriterium 18).
2. **Ride-UI**: Profil + Modus, OFF/LEVEL/STOP, Start/Reset, Stufe ± nur mit Profil.
3. **Profile-UI**: Anlegen / Bearbeiten / Löschen (nicht nur API).
4. **`profileInfo` in `/api/status`**: Name, Grenzen, `leadingZone` — Puls wird Hero
   wenn führende Zone HR (Reha).
5. **`hand-proof.sh`**: wählt vorher `standard`.

ERG/HR/Workout bleiben 501. Hand-Beweis §3 unverändert nötig.
