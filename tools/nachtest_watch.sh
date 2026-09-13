#!/usr/bin/env bash
# Paralleles Mitlesen der Nachtests — Status + Probe + Journal alle 2 s.
#
#   bash tools/nachtest_watch.sh [IP] [out.jsonl]
#
# Ausgabe: eine JSON-Zeile pro Tick.
set -euo pipefail
IP="${1:-192.168.178.88}"
OUT="${2:-debug/captures/nachtest-$(date -u +%Y%m%dT%H%M%SZ).jsonl}"
mkdir -p "$(dirname "$OUT")"
B="http://$IP"
echo "watching $B → $OUT (Ctrl-C stop)"
echo "Tipp: vorher  curl -X POST $B/api/probe/arm"

while true; do
  ts="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  if ! raw="$(curl -sS --max-time 5 "$B/api/status" 2>/dev/null)"; then
    printf '{"ts":"%s","error":"unreachable"}\n' "$ts" | tee -a "$OUT"
    sleep 2
    continue
  fi
  printf '%s\n' "$raw" | python3 -c '
import json,sys
ts="'"$ts"'"
try:
    d=json.load(sys.stdin)
except Exception as e:
    print(json.dumps({"ts":ts,"error":str(e)}))
    raise SystemExit(0)
pr=d.get("probe") or {}
dbg=(d.get("debug") or {}).get("journal") or {}
entries=dbg.get("entries") or []
head=entries[0] if entries else None
ft=(d.get("ftms") or {}).get("data") or {}
row={
  "ts": ts,
  "version": d.get("version"),
  "mode": d.get("mode"),
  "profile": d.get("profile"),
  "bikeLink": d.get("bikeLink"),
  "hrSource": d.get("hrSource"),
  "heartRate": d.get("heartRate"),
  "probe": {
    "watt": pr.get("watt"), "rpm": pr.get("rpm"),
    "hrBike": pr.get("hrBike"), "hrStrap": pr.get("hrStrap"),
    "levelTenths": pr.get("levelTenths"),
    "allowSimulation": pr.get("allowSimulation"),
    "ringOn": pr.get("ringOn"),
    "marks": [{"label":m.get("label"),"watt":m.get("watt"),"rpm":m.get("rpm"),
               "hrBike":m.get("hrBike"),"hrStrap":m.get("hrStrap")}
              for m in (pr.get("marks") or [])],
  },
  "live": {"watt": ft.get("powerW"), "rpm": ft.get("cadenceRpm"), "hr": ft.get("heartRate")},
  "journal": {
    "judged": dbg.get("judged"), "worked": dbg.get("worked"),
    "noEffect": dbg.get("noEffect"), "contradictions": dbg.get("contradictions"),
    "head": ({"cmd":head.get("cmd"),"effect":head.get("effect"),
              "preWatt":head.get("preWatt"),"postWatt":head.get("postWatt"),
              "preRpm":head.get("preRpm"),"postRpm":head.get("postRpm"),
              "reason":head.get("reason"),"result":head.get("result")} if head else None),
  },
  "calib": (d.get("calib") or {}).get("sweep"),
}
print(json.dumps(row, separators=(",",":")))
' | tee -a "$OUT"
  sleep 2
done
