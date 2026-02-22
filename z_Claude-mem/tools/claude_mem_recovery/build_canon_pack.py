#!/usr/bin/env python3
"""
Build an evidence-first Canon Pack from claude-mem SQLite.

This script is intentionally conservative:
- It does not modify the database.
- It only emits claims backed by minimum evidence thresholds.
- If evidence is insufficient, it outputs "No supporting excerpts found".
"""

from __future__ import annotations

import argparse
import json
import re
import sqlite3
from collections import defaultdict
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

UTC = timezone.utc

TOPICS: Dict[str, List[str]] = {
    "alias_glossary": [
        "alias",
        "also called",
        "aka",
        "renamed",
        "formerly",
        "known as",
        "tab5",
        "prism",
        "trinity",
        "lightwave",
    ],
    "constitution": [
        "mission",
        "vision",
        "non-negotiable",
        "must",
        "must not",
        "never",
        "quality bar",
        "success criteria",
        "refuse",
    ],
    "architecture_atlas": [
        "architecture",
        "component",
        "actor",
        "boundary",
        "state ownership",
        "pipeline",
        "dataflow",
        "timing",
        "render",
        "audio",
        "scheduler",
    ],
    "protocols_contracts": [
        "protocol",
        "contract",
        "message",
        "payload",
        "schema",
        "api",
        "websocket",
        "endpoint",
        "ownership",
    ],
    "decision_register": [
        "decided",
        "decision",
        "chose",
        "trade-off",
        "tradeoff",
        "superseded",
        "reverted",
        "rollback",
        "deprecated",
    ],
    "prd_library": [
        "prd",
        "requirement",
        "acceptance",
        "acceptance criteria",
        "non-goal",
        "user story",
        "functional",
        "nonfunctional",
    ],
    "risk_register": [
        "risk",
        "failure",
        "failure mode",
        "regression",
        "issue",
        "mitigation",
        "outage",
        "crash",
        "latency",
    ],
    "roadmap_timeline": [
        "roadmap",
        "timeline",
        "milestone",
        "phase",
        "next",
        "planned",
        "beta",
        "release",
        "kickstarter",
    ],
}

CLAIM_TEMPLATES: List[Dict[str, object]] = [
    {
        "id": "centre_origin_invariant",
        "topic": "constitution",
        "claim": "Effects use centre-origin propagation from LEDs 79/80 and reject linear sweep behaviour.",
        "type": "constraint",
        "status": "current",
        "patterns": [
            "centre origin",
            "center origin",
            "79/80",
            "outward",
            "inward",
            "no linear sweep",
        ],
    },
    {
        "id": "no_rainbow_invariant",
        "topic": "constitution",
        "claim": "Rainbow cycling and full hue-wheel sweeps are explicitly disallowed.",
        "type": "constraint",
        "status": "current",
        "patterns": ["no rainbow", "rainbow cycling", "hue-wheel", "hue wheel"],
    },
    {
        "id": "psram_mandatory",
        "topic": "architecture_atlas",
        "claim": "PSRAM is mandatory for stable runtime memory budgets; builds without PSRAM risk memory failure.",
        "type": "constraint",
        "status": "current",
        "patterns": ["psram mandatory", "board_has_psram", "8mb psram", "esp_err_no_mem"],
    },
    {
        "id": "render_budget_120fps",
        "topic": "architecture_atlas",
        "claim": "Renderer targets 120 FPS with an 8.33ms frame budget and low single-digit millisecond effect execution.",
        "type": "fact",
        "status": "current",
        "patterns": ["120 fps", "8.33ms", "frame budget", "2 ms"],
    },
    {
        "id": "no_heap_in_render",
        "topic": "constitution",
        "claim": "Render paths avoid heap allocation (`new`/`malloc`/`String`) and use static buffers.",
        "type": "constraint",
        "status": "current",
        "patterns": ["no heap alloc in render", "no new", "no malloc", "static buffers"],
    },
    {
        "id": "api_control_plane",
        "topic": "protocols_contracts",
        "claim": "Control plane uses HTTP/REST plus WebSocket messaging contracts for device state and streams.",
        "type": "fact",
        "status": "current",
        "patterns": ["rest", "websocket", "api", "endpoint", "subscribe"],
    },
    {
        "id": "decision_register_exists",
        "topic": "decision_register",
        "claim": "The project history includes superseded and revised architecture decisions requiring explicit status tracking.",
        "type": "decision",
        "status": "current",
        "patterns": ["decision", "superseded", "reverted", "replaced by", "deprecated"],
    },
    {
        "id": "risk_watchdog_budget",
        "topic": "risk_register",
        "claim": "Watchdog stability and timing budget violations are recurring system risks in render/audio integration.",
        "type": "risk",
        "status": "current",
        "patterns": ["watchdog", "frame budget", "timeout", "latency", "starvation"],
    },
]

DOMAIN_TAGS: Dict[str, List[str]] = {
    "OPTICS/LGP": ["lgp", "optics", "diffuser", "edge-lit", "edge lit", "volumetric", "viscous"],
    "LEDS/POWER": ["ws2812", "led", "brightness", "power", "current", "voltage", "connector", "strip"],
    "AUDIO/DSP": ["audio", "dsp", "beat", "flux", "chroma", "fft", "hop size", "onset", "clock"],
    "RENDER/FX": ["render", "effect", "pattern", "palette", "centre-origin", "center-origin", "rainbow", "bloom"],
    "CONTROL/NETWORK": ["network", "ap", "sta", "mdns", "ip", "websocket", "api", "provision", "sync"],
    "UI/UX": ["tab5", "ios", "dashboard", "ux", "ui", "workflow", "control surface", "preset"],
    "PRODUCTION/GTM": ["kickstarter", "pricing", "beta", "tester", "manufacturing", "launch", "gtm"],
}

ALIASES_SEED: Dict[str, List[str]] = {
    "SpectraSynq": ["spectrasynq"],
    "K1-Lightwave": ["k1-lightwave", "k1 lightwave", "lightwave-ledstrip", "lightwave ledstrip", "lightwaveos"],
    "Tab5 Hub": ["tab5", "hub", "encoder device", "tab5.encoder"],
    "PRISM": ["prism", "prism.k1", "prism.unified"],
    "Trinity": ["trinity", "spectrasynq.trinity"],
}

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


@dataclass
class Evidence:
    evidence_id: str
    source: str
    row_id: int
    project: str
    timestamp: str
    session_id: str
    topic: str
    domain_tags: List[str]
    text: str
    quality_score: float


def _clean_text(s: str) -> str:
    s = re.sub(r"\s+", " ", s or "").strip()
    return s


def _first_sentence(s: str, max_len: int = 280) -> str:
    s = _clean_text(s)
    if not s:
        return ""
    parts = [p.strip() for p in re.split(r"(?<=[.!?])\s+", s) if p.strip()]
    for part in parts:
        if _is_weak_excerpt(part):
            continue
        if len(part) <= max_len:
            return part
        return part[: max_len - 1].rstrip() + "…"
    # Fallback to the full cleaned text when sentence splitting yields only weak fragments.
    if len(s) <= max_len:
        return s
    return s[: max_len - 1].rstrip() + "…"


def _normalise_for_dedupe(s: str) -> str:
    s = _clean_text(s.lower())
    s = re.sub(r"[^a-z0-9\s]", " ", s)
    s = re.sub(r"\s+", " ", s).strip()
    return s


def _claim_key(s: str) -> str:
    s = _normalise_for_dedupe(s)
    toks = [t for t in s.split() if len(t) > 2 and t not in STOPWORDS]
    if not toks:
        return ""
    return " ".join(sorted(set(toks[:14])))


def _detect_domains(s: str) -> List[str]:
    s_l = s.lower()
    tags: List[str] = []
    for domain, kws in DOMAIN_TAGS.items():
        for kw in kws:
            if len(kw) <= 3:
                if re.search(rf"\b{re.escape(kw)}\b", s_l):
                    tags.append(domain)
                    break
            elif kw in s_l:
                tags.append(domain)
                break
    return tags


def _infer_claim_type(s: str) -> str:
    s_l = s.lower()
    if any(k in s_l for k in ("decided", "chose", "we use", "will use", "decision")):
        return "decision"
    if any(k in s_l for k in ("must", "never", "mandatory", "required", "non-negotiable")):
        return "constraint"
    if any(k in s_l for k in ("risk", "failure", "regression", "mitigation", "issue")):
        return "risk"
    if any(k in s_l for k in ("prefer", "we want", "goal", "mission", "value")):
        return "preference"
    return "fact"


def _infer_status(s: str) -> str:
    s_l = s.lower()
    if any(k in s_l for k in ("superseded", "deprecated", "reverted", "rollback", "replaced by")):
        return "superseded"
    return "current"


def _quality_score(text: str, project: str, topic: str, domains: Sequence[str]) -> float:
    t = text.strip()
    if not t:
        return 0.0
    score = 0.0
    n = len(t)
    if n >= 100:
        score += 0.25
    if n >= 180:
        score += 0.15
    if any(k in t.lower() for k in ("must", "never", "decided", "contract", "acceptance", "risk")):
        score += 0.2
    if domains:
        score += 0.15
    if project and project != "unknown":
        score += 0.1
    if topic in ("decision_register", "constitution", "protocols_contracts"):
        score += 0.1
    if re.search(r"\b\d{4}-\d{2}-\d{2}\b", t):
        score += 0.05
    if any(
        t.lower().startswith(p)
        for p in ("status:", "quick status:", "i'm going to", "i am going to", "i will")
    ):
        score -= 0.2
    return min(score, 1.0)


def _is_low_signal(text: str) -> bool:
    t = (text or "").lower().strip()
    if not t:
        return True
    noise_patterns = (
        "## my request for codex",
        "# context from my ide setup",
        "<task-notification>",
        "[execute_command",
        "[read_file for",
        "[plan_mode_response]",
        "agent \"",
        "command executed",
        "i see the issue with permissions",
        "captain!",
        "claude_mem_recovery",
        "scrape every single known ide/cli chat session",
        "chat logs and then scrape",
        "scrape, parse, and ingest all",
    )
    if any(p in t for p in noise_patterns):
        return True
    if len(t) < 80:
        return True
    # Reject lines that are mostly punctuation/markup.
    alnum = sum(1 for c in t if c.isalnum())
    if alnum / max(len(t), 1) < 0.45:
        return True
    return False


def _is_weak_excerpt(text: str) -> bool:
    t = _clean_text(text).lower()
    if not t:
        return True
    if len(t) < 35:
        return True
    if re.fullmatch(r"\d+[\.\)]?", t):
        return True
    if any(
        t.startswith(p)
        for p in ("status:", "quick status:", "i'm going to", "i am going to", "i will", "i'll")
    ):
        return True
    alpha = sum(1 for c in t if c.isalpha())
    if alpha < 20:
        return True
    return False


def _load_observations(con: sqlite3.Connection, projects: Optional[Sequence[str]]) -> List[Dict[str, str]]:
    cur = con.cursor()
    params: List[str] = []
    q = (
        "SELECT id, memory_session_id, project, created_at, type, title, subtitle, narrative, text, facts "
        "FROM observations"
    )
    if projects:
        q += " WHERE project IN (" + ",".join(["?"] * len(projects)) + ")"
        params.extend(projects)
    rows = cur.execute(q, params).fetchall()
    out: List[Dict[str, str]] = []
    for row in rows:
        rid, mem_id, project, created_at, obs_type, title, subtitle, narrative, text, facts = row
        body = " ".join(
            x for x in [title or "", subtitle or "", narrative or "", text or "", facts or ""] if x.strip()
        )
        if _is_low_signal(body):
            continue
        out.append(
            {
                "source": "observation",
                "row_id": rid,
                "session_id": mem_id or "",
                "project": project or "unknown",
                "timestamp": created_at or "",
                "obs_type": obs_type or "",
                "text": _clean_text(body),
            }
        )
    return out


def _load_prompts(con: sqlite3.Connection, projects: Optional[Sequence[str]]) -> List[Dict[str, str]]:
    cur = con.cursor()
    params: List[str] = []
    q = (
        "SELECT p.id, p.content_session_id, p.created_at, p.prompt_text, s.project "
        "FROM user_prompts p "
        "LEFT JOIN sdk_sessions s ON s.content_session_id = p.content_session_id"
    )
    if projects:
        q += " WHERE s.project IN (" + ",".join(["?"] * len(projects)) + ")"
        params.extend(projects)
    rows = cur.execute(q, params).fetchall()
    out: List[Dict[str, str]] = []
    for row in rows:
        rid, content_id, created_at, prompt_text, project = row
        if _is_low_signal(prompt_text or ""):
            continue
        out.append(
            {
                "source": "prompt",
                "row_id": rid,
                "session_id": content_id or "",
                "project": project or "unknown",
                "timestamp": created_at or "",
                "obs_type": "",
                "text": _clean_text(prompt_text or ""),
            }
        )
    return out


def _find_topic_matches(
    rows: Sequence[Dict[str, str]],
    topic: str,
    keywords: Sequence[str],
    min_chars: int,
    max_evidence: int,
) -> List[Evidence]:
    scored: List[Evidence] = []
    seen: set = set()
    for row in rows:
        txt = row["text"]
        if len(txt) < min_chars:
            continue
        txt_l = txt.lower()
        hit_count = sum(1 for kw in keywords if kw in txt_l)
        if hit_count == 0:
            continue
        if topic in ("constitution", "protocols_contracts", "decision_register") and hit_count < 2:
            # Tighten high-risk topics so single noisy token does not pass.
            continue
        excerpt = _first_sentence(txt)
        dn = _normalise_for_dedupe(excerpt)
        if not dn or dn in seen:
            continue
        seen.add(dn)
        domains = _detect_domains(txt)
        q = _quality_score(txt, row["project"], topic, domains) + min(hit_count * 0.03, 0.2)
        evidence_id = f"{row['source'][0].upper()}{row['row_id']}"
        scored.append(
            Evidence(
                evidence_id=evidence_id,
                source=row["source"],
                row_id=int(row["row_id"]),
                project=row["project"],
                timestamp=row["timestamp"],
                session_id=row["session_id"],
                topic=topic,
                domain_tags=domains,
                text=excerpt,
                quality_score=min(q, 1.0),
            )
        )
    scored.sort(key=lambda e: (e.quality_score, e.timestamp), reverse=True)
    return scored[:max_evidence]


def _collect_template_claims(
    rows: Sequence[Dict[str, str]],
    min_evidence: int,
    min_sessions: int,
) -> List[Dict[str, object]]:
    claims: List[Dict[str, object]] = []
    for t in CLAIM_TEMPLATES:
        patterns = [p.lower() for p in t["patterns"]]
        ev: List[Dict[str, object]] = []
        seen_excerpt: set = set()
        sessions: set = set()
        for row in rows:
            txt = row["text"]
            txt_l = txt.lower()
            if not any(p in txt_l for p in patterns):
                continue
            ex = _first_sentence(txt)
            norm = _normalise_for_dedupe(ex)
            if not norm or norm in seen_excerpt:
                continue
            seen_excerpt.add(norm)
            sessions.add(row["session_id"])
            ev.append(
                {
                    "evidence_id": f"{row['source'][0].upper()}{row['row_id']}",
                    "timestamp": row["timestamp"],
                    "source": row["source"],
                    "row_id": row["row_id"],
                    "project": row["project"],
                    "session_id": row["session_id"],
                    "excerpt": ex,
                    "domain_tags": _detect_domains(txt),
                }
            )
        if len(ev) < min_evidence or len([s for s in sessions if s]) < min_sessions:
            continue
        confidence = "medium"
        if len(ev) >= 6 and len([s for s in sessions if s]) >= 3:
            confidence = "high"
        claims.append(
            {
                "template_id": t["id"],
                "claim": t["claim"],
                "topic": t["topic"],
                "type": t["type"],
                "status": t["status"],
                "confidence": confidence,
                "support_count": len(ev),
                "session_count": len([s for s in sessions if s]),
                "citations": [
                    {
                        "evidence_id": e["evidence_id"],
                        "timestamp": e["timestamp"],
                        "source": e["source"],
                        "row_id": e["row_id"],
                        "project": e["project"],
                    }
                    for e in ev[:10]
                ],
            }
        )
    claims.sort(key=lambda c: (c["topic"], c["support_count"]), reverse=True)
    return claims


def _build_alias_map(rows: Sequence[Dict[str, str]], max_evidence: int = 60) -> Dict[str, Dict[str, object]]:
    out: Dict[str, Dict[str, object]] = {}
    for concept, aliases in ALIASES_SEED.items():
        found_aliases: set = set()
        ev: List[Dict[str, str]] = []
        for row in rows:
            txt_l = row["text"].lower()
            hits = [a for a in aliases if a in txt_l]
            if not hits:
                continue
            for h in hits:
                found_aliases.add(h)
            ev.append(
                {
                    "evidence_id": f"{row['source'][0].upper()}{row['row_id']}",
                    "timestamp": row["timestamp"],
                    "source": row["source"],
                    "row_id": row["row_id"],
                    "project": row["project"],
                    "session_id": row["session_id"],
                    "excerpt": _first_sentence(row["text"]),
                }
            )
            if len(ev) >= max_evidence:
                break
        out[concept] = {
            "aliases_found": sorted(found_aliases),
            "evidence": ev,
            "definition": "No supporting excerpts found",
        }
        if ev:
            out[concept]["definition"] = _first_sentence(ev[0]["excerpt"], max_len=160)
    return out


def _compile_claims(evidence: Sequence[Evidence], min_evidence: int) -> List[Dict[str, object]]:
    clusters: Dict[str, List[Evidence]] = defaultdict(list)
    for e in evidence:
        key = _claim_key(e.text)
        if key:
            clusters[key].append(e)

    claims: List[Dict[str, object]] = []
    for key, group in clusters.items():
        if len(group) < min_evidence:
            continue
        sessions = {g.session_id for g in group if g.session_id}
        if len(sessions) < 2:
            continue
        group_sorted = sorted(group, key=lambda x: x.quality_score, reverse=True)
        representative = group_sorted[0]
        domains = sorted({d for g in group for d in g.domain_tags})
        claim_type = _infer_claim_type(representative.text)
        status = _infer_status(representative.text)

        confidence = "medium"
        if len(group) >= 5 and len(sessions) >= 3:
            confidence = "high"
        elif len(group) <= 2:
            confidence = "low"

        claims.append(
            {
                "claim": representative.text,
                "topic": representative.topic,
                "type": claim_type,
                "status": status,
                "confidence": confidence,
                "domains": domains,
                "support_count": len(group),
                "session_count": len(sessions),
                "citations": [
                    {
                        "evidence_id": g.evidence_id,
                        "timestamp": g.timestamp,
                        "source": g.source,
                        "row_id": g.row_id,
                        "project": g.project,
                    }
                    for g in group_sorted[:8]
                ],
            }
        )
    claims.sort(key=lambda c: (c["topic"], c["confidence"], c["support_count"]), reverse=True)
    return claims


def _write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")


def _write_jsonl(path: Path, rows: Iterable[Dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, ensure_ascii=False) + "\n")


def _render_markdown(
    output: Path, claims: Sequence[Dict[str, object]], topic_to_evidence: Dict[str, Sequence[Evidence]]
) -> None:
    lines: List[str] = []
    lines.append("# Canon Pack (Evidence-First)")
    lines.append("")
    lines.append("This pack is generated from claude-mem evidence with quality gates.")
    lines.append("Claims with insufficient support are excluded.")
    lines.append("")

    for topic in TOPICS.keys():
        lines.append(f"## {topic}")
        ev = topic_to_evidence.get(topic, [])
        if not ev:
            lines.append("")
            lines.append("No supporting excerpts found.")
            lines.append("")
            continue

        lines.append("")
        lines.append("### Evidence excerpts")
        for e in ev:
            lines.append(
                f"- [{e.evidence_id}] {e.timestamp} | {e.project} | {','.join(e.domain_tags) if e.domain_tags else 'UNTAGGED'} | {e.text}"
            )
        lines.append("")
        lines.append("### Claims")
        topic_claims = [c for c in claims if c["topic"] == topic]
        if not topic_claims:
            lines.append("- No supporting excerpts found.")
            lines.append("")
            continue
        for c in topic_claims:
            lines.append(
                f"- {c['claim']} (type={c['type']}, status={c['status']}, confidence={c['confidence']}, support={c['support_count']})"
            )
            lines.append(
                "  citations: "
                + ", ".join(f"{x['evidence_id']}@{x['timestamp']}" for x in c["citations"][:4])
            )
        lines.append("")

    output.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser(description="Build quality-gated Canon Pack from claude-mem.")
    ap.add_argument("--db", default=str(Path.home() / ".claude-mem" / "claude-mem.db"))
    ap.add_argument(
        "--projects",
        default="",
        help="Comma-separated project filter. Empty means all projects.",
    )
    ap.add_argument("--out-dir", default="z_Claude-mem/canon_pack")
    ap.add_argument("--min-chars", type=int, default=90)
    ap.add_argument("--max-evidence-per-topic", type=int, default=80)
    ap.add_argument("--min-evidence-per-claim", type=int, default=2)
    ap.add_argument("--min-sessions-per-claim", type=int, default=2)
    ap.add_argument(
        "--include-prompts",
        action="store_true",
        help="Include user_prompts as evidence candidates (default: observations only).",
    )
    args = ap.parse_args()

    db = Path(args.db).expanduser()
    if not db.exists():
        raise SystemExit(f"DB not found: {db}")

    projects: Optional[List[str]] = None
    if args.projects.strip():
        projects = [p.strip() for p in args.projects.split(",") if p.strip()]

    con = sqlite3.connect(str(db))
    obs_rows = _load_observations(con, projects)
    prompt_rows = _load_prompts(con, projects) if args.include_prompts else []
    con.close()
    all_rows = obs_rows + prompt_rows

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    topic_to_evidence: Dict[str, List[Evidence]] = {}
    for topic, kws in TOPICS.items():
        topic_to_evidence[topic] = _find_topic_matches(
            all_rows, topic, kws, args.min_chars, args.max_evidence_per_topic
        )

    all_evidence = [e for group in topic_to_evidence.values() for e in group]
    clustered_claims = _compile_claims(all_evidence, args.min_evidence_per_claim)
    template_claims = _collect_template_claims(
        all_rows, args.min_evidence_per_claim, args.min_sessions_per_claim
    )
    # Prefer deterministic template claims for high precision.
    claims = template_claims + clustered_claims
    alias_map = _build_alias_map(all_rows)

    _write_json(
        out_dir / "metadata.json",
        {
            "db": str(db),
            "projects_filter": projects or "ALL",
            "rows_loaded": len(all_rows),
            "observations_loaded": len(obs_rows),
            "prompts_loaded": len(prompt_rows),
            "include_prompts": args.include_prompts,
            "generated_at": datetime.now(tz=UTC).isoformat().replace("+00:00", "Z"),
            "quality_gate": {
                "min_chars": args.min_chars,
                "max_evidence_per_topic": args.max_evidence_per_topic,
                "min_evidence_per_claim": args.min_evidence_per_claim,
                "min_sessions_per_claim": args.min_sessions_per_claim,
            },
            "contract": {
                "rule": "No supporting excerpts found when evidence is insufficient",
                "claims_require_distinct_sessions": True,
            },
        },
    )
    _write_json(out_dir / "alias_map.json", alias_map)
    _write_json(
        out_dir / "claims.json",
        {
            "claims": claims,
            "template_claims": len(template_claims),
            "clustered_claims": len(clustered_claims),
            "excluded_topics_with_no_claims": [
                t for t in TOPICS.keys() if not any(c["topic"] == t for c in claims)
            ],
        },
    )

    _write_jsonl(
        out_dir / "evidence.jsonl",
        [
            {
                "evidence_id": e.evidence_id,
                "topic": e.topic,
                "source": e.source,
                "row_id": e.row_id,
                "project": e.project,
                "timestamp": e.timestamp,
                "session_id": e.session_id,
                "domain_tags": e.domain_tags,
                "quality_score": round(e.quality_score, 3),
                "text": e.text,
            }
            for e in all_evidence
        ],
    )

    _render_markdown(out_dir / "canon_pack.md", claims, topic_to_evidence)
    print(f"Wrote Canon Pack to {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
