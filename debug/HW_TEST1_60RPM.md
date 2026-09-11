# Test 1 — Stufen-Sweep 60 rpm (Hardware), 2. Lauf

Stand: **2026-09-11** ~20:55. Profil **standard** (max Stufe 16 / 300 W).
Erster Lauf war unter versehentlich aktivem `reha` ungültig
(siehe Abschnitt unten / ältere Notiz).

## Sweep-Ergebnis (aus `/api/status` → `calib.sweep`)

Zielkadenz 60 rpm. 8 von 9 Punkten gültig. Stufe 1 verworfen
(„Kadenz nicht gehalten").

| Stufe | Mittel-W | Mittel-rpm | rpm min…max | gültig |
|------:|---------:|-----------:|-------------|:------:|
| 1,0 | 26,0 | 61,6 | 56…64 | nein |
| 2,0 | 30,7 | 61,9 | 60…63 | ja |
| 4,0 | 50,0 | 60,0 | 58…61 | ja |
| 6,0 | 69,3 | 59,6 | 58…61 | ja |
| 8,0 | 89,4 | 59,7 | 58…61 | ja |
| 10,0 | 109,7 | 59,8 | 59…62 | ja |
| 12,0 | 129,4 | 60,0 | 57…62 | ja |
| 14,0 | 150,1 | 60,0 | 58…62 | ja |
| 16,0 | 170,2 | 60,1 | 57…62 | ja |

Δ je zwei Stufen (2→4→…→16): ca. **+20 W**. Nahezu linear bis Stufe 16.

## Steuer-Journal

12 beurteilt: **8× WORKS**, 0× NO_EFFECT, 0 Widersprüche, 4 unjudged
(u. a. Kadenz zu niedrig). Der Widerstandskanal wirkt.

## Entscheidung laut NACHTESTS.md Test 1

Stufe 16 bei 60 rpm: **~170 W** → Band **130–200 W**.

- Widerstandskanal trägt Grundlage und Intervalle bis Schwelle.
- Spitzen deutlich über ~170 W bei 60 rpm brauchen höhere Kadenz und/oder
  Simulation `0x11` (Nachtest 4) — kein Totalausfall des Stufenkanals.

## Nächster Schritt

**Test 2** — verkürzter Sweep bei **80 rpm** (Stufen 4, 8, 12, 16).
