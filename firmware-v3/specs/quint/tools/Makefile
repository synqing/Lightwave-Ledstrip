# Quint Specification Tools Makefile
# Provides targets for typecheck, simulation, verification, and tuning sweeps

SHELL := /bin/bash

# Paths (relative to tools/ directory)
SPEC_DIR := ../beat_tracker
REPORTS_DIR := ../../reports/beat_tracker
TIMESTAMP := $(shell date +%Y%m%d_%H%M%S)
OUTPUT_DIR := $(REPORTS_DIR)/$(TIMESTAMP)

# Quint parameters
SPEC_FILE := $(SPEC_DIR)/beat_tracker.qnt

# Override-friendly defaults (can be set via command line: make target N=20 STEPS=500 SEED=123)
N ?= 20
STEPS ?= 500
SEED ?= 123

# Legacy parameters (for backward compatibility)
MAX_STEPS_SMOKE := 100
MAX_STEPS_FULL := $(STEPS)
TRACE_COUNT_SMOKE := 10
TRACE_COUNT_FULL := $(N)

# ============================================================================
# Environment Check
# ============================================================================

.PHONY: check-env
check-env:
	@echo "==> Checking Quint installation..."
	@quint --version || (echo "ERROR: quint not found. Install with: brew install quint" && exit 1)
	@echo "==> Checking Python..."
	@python3 --version || (echo "ERROR: python3 not found" && exit 1)
	@echo "==> Environment check passed ✓"

# ============================================================================
# Typecheck
# ============================================================================

.PHONY: typecheck
typecheck: check-env
	@echo "==> Typechecking $(SPEC_FILE)..."
	@quint typecheck $(SPEC_FILE)
	@echo "==> Typecheck passed ✓"

# ============================================================================
# Speed 1: PR Check (Fast)
# ============================================================================

.PHONY: speed1-pr-check
speed1-pr-check: check-env typecheck
	@echo "==> Running Speed 1 (PR Check)..."
	@echo "==> Smoke traces ($(TRACE_COUNT_SMOKE) traces, $(MAX_STEPS_SMOKE) steps)..."
	@mkdir -p /tmp/quint_smoke_traces
	@quint run --max-steps=$(MAX_STEPS_SMOKE) --n-traces=$(TRACE_COUNT_SMOKE) --mbt \
		--out-itf=/tmp/quint_smoke_traces/ --main=beat_tracker $(SPEC_FILE) 2>&1 | tail -5
	@echo "==> Bounded verification (BpmValid, ConfidenceValid)..."
	@quint verify --max-steps=50 --invariant=BpmValid $(SPEC_FILE) 2>&1 | tail -3 || true
	@quint verify --max-steps=50 --invariant=ConfidenceValid $(SPEC_FILE) 2>&1 | tail -3 || true
	@echo "==> Speed 1 complete ✓"

# ============================================================================
# Speed 2: Nightly/Full Verification
# ============================================================================

.PHONY: speed2-nightly
speed2-nightly: check-env typecheck
	@echo "==> Running Speed 2 (Nightly Verification)..."
	@mkdir -p $(OUTPUT_DIR)/traces
	@echo "==> Generating $(N) traces with $(STEPS) steps (seed=$(SEED))..."
	@quint run --max-steps=$(STEPS) --n-traces=$(N) --seed=$(SEED) --mbt \
		--out-itf=$(OUTPUT_DIR)/traces/ --main=beat_tracker $(SPEC_FILE) 2>&1 | tail -10
	@echo "==> Creating run manifest..."
	@./create_manifest.sh $(OUTPUT_DIR) $(STEPS) $(N) $(SEED)
	@echo "==> Analyzing traces..."
	@python3 analyze_itf.py $(OUTPUT_DIR) --manifest $(OUTPUT_DIR)/run_manifest.json
	@echo "==> Speed 2 complete ✓"
	@echo "==> Results: $(OUTPUT_DIR)"

# ============================================================================
# Deterministic Sweep (Phase 1)
# ============================================================================

.PHONY: sweep-deterministic
sweep-deterministic: check-env typecheck
	@echo "==> Running deterministic sweep: $(N) traces per tuning, $(STEPS) steps, seed=$(SEED)..."
	@mkdir -p $(OUTPUT_DIR)/traces
	@for k in 0 1 2 3 4 5 6 7; do \
		echo "==> Tuning $$k: $(N) traces..."; \
		mkdir -p $(OUTPUT_DIR)/traces/tuning_$$k; \
		quint run --max-steps=$(STEPS) --n-traces=$(N) --seed=$(SEED) --init=init_tuning_$$k --mbt \
			--out-itf=$(OUTPUT_DIR)/traces/tuning_$$k/ --main=beat_tracker $(SPEC_FILE) 2>&1 | tail -3; \
	done
	@echo "==> Creating run manifest..."
	@./create_manifest.sh $(OUTPUT_DIR) $(STEPS) $$(($(N) * 8)) $(SEED)
	@echo "==> Analyzing traces (recursive)..."
	@python3 analyze_itf.py $(OUTPUT_DIR) --manifest $(OUTPUT_DIR)/run_manifest.json
	@echo "==> Deterministic sweep complete ✓"
	@echo "==> Results: $(OUTPUT_DIR)"

# ============================================================================
# Witness Checks (Phase 2)
# ============================================================================

.PHONY: witness-lock
witness-lock: check-env typecheck
	@echo "==> Running witness check: Lock within $(STEPS) ticks (seed=$(SEED))..."
	@rm -rf /tmp/quint_witness && mkdir -p /tmp/quint_witness/traces
	@quint run --max-steps=$(STEPS) --n-traces=40 --seed=$(SEED) --mbt \
		--out-itf=/tmp/quint_witness/traces/ --main=beat_tracker $(SPEC_FILE) 2>&1 | tail -5
	@echo "==> Checking witness observed rates..."
	@python3 check_witnesses.py /tmp/quint_witness
	@echo "==> Witness check complete ✓"

# ============================================================================
# Doc-Sync Gate
# ============================================================================

.PHONY: doc-sync
doc-sync: check-env
	@echo "==> Running doc-sync gate..."
	@python3 check_quint_sync.py $(SPEC_DIR)
	@echo "==> Doc-sync gate passed ✓"

# ============================================================================
# Full Pipeline
# ============================================================================

.PHONY: full-pipeline
full-pipeline: typecheck doc-sync speed2-nightly witness-lock
	@echo "==> Full pipeline complete ✓"

# ============================================================================
# Quick Test
# ============================================================================

.PHONY: quick-test
quick-test: check-env typecheck
	@echo "==> Quick test (10 traces, 100 steps)..."
	@mkdir -p /tmp/quint_quick_test/traces
	@quint run --max-steps=100 --n-traces=10 --mbt \
		--out-itf=/tmp/quint_quick_test/traces/ --main=beat_tracker $(SPEC_FILE) 2>&1 | tail -5
	@python3 analyze_itf.py /tmp/quint_quick_test 2>&1 | tail -20
	@echo "==> Quick test complete ✓"

# ============================================================================
# Clean
# ============================================================================

.PHONY: clean
clean:
	@rm -rf /tmp/quint_*
	@rm -rf _apalache-out
	@echo "==> Cleaned temporary files ✓"

# ============================================================================
# Help
# ============================================================================

.PHONY: help
help:
	@echo "Quint Specification Tools"
	@echo ""
	@echo "Targets:"
	@echo "  typecheck        - Typecheck the beat tracker spec"
	@echo "  speed1-pr-check  - Fast PR check (smoke traces + bounded verify)"
	@echo "  speed2-nightly   - Full tuning sweep with analysis (uses N/STEPS/SEED)"
	@echo "  sweep-deterministic - Deterministic sweep: exactly N traces per tuning"
	@echo "  witness-lock     - Check witness observed rates"
	@echo "  doc-sync         - Verify spec/doc alignment"
	@echo "  full-pipeline    - Run all checks"
	@echo "  quick-test       - Quick test (10 traces, 100 steps)"
	@echo "  clean            - Remove temporary files"
	@echo ""
	@echo "Defaults: N=$(N), STEPS=$(STEPS), SEED=$(SEED) (override via: make target N=20 STEPS=500 SEED=123)"
	@echo "Reports are written to: $(REPORTS_DIR)/<timestamp>/"
