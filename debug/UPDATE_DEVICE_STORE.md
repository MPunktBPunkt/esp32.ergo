# UPDATE — DeviceStore (0.3.11-dev)

Stand: **2026-09-13**. Pflichtenheft §5 / §9: Geräteprofil je MAC.

## Was

### `DeviceStore` (Arduino-frei, hosttestbar)
- Bis zu **4** Geräte: MAC, Name, AddrType, Widerstandsformat, Watt-Vertrauen,
  Request-Control-Flag, gemessene Leistungsdecke
- Persistenz NVS `ergodev` / `meta`
- Kennfläche je Slot: `m0`…`m3` (nicht mehr eine globale `ergomap/pmap`)
- Migration: vorhandene Legacy-Map → aktiver Slot beim ersten Laden
- Varon-MAC `c2:32:a5:1e:bf:b5` bekommt Defaults (sint16, untrusted)

### Verdrahtung
- Connect → remember + Map-Slot laden + Caps-Overrides + optional Request Control
- `FtmsClient::applyDeviceOverrides`
- Status: `device.{mac,format,powerTrusted,ceilingW,slot,…}`
- API: `GET /api/devices`, `GET|POST /api/device`
- UI: Einstellungen → „Aktives Bike“

## Nicht in diesem Paket
- LittleFS-JSON-Export der Maps
- Multi-Bike-UI ohne Connect
- Lab-Seed-Map / „ungemessen“-Flag

## Tests
`pio test -e native -f test_device`
