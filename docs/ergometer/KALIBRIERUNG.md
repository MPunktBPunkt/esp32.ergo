# Kennfläche — Snapshot 2026-09-13 (Abend)

Stand nach passivem Lernen (u. a. ~40 rpm und Versuche ~100 rpm).
Rohdaten: [`kalibrierung-map-20260913.json`](kalibrierung-map-20260913.json).
Gerät: TC174 `c2:32:a5:1e:bf:b5`, Slot 0, ceilingW ≈ 224,
62 Zellen belegt, 7 von 8 Kadenzbändern
(110–120 noch leer). `*` = Sweep-Stützstelle.

## Raster Watt (passiv + Sweep)

| Stufe | 40–50 | 50–60 | 60–70 | 70–80 | 80–90 | 90–100 | 100–110 | 110–120 |
|------:|------:|------:|------:|------:|------:|------:|------:|------:|
| 1 | 36 | 85 | 65 | 34 | 36 | — | — | — |
| 2 | 19 | 29 | 31* | 36 | 44 | 46 | 56 | — |
| 3 | 23 | 37 | 45 | 46 | — | — | — | — |
| 4 | 30 | 48 | 52* | 62 | 73* | — | 89 | — |
| 5 | 34 | — | 62 | 73 | 82 | 105 | — | — |
| 6 | 43 | 65* | 78 | 109 | 132 | 124 | — | — |
| 7 | 49 | 66 | 88 | 122 | — | — | — | — |
| 8 | 56 | 89* | 97 | 122* | — | — | — | — |
| 9 | 63 | 90 | 107 | 129 | 142 | 171 | — | — |
| 10 | 65 | 110* | — | — | — | — | — | — |
| 11 | 73 | — | — | — | — | — | — | — |
| 12 | 78 | 129* | — | — | — | — | — | — |
| 13 | 86 | — | — | — | — | — | — | — |
| 14 | 89 | — | 151* | — | — | — | — | — |
| 15 | 100 | — | — | — | — | — | — | — |
| 16 | 112 | 154 | 173* | 204 | 245* | — | 339 | — |

## Hinweise zum Füllen

- Spalte **100–110** füllt nur bei rpm ≥ 100 (98 landet in 90–100).
- Passiv: Stufe ≥ 20 s halten, dann alle 5 s ein Punkt.
- Band **110–120** und rpm ≥ 120: noch leer bzw. verworfen.
- Frühere Sweep-Protokolle: [../debug/HW_TEST1_60RPM.md](../../debug/HW_TEST1_60RPM.md),
  [HW_TEST2_80RPM.md](../../debug/HW_TEST2_80RPM.md).

