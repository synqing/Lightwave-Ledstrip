.PHONY: help esv11-tests esv11-tests-fast esv11-benchmark-capture esv11-benchmark-test harmonixset-catalogue harmonixset-metrics harmonixset-enrich harmonixset-pack harmonixset-all

help:
	@echo "Targets:"
	@echo "  esv11-tests       Run all ESV11 validation suites (parity + real music at 12.8k and 32k)"
	@echo "  esv11-tests-fast  Run parity + 12.8k real-music suites"
	@echo "  esv11-benchmark-capture  Capture/update corpus baselines for 12.8k + 32k"
	@echo "  esv11-benchmark-test     Run corpus regression gates for 12.8k + 32k"
	@echo "  harmonixset-catalogue  Build sorted metadata manifests for harmonixset-main/audio"
	@echo "  harmonixset-metrics    Extract tempo/features and build challenge/reference packs"
	@echo "  harmonixset-enrich     Enrich manifests with YouTube metadata (title/channel/tags)"
	@echo "  harmonixset-pack       Build stratified ESV11 corpus pack + 12.8k/32k WAV assets"
	@echo "  harmonixset-all        Run catalogue + metrics + enrichment pipeline"

esv11-tests:
	pio test -e native_test_esv11
	pio test -e native_test_esv11_music
	pio test -e native_test_esv11_music_32khz

esv11-tests-fast:
	pio test -e native_test_esv11
	pio test -e native_test_esv11_music

esv11-benchmark-capture:
	LW_ESV11_CAPTURE_BASELINE=1 pio test -e native_test_esv11_music_corpus
	LW_ESV11_CAPTURE_BASELINE=1 pio test -e native_test_esv11_music_corpus_32khz

esv11-benchmark-test:
	pio test -e native_test_esv11_music_corpus
	pio test -e native_test_esv11_music_corpus_32khz

harmonixset-catalogue:
	python3 tools/build_harmonixset_catalogue.py

harmonixset-metrics:
	python3 tools/analyse_harmonixset_metrics.py

harmonixset-enrich:
	python3 tools/enrich_harmonixset_youtube_metadata.py

harmonixset-pack:
	python3 tools/build_esv11_benchmark_pack.py

harmonixset-all: harmonixset-catalogue harmonixset-metrics harmonixset-enrich
