# LightwaveOS Tools Makefile
#
# Usage:
#   make contextpack           - Generate context pack with TOONified fixtures
#   make contextpack-lint      - Validate config and templates
#   make contextpack-clean     - Remove generated context pack
#   make toon-install          - Install TOON CLI dependencies
#   make test                  - Run unit tests
#   make help                  - Show this help

.PHONY: contextpack contextpack-semantic contextpack-minify contextpack-summaries contextpack-optimised contextpack-lint contextpack-clean toon-install test help

# Default output directory
OUT_DIR ?= ../contextpack

# Context pack generator (full pipeline)
contextpack: toon-install
	@./contextpack_wrapper.sh --out $(OUT_DIR)

# Context pack with semantic chunking
contextpack-semantic: toon-install
	@./contextpack_wrapper.sh --out $(OUT_DIR) --chunk-strategy semantic

# Context pack with minified prompt
contextpack-minify: toon-install
	@./contextpack_wrapper.sh --out $(OUT_DIR) --minify-prompt

# Context pack with hierarchical summaries
contextpack-summaries: toon-install
	@./contextpack_wrapper.sh --out $(OUT_DIR) --summaries

# Context pack with all token-saving features
contextpack-optimised: toon-install
	@./contextpack_wrapper.sh --out $(OUT_DIR) --chunk-strategy semantic --minify-prompt --summaries

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

# Run unit tests (both suites)
test: toon-install
	@python -m pytest -v test_contextpack.py test_contextpack_features.py 2>/dev/null || (python test_contextpack.py && python test_contextpack_features.py)

# Help target
help:
	@echo "LightwaveOS Tools"
	@echo ""
	@echo "Usage:"
	@echo "  make contextpack           Generate context pack (full pipeline)"
	@echo "  make contextpack OUT_DIR=path  Generate to custom directory"
	@echo "  make contextpack-semantic  Use semantic chunking (groups by directory)"
	@echo "  make contextpack-minify    Minify prompt.md (remove whitespace/comments)"
	@echo "  make contextpack-summaries Generate hierarchical summaries (1-line, 5-line, 1-para)"
	@echo "  make contextpack-optimised All token-saving features enabled"
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
