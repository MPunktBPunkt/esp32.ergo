# debug/ — Handoff zwischen Cursor-Instanzen

Kurzprotokoll und Arbeitsnotizen vom ersten Build (2026-09-11).
Ziel: andere Cursor-Instanz kann ohne Chat-Historie weiterarbeiten.

| Datei | Inhalt |
|-------|--------|
| [FIRST_BUILD.md](FIRST_BUILD.md) | Was gebaut/getestet wurde, Artefakte, Befunde |
| [PLATFORMIO_FIX.md](PLATFORMIO_FIX.md) | Warum `pio test -e native` scheiterte und was geändert wurde |
| [RECOMMENDATIONS.md](RECOMMENDATIONS.md) | Nächste sinnvolle Schritte (CI, App-Schicht, Doku) |
| [commands.sh](commands.sh) | Copy-paste-fähige Build-/Test-Befehle |

**Stand Repo:** Clone von `MPunktBPunkt/esp32.ergo` @ `9cd918a` (Upload), lokal `platformio.ini` bereits gefixt (noch uncommitted).

**Nicht committen:** `.pio/` (Build-Cache). Gehört ggf. in `.gitignore`.
