# UPDATE — Workout-Bibliothek mit Vorschau

Stand: 2026-09-12

## Was

Workouts-Reiter als Bibliothek + Zeitachse (WEBINTERFACE §5, ohne vollen Editor):

- **Karten** für Builtins und LittleFS-Dateien (Vorschau / Start)
- **Canvas-Vorschau** der Leistung über die Zeit
- Schritt-Tabelle, Dauer, Spitze, Ø Watt
- **Machbarkeit** gegen aktives Profil (max Watt, FTP für `ftp_pct`, Pulsdeckel)
  und Kennfläche (~80 rpm)
- JSON: Drag&Drop, Live-Prüfen beim Tippen, Speichern
- `/api/workout/validate` liefert `timeline`, `peakW`, `warnings`, `feasible`

## Nutzung

1. Profil wählen (z. B. Martin)
2. Reiter Workouts → Karte antippen → Vorschau
3. Warnungen lesen, ggf. FTP/max Watt anpassen
4. Start mit optionalem Zeitfaktor

## Nicht drin

Schritt-Editor (Zeitachse bauen), Intervalblöcke, Progression-UI (v0.2).
