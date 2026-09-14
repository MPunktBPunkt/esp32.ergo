# Notiz für die Entwurfs-Instanz — Doku-Aufräumen 2026-09-14

Umgesetzt nach `debug/TODO_DOKUMENTATION.md` (Definition of Done).

## Was geändert wurde

- **CHANGELOG.md** neu (0.1.0 … 0.3.23-dev); 39 `UPDATE_*`/`RELEASE_v0.3.2x` gelöscht
- **debug/archiv/** für erledigte Aufträge; captures/calib READMEs; powermap-latest → Symlink
- **STATE.md** gestrafft (Kopf, Offen, Flash 74,9 %, 24 Kriterienzeilen, keine UPDATE-Linkliste)
- **Teil A** in Kanondocs: Fixture-Zahlen 274/383/5, Bridge v0.3, Modulnamen, platformio-Snippet,
  NACHTESTS als Ergebnisdok, GERAETEPROFIL §6–8, README API/Build/Docs, WEBINTERFACE §8,
  BLE-SCAN umgeschrieben, BEDIENUNG.md neu
- **ENTWICKLERDOKU**: Persistenz, Statusobjekt, Sicherheit, Regelung, Glossar; HR machine nicht refused
- **tools/docs_html.py** + CI-Schritt `--check`; `docs/HANDBUCH.html` gitignored
- **fixtures_ibd.h** Kopfkommentar korrigiert (einzige Code-Ausnahme laut Auftrag)

## Zahlen noch offen / bewusst so

- Flash **74,9 %** = Bin-Größe 1 472 992 / Partition 1 966 080 (0.3.23-dev), kein frischer `pio size`-Log
- ceilingW **224** in KALIBRIERUNG/ENTWICKLERDOKU; **226** bleibt Momentaufnahme in HW_TEST2
- Abnahmekriterien 9–12, 14–17, 19–22 in STATE als „nicht angefangen / nicht formal“ — Status unklar im Detail

## Frage aus C4 (Code beantwortet)

Firmware **verweigert HR_HOLD/Reha nicht** bei `hrSource=machine`. Für Reha problematisch
(+25 bpm Bike vs Strap). Ob das geändert werden soll, ist Produktentscheidung — nicht still
geändert.

## Was die Entwurfs-Instanz prüfen sollte

- Inhaltliche Stichprobe ENTWICKLERDOKU §9–13 und NACHTESTS Test 6
- Ob `BEDIENUNG.md` reicht oder erweitert werden soll
- Ob `TODO_DOKUMENTATION.md` nach Abnahme selbst nach `archiv/` wandert
