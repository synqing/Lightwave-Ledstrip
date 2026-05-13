# K1v2 SRAM/PSRAM Reclaim Run - 2026-05-06

## RBDO Status

GROUNDED for startup documents, handoff authority, git state, baseline/post-patch build evidence, symbol deltas, K1v2 upload, and serial memory/status/effect-switch checks recorded below.

## Scope

Recover K1v2 internal DRAM/SRAM headroom by moving or eliminating safe cold/control-path allocations first, without lowering heap guards, changing WiFi mode, touching render hot paths, or staging unrelated dirty files.

Authority: `BACKLOG.md` Performance -> "K1v2 SRAM/PSRAM reclaim pass" and `firmware-v3/docs/research/k1v2_sram_psram_reclaim_handoff_2026-05-06.md`.

## Plan

- [x] Read mandatory startup and authority documents.
- [x] Record current git state and verify handoff commits.
- [x] Build current K1v2 production baseline.
- [x] Capture baseline ELF symbol-size list.
- [x] Inspect Batch A cold/control-path candidates.
- [x] Patch only the lowest-risk Batch A candidates.
- [x] Rebuild and compare RAM/symbol deltas.
- [x] Upload to K1v2 after MAC verification and run serial memory/status/effect-switch checks.
- [x] Commit only if hardware target is met and unrelated dirty files remain untouched.

## Findings

- Startup dirty tree matches the handoff caution: unrelated edits exist in root governance/docs files and one untracked changelog.
- Current HEAD is `7d7d9bf5 docs(firmware-v3): hand off k1v2 sram reclaim pass`; previous relevant heap commit is `63a4b392 fix(firmware-v3): restore k1v2 heap headroom`.
- `$RECALL_CLI` is not exported in this Codex shell; repo memory was checked via the local Codex memory file instead.
- clangd MCP tools did not surface through tool discovery; exact source-file reads remain available, but symbol navigation will stop if required.
- Attempting to create `.codex/` failed with `Operation not permitted`; this run report is the checkpoint surface instead.
- Baseline build passed for `esp32dev_audio_esv11_k1v2_32khz`: RAM `136084 / 327680` bytes (`41.5%`), flash `2459377 / 7340032` bytes (`33.5%`).
- Baseline Batch A symbol guard failed as expected: `CaptureStreamer` singleton `3024` B, static launcher buffer `3072` B, WS handler table `2304` B, builtin effect entries `2048` B.
- Post-patch build passed for `esp32dev_audio_esv11_k1v2_32khz`: RAM `125748 / 327680` bytes (`38.4%`), flash `2456937 / 7340032` bytes (`33.5%`).
- Static internal RAM reduction from PlatformIO summary: `10336` B.
- Post-patch Batch A symbol guard passed: `CaptureStreamer` singleton `84` B, static launcher buffer `0` B, WS handler table pointer `4` B, builtin effect entries pointer `4` B.
- K1v2 MAC verification and upload both reported `b4:3a:45:a5:87:f8` on `/dev/cu.usbmodem2101`.
- Hardware boot memory after upload: free internal heap `28088` B, min free `26160` B, max alloc `18420` B.
- Effect-switch smoke accepted `0x2103`, `0x0100`, and `0x2102`; post-switch memory was free internal heap `27940` B, min free `26160` B, max alloc `18420` B.
- Serial status after switch to `0x2102`: `showSkips=0`, LED show avg `6168` us, no panics or RMT errors observed in captured serial output.

## Baseline Evidence

- Build command: `pio run -e esp32dev_audio_esv11_k1v2_32khz`.
- ELF command: `xtensa-esp32s3-elf-nm -S --size-sort .pio/build/esp32dev_audio_esv11_k1v2_32khz/firmware.elf`.
- Batch A test command: `python3 scripts/check_k1v2_batch_a_sram_reclaim.py .pio/build/esp32dev_audio_esv11_k1v2_32khz/firmware.elf`.

## Batch A Candidate Notes

- `CaptureStreamer`: internal object was dominated by `m_frameBufFallback[1024]`, `m_dumpFrameFallback[320]`, and `m_taskFrameBuf[320]`; patch removes permanent fallbacks and fails capture commands cleanly if PSRAM scratch is unavailable.
- `StaticAssetRoutes`: root-page `static char buf[3072]` was a cold HTTP path; patch allocates a persistent buffer on first request, preferring PSRAM.
- `WsCommandRouter`: `s_handlers[MAX_HANDLERS]` consumed `2304` B internal BSS; patch changes it to one-time allocated table storage before registration.
- `BuiltinEffectRegistry`: `s_entries[MAX_EFFECTS]` consumed `2048` B internal BSS; patch changes it to one-time allocated table storage before registration.

## Verification

- `pio run -e esp32dev_audio_esv11_k1v2_32khz` -> PASS.
- `python3 scripts/check_k1v2_batch_a_sram_reclaim.py .pio/build/esp32dev_audio_esv11_k1v2_32khz/firmware.elf` -> PASS after patch; FAIL on baseline before production edits.
- `pio run -e native_test_ws_router` -> PASS.
- `.pio/build/native_test_ws_router/program` -> PASS, `14 Tests 0 Failures 0 Ignored`.
- `~/.platformio/penv/bin/python -m esptool --chip esp32s3 --port /dev/cu.usbmodem2101 read_mac` -> MAC `b4:3a:45:a5:87:f8`.
- `pio run -e esp32dev_audio_esv11_k1v2_32khz -t upload --upload-port /dev/cu.usbmodem2101` -> PASS.
- Serial commands: `dbg memory`, `s`, `effect 0x2103`, `effect 0x0100`, `effect 0x2102`, `dbg memory`, `s` -> PASS against the completion target.

## Errors Encountered

| Error | Attempt | Resolution |
|-------|---------|------------|
| `.codex/` creation failed with `Operation not permitted` | Create project-local planning checkpoint under `.codex/` | Use this existing `firmware-v3/docs/research/` run report as the on-disk checkpoint. |
