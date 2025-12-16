# LightwaveOS Scriptable Effects Platform: Technical Plan

## 1. Introduction & Rationale
To empower users and developers to create, test, and share custom light show patterns, transitions, and motion mechanics, LightwaveOS will support a scriptable effects platform. This will enable rapid prototyping, community-driven innovation, and advanced customization without requiring firmware recompilation.

---

## 2. Requirements
- **Low resource usage:** Must run on ESP32-S3 with limited RAM/flash.
- **Performance:** Real-time execution for 100+ FPS LED updates.
- **Safety:** Scripts must be sandboxed to prevent system compromise.
- **Extensibility:** Easy to add new effect types and parameters.
- **Developer Experience:** Simple API, live reload, and debugging support.
- **Web Integration:** Upload, edit, and preview scripts via browser.

---

## 3. Candidate Scripting Languages

### 3.1 Lua
- Lightweight, fast, and designed for embedding.
- Mature C/C++ API, easy sandboxing.
- Used in NodeMCU, ESP8266/ESP32 Lua projects.

### 3.2 JavaScript (Duktape, JerryScript, Espruino)
- Familiar to web developers.
- Large ecosystem, easy web integration.
- Heavier than Lua, but manageable on ESP32-S3.

### 3.3 MicroPython/CircuitPython
- Very readable, huge community.
- MicroPython runs on ESP32, but higher RAM/flash usage.
- Slower than Lua/JS for tight loops.

### 3.4 Custom Domain-Specific Language (DSL)
- Tailored for LED effects, minimal footprint.
- Maximum safety and performance.
- Requires custom parser, new learning curve for users.

### 3.5 WebAssembly (WASM)
- Multi-language support (C, Rust, etc.).
- Near-native speed, strong sandboxing.
- Large runtime, best for future/advanced hardware.

---

## 4. Evaluation Criteria
| Language   | Footprint | Performance | Familiarity | Sandbox/Safety | Ecosystem | Best For                |
|------------|-----------|-------------|-------------|---------------|-----------|-------------------------|
| Lua        | Tiny      | High        | Medium      | Excellent      | Good      | Embedded, safe, fast    |
| JavaScript | Medium    | Medium      | Excellent   | Good           | Excellent | Web, rapid prototyping  |
| Python     | Large     | Medium      | Excellent   | Good           | Excellent | Education, prototyping  |
| Custom DSL | Tiny      | Highest     | Low         | Excellent      | None      | Specialized, secure     |
| WASM       | Large     | Highest     | Medium      | Excellent      | Growing   | Advanced, future-proof  |

---

## 5. Integration Architecture
- **Script Engine:** Embedded interpreter (Lua, JS, etc.) runs user scripts.
- **API Exposure:** Scripts access LED buffers, parameters, and timing via a controlled API.
- **Web UI:** Users upload/edit scripts, preview effects, and manage libraries.
- **Live Reload:** Scripts can be updated and reloaded without rebooting the device.
- **Simulation:** In-browser preview for rapid iteration.

---

## 6. Sandboxing & Security
- Restrict script access to only allowed APIs (no raw hardware, file system, or network access).
- Limit memory and execution time per script.
- Validate scripts before execution.
- Optionally, sign scripts for trusted deployment.

---

## 7. Developer Experience
- Simple, well-documented API for effect development.
- Example scripts and templates.
- Web-based editor with syntax highlighting and error reporting.
- Community sharing and rating (future phase).

---

## 8. Recommendation
- **Lua** is recommended for initial implementation due to its minimal footprint, speed, and proven embedded use.
- **JavaScript** is a strong alternative if web developer engagement is a top priority and resources allow.
- **MicroPython** is best for education or if Python is a community standard.
- **Custom DSL** or **WASM** can be considered for future, advanced phases.

---

## 9. Next Steps
1. Prototype Lua and JavaScript interpreters on ESP32-S3, benchmark performance and memory.
2. Design the script API (LED buffer access, timing, parameters).
3. Extend the web UI for script upload, editing, and preview.
4. Implement sandboxing and error handling.
5. Document the API and provide example scripts.
6. Gather user feedback and iterate.

---

For questions or contributions, contact the LightwaveOS maintainers. 