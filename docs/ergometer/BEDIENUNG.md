# Bedienung — esp32.ergo

Kurz für jemanden, der das Gerät **nutzt**, nicht baut. Details: Web-UI und
[WEBINTERFACE.md](WEBINTERFACE.md); Stand: [STATE.md](../../STATE.md).

## Fünf Handgriffe

1. **Profil wählen** (Reiter Profile) — ohne Profil keine Last/Session.
2. **Bike verbinden** (Geräte → Suchen → FTMS-Zeile). Optional Gurt/HR-Relay.
3. **Fahren** (Reiter Ride): Stufe, ERG, HR oder Workout starten.
4. **Workout** aus der Bibliothek starten oder Editor nutzen.
5. **Session** danach unter Verlauf ansehen / annotieren.

## Vier Dinge, die überraschen

1. **Konsolendisplay bleibt aus**, solange der ESP als Central am Bike hängt —
   die Web-UI ist die Anzeige.
2. **Nur ein Central** am Bike — MyWhoosh darf nicht parallel am Varon hängen;
   die Bridge am ESP ist der Weg für die App.
3. **Stufe ist ein Schattenwert** — das Bike meldet die gestellte Stufe nicht
   zurück (`~` in der UI).
4. **Pulsregelung braucht den Gurt** — `HR_HOLD` und Reha starten nur mit Gurt
   oder HR-Relay; Bike-HR reicht nicht (≈ +25 bpm Versatz).

## Sicherheit (alltagsnah)

- **STOP** ist immer erreichbar (auch wenn die Bridge steuert).
- Reha / Pulsdeckel: **Gurt oder HR-Relay** Pflicht, nicht nur Bike-HR.
- OTA/Neustart nicht unter Last ohne Stop — siehe harte Regeln in `STATE.md`.
