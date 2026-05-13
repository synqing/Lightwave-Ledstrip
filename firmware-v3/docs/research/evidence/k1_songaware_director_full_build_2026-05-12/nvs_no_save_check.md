RBDO label: GROUNDED

NVS boundary:
- No SongAware persistence/default enablement added.
- `songaware restore` dispatches runtime baseline controls only.
- No NVS save path is called by SongAware core, REST/WS SongAware handlers, or WS SongAware commands.

Source check command:
- `rg -n "saveToNVS|Preferences|nvs|NVS|put[A-Z]|put\\(" firmware-v3/src/core/songaware firmware-v3/src/network/webserver/handlers/SongAwareHandlers.cpp firmware-v3/src/network/webserver/ws/WsSongAwareCommands.cpp -S`

Result:
- no matches.

Existing unrelated serial commands still mention NVS, but they are not SongAware restore/reset/status paths.
