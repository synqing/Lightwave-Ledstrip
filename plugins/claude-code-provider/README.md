# @lightwave/claude-code-provider

Claude-Flow provider that uses the `claude` CLI instead of API keys, enabling swarm coordination with your Claude Code Max subscription.

## Why This Exists

Claude-Flow requires API keys (`ANTHROPIC_API_KEY`, etc.) to spawn agents. This provider bypasses that requirement by shelling out to the `claude` CLI, which uses your existing Claude Code Max subscription via OAuth.

**Result**: Full Claude-Flow swarm features without separate API billing.

## Installation

```bash
cd plugins/claude-code-provider
npm install
npm run build
```

## Usage

### Standalone

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
```

### Streaming

```typescript
for await (const event of provider.streamComplete(request)) {
  if (event.type === 'content') {
    process.stdout.write(event.delta?.content || '');
  }
}
```

### With Claude-Flow

Add to your claude-flow configuration:

```json
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

## Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `config.model` | string | `'sonnet'` | Model alias: `sonnet`, `opus`, `haiku` |
| `claudePath` | string | `'claude'` | Path to claude CLI |
| `timeout` | number | `120000` | Request timeout in ms |
| `skipPermissions` | boolean | `true` | Use `--dangerously-skip-permissions` |

## Supported Models

- `sonnet` / `claude-sonnet-4-5-20250929`
- `opus` / `claude-opus-4-5-20251101`
- `haiku`

## Limitations

1. **Latency**: Process spawn is slower than HTTP API (~1-2s overhead)
2. **Concurrency**: One request at a time (no parallel sessions)
3. **No fallback**: Claude-only (no OpenAI/Gemini failover)
4. **Cost tracking**: Shows $0 (subscription-based billing)

## How It Works

```
ClaudeCodeProvider.complete(request)
  → spawn('claude', ['-p', prompt, '--model', 'sonnet', '--output-format', 'json'])
  → parse JSON output
  → return LLMResponse
```

The provider:
1. Builds a prompt from `LLMMessage[]`
2. Executes `claude -p "prompt" --model sonnet --output-format json`
3. Parses the JSON response into `LLMResponse` format
4. Returns to Claude-Flow for swarm coordination

## License

MIT
