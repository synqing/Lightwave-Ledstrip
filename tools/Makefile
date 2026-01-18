# LightwaveOS Tools Makefile
#
# Usage:
#   make contextpack           - Generate context pack with TOONified fixtures
#   make contextpack-lint      - Validate config and templates
#   make contextpack-clean     - Remove generated context pack
#   make toon-install          - Install TOON CLI dependencies
#   make test                  - Run unit tests
#   make help                  - Show this help

.PHONY: contextpack contextpack-lint contextpack-clean toon-install test help

# Default output directory
OUT_DIR ?= ../contextpack

# Context pack generator (full pipeline)
contextpack: toon-install
	@./contextpack_wrapper.sh --out $(OUT_DIR)

# Lint mode (validate only)
contextpack-lint:
	@python contextpack.py --lint

# Clean generated context pack
contextpack-clean:
	@echo "Removing context pack directory: $(OUT_DIR)"
	@rm -rf $(OUT_DIR)

# Install TOON CLI dependencies (idempotent)
toon-install:
	@if [ ! -d "node_modules" ]; then \
		echo "Installing TOON CLI..."; \
		npm install; \
	fi

# Run unit tests
test: toon-install
	@python test_contextpack.py

# Help target
help:
	@echo "LightwaveOS Tools"
	@echo ""
	@echo "Usage:"
	@echo "  make contextpack           Generate context pack (full pipeline)"
	@echo "  make contextpack OUT_DIR=path  Generate to custom directory"
	@echo "  make contextpack-lint      Validate config and templates"
	@echo "  make contextpack-clean     Remove generated context pack"
	@echo "  make toon-install          Install TOON CLI dependencies"
	@echo "  make test                  Run unit tests"
	@echo ""
	@echo "Features:"
	@echo "  - Denylist/allowlist via .contextpackignore"
	@echo "  - Deterministic output (stable ordering)"
	@echo "  - Size budgets (100KB soft, 200KB hard) with auto-split"
	@echo "  - Cache-friendly prompt.md generation"
	@echo "  - Secret detection in lint mode"
	@echo ""
	@echo "Context Pack workflow:"
	@echo "  1. Add JSON fixtures to contextpack/fixtures/"
	@echo "  2. Run 'make contextpack'"
	@echo "  3. Hand agent the output directory"
	@echo ""
	@echo "See docs/contextpack/README.md for full documentation."
