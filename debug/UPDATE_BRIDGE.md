# UPDATE — Bridge Erweiterungen (v0.3.1-dev)

Stand: 2026-09-12. Open Bridge-Themen ohne Hardware-Abnahme.
**0.3.14-dev:** Exclusive Control → [UPDATE_BRIDGE_EXCLUSIVE.md](UPDATE_BRIDGE_EXCLUSIVE.md).

## Neu gegenüber 0.3.0-dev

### Difficulty + HR-Deckel
- Config: `bridgeDifficultyPct` (50–150, Default 100), `bridgeHrSoft` / `bridgeHrMax` (0 = aus)
- App-Watt → `bridgeApplyDifficulty` → optional `BridgeHrCap` → PowerController
- API `/api/bridge` liefert/setzt Difficulty und HR-Grenzen
- UI unter Einstellungen
- Hosttests: `test/test_bridge`

### CPS / CSC
- Peripheral Services `0x1818` + `0x1816` (Crank-Feature)
- Notify aus Bike-Live (Watt + integrierte Kurbelumdrehungen)
- Encoder: `CyclingCodec` (hosttestbar)

### 0x11 Simulation (vorbereitet, gesperrt)
- ControlWrite dekodiert SIM-Parameter
- Bridge antwortet **NotSupported**, solange Limiter `allowSimulation=false`
- Bei Freigabe (Nachtest 4): Pending → `FtmsClient::setSimulation` durch Limiter

## Nicht erledigt (braucht Zuhause / Messung)
- MyWhoosh-Abnahme ERG / Difficulty / HR-Deckel / Exclusive
- Nachtest 4 → `allowSimulation=true`
- Nachtest 5/6

## Ausrollen
```bash
bash tools/deploy.sh --ota 192.168.178.88 --skip-tests
```
