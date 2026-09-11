# Test 2 leicht — 80 rpm, Stufe 4+8

Stand: **2026-09-11** ~21:44. Profil **standard**.

## Sweep-Punkte

| Stufe | Mittel-W | Mittel-rpm | rpm min…max | gültig |
|------:|---------:|-----------:|-------------|:------:|
| 4,0 | 67,5 | 79,5 | 73…84 | nein („Kadenz nicht gehalten“) |
| 8,0 | **122,7** | 79,4 | 78…82 | ja |

## Vergleich mit Test 1 (60 rpm)

| Stufe | 60 rpm | 80 rpm | Δ |
|------:|-------:|-------:|--:|
| 4 | 50 W | ~68 W (verworfen, nur Richtung) | ≈ +35 % |
| 8 | 89 W | **123 W** | **+37 %** |

## Entscheidung (NACHTESTS.md Test 2)

Bei **fester Stufe** steigt die Leistung mit der Kadenz — wie erwartet.
Die Konsole „drehzahlunabhängig“ ist Eigenschaft des **Watt-Programms**, nicht
der FTMS-Widerstandsstufe. Die Kennfläche muss eine **Fläche** Stufe×Kadenz
sein, keine reine Stufe→Watt-Tabelle.

Ein voller Test 2 (Stufen 12/16 @ 80 rpm) ist für die Architekturentscheidung
**nicht nötig**. Optional später für dichtere Abdeckung der Map.
