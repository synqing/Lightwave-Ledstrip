# @lightwave/claude-code-provider

**Version:** 1.0.0
**Status:** Production Ready

Claude-Flow provider that uses the `claude` CLI instead of API keys, enabling swarm coordination with your Claude Code Max subscription.

## Why This Exists

Claude-Flow requires API keys (`ANTHROPIC_API_KEY`, etc.) to spawn agents. This provider bypasses that requirement by shelling out to the `claude` CLI, which uses your existing Claude Code Max subscription via OAuth.

**Result**: Full Claude-Flow swarm features without separate API billing.

## Quick Start

```bash
# Build the provider
cd plugins/claude-code-provider
npm install
npm run build

# Run tests
node test-comprehensive.mjs
```

## Usage

### Basic Usage

```typescript
import { ClaudeCodeProvider } from '@lightwave/claude-code-provider';

const provider = new ClaudeCodeProvider({
  config: {
    provider: 'claude-code',
    model: 'sonnet',
  },
});

await provider.initialize();

const response = await provider.complete({
  messages: [
    { role: 'user', content: 'Hello, world!' },
  ],
});

console.log(response.content);

// Cleanup
provider.destroy();
```

### Streaming

```typescript
for await (const event of provider.streamComplete(request)) {
  if (event.type === 'content') {
    process.stdout.write(event.delta?.content || '');
  }
  if (event.type === 'done') {
    console.log('\n\nTokens used:', event.usage?.totalTokens);
  }
}
```

### Multi-Turn Conversations

```typescript
const response = await provider.complete({
  messages: [
    { role: 'system', content: 'You are a helpful assistant.' },
    { role: 'user', content: 'What is 2+2?' },
    { role: 'assistant', content: 'The answer is 4.' },
    { role: 'user', content: 'And 3+3?' },
  ],
});
```

### Tool Results

```typescript
const response = await provider.complete({
  messages: [
    { role: 'user', content: 'Search for X' },
    { role: 'tool', content: '{"results": [...]}', name: 'search' },
  ],
});
```

## Integration with Claude-Flow

### Option A: MCP Server Mode (Recommended)

Add Claude-Flow as an MCP server - Claude Code handles LLM calls, no provider needed:

```json
// ~/.claude/settings.json
{
  "mcpServers": {
    "claude-flow": {
      "command": "npx",
      "args": ["claude-flow@v3alpha", "mcp", "start"]
    }
  }
}
```

### Option B: Standalone Daemon Mode

For running Claude-Flow daemon independently:

```json
// claude-flow.config.json
{
  "providers": {
    "claude-code": {
      "module": "./plugins/claude-code-provider/dist/index.js",
      "priority": 100,
      "enabled": true
    }
  }
}
```

## Configuration Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `config.model` | string | `'sonnet'` | Model: `sonnet`, `opus`, `haiku`, or full names |
| `claudePath` | string | `'claude'` | Path to claude CLI executable |
| `timeout` | number | `300000` | Request timeout in ms (5 minutes) |
| `skipPermissions` | boolean | `true` | Use `--dangerously-skip-permissions` |

## Supported Models

| Alias | Full Model Name | Max Context | Max Output |
|-------|-----------------|-------------|------------|
| `sonnet` | `claude-sonnet-4-5-20250929` | 200K tokens | 64K tokens |
| `opus` | `claude-opus-4-5-20251101` | 200K tokens | 64K tokens |
| `haiku` | `claude-haiku-3-5-20241022` | 200K tokens | 8K tokens |

## Capabilities

```typescript
const caps = provider.capabilities;
// {
//   supportedModels: ['sonnet', 'opus', 'haiku', ...],
//   maxContextLength: { sonnet: 200000, opus: 200000, haiku: 200000 },
//   maxOutputTokens: { sonnet: 64000, opus: 64000, haiku: 8192 },
//   supportsStreaming: true,
//   supportsToolCalling: true,
//   supportsSystemMessages: true,
//   supportsVision: true,
//   supportsAudio: false,
//   supportsFineTuning: false,
//   supportsEmbeddings: false,
//   supportsBatching: false,
//   pricing: { ... } // All $0 (Max subscription)
// }
```

## Response Format

```typescript
interface LLMResponse {
  id: string;                    // Session ID from CLI
  model: string;                 // Actual model used
  provider: 'claude-code';
  content: string;               // Response text
  usage: {
    promptTokens: number;        // Input tokens (including cache)
    completionTokens: number;    // Output tokens
    totalTokens: number;
  };
  cost: {
    promptCost: 0;               // Included in subscription
    completionCost: 0;
    totalCost: number;           // Actual cost from CLI
    currency: 'USD';
  };
  finishReason: 'stop' | 'length' | 'tool_calls' | 'content_filter';
}
```

## Health Check

```typescript
const health = await provider.healthCheck();
// { healthy: true, latency: 245, timestamp: Date }
```

## Events

```typescript
provider.on('initialized', ({ version }) => {
  console.log('Claude CLI version:', version);
});

provider.on('response', ({ provider, model, latency, tokens }) => {
  console.log(`${model}: ${tokens} tokens in ${latency}ms`);
});
```

## Testing

```bash
# Comprehensive test suite (9 tests)
node test-comprehensive.mjs

# Error handling tests
node test-errors.mjs

# Quick sanity check
node test-basic.mjs
```

## Limitations

| Limitation | Impact | Workaround |
|------------|--------|------------|
| Process spawn overhead | ~1-2s per request | Batch requests where possible |
| Context caching | First request 60s+ | Subsequent requests ~10s |
| Single session | No parallel requests | Queue requests |
| Claude-only | No OpenAI/Gemini fallback | MCP mode recommended |

## Critical Implementation Notes

### Stdin Must Be Closed

The `claude` CLI waits for stdin to close before processing. The provider handles this automatically:

```typescript
// CRITICAL: Close stdin immediately - claude CLI waits for stdin to close
proc.stdin.end();
```

Without this, the process hangs indefinitely (exit code 143 from SIGTERM on timeout).

### Timeout Handling

Node's `spawn` timeout option is unreliable. The provider uses manual timeout handling:

```typescript
const timeoutId = setTimeout(() => {
  proc.kill('SIGTERM');
  reject(new Error(`Claude CLI timed out after ${this.timeout}ms`));
}, this.timeout);
```

### Context Caching

First request in a project directory may take 60+ seconds due to Claude Code's context caching. Subsequent requests are much faster (~10s).

## How It Works

```
ClaudeCodeProvider.complete(request)
  1. Build prompt from messages (system/user/assistant/tool roles)
  2. Spawn: claude -p "prompt" --model sonnet --output-format json --dangerously-skip-permissions --no-session-persistence
  3. Close stdin immediately (proc.stdin.end())
  4. Parse JSON response
  5. Return LLMResponse to caller
```

## Troubleshooting

### Process hangs (exit code 143)

- Cause: stdin not closed
- Fixed in: v1.0.0 (proc.stdin.end() after spawn)

### Empty output

- Check: `claude --version` works
- Check: Not rate-limited (Max subscription)

### Unknown model error

- Use aliases: `sonnet`, `opus`, `haiku`
- Or full names: `claude-sonnet-4-5-20250929`

### Timeout errors

- Default: 5 minutes (300000ms)
- Increase via `timeout` option for large contexts

## Related Documentation

- [Claude-Flow MCP Setup](../../docs/operations/claude-flow/MCP_SETUP.md)
- [Claude-Flow Integration Overview](../../docs/operations/claude-flow/README.md)
- [CLAUDE.md Command Centre](../../CLAUDE.md#command-centre--claude-flow-integration)

## License

MIT
