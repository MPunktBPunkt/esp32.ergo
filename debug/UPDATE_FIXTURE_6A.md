# Fixture-Export-Roundtrip (Abnahmekriterium 6a)

Stand: **2026-09-11**.

## Ergebnis

`GET /api/debug/export` liefert JSONL im Sondenformat. Dasselbe Format liest
`tools/make-fixtures.py` als `bike-data.jsonl`.

Nachgewiesen:

1. **Eingecheckte Stichprobe** `test/fixtures/debug-export/` enthält die fünf
   kuratierten Varon-Pakete aus `fixtures_ibd.h` plus weitere Notifies und
   Write/Response-Zeilen (wie der Ring sie schreibt).
2. **`python tools/make-fixtures.py --verify-curated`** — alle 5 kuratierten
   Hex-Pakete matchen `tools/ref/ftms.py` (unabhängige Referenz).
3. **Live auf `.88`:** Ring an → Export → `make-fixtures.py --scan` erzeugt
   Header ohne Fehler.
4. **CI:** derselbe `--verify-curated`-Schritt im Workflow `native-tests`.

`fixtures_ibd.h` bleibt die von Hand kommentierte Kernmenge. Volle Regenerierung
nur mit `--write-curated` (bewusst, sonst Temp-Datei).

Zusätzlich: Sweep-Start verlangt jetzt ein aktives Profil (wie jede Last).
