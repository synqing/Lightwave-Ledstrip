# Ralph Agent Instructions

You are an autonomous coding agent working on the Lightwave-Ledstrip firmware project.

## Your Task

1. Read the PRD at `prd.json` (in the same directory as this file)
2. Read the progress log at `progress.txt` (check Codebase Patterns section first)
3. Check you're on the correct branch from PRD `branchName`. If not, check it out or create from main.
4. Pick the **highest priority** user story where `passes: false`
5. Implement that single user story
6. Run quality checks (see below)
7. Update CLAUDE.md files if you discover reusable patterns
8. If checks pass, commit ALL changes with message: `feat: [Story ID] - [Story Title]`
9. Update the PRD to set `passes: true` for the completed story
10. Append your progress to `progress.txt`

## Quality Requirements

Run these checks before committing:

### PlatformIO Builds
```bash
cd firmware/v2
pio run -e esp32dev_audio_esv11    # MUST compile - this is the auth-enabled build
pio run -e esp32dev_audio_esv11   # MUST compile - auth disabled here
```

### Critical Invariants
- **CENTER ORIGIN**: Not applicable to this feature (network code)
- **NO RAINBOWS**: Not applicable to this feature (network code)
- **NO MALLOC IN RENDER**: Not applicable (network code runs on Core 0)

### Code Standards
- Follow existing code patterns in `src/network/`
- Use `LW_LOGI`, `LW_LOGW`, `LW_LOGE` for logging (from utils/Log.h)
- Use `ApiResponse::` helpers for REST responses
- Use existing `RateLimiter` patterns for rate limiting

ALL checks must pass. Do NOT commit code that doesn't compile.

## Project Context

### What Already Exists
- `checkAPIKey()` in WebServer.cpp - REST auth implementation
- WebSocket auth handler in WsGateway setup - checks for `auth` message type
- `m_authenticatedClients` set - tracks authenticated WS clients
- `ApiResponse::UNAUTHORIZED` - 401 error code
- `RateLimiter` - existing rate limiting infrastructure
- `WiFiCredentialManager` - example of NVS persistence pattern

### Key Files
- `src/network/WebServer.cpp` - Main server, auth integration point
- `src/network/WebServer.h` - Server class definition
- `src/config/network_config.h` - API_KEY_VALUE constant
- `src/config/features.h` - FEATURE_API_AUTH flag
- `src/network/webserver/V1ApiRoutes.cpp` - REST route registration
- `src/network/webserver/ws/WsCommandRouter.cpp` - WS command registration
- `data/index.html` - Web dashboard
- `data/app.js` - Dashboard JavaScript

### NVS Pattern (from WiFiCredentialManager)
```cpp
#include <Preferences.h>

Preferences prefs;
prefs.begin("namespace", false);  // false = read-write
String value = prefs.getString("key", "default");
prefs.putString("key", newValue);
prefs.end();
```

## Progress Report Format

APPEND to progress.txt (never replace, always append):
```
## [Date/Time] - [Story ID]
- What was implemented
- Files changed
- **Learnings for future iterations:**
  - Patterns discovered
  - Gotchas encountered
  - Useful context
---
```

## Stop Condition

After completing a user story, check if ALL stories have `passes: true`.

If ALL stories are complete and passing, reply with:
<promise>COMPLETE</promise>

If there are still stories with `passes: false`, end your response normally.

## Important

- Work on ONE story per iteration
- Commit frequently
- Keep builds green
- Read the Codebase Patterns section in progress.txt before starting
