/**
 * @lightwave/claude-code-provider
 *
 * Claude-Flow provider that uses Claude Code CLI instead of API keys.
 * Enables Claude-Flow swarm coordination with Claude Code Max subscription.
 */

export {
  ClaudeCodeProvider,
  type ClaudeCodeProviderOptions,
  type LLMProvider,
  type LLMModel,
  type LLMMessage,
  type LLMRequest,
  type LLMResponse,
  type LLMStreamEvent,
  type HealthCheckResult,
  type ProviderCapabilities,
  type LLMProviderConfig,
} from './claude-code-provider.js';
