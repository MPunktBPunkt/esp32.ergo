# debug/captures/

Rohmitschnitte (NDJSON). Welcher Mitschnitt zu welchem Protokoll gehört:

| Datei | Protokoll / Zweck | Firmware | Hinweis |
|-------|-------------------|----------|---------|
| `nachtest-20260913T085205Z.jsonl` | `HW_NACHTEST_20260913.md` | 0.3.x-dev | Primärbeleg; groß (~536 kB) |
| `nachtest-20260913T145717Z.jsonl` | Nachtest-Abend | 0.3.x-dev | Ergänzung |
| `nachtest-live.jsonl` | laufender/letzter Mitschnitt | — | Arbeitsdatei |
| `debug-20260912T124351Z.jsonl` | Kalibrier-/Debuglauf | — | zu `calib/powermap-*` |
| `ride-20260912T082147Z.jsonl` | Ride-Session | — | — |

`ibdEvery` und Codec-Fixture-Tauglichkeit stehen im zugehörigen Protokollkopf,
falls dokumentiert. Ohne Protokollverweis: nur Rohdaten.
