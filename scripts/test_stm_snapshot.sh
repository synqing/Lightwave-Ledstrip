#!/usr/bin/env bash
# Test STM snapshot endpoint on K1
# Usage: Connect to Lightwave-AP WiFi, then run: ./scripts/test_stm_snapshot.sh
#
# Validates:
#   1. K1 is reachable at 192.168.4.1
#   2. /api/v1/audio/stm returns valid JSON
#   3. STM pipeline is warm (ready=true)
#   4. 42 spectral bins + 16 temporal bands present
#   5. Derived metrics (centroid, dominant bin, max, RMS) are computed

K1_HOST="192.168.4.1"
ENDPOINT="http://${K1_HOST}/api/v1/audio/stm"

echo "=== STM Snapshot Test ==="
echo "Endpoint: ${ENDPOINT}"
echo ""

# Check connectivity
if ! ping -c 1 -W 2 "${K1_HOST}" &>/dev/null; then
    echo "FAIL: Cannot reach ${K1_HOST}"
    echo "      Are you connected to the Lightwave-AP WiFi network?"
    exit 1
fi
echo "OK: K1 reachable"

# Fetch snapshot
RESPONSE=$(curl -s --connect-timeout 5 "${ENDPOINT}" 2>&1)
if [ $? -ne 0 ]; then
    echo "FAIL: curl failed — ${RESPONSE}"
    exit 1
fi

# Pretty-print
echo ""
echo "--- Raw Response ---"
echo "${RESPONSE}" | python3 -m json.tool 2>/dev/null || echo "${RESPONSE}"
echo ""

# Validate with python
python3 -c "
import json, sys

try:
    d = json.loads('''${RESPONSE}''')
except json.JSONDecodeError as e:
    print(f'FAIL: Invalid JSON — {e}')
    sys.exit(1)

if not d.get('success'):
    print(f'FAIL: success={d.get(\"success\")} — {d}')
    sys.exit(1)

data = d.get('data', {})
checks = []

# STM ready
ready = data.get('ready', False)
checks.append(('STM ready', ready, 'true' if ready else 'false (pipeline still warming)'))

# Spectral bins
sp = data.get('spectral', {})
sp_vals = sp.get('values', [])
sp_bins = sp.get('bins', 0)
checks.append(('Spectral bins count', sp_bins == 42, f'{sp_bins} (expected 42)'))
checks.append(('Spectral values length', len(sp_vals) == 42, f'{len(sp_vals)} (expected 42)'))
checks.append(('Spectral energy', sp.get('energy') is not None, f'{sp.get(\"energy\", \"MISSING\")}'))

# Temporal bands
tp = data.get('temporal', {})
tp_vals = tp.get('values', [])
tp_bands = tp.get('bands', 0)
checks.append(('Temporal bands count', tp_bands == 16, f'{tp_bands} (expected 16)'))
checks.append(('Temporal values length', len(tp_vals) == 16, f'{len(tp_vals)} (expected 16)'))
checks.append(('Temporal energy', tp.get('energy') is not None, f'{tp.get(\"energy\", \"MISSING\")}'))

# Derived metrics
dr = data.get('derived', {})
checks.append(('Spectral centroid bin', dr.get('spectralCentroidBin') is not None, f'{dr.get(\"spectralCentroidBin\", \"MISSING\")}'))
checks.append(('Spectral dominant bin', dr.get('spectralDominantBin') is not None, f'{dr.get(\"spectralDominantBin\", \"MISSING\")}'))
checks.append(('Spectral energy max', dr.get('spectralEnergyMax') is not None, f'{dr.get(\"spectralEnergyMax\", \"MISSING\")}'))
checks.append(('Spectral energy RMS', dr.get('spectralEnergyRms') is not None, f'{dr.get(\"spectralEnergyRms\", \"MISSING\")}'))

# Non-zero check (only if ready)
if ready:
    any_nonzero_sp = any(v > 0.001 for v in sp_vals)
    any_nonzero_tp = any(v > 0.001 for v in tp_vals)
    checks.append(('Spectral has signal', any_nonzero_sp, 'non-zero values present' if any_nonzero_sp else 'ALL ZEROS — check audio input'))
    checks.append(('Temporal has signal', any_nonzero_tp, 'non-zero values present' if any_nonzero_tp else 'ALL ZEROS — check audio input'))

print('--- Validation ---')
all_pass = True
for name, ok, detail in checks:
    status = 'OK' if ok else 'WARN' if 'warming' in str(detail) else 'FAIL'
    if not ok:
        all_pass = False
    print(f'  {status}: {name} — {detail}')

print('')
if all_pass:
    print('PASS: All checks passed')
else:
    print('WARN: Some checks did not pass — see above')
" 2>&1

echo ""
echo "=== Done ==="
