#!/usr/bin/env python3
"""
Convert exported Cursor composer logs (JSONL) into claude-mem import payloads
and optionally POST them to the running claude-mem worker via /api/import.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import sys
import time
import urllib.error
import urllib.request
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

UTC = timezone.utc
KEYWORD_CONCEPTS = {
    'react', 'typescript', 'javascript', 'python', 'rust', 'go', 'api',
    'database', 'sql', 'graphql', 'rest', 'authentication', 'authorization',
    'security', 'testing', 'docker', 'kubernetes', 'aws', 'gcp', 'azure',
    'git', 'ci/cd', 'deployment', 'performance', 'optimization', 'caching',
    'validation', 'logging'
}
FILE_PATTERNS = [
    re.compile(r'(?:^|\s)([\/~][\w\-\.\/]+\.[\w\d]{1,10})(?:\s|$|:|\)|,)'),
    re.compile(r'(?:^|\s)(\.{1,2}\/[^\s]+\.[\w\d]{1,10})(?:\s|$|:|\)|,)'),
    re.compile(r'(?:^|\s)([\w\-]+\.[a-z]{2,6})(?:\s|$|:|\)|,)'),
    re.compile(r'`([^`]+\.[a-z]{2,6})`')
]


def load_jsonl(path: Path) -> Dict[str, Dict[str, object]]:
    conversations: Dict[str, Dict[str, object]] = {}
    with path.open() as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            obj = json.loads(line)
            conv_id = obj.get('conversation_id')
            if not isinstance(conv_id, str):
                continue
            conv = conversations.setdefault(conv_id, {'meta': {}, 'messages': []})
            if obj.get('record_type') == 'cursor_composer_meta':
                conv['meta'] = obj
                continue
            if obj.get('record_type') != 'cursor_bubble':
                continue
            text = (obj.get('text') or '').strip()
            if not text:
                text = parse_rich_text(obj.get('rich_text'))
            text = text.strip()
            if not text:
                # Skip empty bubbles
                continue
            message = {
                'role': obj.get('role') or 'unknown',
                'text': text,
                'created_at_raw': obj.get('created_at'),
                'metadata': obj.get('metadata') or {},
                'order': len(conv['messages']),
            }
            conv['messages'].append(message)
    return conversations


def parse_rich_text(raw: Optional[str]) -> str:
    if not raw or not isinstance(raw, str):
        return ''
    try:
        data = json.loads(raw)
    except Exception:
        return ''
    texts: List[str] = []

    def walk(node):
        if isinstance(node, dict):
            text = node.get('text')
            if isinstance(text, str):
                texts.append(text)
            for child_key in ('children', 'root', 'body'):
                child = node.get(child_key)
                walk(child)
        elif isinstance(node, list):
            for item in node:
                walk(item)

    walk(data)
    return ' '.join(t for t in texts if t)


def parse_iso(ts: Optional[str]) -> Optional[int]:
    if not ts or not isinstance(ts, str):
        return None
    ts = ts.strip()
    if not ts:
        return None
    try:
        if ts.endswith('Z'):
            dt = datetime.fromisoformat(ts[:-1]).replace(tzinfo=UTC)
        else:
            dt = datetime.fromisoformat(ts)
            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=UTC)
        return int(dt.timestamp() * 1000)
    except Exception:
        return None


def epoch_to_iso(ms: int) -> str:
    return datetime.fromtimestamp(ms / 1000.0, tz=UTC).isoformat().replace('+00:00', 'Z')


def infer_type(content: str, user_content: Optional[str]) -> str:
    content_lower = content.lower()
    user_lower = (user_content or '').lower()
    def contains(words: Iterable[str], haystack: str) -> bool:
        return any(w in haystack for w in words)
    if contains(['fix', 'bug', 'error', 'issue', 'regression'], content_lower + user_lower):
        return 'bugfix'
    if contains(['implement', 'add ', 'create ', 'new feature'], content_lower + user_lower):
        return 'feature'
    if contains(['refactor', 'clean up', 'reorganize', 'restructure'], content_lower + user_lower):
        return 'refactor'
    if contains(['found', 'discovered', 'learned', 'investigated', 'analysis'], content_lower):
        return 'discovery'
    if contains(['decided', 'chose', 'will use', 'recommend', 'should use'], content_lower):
        return 'decision'
    return 'change'


def extract_title(content: str) -> Optional[str]:
    for line in content.splitlines():
        trimmed = line.strip()
        if not trimmed or trimmed.startswith('#') or trimmed.startswith('```'):
            continue
        if len(trimmed) < 5:
            continue
        return trimmed[:100] if len(trimmed) > 100 else trimmed
    cleaned = ' '.join(content.split())
    if not cleaned:
        return None
    return cleaned[:100] if len(cleaned) > 100 else cleaned


def extract_concepts(content: str) -> List[str]:
    content_lower = content.lower()
    found = {kw for kw in KEYWORD_CONCEPTS if kw in content_lower}
    if 'function' in content_lower or 'def ' in content_lower:
        found.add('functions')
    if 'class ' in content_lower or 'interface ' in content_lower:
        found.add('oop')
    if 'async' in content_lower or 'await' in content_lower:
        found.add('async')
    return sorted(found)[:10]


def extract_files(content: str) -> List[str]:
    files = []
    for pattern in FILE_PATTERNS:
        for match in pattern.finditer(content):
            candidate = match.group(1)
            if not candidate or 'http' in candidate:
                continue
            files.append(candidate)
    seen = []
    for f in files:
        if f not in seen:
            seen.append(f)
    return seen[:20]


def token_count(metadata: Dict[str, object]) -> int:
    tc = metadata.get('token_count')
    total = 0
    def add_value(val):
        nonlocal total
        if isinstance(val, (int, float)):
            total += int(val)
        elif isinstance(val, dict):
            for inner in val.values():
                add_value(inner)
    if isinstance(tc, dict):
        add_value(tc)
    return total


LESSON_LEARNED_PATTERNS = [
    r'i learned that',
    r'important:?\s+remember that',
    r'we discovered',
    r'key insight:?',
    r'remember this pattern',
    r'important note:?',
    r'lesson learned:?',
    r'takeaway:?',
    r'key takeaway:?',
    r'important to remember',
    r'worth noting',
    r'critical:?\s+remember',
]


def is_trivial_conversation(messages: List[Dict[str, object]]) -> bool:
    """
    Determine if a conversation is trivial (low value):
    - <3 total messages
    - No code blocks
    - No file references
    - Very short responses (<50 words average)
    """
    if len(messages) < 3:
        return True
    
    all_text = ' '.join(str(msg.get('text', '')) for msg in messages)
    
    # Check for code blocks
    if '```' in all_text:
        return False
    
    # Check for file references
    files = extract_files(all_text)
    if len(files) > 0:
        return False
    
    # Check average message length
    total_words = sum(len(str(msg.get('text', '')).split()) for msg in messages)
    avg_words = total_words / len(messages) if messages else 0
    if avg_words < 50:
        return True
    
    return False


def detect_lessons_learned(messages: List[Dict[str, object]]) -> bool:
    """
    Detect if conversation contains explicit lessons learned or important insights.
    Returns True if patterns are found.
    """
    import re
    all_text = ' '.join(str(msg.get('text', '')) for msg in messages).lower()
    
    for pattern in LESSON_LEARNED_PATTERNS:
        if re.search(pattern, all_text, re.IGNORECASE):
            return True
    
    return False


def score_conversation_value(messages: List[Dict[str, object]], metadata: Optional[Dict[str, object]] = None) -> float:
    """
    Score conversation value from 0.0 to 1.0 based on:
    - Multi-turn depth (≥5 messages = +0.2)
    - Code blocks presence (+0.2)
    - File references (≥3 files = +0.2)
    - Discovery/decision keywords (+0.2)
    - Token count (substantial = +0.2)
    """
    if not messages:
        return 0.0
    
    score = 0.0
    
    # Multi-turn conversations are more valuable
    if len(messages) >= 5:
        score += 0.2
    
    # Code blocks indicate implementation work
    all_text = ' '.join(str(msg.get('text', '')) for msg in messages)
    if '```' in all_text:
        score += 0.2
    
    # File references indicate project-specific work
    files = extract_files(all_text)
    if len(files) >= 3:
        score += 0.2
    
    # Discovery/decision types are high value
    types = []
    for msg in messages:
        if msg.get('role') == 'assistant':
            text = str(msg.get('text', ''))
            user_text = None
            # Try to find preceding user message
            msg_idx = messages.index(msg)
            if msg_idx > 0:
                prev_msg = messages[msg_idx - 1]
                if prev_msg.get('role') == 'user':
                    user_text = str(prev_msg.get('text', ''))
            msg_type = infer_type(text, user_text)
            types.append(msg_type)
    
    if 'discovery' in types or 'decision' in types:
        score += 0.2
    
    # Substantial token usage
    total_tokens = 0
    for msg in messages:
        total_tokens += token_count(msg.get('metadata', {}))
    if total_tokens > 1000:
        score += 0.2
    
    return min(score, 1.0)


def normalize_conversations(conversations: Dict[str, Dict[str, object]], project: str, min_value_score: float = 0.0, filter_lessons_only: bool = False, filter_trivial: bool = False) -> Tuple[List[Dict[str, object]], Dict[str, List[Dict[str, object]]], Dict[str, List[Dict[str, object]]]]:
    sessions: List[Dict[str, object]] = []
    observations_by_session: Dict[str, List[Dict[str, object]]] = defaultdict(list)
    prompts_by_session: Dict[str, List[Dict[str, object]]] = defaultdict(list)
    now_ms = int(time.time() * 1000)

    for conv_id, conv in conversations.items():
        messages = conv.get('messages') or []
        if not messages:
            continue
        meta = conv.get('meta') or {}
        meta_ms = parse_iso(meta.get('created_at')) if isinstance(meta, dict) else None
        last_known = None
        for idx, msg in enumerate(messages):
            raw_ms = parse_iso(msg.get('created_at_raw'))
            if raw_ms is None:
                if last_known is not None:
                    raw_ms = last_known + 1000
                elif meta_ms is not None:
                    raw_ms = meta_ms + idx * 1000
                else:
                    raw_ms = now_ms + idx * 1000
            last_known = raw_ms
            msg['created_at_epoch'] = raw_ms
            msg['created_at'] = epoch_to_iso(raw_ms)
        messages.sort(key=lambda m: (m['created_at_epoch'], m['order']))
        
        # Score conversation value
        conversation_score = score_conversation_value(messages, meta)
        
        # Detect lessons learned
        has_lesson = detect_lessons_learned(messages)
        
        # Filter by minimum value score if specified
        if conversation_score < min_value_score:
            continue
        
        # Filter to lessons only if specified
        if filter_lessons_only and not has_lesson:
            continue
        
        # Filter trivial conversations if specified
        if filter_trivial and is_trivial_conversation(messages):
            continue
        
        session_id = f"cursor-{conv_id}"
        first_user_text = next((m['text'] for m in messages if m['role'] == 'user' and m['text']), None)
        start_ms = messages[0]['created_at_epoch']
        end_ms = messages[-1]['created_at_epoch']
        session_record = {
            'content_session_id': session_id,
            'memory_session_id': session_id,
            'project': project,
            'user_prompt': (first_user_text[:500] if first_user_text else None),
            'started_at': epoch_to_iso(start_ms),
            'started_at_epoch': start_ms,
            'completed_at': epoch_to_iso(end_ms),
            'completed_at_epoch': end_ms,
            'status': 'completed',
            'value_score': round(conversation_score, 3),
            'has_lesson_learned': has_lesson
        }
        sessions.append(session_record)

        prompt_number = 0
        last_user_text = None
        for msg in messages:
            role = msg.get('role')
            text = msg.get('text', '').strip()
            if not text:
                continue
            created_iso = msg['created_at']
            created_epoch = msg['created_at_epoch']
            if role == 'user':
                prompt_number += 1
                last_user_text = text
                prompts_by_session[session_id].append({
                    'content_session_id': session_id,
                    'prompt_number': prompt_number,
                    'prompt_text': text,
                    'created_at': created_iso,
                    'created_at_epoch': created_epoch
                })
            elif role == 'assistant':
                obs = {
                    'memory_session_id': session_id,
                    'project': project,
                    'text': text[:2000],
                    'type': infer_type(text, last_user_text),
                    'title': extract_title(text),
                    'subtitle': (last_user_text[:200] if last_user_text else None),
                    'facts': None,
                    'narrative': text,
                    'concepts': json.dumps(extract_concepts(text)) or None,
                    'files_read': json.dumps(extract_files(text)) or None,
                    'files_modified': None,
                    'prompt_number': prompt_number or None,
                    'discovery_tokens': token_count(msg.get('metadata', {})),
                    'created_at': created_iso,
                    'created_at_epoch': created_epoch
                }
                if obs['concepts'] == '[]':
                    obs['concepts'] = None
                if obs['files_read'] == '[]':
                    obs['files_read'] = None
                observations_by_session[session_id].append(obs)
            else:
                continue
    return sessions, observations_by_session, prompts_by_session


def chunked(sequence: Sequence[str], size: int) -> Iterable[List[str]]:
    for idx in range(0, len(sequence), size):
        yield list(sequence[idx: idx + size])


def post_payload(api_url: str, payload: Dict[str, object]) -> Dict[str, object]:
    data = json.dumps(payload).encode('utf-8')
    req = urllib.request.Request(api_url, data=data, headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=120) as resp:
        return json.loads(resp.read())


def main(argv: Optional[Sequence[str]] = None) -> int:
    ap = argparse.ArgumentParser(description='Import Cursor logs into claude-mem via /api/import')
    ap.add_argument('--input', required=True, help='Path to cursor_composer.jsonl export')
    ap.add_argument('--project', default='cursor', help='Project label to assign to imported sessions')
    ap.add_argument('--chunk-size', type=int, default=25, help='Number of sessions per API call')
    ap.add_argument('--api-url', default='http://127.0.0.1:37777/api/import', help='claude-mem import endpoint')
    ap.add_argument('--write-json', help='Optional path to write the combined payload (single chunk) for inspection')
    ap.add_argument('--post', action='store_true', help='Actually POST to /api/import (otherwise just build stats)')
    ap.add_argument('--min-value-score', type=float, default=0.0, help='Minimum conversation value score (0.0-1.0) to import (default: 0.0 = all)')
    ap.add_argument('--filter-lessons-only', action='store_true', help='Only import conversations with explicit lessons learned patterns')
    ap.add_argument('--filter-trivial', action='store_true', help='Filter out trivial conversations (<3 messages, no code, no files, short responses)')
    args = ap.parse_args(list(argv) if argv is not None else None)

    jsonl_path = Path(os.path.expanduser(args.input))
    if not jsonl_path.exists():
        print(f"Input file not found: {jsonl_path}", file=sys.stderr)
        return 2

    conversations = load_jsonl(jsonl_path)
    total_before = len(conversations)
    sessions, observations_by_session, prompts_by_session = normalize_conversations(conversations, args.project, args.min_value_score, args.filter_lessons_only, args.filter_trivial)
    total_after = len(sessions)
    
    if args.min_value_score > 0.0:
        print(f"Filtered to conversations with value score >= {args.min_value_score}")
    if args.filter_lessons_only:
        print("Filtered to conversations with lessons learned patterns")
    if args.filter_trivial:
        filtered_count = total_before - total_after
        filtered_pct = (filtered_count * 100 // total_before) if total_before > 0 else 0
        print(f"Filtered {filtered_count} trivial conversations ({filtered_pct}% of total)")

    session_ids = [s['content_session_id'] for s in sessions]
    stats = {
        'sessions': len(sessions),
        'observations': sum(len(observations_by_session[sid]) for sid in session_ids),
        'prompts': sum(len(prompts_by_session[sid]) for sid in session_ids)
    }

    print(f"Prepared {stats['sessions']} sessions, {stats['observations']} observations, {stats['prompts']} prompts")

    if args.write_json:
        payload = {
            'sessions': sessions,
            'summaries': [],
            'observations': [obs for sid in session_ids for obs in observations_by_session[sid]],
            'prompts': [p for sid in session_ids for p in prompts_by_session[sid]]
        }
        Path(args.write_json).write_text(json.dumps(payload, indent=2), encoding='utf-8')
        print(f"Wrote payload preview to {args.write_json}")

    if not args.post:
        return 0

    total_responses = {'sessionsImported': 0, 'sessionsSkipped': 0, 'observationsImported': 0, 'observationsSkipped': 0, 'promptsImported': 0, 'promptsSkipped': 0}
    for chunk in chunked(session_ids, args.chunk_size):
        chunk_payload = {
            'sessions': [next(s for s in sessions if s['content_session_id'] == sid) for sid in chunk],
            'summaries': [],
            'observations': [obs for sid in chunk for obs in observations_by_session[sid]],
            'prompts': [p for sid in chunk for p in prompts_by_session[sid]]
        }
        try:
            resp = post_payload(args.api_url, chunk_payload)
        except urllib.error.HTTPError as exc:
            print(f"HTTP error during import: {exc.read().decode()}", file=sys.stderr)
            return 3
        except Exception as exc:
            print(f"Failed to import chunk ({len(chunk)} sessions): {exc}", file=sys.stderr)
            return 3
        stats_resp = resp.get('stats') or {}
        for key in total_responses:
            total_responses[key] += int(stats_resp.get(key, 0))
        print(f"Imported chunk ({len(chunk)} sessions) -> {stats_resp}")
    print('Import complete:', total_responses)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
