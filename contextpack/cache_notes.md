# Prompt Cache Notes

## Cache-Friendly Structure

The `prompt.md` file is structured for optimal prompt caching:

1. **Stable prefix** (Section 1): LLM context that rarely changes
2. **Semi-stable** (Section 2): Packet template with placeholders
3. **Volatile tail** (Section 3+): Diff, fixtures, and your specific ask

## Why This Matters

### OpenAI Prompt Caching
- Automatically caches repeated prompt prefixes (≥1024 tokens)
- Cached prefixes reduce cost and latency on subsequent calls
- Keep stable content at the START of your prompt

### Anthropic Prompt Caching
- Supports explicit cache control blocks via API
- Cache stable prefixes to avoid re-processing
- Particularly effective for long context windows

## Best Practices

1. **Don't modify Section 1** unless the project fundamentally changes
2. **Fill in Section 2** (packet) with your specific goal/symptom
3. **Attach Section 3** files as needed (diff, fixtures)
4. **Write Section 4** (your ask) last

## Maximising Cache Hits

- Use the same `docs/LLM_CONTEXT.md` across all prompts
- Keep packet template structure consistent
- Only the volatile tail should change between prompts

---

*See OpenAI and Anthropic documentation for API-specific caching features.*
