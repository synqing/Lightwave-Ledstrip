#!/usr/bin/env python3
"""
Deterministic Canon Pack phase compilers for claude-mem.

Pipeline:
1) Build stitched evidence candidates (session-local context windows for short rows).
2) Compile phase citation tables:
   - Alias Map
   - Constitution
   - Product Spec
   - AS-IS Architecture Atlas
   - Decision Register
3) Evaluate Canon Pack readiness gates and emit pass/fail report.

Design constraints:
- Read-only DB access.
- Primary evidence drives claims; supplemental evidence is optional context only.
- Claims never bypass citation gates.
- Missing support is rendered as explicit "unknown"/"no supporting excerpts".
"""

from __future__ import annotations

import argparse
import json
import re
import sqlite3
from collections import Counter, defaultdict
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

UTC = timezone.utc

DEFAULT_PRIMARY_PROJECTS = [
    "Lightwave-Ledstrip",
    "Tab5.DSP",
    "v2",
    "PRISM.k1",
    "K1.reinvented",
    "lightwave-controller",
    "K1.juce",
    "K1.Sliders",
]

DEFAULT_SUPPLEMENTAL_PROJECTS = ["cursor", "codex"]

ALIAS_SEEDS: Dict[str, List[str]] = {
    "SpectraSynq": ["spectrasynq"],
    "K1-Lightwave": ["k1-lightwave", "k1 lightwave", "lightwave-ledstrip", "lightwave ledstrip", "lightwaveos"],
    "Tab5 Hub": ["tab5", "tab5.encoder", "encoder device", "hub"],
    "PRISM": ["prism", "prism.k1", "prism.unified"],
    "Trinity": ["trinity", "spectrasynq.trinity"],
    "v2 Firmware": ["v2", "firmware v2", "lightwaveos v2"],
}

DOMAIN_TAGS: Dict[str, List[str]] = {
    "OPTICS/LGP": ["lgp", "optics", "diffuser", "edge-lit", "edge lit", "volumetric", "viscous", "light guide"],
    "LEDS/POWER": ["ws2812", "led", "brightness", "power", "current", "voltage", "strip", "connector"],
    "AUDIO/DSP": ["audio", "dsp", "beat", "flux", "chroma", "fft", "hop", "tempo", "clock"],
    "RENDER/FX": ["render", "effect", "pattern", "palette", "bloom", "centre-origin", "center-origin"],
    "CONTROL/NETWORK": ["network", "ap", "sta", "ip", "mdns", "websocket", "rest", "api", "provision"],
    "UI/UX": ["ios", "tab5", "dashboard", "ui", "ux", "preset", "control surface"],
    "PRODUCTION/GTM": ["kickstarter", "pricing", "beta", "manufacturing", "launch", "gtm"],
}

NOISE_PATTERNS = (
    "## my request for codex",
    "# context from my ide setup",
    "<task-notification>",
    "[execute_command",
    "[read_file for",
    "[plan_mode_response]",
    "command executed",
    "i see the issue with permissions",
)

DECISION_SIGNALS = (
    "we decided",
    "decision",
    "we will",
    "lock in",
    "non-negotiable",
    "must",
    "must not",
    "never",
    "acceptance",
    "budget",
    "target",
    "regression",
    "replace",
    "superseded",
    "deprecated",
)
DECISION_CORE_SIGNALS = (
    "we decided",
    "decided",
    "we choose",
    "we chose",
    "selected",
    "lock in",
    "non-negotiable",
    "must",
    "must not",
    "never",
)
DECISION_REASON_SIGNALS = (
    "we decided",
    "decided",
    "we chose",
    "lock in",
)

SUPERSEDED_SIGNALS = ("superseded", "replaced by", "deprecated", "rollback", "reverted")
CONTEST_SIGNALS = ("contradict", "conflict", "inconsistent", "not true", "no longer")
ASPIRATIONAL_SIGNALS = ("plan", "planned", "proposal", "should", "could", "would", "aspirational", "future")
SUPERSEDED_STRONG_RE = re.compile(
    r"\b(?:superseded by|replaced by|deprecated in favour of|deprecated in favor of|rolled back to|reverted to)\b",
    flags=re.IGNORECASE,
)

DECISION_THEME_SPECS = [
    {
        "id": "decision_centre_origin",
        "statement": "Centre-origin propagation from LEDs 79/80 is locked as a rendering invariant.",
        "patterns": ["79/80", "centre origin", "center origin", "no linear sweep"],
        "min_hits": 1,
    },
    {
        "id": "decision_no_rainbow",
        "statement": "Rainbow cycling and full hue-wheel sweeps are intentionally excluded.",
        "patterns": ["no rainbow", "rainbow cycling", "hue-wheel", "hue wheel"],
        "min_hits": 1,
    },
    {
        "id": "decision_psram_required",
        "statement": "PSRAM-enabled builds are required to avoid runtime memory failures.",
        "patterns": ["psram mandatory", "board_has_psram", "8mb psram", "esp_err_no_mem"],
        "min_hits": 1,
    },
    {
        "id": "decision_120fps_budget",
        "statement": "Renderer cadence and effect code are budgeted around 120 FPS operation.",
        "patterns": ["120 fps", "8.33ms", "frame budget", "2 ms"],
        "min_hits": 2,
    },
    {
        "id": "decision_no_heap_render",
        "statement": "Render-path heap allocation is prohibited in favour of static buffers.",
        "patterns": ["no heap alloc in render", "no malloc", "no new", "static buffer"],
        "min_hits": 1,
    },
    {
        "id": "decision_control_plane_rest_ws",
        "statement": "Device control contracts use REST for commands and WebSocket for streaming/state.",
        "patterns": ["rest", "websocket", "subscribe", "endpoint", "api"],
        "min_hits": 2,
    },
    {
        "id": "decision_ap_sta_fallback",
        "statement": "Connectivity strategy keeps AP/STA fallback behaviour for resilience.",
        "patterns": ["ap mode", "sta", "fallback", "wifi state", "reconnect"],
        "min_hits": 2,
    },
    {
        "id": "decision_ip_first_discovery",
        "statement": "IP-first discovery with mDNS as hint/fallback is favoured for deterministic connection.",
        "patterns": ["ip-first", "mdns", "manual ip", "discovery", "fallback"],
        "min_hits": 2,
    },
    {
        "id": "decision_actor_state_ownership",
        "statement": "Actor boundaries and explicit state ownership are the primary concurrency model.",
        "patterns": ["actor", "state ownership", "orchestrator", "thread-safe state"],
        "min_hits": 2,
    },
    {
        "id": "decision_render_priority_budget",
        "statement": "RendererActor scheduling and buffer ownership decisions are locked for Core 1 execution.",
        "patterns": ["RendererActor", "frame budget", "Core 1", "led buffer", "priority", "render loop"],
        "min_hits": 2,
    },
    {
        "id": "decision_zone_boundaries",
        "statement": "Zone/segment boundaries are fixed with explicit composer/state contracts.",
        "patterns": ["ZoneComposer", "zones", "segment", "boundaries", "state contract", "composer"],
        "min_hits": 2,
    },
    {
        "id": "decision_watchdog_guardrails",
        "statement": "Watchdog-related timing guardrails are treated as mandatory stability constraints.",
        "patterns": ["watchdog", "timeout", "starvation", "frame budget", "yield"],
        "min_hits": 2,
    },
    {
        "id": "decision_limits_single_source",
        "statement": "System limits are maintained via single-source constants with validation checks.",
        "patterns": ["single source of truth", "limits.h", "static assertion", "max_effects"],
        "min_hits": 2,
    },
    {
        "id": "decision_hardware_modularity",
        "statement": "Hardware topology and modularity boundaries separate device, hub, and tooling responsibilities.",
        "patterns": ["k1", "lightwave", "modular", "bar", "base station", "hub", "Tab5", "node", "segments"],
        "min_hits": 2,
    },
    {
        "id": "decision_led_topology_constraints",
        "statement": "LED silicon choice and dual-channel brightness caps are locked for current HW.",
        "patterns": ["ws2812", "apa102", "hd108", "2020", "1515", "dual channel", "brightness cap", "power budget", "current"],
        "min_hits": 2,
    },
    {
        "id": "decision_audio_capture_chain",
        "statement": "Audio capture chain decisions nail mic/interface selection and hop/latency budgets.",
        "patterns": ["sph0645", "im69d130", "i2s", "pdm", "es7210", "adau7002", "hop", "8ms", "sample rate", "latency"],
        "min_hits": 2,
    },
    {
        "id": "decision_control_plane_portable_mode",
        "statement": "Control plane choices keep AP/STA, IP-first discovery, and portable-mode recoveries deterministic.",
        "patterns": ["softap", "sta", "portable mode", "ip-first", "mdns", "discovery", "fallback", "websocket"],
        "min_hits": 2,
    },
    {
        "id": "decision_storage_persistence",
        "statement": "Storage/persistence decisions lock in LittleFS and recovery paths.",
        "patterns": ["littlefs", "filesystem", "persist", "repair", "storage", "format", "UserDefaults"],
        "min_hits": 2,
    },
    {
        "id": "decision_palette_parity",
        "statement": "Palette/preset control decisions are enforced through API handlers with validation.",
        "patterns": ["palette", "preset", "EffectPresetHandler", "Bloom", "gHue", "color parity"],
        "min_hits": 2,
    },
]

STOPWORDS = {
    "the",
    "and",
    "for",
    "with",
    "that",
    "this",
    "from",
    "into",
    "onto",
    "over",
    "under",
    "within",
    "about",
    "their",
    "there",
    "which",
    "when",
    "where",
    "while",
    "have",
    "has",
    "had",
    "were",
    "was",
    "are",
    "is",
    "be",
    "been",
    "being",
    "will",
    "would",
    "should",
    "could",
    "must",
    "may",
    "might",
    "also",
    "not",
    "only",
    "very",
    "more",
    "most",
}

CONSTITUTION_SPECS = [
    {
        "id": "mission_audio_reactive",
        "statement": "The project mission is an audio-reactive LED controller experience with web/app control.",
        "type": "mission",
        "patterns": ["audio-reactive", "web-controlled", "led controller", "lightwaveos"],
        "conflicts": [],
    },
    {
        "id": "invariant_centre_origin",
        "statement": "Effects originate from centre LEDs 79/80 and propagate outward/inward, not linear sweeps.",
        "type": "invariant",
        "patterns": ["79/80", "centre origin", "center origin", "outward", "inward", "linear sweep"],
        "conflicts": ["left-to-right sweep", "linear sweep required"],
    },
    {
        "id": "invariant_no_linear_sweep",
        "statement": "Linear sweep rendering is rejected in favour of centre-origin motion semantics.",
        "type": "invariant",
        "patterns": ["no linear sweep", "linear sweep", "centre-origin", "center-origin"],
        "conflicts": ["left-to-right sweep required", "linear sweep required"],
    },
    {
        "id": "invariant_no_rainbow",
        "statement": "Rainbow cycling and full hue-wheel sweeps are disallowed.",
        "type": "invariant",
        "patterns": ["no rainbow", "rainbow cycling", "hue-wheel", "hue wheel"],
        "conflicts": ["full rainbow sweep", "enable rainbow cycle"],
    },
    {
        "id": "invariant_no_heap_render",
        "statement": "Render paths avoid dynamic heap allocation and use static buffers.",
        "type": "invariant",
        "patterns": ["no heap alloc in render", "no malloc", "no new", "static buffer", "render()"],
        "conflicts": ["malloc in render", "new in render"],
    },
    {
        "id": "invariant_static_buffers_render",
        "statement": "Fixed/static render buffers are a non-negotiable requirement for runtime stability.",
        "type": "invariant",
        "patterns": ["static buffer", "fixed array", "render buffer", "no heap alloc in render"],
        "conflicts": ["dynamic allocation in render", "malloc in render"],
    },
    {
        "id": "invariant_psram_required",
        "statement": "PSRAM is mandatory for runtime stability and memory budgets.",
        "type": "invariant",
        "patterns": ["psram mandatory", "board_has_psram", "8mb psram", "esp_err_no_mem"],
        "conflicts": ["without psram is fine", "psram optional"],
    },
    {
        "id": "invariant_120fps_budget",
        "statement": "The renderer targets 120 FPS and strict per-frame execution budgets.",
        "type": "invariant",
        "patterns": ["120 fps", "8.33ms", "frame budget", "2 ms"],
        "conflicts": [],
    },
    {
        "id": "invariant_watchdog_safety",
        "statement": "Watchdog-safe scheduling and timing behaviour are mandatory system guardrails.",
        "type": "invariant",
        "patterns": ["watchdog", "timeout", "starvation", "frame budget", "yield"],
        "conflicts": ["ignore watchdog", "watchdog disabled permanently"],
    },
    {
        "id": "invariant_rest_ws_contract",
        "statement": "REST plus WebSocket contracts are stable control-plane invariants for device control and streaming.",
        "type": "invariant",
        "patterns": ["rest", "websocket", "api", "subscribe", "endpoint"],
        "conflicts": ["remove websocket", "remove rest api"],
    },
    {
        "id": "invariant_ap_sta_resilience",
        "statement": "AP/STA fallback behaviour is preserved as a resilience invariant.",
        "type": "invariant",
        "patterns": ["ap mode", "sta", "fallback", "wifi state", "reconnect"],
        "conflicts": ["sta-only without fallback", "disable ap fallback"],
    },
    {
        "id": "invariant_ip_first_discovery",
        "statement": "Discovery prioritises deterministic IP paths with mDNS as hint/fallback.",
        "type": "invariant",
        "patterns": ["ip-first", "mdns", "manual ip", "discovery", "fallback"],
        "conflicts": ["mdns-only discovery", "no ip fallback"],
    },
    {
        "id": "invariant_actor_state_ownership",
        "statement": "Actor boundaries and explicit state ownership are maintained as concurrency invariants.",
        "type": "invariant",
        "patterns": ["actor", "state ownership", "orchestrator", "thread-safe state"],
        "conflicts": ["shared mutable global state", "state ownership unclear"],
    },
    {
        "id": "invariant_limits_single_source",
        "statement": "System limits are enforced through single-source constants with validation checks.",
        "type": "invariant",
        "patterns": ["single source of truth", "limits.h", "static assertion", "max_effects"],
        "conflicts": ["scattered hardcoded limits", "limit drift"],
    },
    {
        "id": "invariant_led_topology",
        "statement": "Dual-strip 320-LED WS2812 topology is a fixed product invariant.",
        "type": "invariant",
        "patterns": ["320 ws2812", "dual-strip", "dual strip", "320 leds", "ws2812"],
        "conflicts": ["single strip topology"],
    },
    {
        "id": "refusal_guessing",
        "statement": "Unproven assumptions are explicitly rejected in favour of evidence-backed decisions.",
        "type": "refusal",
        "patterns": ["no supporting excerpts found", "instead of guessing", "evidence-backed", "do not guess"],
        "conflicts": [],
    },
]

PRODUCT_SPEC_SPECS = [
    {
        "id": "product_hw_led_topology",
        "statement": "K1-Lightwave uses dual-strip LED topology with 320 WS2812 pixels.",
        "type": "spec",
        "patterns": ["320 ws2812", "dual-strip", "dual strip", "320 leds", "ws2812"],
    },
    {
        "id": "product_runtime_constraints",
        "statement": "Runtime constraints include PSRAM reliance, deterministic render cadence, and watchdog-safe execution.",
        "type": "spec",
        "patterns": ["psram", "watchdog", "frame budget", "render loop", "deterministic"],
    },
    {
        "id": "product_audio_visual_path",
        "statement": "Audio analysis features drive visual parameter mapping in the render pipeline.",
        "type": "spec",
        "patterns": ["audio", "flux", "tempo", "chroma", "render", "features"],
    },
    {
        "id": "product_control_interfaces",
        "statement": "Control surfaces include REST/WebSocket APIs and app/dashboard clients.",
        "type": "spec",
        "patterns": ["rest", "websocket", "ios", "dashboard", "api"],
    },
]

ARCHITECTURE_SPECS = [
    {
        "id": "arch_renderer_actor",
        "statement": "RendererActor is the core visual engine with exclusive frame ownership and high-frequency scheduling.",
        "type": "architecture",
        "dimension": "components",
        "patterns": ["rendereractor", "core visual engine", "exclusive", "led buffer", "core 1"],
    },
    {
        "id": "arch_audio_pipeline",
        "statement": "Audio pipeline computes analysis features (tempo/chroma/flux) and feeds render-time controls.",
        "type": "architecture",
        "dimension": "components",
        "patterns": ["audioactor", "tempo", "chroma", "flux", "backend", "analysis"],
    },
    {
        "id": "arch_state_ownership",
        "statement": "State ownership is actor-mediated with explicit boundaries between render, audio, and control layers.",
        "type": "architecture",
        "dimension": "state_ownership",
        "patterns": ["actor", "state ownership", "orchestrator", "owns", "thread-safe state"],
    },
    {
        "id": "arch_timing_model",
        "statement": "Timing model couples audio hop cadence with render cadence under deterministic frame budgets.",
        "type": "architecture",
        "dimension": "timing_model",
        "patterns": ["hop size", "cadence", "frame budget", "120 fps", "tick", "timing model"],
    },
    {
        "id": "arch_control_plane",
        "statement": "Control plane combines REST endpoints and WebSocket streams with fallback handling.",
        "type": "architecture",
        "dimension": "control_plane",
        "patterns": ["rest", "websocket", "subscribe", "endpoint", "stream", "fallback"],
    },
    {
        "id": "arch_network_modes",
        "statement": "Network behaviour includes AP/STA modes with discovery and IP fallback strategies.",
        "type": "architecture",
        "dimension": "boundaries",
        "patterns": ["ap mode", "sta", "mdns", "ip fallback", "discovery", "wifi state"],
    },
    {
        "id": "arch_client_boundary",
        "statement": "iOS/Tab5/dashboard clients are boundary consumers of firmware-owned state and contracts.",
        "type": "architecture",
        "dimension": "boundaries",
        "patterns": ["ios app", "tab5", "dashboard", "client", "websocketservice", "restclient"],
    },
]


@dataclass
class Observation:
    obs_id: int
    session_id: str
    project: str
    timestamp: str
    created_at_epoch: int
    obs_type: str
    text: str
    source_tier: str


@dataclass
class EvidenceCandidate:
    evidence_id: str
    source_obs_ids: List[int]
    session_id: str
    project: str
    timestamp: str
    obs_type: str
    text: str
    source_tier: str
    stitched: bool


def _clean_text(text: str) -> str:
    return re.sub(r"\s+", " ", text or "").strip()


def _parse_ts(ts: str) -> datetime:
    if not ts:
        return datetime(1970, 1, 1, tzinfo=UTC)
    s = ts.replace("Z", "+00:00")
    try:
        dt = datetime.fromisoformat(s)
        if dt.tzinfo is None:
            return dt.replace(tzinfo=UTC)
        return dt
    except ValueError:
        return datetime(1970, 1, 1, tzinfo=UTC)


def _ts_iso(dt: datetime) -> str:
    return dt.astimezone(UTC).isoformat().replace("+00:00", "Z")


def _normalise_key(text: str) -> str:
    t = _clean_text(text).lower()
    t = re.sub(r"[^a-z0-9\s]", " ", t)
    toks = [x for x in t.split() if len(x) > 2 and x not in STOPWORDS]
    return " ".join(sorted(set(toks[:18])))


def _first_sentence(text: str, max_len: int = 320) -> str:
    t = _clean_text(text)
    if not t:
        return ""
    parts = [p.strip() for p in re.split(r"(?<=[.!?])\s+", t) if p.strip()]
    for p in parts:
        if len(p) < 40:
            continue
        if len(p) <= max_len:
            return p
        return p[: max_len - 1].rstrip() + "…"
    if len(t) <= max_len:
        return t
    return t[: max_len - 1].rstrip() + "…"


def _contains_any(text: str, patterns: Sequence[str]) -> bool:
    t = text.lower()
    return any(p.lower() in t for p in patterns)


def _detect_domains(text: str) -> List[str]:
    t = text.lower()
    out: List[str] = []
    for domain, kws in DOMAIN_TAGS.items():
        for kw in kws:
            if len(kw) <= 3:
                if re.search(rf"\b{re.escape(kw)}\b", t):
                    out.append(domain)
                    break
            elif kw in t:
                out.append(domain)
                break
    return out


def _is_low_signal(text: str) -> bool:
    t = (text or "").strip().lower()
    if not t:
        return True
    if any(p in t for p in NOISE_PATTERNS):
        return True
    if len(t) < 50:
        return True
    alnum = sum(1 for c in t if c.isalnum())
    if alnum / max(len(t), 1) < 0.45:
        return True
    return False


def _has_explicit_identifier(text: str) -> bool:
    patterns = (
        r"\b[a-zA-Z0-9_\-/]+\.(?:cpp|h|hpp|c|md|ini|swift|json|py)\b",
        r"\b[A-Z_]{3,}\b",
        r"\b\d+(?:\.\d+)?\s?(?:ms|fps|hz|khz|kb|mb|gb)\b",
        r"\b(?:RendererActor|AudioActor|WebServer|WiFiManager|ZoneComposer)\b",
        r"\b(?:/api/[a-z0-9_/.-]+)\b",
    )
    return any(re.search(p, text) for p in patterns)


def _citation_quality(text: str, min_chars: int) -> bool:
    return len(_clean_text(text)) >= min_chars or _has_explicit_identifier(text)


def _load_observations(
    con: sqlite3.Connection,
    primary_projects: Sequence[str],
    supplemental_projects: Sequence[str],
) -> List[Observation]:
    projects = list(primary_projects) + list(supplemental_projects)
    placeholders = ",".join(["?"] * len(projects))
    q = (
        "SELECT id, memory_session_id, project, created_at, created_at_epoch, type, title, subtitle, narrative, text, facts "
        "FROM observations "
        f"WHERE project IN ({placeholders}) "
        "ORDER BY created_at_epoch ASC, id ASC"
    )
    rows = con.execute(q, projects).fetchall()
    out: List[Observation] = []
    primary_set = set(primary_projects)
    for row in rows:
        rid, session_id, project, created_at, created_at_epoch, obs_type, title, subtitle, narrative, text, facts = row
        joined = " ".join(
            x for x in [title or "", subtitle or "", narrative or "", text or "", facts or ""] if x and x.strip()
        )
        joined = _clean_text(joined)
        if _is_low_signal(joined):
            continue
        tier = "primary" if project in primary_set else "supplemental"
        out.append(
            Observation(
                obs_id=int(rid),
                session_id=session_id or "",
                project=project or "unknown",
                timestamp=created_at or "",
                created_at_epoch=int(created_at_epoch or 0),
                obs_type=obs_type or "",
                text=joined,
                source_tier=tier,
            )
        )
    return out


def _stitch_candidates(
    observations: Sequence[Observation],
    short_threshold: int,
    stitch_window: int,
) -> List[EvidenceCandidate]:
    by_session: Dict[str, List[Observation]] = defaultdict(list)
    for obs in observations:
        by_session[obs.session_id or f"obs-{obs.obs_id}"].append(obs)

    out: List[EvidenceCandidate] = []
    for _, session_rows in by_session.items():
        rows = sorted(session_rows, key=lambda r: (r.created_at_epoch, r.obs_id))
        for idx, row in enumerate(rows):
            stitched = False
            source_ids = [row.obs_id]
            text = row.text
            if len(_clean_text(row.text)) < short_threshold:
                start = max(0, idx - stitch_window)
                end = min(len(rows), idx + stitch_window + 1)
                chunk_rows = rows[start:end]
                parts: List[str] = []
                seen = set()
                source_ids = []
                for item in chunk_rows:
                    norm = _normalise_key(item.text)
                    if not norm or norm in seen:
                        continue
                    seen.add(norm)
                    parts.append(item.text)
                    source_ids.append(item.obs_id)
                if parts:
                    text = _clean_text(" ".join(parts))
                    stitched = True
            evidence_id = f"O{row.obs_id}"
            if stitched:
                evidence_id = "S" + "-".join(str(x) for x in source_ids[:4])
            out.append(
                EvidenceCandidate(
                    evidence_id=evidence_id,
                    source_obs_ids=source_ids,
                    session_id=row.session_id,
                    project=row.project,
                    timestamp=row.timestamp,
                    obs_type=row.obs_type,
                    text=text,
                    source_tier=row.source_tier,
                    stitched=stitched,
                )
            )
    return out


def _dedupe_candidates(candidates: Sequence[EvidenceCandidate]) -> List[EvidenceCandidate]:
    out: List[EvidenceCandidate] = []
    seen = set()
    for c in candidates:
        key = _normalise_key(_first_sentence(c.text))
        if not key or key in seen:
            continue
        seen.add(key)
        out.append(c)
    return out


def _citation_from_candidate(c: EvidenceCandidate, min_chars: int) -> Dict[str, object]:
    excerpt = _first_sentence(c.text)
    return {
        "evidence_id": c.evidence_id,
        "source_obs_ids": c.source_obs_ids,
        "project": c.project,
        "timestamp": c.timestamp,
        "session_id": c.session_id,
        "source_tier": c.source_tier,
        "stitched": c.stitched,
        "excerpt": excerpt,
        "excerpt_chars": len(excerpt),
        "explicit_identifier": _has_explicit_identifier(excerpt),
        "citation_quality_pass": _citation_quality(excerpt, min_chars),
        "domain_tags": _detect_domains(excerpt),
    }


def _claim_status_from_conflicts(
    support: Sequence[EvidenceCandidate],
    conflict_patterns: Sequence[str],
) -> Tuple[str, List[EvidenceCandidate]]:
    if not conflict_patterns:
        if any(_contains_any(c.text, SUPERSEDED_SIGNALS) for c in support):
            return "superseded", []
        return "current", []
    conflicts = [c for c in support if _contains_any(c.text, conflict_patterns)]
    if conflicts:
        if any(_contains_any(c.text, SUPERSEDED_SIGNALS) for c in support):
            return "superseded", conflicts
        return "contested", conflicts
    if any(_contains_any(c.text, SUPERSEDED_SIGNALS) for c in support):
        return "superseded", []
    return "current", []


def _confidence(primary_count: int, session_count: int) -> str:
    if primary_count >= 4 and session_count >= 2:
        return "high"
    if primary_count >= 2:
        return "medium"
    return "low"


def _range_from_candidates(cands: Sequence[EvidenceCandidate]) -> Tuple[str, str]:
    if not cands:
        return "", ""
    dts = sorted(_parse_ts(c.timestamp) for c in cands)
    return _ts_iso(dts[0]), _ts_iso(dts[-1])


def _compile_template_phase(
    phase_name: str,
    specs: Sequence[Dict[str, object]],
    candidates: Sequence[EvidenceCandidate],
    min_chars: int,
    min_primary_citations: int,
) -> Dict[str, object]:
    rows = []
    for spec in specs:
        patterns = spec["patterns"]
        support = [c for c in candidates if _contains_any(c.text, patterns)]
        support = _dedupe_candidates(support)
        support_sorted = sorted(support, key=lambda c: (_parse_ts(c.timestamp), c.evidence_id), reverse=True)
        primary_support = [c for c in support_sorted if c.source_tier == "primary"]
        primary_support = [c for c in primary_support if _citation_quality(_first_sentence(c.text), min_chars)]
        supplemental_support = [c for c in support_sorted if c.source_tier == "supplemental"]
        sessions = {c.session_id for c in primary_support if c.session_id}
        status, conflicts = _claim_status_from_conflicts(support_sorted, spec.get("conflicts", []))
        first_seen, last_seen = _range_from_candidates(primary_support)
        include = len(primary_support) >= min_primary_citations

        row = {
            "claim_id": spec["id"],
            "statement": spec["statement"],
            "phase": phase_name,
            "type": spec["type"],
            "status": status,
            "confidence": _confidence(len(primary_support), len(sessions)),
            "first_seen": first_seen,
            "last_seen": last_seen,
            "primary_citation_count": len(primary_support),
            "supplemental_citation_count": len(supplemental_support),
            "session_count": len(sessions),
            "include_in_phase_doc": include,
            "dimension": spec.get("dimension", ""),
            "citations": [_citation_from_candidate(c, min_chars) for c in primary_support[:10]],
            "supplemental_citations": [_citation_from_candidate(c, min_chars) for c in supplemental_support[:5]],
            "conflict_citations": [_citation_from_candidate(c, min_chars) for c in conflicts[:5]],
        }
        rows.append(row)

    included = [r for r in rows if r["include_in_phase_doc"]]
    return {
        "phase": phase_name,
        "claim_rows": rows,
        "included_claims": included,
        "excluded_claims": [r["claim_id"] for r in rows if not r["include_in_phase_doc"]],
    }


def _extract_alias_candidates(text: str) -> List[str]:
    out = []
    patterns = [
        r"(?:aka|also called|known as|formerly|renamed to)\s+([A-Za-z0-9_.-]+(?:\s+[A-Za-z0-9_.-]+){0,2})",
    ]
    for pat in patterns:
        for m in re.finditer(pat, text, flags=re.IGNORECASE):
            alias = _clean_text(m.group(1))
            if 2 <= len(alias) <= 40:
                out.append(alias)
    return out


def _compile_alias_map(
    candidates: Sequence[EvidenceCandidate],
    min_chars: int,
) -> Dict[str, object]:
    entries = []
    for concept, seeds in ALIAS_SEEDS.items():
        support = [c for c in candidates if _contains_any(c.text, seeds)]
        support = _dedupe_candidates(support)
        support = sorted(support, key=lambda c: (_parse_ts(c.timestamp), c.evidence_id), reverse=True)
        primary = [c for c in support if c.source_tier == "primary"]
        primary = [c for c in primary if _citation_quality(_first_sentence(c.text), min_chars)]
        discovered = set(seeds)
        for c in support[:40]:
            for alias in _extract_alias_candidates(c.text):
                discovered.add(alias.lower())
        aliases = sorted(discovered)
        first_seen, last_seen = _range_from_candidates(primary)
        entry = {
            "concept": concept,
            "aliases": aliases,
            "first_seen": first_seen,
            "last_seen": last_seen,
            "status": "current" if primary else "unknown",
            "citation_count": len(primary),
            "citations": [_citation_from_candidate(c, min_chars) for c in primary[:12]],
            "definition": _first_sentence(primary[0].text, 180) if primary else "No supporting excerpts found.",
        }
        entries.append(entry)
    return {
        "phase": "alias_map",
        "entries": entries,
        "included_entries": [e for e in entries if e["citation_count"] >= 1],
        "excluded_entries": [e["concept"] for e in entries if e["citation_count"] == 0],
    }


def _extract_decision_statement(text: str) -> str:
    s = _first_sentence(text, max_len=260)
    s = re.sub(r"^(status update:|status:|quick status:)\s*", "", s, flags=re.IGNORECASE)
    return _clean_text(s)


def _extract_rationale(text: str) -> str:
    t = _clean_text(text)
    patterns = [
        r"\b(?:because|since|to avoid|so that|in order to)\b[^.]{20,220}",
    ]
    for pat in patterns:
        m = re.search(pat, t, flags=re.IGNORECASE)
        if m:
            return _clean_text(m.group(0))
    return ""


def _extract_alternatives(text: str) -> str:
    t = _clean_text(text)
    patterns = [
        r"\b(?:instead of|rather than|vs\.?|versus|alternative)\b[^.]{20,200}",
    ]
    for pat in patterns:
        m = re.search(pat, t, flags=re.IGNORECASE)
        if m:
            return _clean_text(m.group(0))
    return ""


def _pick_rationale(candidates: Sequence[EvidenceCandidate]) -> str:
    patterns = [
        r"\b(?:we decided|decided|we chose|lock in)\b[^.]{0,120}\b(?:because|since|to avoid|so that|in order to)\b[^.]{20,220}",
        r"\b(?:because|since|to avoid|so that|in order to)\b[^.]{20,220}\b(?:we decided|decided|we chose|lock in)\b[^.]{0,120}",
    ]
    for c in candidates:
        t = _clean_text(c.text)
        for pat in patterns:
            m = re.search(pat, t, flags=re.IGNORECASE)
            if m:
                return _clean_text(m.group(0))
    return ""


def _pick_alternative(candidates: Sequence[EvidenceCandidate]) -> str:
    patterns = [
        r"\b(?:we decided|decided|we chose|lock in)\b[^.]{0,120}\b(?:instead of|rather than|vs\.?|versus|alternative)\b[^.]{20,220}",
        r"\b(?:instead of|rather than|vs\.?|versus|alternative)\b[^.]{20,220}\b(?:we decided|decided|we chose|lock in)\b[^.]{0,120}",
    ]
    for c in candidates:
        t = _clean_text(c.text)
        for pat in patterns:
            m = re.search(pat, t, flags=re.IGNORECASE)
            if m:
                return _clean_text(m.group(0))
    return ""


def _infer_decision_status(candidates: Sequence[EvidenceCandidate]) -> str:
    superseded_hits = 0
    contested_hits = 0
    for c in candidates:
        t = c.text.lower()
        if SUPERSEDED_STRONG_RE.search(t) and _contains_any(t, DECISION_REASON_SIGNALS):
            superseded_hits += 1
        if _contains_any(t, CONTEST_SIGNALS) and _contains_any(t, DECISION_REASON_SIGNALS):
            contested_hits += 1
    if superseded_hits >= 2:
        return "superseded"
    if contested_hits >= 2:
        return "contested"
    return "current"


def _compile_decision_register(
    candidates: Sequence[EvidenceCandidate],
    min_chars: int,
    min_primary_citations: int,
    include_dynamic_clusters: bool,
) -> Dict[str, object]:
    rows = []
    used_keys = set()

    # Pass A: Theme-based decision extraction (stable, high-precision anchors).
    for spec in DECISION_THEME_SPECS:
        support = []
        for c in candidates:
            decision_like = c.obs_type == "decision" or _contains_any(c.text, DECISION_CORE_SIGNALS)
            if not decision_like:
                continue
            text_l = c.text.lower()
            hit_count = sum(1 for p in spec["patterns"] if p.lower() in text_l)
            if hit_count >= int(spec.get("min_hits", 2)):
                support.append(c)
        support = _dedupe_candidates(support)
        support = sorted(support, key=lambda c: (_parse_ts(c.timestamp), c.evidence_id), reverse=True)
        primary = [c for c in support if c.source_tier == "primary"]
        primary = [c for c in primary if _citation_quality(_first_sentence(c.text), min_chars)]
        if len(primary) < min_primary_citations:
            continue
        sessions = {c.session_id for c in primary if c.session_id}
        rationale = _pick_rationale(primary)
        alternatives = _pick_alternative(primary)
        status = _infer_decision_status(primary)
        first_seen, last_seen = _range_from_candidates(primary)
        used_keys.add(_normalise_key(spec["statement"]))
        rows.append(
            {
                "decision_id": f"decision_{len(rows)+1:03d}",
                "statement": spec["statement"],
                "type": "decision",
                "status": status,
                "confidence": _confidence(len(primary), len(sessions)),
                "date_range": {"first_seen": first_seen, "last_seen": last_seen},
                "rationale": rationale or "No supporting excerpts found.",
                "alternatives_rejected": alternatives or "No supporting excerpts found.",
                "primary_citation_count": len(primary),
                "supplemental_citation_count": len(support) - len(primary),
                "session_count": len(sessions),
                "citations": [_citation_from_candidate(c, min_chars) for c in primary[:10]],
                "origin": "theme",
                "theme_id": spec["id"],
            }
        )

    # Pass B (optional): dynamic clusters from explicit decision rows and decision-signal rows.
    if include_dynamic_clusters:
        decision_candidates = [
            c for c in candidates if c.obs_type == "decision" or _contains_any(c.text, DECISION_CORE_SIGNALS)
        ]
        decision_candidates = _dedupe_candidates(decision_candidates)
        clusters: Dict[str, List[EvidenceCandidate]] = defaultdict(list)
        for c in decision_candidates:
            statement = _extract_decision_statement(c.text)
            key = _normalise_key(statement)
            if not key:
                continue
            clusters[key].append(c)

        for key, group in clusters.items():
            if key in used_keys:
                continue
            group = sorted(group, key=lambda c: (_parse_ts(c.timestamp), c.evidence_id), reverse=True)
            primary = [c for c in group if c.source_tier == "primary"]
            primary = [c for c in primary if _citation_quality(_first_sentence(c.text), min_chars)]
            if len(primary) < min_primary_citations:
                continue
            sessions = {c.session_id for c in primary if c.session_id}
            statement = _extract_decision_statement(primary[0].text)
            statement_key = _normalise_key(statement)
            if not statement_key or statement_key in used_keys:
                continue
            rationale = _pick_rationale(primary)
            alternatives = _pick_alternative(primary)
            status = _infer_decision_status(primary)
            first_seen, last_seen = _range_from_candidates(primary)
            rows.append(
                {
                    "decision_id": f"decision_{len(rows)+1:03d}",
                    "statement": statement,
                    "type": "decision",
                    "status": status,
                    "confidence": _confidence(len(primary), len(sessions)),
                    "date_range": {"first_seen": first_seen, "last_seen": last_seen},
                    "rationale": rationale or "No supporting excerpts found.",
                    "alternatives_rejected": alternatives or "No supporting excerpts found.",
                    "primary_citation_count": len(primary),
                    "supplemental_citation_count": len(group) - len(primary),
                    "session_count": len(sessions),
                    "citations": [_citation_from_candidate(c, min_chars) for c in primary[:10]],
                    "origin": "dynamic_cluster",
                }
            )
            used_keys.add(statement_key)

    rows.sort(key=lambda r: (r["confidence"], r["primary_citation_count"]), reverse=True)
    return {
        "phase": "decision_register",
        "decision_rows": rows,
        "included_decisions": rows,
    }


def _render_alias_md(data: Dict[str, object]) -> str:
    lines = ["# Alias Map", ""]
    lines.append("## Evidence Table")
    included = data["included_entries"]
    if not included:
        lines.append("No supporting excerpts found.")
        return "\n".join(lines)
    for e in included:
        lines.append(f"- `{e['concept']}` aliases: {', '.join(e['aliases'])}")
        for c in e["citations"][:4]:
            lines.append(f"  - [{c['evidence_id']}] {c['timestamp']} | {c['project']} | {c['excerpt']}")
    lines.append("")
    lines.append("## Glossary")
    for e in included:
        lines.append(f"- `{e['concept']}`: {e['definition']}")
    return "\n".join(lines)


def _render_claim_phase_md(title: str, included_claims: Sequence[Dict[str, object]], no_data_message: str) -> str:
    lines = [f"# {title}", ""]
    lines.append("## Evidence Table")
    if not included_claims:
        lines.append(no_data_message)
        return "\n".join(lines)
    for c in included_claims:
        lines.append(
            f"- `{c['claim_id']}` | type={c['type']} | status={c['status']} | confidence={c['confidence']} | first_seen={c['first_seen']} | last_seen={c['last_seen']}"
        )
        lines.append(f"  - statement: {c['statement']}")
        for ct in c["citations"][:4]:
            lines.append(f"  - [{ct['evidence_id']}] {ct['timestamp']} | {ct['project']} | {ct['excerpt']}")
        if c.get("conflict_citations"):
            lines.append("  - conflicts:")
            for ct in c["conflict_citations"][:3]:
                lines.append(f"    - [{ct['evidence_id']}] {ct['timestamp']} | {ct['project']} | {ct['excerpt']}")
    return "\n".join(lines)


def _render_architecture_md(data: Dict[str, object]) -> str:
    included = data["included_claims"]
    lines = ["# AS-IS Architecture Atlas", ""]
    lines.append("## Evidence Table")
    if not included:
        lines.append("No supporting excerpts found.")
        return "\n".join(lines)
    for c in included:
        lines.append(
            f"- `{c['claim_id']}` | dimension={c.get('dimension','')} | status={c['status']} | confidence={c['confidence']}"
        )
        lines.append(f"  - statement: {c['statement']}")
        for ct in c["citations"][:4]:
            lines.append(f"  - [{ct['evidence_id']}] {ct['timestamp']} | {ct['project']} | {ct['excerpt']}")
    lines.append("")
    lines.append("## Implemented vs Aspirational")
    implemented = []
    aspirational = []
    for c in included:
        joined = " ".join(x["excerpt"] for x in c["citations"][:4]).lower()
        if _contains_any(joined, ASPIRATIONAL_SIGNALS):
            aspirational.append(c["statement"])
        else:
            implemented.append(c["statement"])
    if implemented:
        lines.append("- Implemented:")
        for s in implemented:
            lines.append(f"  - {s}")
    else:
        lines.append("- Implemented: No supporting excerpts found.")
    if aspirational:
        lines.append("- Aspirational:")
        for s in aspirational:
            lines.append(f"  - {s}")
    else:
        lines.append("- Aspirational: No supporting excerpts found.")
    return "\n".join(lines)


def _render_decision_md(data: Dict[str, object]) -> str:
    rows = data["included_decisions"]
    lines = ["# Decision Register", ""]
    lines.append("## Evidence Table")
    if not rows:
        lines.append("No supporting excerpts found.")
        return "\n".join(lines)
    for r in rows:
        lines.append(
            f"- `{r['decision_id']}` | status={r['status']} | confidence={r['confidence']} | first_seen={r['date_range']['first_seen']} | last_seen={r['date_range']['last_seen']}"
        )
        lines.append(f"  - decision: {r['statement']}")
        lines.append(f"  - rationale: {r['rationale']}")
        lines.append(f"  - alternatives: {r['alternatives_rejected']}")
        for ct in r["citations"][:4]:
            lines.append(f"  - [{ct['evidence_id']}] {ct['timestamp']} | {ct['project']} | {ct['excerpt']}")
    return "\n".join(lines)


def _readiness_report(
    constitution: Dict[str, object],
    product_spec: Dict[str, object],
    architecture: Dict[str, object],
    decisions: Dict[str, object],
) -> Dict[str, object]:
    constitution_claims = constitution["included_claims"]
    product_claims = product_spec["included_claims"]
    architecture_claims = architecture["included_claims"]
    decision_rows = decisions["included_decisions"]

    high_conf_total = (
        sum(1 for c in constitution_claims if c["confidence"] == "high")
        + sum(1 for c in product_claims if c["confidence"] == "high")
        + sum(1 for c in architecture_claims if c["confidence"] == "high")
        + sum(1 for d in decision_rows if d["confidence"] == "high")
    )
    invariants = [c for c in constitution_claims if c["type"] == "invariant" and c["confidence"] == "high"]
    current_decisions = [d for d in decision_rows if d["status"] == "current"]
    decision_count = len(current_decisions)
    required_dimensions = {"components", "boundaries", "state_ownership", "timing_model", "control_plane"}
    present_dimensions = {c.get("dimension") for c in architecture_claims if c.get("dimension")}
    missing_dimensions = sorted(required_dimensions - present_dimensions)

    gates = {
        "claims_total_gte_30": {
            "pass": high_conf_total >= 30,
            "value": high_conf_total,
            "threshold": 30,
        },
        "invariants_gte_10": {
            "pass": len(invariants) >= 10,
            "value": len(invariants),
            "threshold": 10,
        },
        "decisions_gte_15": {
            "pass": decision_count >= 15,
            "value": decision_count,
            "threshold": 15,
        },
        "architecture_dimensions_complete": {
            "pass": len(missing_dimensions) == 0,
            "missing_dimensions": missing_dimensions,
        },
        "unknown_domains_explicit": {
            "pass": True,
            "unknown": missing_dimensions,
        },
    }
    pack_ready = all(v["pass"] for v in gates.values())
    return {
        "pack_ready": pack_ready,
        "high_conf_claim_total": high_conf_total,
        "constitution_claims": len(constitution_claims),
        "product_spec_claims": len(product_claims),
        "architecture_claims": len(architecture_claims),
        "decision_count": decision_count,
        "invariant_count": len(invariants),
        "gates": gates,
    }


def _render_readiness_md(readiness: Dict[str, object]) -> str:
    lines = ["# Canon Readiness Report", ""]
    lines.append(f"- pack_ready: `{str(readiness['pack_ready']).lower()}`")
    lines.append(f"- high_conf_claim_total: `{readiness['high_conf_claim_total']}`")
    lines.append(f"- invariant_count: `{readiness['invariant_count']}`")
    lines.append(f"- decision_count: `{readiness['decision_count']}`")
    lines.append("")
    lines.append("## Gates")
    for gate, payload in readiness["gates"].items():
        lines.append(f"- `{gate}`: pass={str(payload['pass']).lower()} | details={json.dumps(payload, ensure_ascii=True)}")
    return "\n".join(lines)


def _write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")


def _write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def _write_jsonl(path: Path, rows: Iterable[Dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, ensure_ascii=True) + "\n")


def main() -> int:
    ap = argparse.ArgumentParser(description="Compile Canon Pack phases with strict citation gates.")
    ap.add_argument("--db", default=str(Path.home() / ".claude-mem" / "claude-mem.db"))
    ap.add_argument("--out-dir", default="z_Claude-mem/canon_pack_pipeline")
    ap.add_argument("--primary-projects", default=",".join(DEFAULT_PRIMARY_PROJECTS))
    ap.add_argument("--supplemental-projects", default=",".join(DEFAULT_SUPPLEMENTAL_PROJECTS))
    ap.add_argument("--short-threshold", type=int, default=120)
    ap.add_argument("--stitch-window", type=int, default=2)
    ap.add_argument("--min-citation-chars", type=int, default=180)
    ap.add_argument("--min-primary-citations", type=int, default=2)
    ap.add_argument(
        "--include-dynamic-decisions",
        action="store_true",
        help="Also include dynamic clustered decisions beyond strict curated themes.",
    )
    args = ap.parse_args()

    db = Path(args.db).expanduser()
    if not db.exists():
        raise SystemExit(f"DB not found: {db}")

    primary_projects = [x.strip() for x in args.primary_projects.split(",") if x.strip()]
    supplemental_projects = [x.strip() for x in args.supplemental_projects.split(",") if x.strip()]

    con = sqlite3.connect(str(db))
    observations = _load_observations(con, primary_projects, supplemental_projects)
    con.close()

    candidates = _stitch_candidates(
        observations=observations,
        short_threshold=args.short_threshold,
        stitch_window=args.stitch_window,
    )
    candidates = _dedupe_candidates(candidates)

    alias_map = _compile_alias_map(candidates, args.min_citation_chars)
    constitution = _compile_template_phase(
        phase_name="constitution",
        specs=CONSTITUTION_SPECS,
        candidates=candidates,
        min_chars=args.min_citation_chars,
        min_primary_citations=args.min_primary_citations,
    )
    product_spec = _compile_template_phase(
        phase_name="product_spec",
        specs=PRODUCT_SPEC_SPECS,
        candidates=candidates,
        min_chars=args.min_citation_chars,
        min_primary_citations=args.min_primary_citations,
    )
    architecture = _compile_template_phase(
        phase_name="architecture_atlas",
        specs=ARCHITECTURE_SPECS,
        candidates=candidates,
        min_chars=args.min_citation_chars,
        min_primary_citations=args.min_primary_citations,
    )
    decisions = _compile_decision_register(
        candidates=candidates,
        min_chars=args.min_citation_chars,
        min_primary_citations=args.min_primary_citations,
        include_dynamic_clusters=args.include_dynamic_decisions,
    )

    readiness = _readiness_report(
        constitution=constitution,
        product_spec=product_spec,
        architecture=architecture,
        decisions=decisions,
    )

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    metadata = {
        "db": str(db),
        "generated_at": _ts_iso(datetime.now(tz=UTC)),
        "primary_projects": primary_projects,
        "supplemental_projects": supplemental_projects,
        "rows_loaded": len(observations),
        "candidates_after_stitching_and_dedupe": len(candidates),
        "gates": {
            "min_primary_citations": args.min_primary_citations,
            "min_citation_chars": args.min_citation_chars,
            "short_threshold": args.short_threshold,
            "stitch_window": args.stitch_window,
            "include_dynamic_decisions": args.include_dynamic_decisions,
        },
        "contract": {
            "primary_required_for_claims": True,
            "supplemental_context_allowed": True,
            "no_support_string": "No supporting excerpts found.",
        },
        "stitching_stats": {
            "stitched_candidates": sum(1 for c in candidates if c.stitched),
            "unstitched_candidates": sum(1 for c in candidates if not c.stitched),
        },
    }

    _write_json(out_dir / "metadata.json", metadata)
    _write_json(
        out_dir / "phase_index.json",
        {
            "alias_map": "alias_map.json",
            "constitution": "constitution_claims.json",
            "product_spec": "product_spec_claims.json",
            "architecture_atlas": "architecture_claims.json",
            "decision_register": "decision_register.json",
            "readiness": "canon_readiness.json",
        },
    )
    _write_json(out_dir / "alias_map.json", alias_map)
    _write_json(out_dir / "constitution_claims.json", constitution)
    _write_json(out_dir / "product_spec_claims.json", product_spec)
    _write_json(out_dir / "architecture_claims.json", architecture)
    _write_json(out_dir / "decision_register.json", decisions)
    _write_json(out_dir / "canon_readiness.json", readiness)

    _write_text(out_dir / "alias_map.md", _render_alias_md(alias_map))
    _write_text(
        out_dir / "constitution.md",
        _render_claim_phase_md(
            title="Constitution",
            included_claims=constitution["included_claims"],
            no_data_message="No supporting excerpts found.",
        ),
    )
    _write_text(
        out_dir / "product_spec.md",
        _render_claim_phase_md(
            title="Product Spec",
            included_claims=product_spec["included_claims"],
            no_data_message="No supporting excerpts found.",
        ),
    )
    _write_text(out_dir / "architecture_atlas.md", _render_architecture_md(architecture))
    _write_text(out_dir / "decision_register.md", _render_decision_md(decisions))
    _write_text(out_dir / "canon_readiness.md", _render_readiness_md(readiness))

    _write_jsonl(
        out_dir / "evidence_candidates.jsonl",
        [
            {
                "evidence_id": c.evidence_id,
                "source_obs_ids": c.source_obs_ids,
                "session_id": c.session_id,
                "project": c.project,
                "timestamp": c.timestamp,
                "obs_type": c.obs_type,
                "source_tier": c.source_tier,
                "stitched": c.stitched,
                "domain_tags": _detect_domains(c.text),
                "text": _first_sentence(c.text, max_len=420),
            }
            for c in candidates
        ],
    )

    print(f"Wrote Canon phase outputs to {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
