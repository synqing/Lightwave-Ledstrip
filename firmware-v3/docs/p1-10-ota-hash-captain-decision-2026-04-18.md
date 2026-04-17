---
abstract: "Captain decision brief for forensic finding P1-10 (OTA has no SHA-256 / no signed-image verification). Documents the phase-1 SHA-256 streaming hardening that was shipped in sandbox on 2026-04-18, contrasts it with phase-2 ESP-IDF Secure Boot + Signed Apps, and lays out the recommended ship order. Read before deploying the hash-enforcement change — client deploys must be coordinated because the change is an intentional forced break (unsigned uploads rejected)."
---

# P1-10 — OTA integrity hardening: captain decision (2026-04-18)

## Summary

Forensic audit entry P1-10 flagged that OTA on K1 has no SHA-256 digest and no signed-image verification. MD5 was optional — the server silently accepted any firmware that passed the OTA token check. Because the K1 AP is an open network, the token is plaintext on air and any AP-range attacker could push arbitrary unsigned firmware. Partition rollback was the only defence against a bad image.

This brief documents the phase-1 hardening (SHA-256 streaming + mandatory integrity hash, shipped in the sandbox on 2026-04-18) and the phase-2 option (ESP-IDF Secure Boot v2 + Signed Apps), and recommends the ship order.

## Decision requested

Three choices, one must be picked:

1. **Ship phase-1 (SHA-256) immediately, defer phase-2.** Coordinated client deploy required.
2. **Ship both phase-1 and phase-2 together.** Adds a weeks-of-work signing toolchain prerequisite.
3. **Defer both until the next hardware revision.** Keep current plaintext-token-only flow.

Recommended: **option 1 (ship phase-1 now).** See reasoning below.

## Phase 1 — what was implemented

### Behaviour change (forced break from prior default-allow)

- `ota.begin` / `ota.verify` WS commands and the REST `/api/v1/firmware/update`, `/api/v1/firmware/filesystem`, `/update` endpoints now **require** either `sha256` (preferred) or `md5` (legacy, deprecated). Uploads with neither are rejected with `INVALID_VALUE`. This is the forensic audit fix — unsigned firmware uploads no longer silently succeed.
- SHA-256 is computed in the firmware using a streaming `mbedtls_sha256_context` that is allocated statically at file scope. **No heap allocation in the chunk path.** Digest is compared against the expected value before `Update.end(true)` runs, so a tampered image is never committed to the inactive partition.
- MD5 stays valid for one release as a legacy migration runway. When only `md5` is supplied, an `ota.*.md5_deprecated` telemetry line is emitted so clients that have not migrated can be identified. When both hashes are supplied, SHA-256 is authoritative and MD5 is ignored.
- Constant-time hex comparison for SHA-256 verification (prevents timing side channel when the attacker can observe reject-time).

### Surface change

**WebSocket — `ota.begin` request** gains one new field:
```jsonc
{
  "type": "ota.begin",
  "size": 1234567,
  "sha256": "<64 lowercase hex chars>",  // NEW, required (or legacy md5)
  "md5":    "<32 hex chars>",            // DEPRECATED, legacy only
  "token":  "<ota-token>",
  ...
}
```
`ota.verify` optionally accepts `sha256` again as a safety-net for streaming producers that compute the digest as they send; if supplied twice it must match.

**REST — new header `X-OTA-SHA256`** alongside the existing `X-OTA-MD5`. Same precedence: SHA-256 wins, MD5 is legacy.

### Cross-stack impact (FORCED BREAK)

The change is intentionally NOT backward-compatible at the default-allow boundary. Clients that send neither hash will be rejected. iOS, Tab5 and web dashboard must be deployed together with the firmware or their OTA updates will fail.

- **iOS (`lightwave-ios-v2`)**: swap the MD5 hex computation to SHA-256 (CryptoKit: `SHA256.hash(data:)`). Set `sha256` in the `ota.begin` WS payload.
- **Tab5 (`tab5-encoder`)**: same — if Tab5 pushes firmware to K1, it must send `sha256`.
- **Web dashboard (`lightwave-dashboard`)**: use `crypto.subtle.digest('SHA-256', ...)` on the firmware `ArrayBuffer`, send as `X-OTA-SHA256` header or `sha256` WS field.

## Phase 2 — what was NOT implemented (research notes only)

ESP-IDF provides `CONFIG_SECURE_BOOT` and `CONFIG_SECURE_SIGNED_APPS`. Enabling them gives tamper-proof firmware: the bootloader refuses to execute any image not signed by the configured ECDSA key, and secure-boot v2 cryptographically validates the signature before swap. SHA-256 (phase 1) proves the image wasn't corrupted in transit; secure-boot proves it was signed by us.

### What it requires

1. **Key material.** An ECDSA-256 secure-boot signing key (`.pem`), stored in HSM / offline signer. Loss of the key = bricked fleet.
2. **eFuse burn (one-way).** Secure-boot v2 burns a hash of the signing public key into eFuses. Once burnt, the device will only boot images signed by that key. **This is irreversible.** A wrong-key burn bricks the device permanently.
3. **Build pipeline changes.** CI must run `espsecure.py sign_data` on every firmware release. Nightly builds must be signed or they stop booting.
4. **Bootloader size increase.** Secure bootloader adds ~10 KB; verify partition table still fits in 4 MB flash layout (`partitions_4mb.csv`).
5. **Flash encryption decision.** Secure-boot is orthogonal to flash encryption. If flash encryption is also enabled, OTA partitions must be encrypted too — adds per-chunk overhead.
6. **No user-compiled firmware without key access.** Community builds would require either distributing the key (defeats purpose) or accepting that only signed official firmware runs. Project policy call.

### What breaks if we ship it

- Anyone without the signing key cannot flash the device. Tab5 and iOS uploads still work because they are uploading pre-signed images; the signing happens at CI, not on the client.
- Local development flashing (`pio run -t upload`) requires the developer to have the signing key available or a separate unsigned development firmware variant.
- Recovery from a lost signing key requires hardware replacement (eFuses are one-way).

### Recommendation on phase 2

Defer. Phase 1 closes the largest attack surface (on-air tamper of an unsigned image) at ~300 lines of firmware change and no key-management burden. Phase 2 is a separate project with hardware, CI, and governance prerequisites that justify their own design review.

## Ship recommendation

**Option 1: apply phase 1 now, coordinate client deploy.**

1. **Land this sandbox PR** onto `main` after review.
2. **Update `docs/protocol/k1-ws-contract.yaml` and `docs/protocol/k1-rest-contract.yaml`** (contract diffs included in the agent return; not applied to the real-tree files from sandbox).
3. **Client deploy fan-out (in this order):**
   1. iOS — ship a TestFlight build that sends `sha256`. Verify against a staging K1 running the new firmware.
   2. Tab5 — ship the firmware updater change.
   3. Web dashboard — ship the `crypto.subtle.digest` swap.
4. **Only after all three clients are out** — flash the new K1 firmware to the production fleet. Any client still on the pre-change code will error out cleanly on OTA (they will just continue running their existing firmware).
5. **Remove MD5 path** in the next release (one release runway). Log `md5_deprecated` telemetry tells us when to pull the trigger.

Phase 2 (secure boot) is a separate, later project once we have a signing-key governance policy agreed.

## Risks and open questions

- **Risk**: an in-flight OTA that was started against the old firmware but finishes against a client on the new firmware will fail the integrity check. Low probability (OTAs are short), but schedule the deploy window outside active updates.
- **Open**: should we add a `sha256_file` attribute to the release artefact manifest so clients don't need to hash on device? Minor UX win for web dashboard. Not blocking.
- **Open**: do we want to sign the release manifest itself (GPG / Sigstore)? Blocks CDN tampering. Phase 2 adjacent, out of scope here.

---
**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-04-18 | agent:network-api-engineer | Created — captain decision brief for P1-10 OTA integrity hardening. |
