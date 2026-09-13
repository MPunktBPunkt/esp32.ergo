# Bridge — Rollen, Exklusiv-Steuerung, MyWhoosh

Stand: **2026-09-13**, Firmware **0.3.14-dev**.
Arbeitsnotiz zum gleichen Stand: [`debug/UPDATE_BRIDGE_EXCLUSIVE.md`](../../debug/UPDATE_BRIDGE_EXCLUSIVE.md).
Einstieg / Was läuft: [`STATE.md`](../../STATE.md).

Die Bridge macht aus dem Varon (nur Widerstandsstufen) einen FTMS-Trainer mit
Wattziel für Apps wie MyWhoosh. Dieses Dokument hält die **Betriebsregeln**,
die aus der Live-Abnahme folgen — nicht nur den Feature-Wunsch aus dem
Pflichtenheft.

---

## 1. Topologie

```
Varon ──BLE Central──▶ esp32.ergo ──BLE Peripheral──▶ MyWhoosh (FTMS/CPS/CSC)
                              │
                              └── WiFi ──▶ Coach-UI + Hub
```

Puls kommt typisch über `esp32.heartrate` (Relay), nicht über die Bridge.
MyWhoosh koppelt den Gurt getrennt.

---

## 2. Observer vs. Controller

Nicht jeder Bridge-Client steuert. Die Firmware unterscheidet:

| `clientRole` | Bedingung |
|--------------|-----------|
| `none` | kein Client |
| `connected` | verbunden, ohne IBD-Abo und ohne Control-Grant |
| `observer` | IBD-Abo und/oder Control granted, **noch kein** Lastkommando |
| `controller` | mindestens ein Lastkommando (siehe unten) |

**Lastkommandos** (machen aus Observer einen Controller):

- `Set Target Power` (`0x05`)
- `Set Target Resistance` (`0x04`)
- `Set Indoor Bike Simulation` (`0x11`)

Nur Daten lesen / Control Point freigeben ohne Watt/Stufe/Sim = **Observer**.
Die Coach-UI darf weiter Last setzen.

Felder in `/api/status` → `bridge` und `/api/bridge`:

| Feld | Bedeutung |
|------|-----------|
| `controlling` | `true` = Controller |
| `clientRole` | eine der Rollen oben |
| `loadCommands` | Zähler der Lastkommandos seit Grant/Connect |
| `exclusive` | nur `/api/bridge`: Coach-Last ist gesperrt |
| `driving` | Bridge-ERG aktiv (Wattziel läuft über PowerController) |

---

## 3. Exklusiv-Steuerung

**Regel:** Sobald die App Last schickt (`controlling`), übernimmt sie die
Regelung vollständig. Coach und App dürfen nicht parallel die Stufe jagen.

Solange `controlling`:

- Coach-API für Last → **HTTP 409**  
  betrifft u. a. `mode` (außer `off`), `level`, `power`, `sim`, Workout-Start, Sweep
- Coach-UI: Last-Buttons disabled, Hinweis „Bridge-App steuert exklusiv“
- **STOP** (`/api/control/stop`) bleibt erlaubt und gibt den Lock frei
  (`FtmsServer::releaseController`)

Lock endet auch bei: App-Stop/Reset (nicht Pause), Disconnect, Bridge aus.

Motivation: Live-Session zeigte „Apps kämpfen“ — Coach-LEVEL und MyWhoosh-ERG
überschrieben sich gegenseitig.

---

## 4. MyWhoosh: Power ↔ Resistance

Beobachtung: MyWhoosh wechselt im ERG oft zwischen **Set Power** und
**Set Resistance**. Ohne Schutz schaltet die Bridge dann zwischen
`MANUAL_ERG` und `MANUAL_LEVEL` hin und her → Stufenjagd, `UNJUDGED` im Journal.

**Schutz (0.3.14):** Solange Bridge-ERG aktiv (`driving`), werden eingehende
Resistance-Kommandos **nicht angewendet** (BLE bereits mit Success quittiert).
Log: `Stufe … ignoriert — ERG aktiv (Exklusiv)`.

Wattziele laufen weiter über Difficulty → optional HR-Deckel → PowerController
→ Stufen am Bike (nie Opcode `0x05` ans Varon, der ist tot).

---

## 5. Difficulty und HR-Deckel

Unverändert zur Bridge-MVP-Erweiterung:

- `bridgeDifficultyPct` (50–150): skaliert App-Watt vor dem Regler
- `bridgeHrSoft` / `bridgeHrMax` (0 = aus): Soft/Hard-Deckel nur auf Bridge-ERG

Siehe auch [`debug/UPDATE_BRIDGE.md`](../../debug/UPDATE_BRIDGE.md).

---

## 6. Abnahme-Checkliste (Zuhause)

1. Bridge an → Phone findet Indoor Bike / FTMS
2. Nur verbinden, noch kein Workout → UI: **App beobachtet** (`observer`), Coach-Last frei
3. ERG in MyWhoosh starten → **App steuert** (`controller`), Coach-Last 409 / Buttons aus
4. Wattziele ≈ Bike-Ist (Difficulty 100 %), keine wilden Stufensprünge durch Resistance
5. Coach **STOP** → Lock weg, Bike entlastet; MyWhoosh neu Last → Lock wieder da
6. Difficulty / HR-Deckel optional prüfen

Offen bleibt ggf. Dämpfung der ERG-Stufenjagd durch Kadenz/Map (separates Thema).
