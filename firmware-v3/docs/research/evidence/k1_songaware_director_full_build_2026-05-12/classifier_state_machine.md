RBDO label: GROUNDED

Implemented coarse states:
- silence
- ambient
- steady
- build
- drop
- breakdown
- dense
- transition
- unknown

Telemetry fields exposed:
- rawSongState
- previousSongState
- currentSongState
- candidateSongState
- confidence
- stateAgeMs
- candidateAgeMs
- candidateHoldRemainingMs
- classificationReason

Runtime observed states:
- drop: `classificationReason=drop_onset`
- dense: `classificationReason=dense_energy`
- build: Director transition reason `build_pressure`
- silence/low confidence suppression after playback stop.

Native tests:
- `test_song_aware_classifier_reports_each_director_state` passed.
