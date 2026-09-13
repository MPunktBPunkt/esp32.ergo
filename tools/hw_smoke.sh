#!/usr/bin/env bash
# Hardware-Smoke ohne Fahrer-Bestaetigung.
# Nutzung: bash tools/hw_smoke.sh [IP]
set -euo pipefail
IP="${1:-192.168.178.88}"
B="http://$IP"

echo "== status =="
curl -sS --max-time 8 "$B/api/status" | python3 -c '
import sys,json
d=json.load(sys.stdin)
print("name", d.get("name"), "fs", d.get("fsReady"), "profile", d.get("profile"),
      "bike", d.get("bikeLink"), "map", (d.get("erg") or {}).get("mapReady"),
      "mode", d.get("mode"))
assert d.get("fsReady") is True or d.get("fsReady") is False
'

echo "== workout list =="
curl -sS --max-time 8 "$B/api/workout/list" | python3 -c '
import sys,json
d=json.load(sys.stdin)
assert d.get("ok")
ids=[x["id"] for x in d.get("builtins",[])]
print("builtins", ids)
assert "physio" in ids and "reha_kurz" in ids
assert "test_ramp" in ids and "test_20min" in ids and "test_recovery" in ids
assert "ss_3x12" in ids and "over_under" in ids and "vo2_5x4" in ids
'

echo "== validate =="
curl -sS --max-time 8 -X POST "$B/api/workout/validate" \
  -H "Content-Type: application/json" \
  --data '{"name":"Smoke","steps":[{"duration_s":30,"target":{"power":40},"limit":{"hr_max":120},"label":"A"}]}' \
  | python3 -c 'import sys,json;d=json.load(sys.stdin);print(d);assert d.get("ok");assert d.get("timeline");assert d.get("durationS")==30;assert d.get("peakW")==40'

echo "== put =="
curl -sS --max-time 8 -X POST "$B/api/workout/put" \
  -H "Content-Type: application/json" \
  --data '{"name":"Smoke Test","id":"smoke_test","steps":[{"duration_s":20,"target":{"power":40},"limit":{"hr_max":120,"hr_soft":115},"label":"A"},{"duration_s":20,"target":{"power":50},"limit":{"hr_max":120},"label":"B"}]}' \
  | python3 -c 'import sys,json;d=json.load(sys.stdin);print(d);assert d.get("ok"), d'

echo "== download builtin =="
curl -sS --max-time 8 "$B/api/workout/download?id=physio" | python3 -c '
import sys,json
d=json.load(sys.stdin)
assert d.get("steps") and len(d["steps"])>=3
print("physio steps", len(d["steps"]))
'

# Workout start braucht Profil + Map. Wenn beides da: kurz starten und stoppen.
READY=$(curl -sS --max-time 8 "$B/api/status" | python3 -c '
import sys,json
d=json.load(sys.stdin)
print("1" if d.get("profile") and (d.get("erg") or {}).get("mapReady") else "0")
')
if [[ "$READY" == "1" ]]; then
  echo "== workout start scale=0.05 =="
  curl -sS --max-time 15 -X POST "$B/api/workout/start?id=reha_kurz&scale=0.05" \
    | python3 -c 'import sys,json;d=json.load(sys.stdin);print(d);assert d.get("ok") or d.get("mode")=="WORKOUT" or d.get("state")'
  sleep 1
  curl -sS --max-time 8 "$B/api/status" | python3 -c '
import sys,json
d=json.load(sys.stdin)
print("mode", d.get("mode"), "wo", d.get("workout"))
assert d.get("mode")=="WORKOUT"
'
  echo "== pause / resume / skip / stop =="
  curl -sS --max-time 8 -X POST "$B/api/workout/pause" >/dev/null
  curl -sS --max-time 8 -X POST "$B/api/workout/resume" >/dev/null
  curl -sS --max-time 8 -X POST "$B/api/workout/skip" >/dev/null
  curl -sS --max-time 8 -X POST "$B/api/workout/stop" >/dev/null
  curl -sS --max-time 8 "$B/api/session/last" | python3 -c '
import sys,json
d=json.load(sys.stdin)
print("session", d)
assert d.get("ok")
'
  curl -sS --max-time 8 "$B/api/session/list" | python3 -c '
import sys,json
d=json.load(sys.stdin)
print("session list", d.get("count"), "items")
assert d.get("ok") and "sessions" in d
'
else
  echo "== skip engine run (kein Profil oder keine Map) =="
fi

# Nachtest 3: raw power — nur wenn Bike + Profil
BIKE=$(curl -sS --max-time 8 "$B/api/status" | python3 -c 'import sys,json;d=json.load(sys.stdin);print("1" if d.get("bikeLink") and d.get("profile") else "0")')
if [[ "$BIKE" == "1" ]]; then
  echo "== Nachtest 3: power raw=1 (erwartete Ablehnung) =="
  curl -sS --max-time 10 -X POST "$B/api/control/power?watt=100&raw=1" \
    | python3 -c '
import sys,json
d=json.load(sys.stdin)
print(d)
# Denied / ok:false / result name — kein stiller Erfolg ohne Hinweis
s=json.dumps(d).lower()
assert ("deny" in s) or ("denied" in s) or d.get("ok") is False or "reject" in s or "abgelehnt" in s or d.get("result")=="Denied" or "Denied" in str(d)
print("Nachtest 3 API: Ablehnung wie erwartet")
'
else
  echo "== skip Nachtest 3 (Bike/Profil fehlt) =="
fi

echo "OK hw_smoke $IP"
