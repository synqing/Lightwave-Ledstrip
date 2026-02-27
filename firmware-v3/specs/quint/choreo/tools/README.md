# Choreo Trace Collection Tools

## ITF Converter (`jsonl_to_itf.py`)

Converts JSONL event logs to ITF (Informal Trace Format) for Quint conformance checking.

**ADR-015 Compliance**: All integers are encoded as `{"#bigint": "num"}` format. Use `validate_itf_bigint.py` to verify compliance.

**Usage**:
```bash
python3 jsonl_to_itf.py <input.jsonl> <output.itf.json> [node_id]
```

**Validation**:
```bash
python3 validate_itf_bigint.py <output.itf.json>
```

## Synthetic Test

Test ITF converter without hardware:
```bash
python3 jsonl_to_itf.py test_synthetic.jsonl test_synthetic.itf.json
python3 validate_itf_bigint.py test_synthetic.itf.json
```

## Trace Collection (Requires Hardware)

See `collect_trace.py` and `extract_jsonl.py` for hardware trace collection.
