RBDO label: GROUNDED

Audited source areas:
- firmware-v3/src/core/songaware/SongAwareDirector.h
- firmware-v3/src/core/songaware/SongAwareDirector.cpp
- firmware-v3/src/core/actors/RendererActor.h
- firmware-v3/src/core/actors/RendererActor.cpp
- firmware-v3/src/core/actors/ShowDirectorActor.cpp
- firmware-v3/src/serial/SerialCLI.cpp
- firmware-v3/src/serial/SerialJsonGateway.cpp
- firmware-v3/src/network/WebServer.cpp
- firmware-v3/src/network/webserver/V1ApiRoutes.cpp
- firmware-v3/src/network/webserver/handlers/SongAwareHandlers.cpp
- firmware-v3/src/network/webserver/ws/WsSongAwareCommands.cpp
- docs/protocol/k1-rest-contract.yaml
- docs/protocol/k1-ws-contract.yaml
- firmware-v3/src/config/effect_ids.h
- firmware-v3/src/effects/CoreEffects.cpp
- firmware-v3/test/test_song_aware_director/test_song_aware_director.cpp

Findings:
- Existing parameter mode was not sufficient.
- Effect IDs for selected allowlist exist in registry source.
- Renderer integration point exists outside pixel/effect render loops and can run at low-rate Director cadence.
- Existing transition path was left intact; Director uses Director-specific queue/telemetry handling.
