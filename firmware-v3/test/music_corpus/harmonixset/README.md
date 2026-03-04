# Harmonixset Catalogue

Generated from:

- `/Users/spectrasynq/Workspace_Management/Software/K1.reinvented/Implementation.plans/harmonixset-main/audio`

Build command:

```bash
cd firmware-v3
make harmonixset-catalogue
make harmonixset-metrics
make harmonixset-enrich
make harmonixset-pack
```

Files:

- `harmonixset_catalogue.csv`  
  Full file-level metadata for all discovered `.mp3` and `.wav` assets.
- `harmonixset_catalogue.json`  
  JSON version of the full catalogue.
- `harmonixset_preferred.csv`  
  One preferred variant per `source_id` (for example, WAV over MP3 when available).
- `harmonixset_test_candidates.csv`  
  Preferred subset constrained to practical test lengths (`30s` to `360s`).
- `harmonixset_summary.json`  
  Aggregate counts (extensions, duration buckets, sample rates, totals).
- `harmonixset_metrics.csv`  
  Per-track signal and tempo metrics from decoded audio excerpts.
- `harmonixset_challenge_pack_120.csv`  
  Hard tracks (low confidence/high ambiguity) for stress testing.
- `harmonixset_reference_pack_120.csv`  
  Cleaner tracks for baseline and regression checks.
- `harmonixset_balanced_pack_120.csv`  
  BPM-bucket balanced subset for broad coverage.
- `harmonixset_youtube_cache.json`  
  Cached YouTube scrape results by `source_id`.
- `harmonixset_preferred_enriched.csv`  
  Preferred manifest augmented with title/channel/upload/tags.
- `harmonixset_test_candidates_enriched.csv`  
  Candidate manifest augmented with YouTube metadata.
- `harmonixset_enriched_summary.json`  
  Scrape coverage totals (cache hits, fetches, success/error counts).
- `esv11_benchmark/manifest.tsv`  
  Stratified benchmark manifest for ESV11 regression (track id + paths for 12.8k/32k WAV assets).
- `esv11_benchmark/summary.json`  
  Pack composition summary (requested size, bucket counts, clip duration).
- `esv11_benchmark/audio_12k8/*.wav`  
  Mono 12.8kHz benchmark assets.
- `esv11_benchmark/audio_32k/*.wav`  
  Mono 32kHz benchmark assets.

Notes:

- The source corpus is mostly YouTube-ID filenames with no embedded title/genre/BPM tags.
- The catalogue sorts by technical metadata (duration/sample-rate/format), not musical tempo.
- BPM-aware sorting and pack generation are provided by `make harmonixset-metrics`.
- YouTube enrichment is cached, so repeated `make harmonixset-enrich` runs only fetch missing IDs.
- ESV11 corpus baselines are captured via:
  - `make esv11-benchmark-capture`
  - then validated in CI/local with `make esv11-benchmark-test`.
