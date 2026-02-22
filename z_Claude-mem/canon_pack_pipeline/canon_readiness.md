# Canon Readiness Report

- pack_ready: `false`
- high_conf_claim_total: `33`
- invariant_count: `4`
- decision_count: `16`

## Gates
- `claims_total_gte_30`: pass=true | details={"pass": true, "value": 33, "threshold": 30}
- `invariants_gte_10`: pass=false | details={"pass": false, "value": 4, "threshold": 10}
- `decisions_gte_15`: pass=true | details={"pass": true, "value": 16, "threshold": 15}
- `architecture_dimensions_complete`: pass=true | details={"pass": true, "missing_dimensions": []}
- `unknown_domains_explicit`: pass=true | details={"pass": true, "unknown": []}