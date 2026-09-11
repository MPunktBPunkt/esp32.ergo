#!/usr/bin/env bash
# Hand-Beweis Stufe 1 vs 16 (STATE.md §3) — wenn jemand vor dem Rad steht.
# Kein Fahrer noetig: Kurbel von Hand drehen.
set -euo pipefail
IP="${1:-192.168.178.88}"
B="http://$IP"

echo "== Status =="
curl -fsS "$B/api/status" | python3 -c 'import json,sys;d=json.load(sys.stdin);print(d["fwType"],d["version"],"wide",d["ftms"]["caps"].get("wide"),"bike",d["ble"]["links"]["bike"]["state"],"mode",d.get("mode"),"profile",d.get("profile"))'

echo "== Mitschnitt an =="
curl -fsS -X POST "$B/api/debug/ring?on=true&every=1"

echo "== Profil (Pflicht vor Last) =="
# Kein stilles Default — Hand-Beweis nimmt die Standard-Vorlage.
curl -fsS -X POST "$B/api/control/mode?mode=off" >/dev/null || true
curl -fsS -X POST "$B/api/profile/select?id=standard"; echo

echo "== Request + Start =="
curl -fsS -X POST "$B/api/control/request"; echo
curl -fsS -X POST "$B/api/control/start"; echo

echo "== Stufe 1.0 — jetzt ~20s Kurbel von Hand drehen, Enter =="
curl -fsS -X POST "$B/api/control/mode?mode=level&tenths=10"; echo
read -r _

echo "== Stufe 16.0 — wieder ~20s drehen, Enter =="
# Rampe: mehrfach anfordern
for _ in $(seq 1 20); do
  curl -fsS -X POST "$B/api/control/level?tenths=160"; echo
  sleep 2.2
  lvl=$(curl -fsS "$B/api/status" | python3 -c 'import json,sys;print(json.load(sys.stdin)["limiter"]["levelTenths"])')
  [ "$lvl" -ge 160 ] && break
done
read -r _

echo "== Journal / Status =="
curl -fsS "$B/api/status" | python3 -c 'import json,sys;d=json.load(sys.stdin);print(json.dumps(d.get("debug",{}).get("journal",{}),indent=2))'

echo "== Export (erste Writes) =="
curl -fsS "$B/api/debug/export" | rg '2AD9|"dir":"write"' | head -20

echo "== Stop =="
curl -fsS -X POST "$B/api/control/stop"; echo
curl -fsS -X POST "$B/api/debug/ring?on=false"; echo
echo "Fertig. Handgefühl Stufe 1 vs 16? Journal contradictory/worked?"
