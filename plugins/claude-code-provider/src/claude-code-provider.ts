/**
 * Claude Code Provider for Claude-Flow
 *
 * Enables Claude-Flow to use the `claude` CLI (Max subscription)
 * instead of requiring separate API keys.
 *
 * @module @lightwave/claude-code-provider
 */

import { spawn } from 'node:child_process';
import { EventEmitter } from 'node:events';

// Types from @claude-flow/providers (inline to avoid import issues during dev)
export type LLMProvider = 'anthropic' | 'openai' | 'google' | 'claude-code' | 'custom';
export type LLMModel = string;

export interface LLMMessage {
  role: 'system' | 'user' | 'assistant' | 'tool';
  content: string;
  name?: string;
}

export interface LLMRequest {
  messages: LLMMessage[];
  model?: LLMModel;
  temperature?: number;
  maxTokens?: number;
  stream?: boolean;
}

export interface LLMResponse {
  id: string;
  model: LLMModel;
  provider: LLMProvider;
  content: string;
  usage: {
    promptTokens: number;
    completionTokens: number;
    totalTokens: number;
  };
  cost?: {
    promptCost: number;
    completionCost: number;
    totalCost: number;
    currency: string;
  };
  finishReason?: 'stop' | 'length' | 'tool_calls' | 'content_filter';
}

export interface LLMStreamEvent {
  type: 'content' | 'tool_call' | 'error' | 'done';
  delta?: {
    content?: string;
  };
  error?: Error;
  usage?: LLMResponse['usage'];
}

export interface HealthCheckResult {
  healthy: boolean;
  latency?: number;
  error?: string;
  timestamp: Date;
}

export interface ProviderCapabilities {
  supportedModels: LLMModel[];
  maxContextLength: Record<string, number>;
  maxOutputTokens: Record<string, number>;
  supportsStreaming: boolean;
  supportsToolCalling: boolean;
  supportsSystemMessages: boolean;
  supportsVision: boolean;
  supportsAudio: boolean;
  supportsFineTuning: boolean;
  supportsEmbeddings: boolean;
  supportsBatching: boolean;
  pricing: Record<string, {
    promptCostPer1k: number;
    completionCostPer1k: number;
    currency: string;
  }>;
}

export interface LLMProviderConfig {
  provider: LLMProvider;
  model: LLMModel;
  apiKey?: string;
  temperature?: number;
  maxTokens?: number;
}

export interface ClaudeCodeProviderOptions {
  config: LLMProviderConfig;
  claudePath?: string;
  timeout?: number;
  skipPermissions?: boolean;
}

/**
 * Claude Code Provider
 *
 * Executes LLM requests via the `claude` CLI, enabling use of
 * Claude Code Max subscription without separate API keys.
 */
export class ClaudeCodeProvider extends EventEmitter {
  readonly name: LLMProvider = 'claude-code';

  readonly capabilities: ProviderCapabilities = {
    supportedModels: ['sonnet', 'opus', 'haiku', 'claude-sonnet-4-5-20250929', 'claude-opus-4-5-20251101', 'claude-haiku-3-5-20241022'],
    maxContextLength: {
      sonnet: 200000,
      opus: 200000,
      haiku: 200000,
      'claude-sonnet-4-5-20250929': 200000,
      'claude-opus-4-5-20251101': 200000,
      'claude-haiku-3-5-20241022': 200000,
    },
    maxOutputTokens: {
      sonnet: 64000,
      opus: 64000,
      haiku: 8192,
      'claude-sonnet-4-5-20250929': 64000,
      'claude-opus-4-5-20251101': 64000,
      'claude-haiku-3-5-20241022': 8192,
    },
    supportsStreaming: true,
    supportsToolCalling: true,
    supportsSystemMessages: true,
    supportsVision: true,
    supportsAudio: false,
    supportsFineTuning: false,
    supportsEmbeddings: false,
    supportsBatching: false,
    // Cost is $0 since using Max subscription
    pricing: {
      sonnet: { promptCostPer1k: 0, completionCostPer1k: 0, currency: 'USD' },
      opus: { promptCostPer1k: 0, completionCostPer1k: 0, currency: 'USD' },
      haiku: { promptCostPer1k: 0, completionCostPer1k: 0, currency: 'USD' },
    },
  };

  public config: LLMProviderConfig;
  private claudePath: string;
  private timeout: number;
  private skipPermissions: boolean;
  private initialized = false;

  constructor(options: ClaudeCodeProviderOptions) {
    super();
    this.config = options.config;
    this.claudePath = options.claudePath || 'claude';
    // Default 5 minutes - Claude CLI with context caching can be slow on first call
    this.timeout = options.timeout || 300000;
    this.skipPermissions = options.skipPermissions ?? true;
  }

  /**
   * Initialize the provider
   */
  async initialize(): Promise<void> {
    if (this.initialized) return;

    // Verify claude CLI is available
    const result = await this.execClaude(['--version']);
    if (!result.success) {
      throw new Error(
        `Claude CLI not found at "${this.claudePath}". ` +
        'Install with: npm install -g @anthropic-ai/claude-code'
      );
    }

    this.initialized = true;
    this.emit('initialized', { version: result.stdout.trim() });
  }

  /**
   * Complete a request (non-streaming)
   */
  async complete(request: LLMRequest): Promise<LLMResponse> {
    if (!this.initialized) {
      await this.initialize();
    }

    // Validate request
    if (!request.messages || request.messages.length === 0) {
      throw new Error('Request must contain at least one message');
    }

    const startTime = Date.now();
    const prompt = this.buildPrompt(request);

    if (!prompt.trim()) {
      throw new Error('Built prompt is empty - ensure messages have content');
    }

    const model = request.model || this.config.model || 'sonnet';

    const args = this.buildArgs(prompt, model, request);
    args.push('--output-format', 'json');

    const result = await this.execClaude(args);

    if (!result.success) {
      // Claude CLI may output errors to stdout as JSON
      const errorMsg = result.stderr || result.stdout || 'Unknown error';
      throw new Error(`Claude CLI failed (exit code non-zero): ${errorMsg}`);
    }

    if (!result.stdout.trim()) {
      throw new Error('Claude CLI returned empty output');
    }

    const response = this.parseJsonResponse(result.stdout, model);
    const latency = Date.now() - startTime;

    this.emit('response', {
      provider: this.name,
      model: response.model,
      latency,
      tokens: response.usage.totalTokens,
    });

    return response;
  }

  /**
   * Stream complete a request
   */
  async *streamComplete(request: LLMRequest): AsyncIterable<LLMStreamEvent> {
    if (!this.initialized) {
      await this.initialize();
    }

    // Validate request
    if (!request.messages || request.messages.length === 0) {
      throw new Error('Request must contain at least one message');
    }

    const prompt = this.buildPrompt(request);

    if (!prompt.trim()) {
      throw new Error('Built prompt is empty - ensure messages have content');
    }

    const model = request.model || this.config.model || 'sonnet';

    const args = this.buildArgs(prompt, model, request);
    args.push('--output-format', 'stream-json');

    const inputTokens = 0;
    let outputTokens = 0;

    for await (const chunk of this.execClaudeStream(args)) {
      const event = this.parseStreamChunk(chunk);
      if (event) {
        if (event.delta?.content) {
          outputTokens += Math.ceil(event.delta.content.length / 4);
        }
        yield event;
      }
    }

    // Final done event with usage stats
    yield {
      type: 'done',
      usage: {
        promptTokens: inputTokens,
        completionTokens: outputTokens,
        totalTokens: inputTokens + outputTokens,
      },
    };
  }

  /**
   * Perform health check
   * Uses --version instead of doctor for faster response
   */
  async healthCheck(): Promise<HealthCheckResult> {
    const startTime = Date.now();

    try {
      // Use --version for fast health check (doctor is slow)
      const result = await this.execClaude(['--version']);
      return {
        healthy: result.success && result.stdout.includes('Claude Code'),
        latency: Date.now() - startTime,
        timestamp: new Date(),
      };
    } catch (error) {
      return {
        healthy: false,
        error: error instanceof Error ? error.message : String(error),
        latency: Date.now() - startTime,
        timestamp: new Date(),
      };
    }
  }

  /**
   * Build prompt string from messages
   */
  private buildPrompt(request: LLMRequest): string {
    const parts: string[] = [];

    for (const msg of request.messages) {
      if (msg.role === 'system') {
        parts.push(`[System]: ${msg.content}`);
      } else if (msg.role === 'user') {
        parts.push(`[User]: ${msg.content}`);
      } else if (msg.role === 'assistant') {
        parts.push(`[Assistant]: ${msg.content}`);
      } else if (msg.role === 'tool') {
        // Tool results - include tool name if available
        const toolName = msg.name ? ` (${msg.name})` : '';
        parts.push(`[Tool Result${toolName}]: ${msg.content}`);
      }
    }

    return parts.join('\n\n');
  }

  /**
   * Build CLI arguments
   *
   * Valid model aliases: sonnet, opus, haiku (or full model names)
   * If model is 'sonnet', 'opus', or 'haiku', pass it to --model
   * Otherwise, let Claude Code use its default
   */
  private buildArgs(prompt: string, model: string, _request: LLMRequest): string[] {
    const args = ['-p', prompt];

    // Only add --model for valid aliases or full model names
    const validAliases = ['sonnet', 'opus', 'haiku'];
    if (validAliases.includes(model) || model.startsWith('claude-')) {
      args.push('--model', model);
    }
    // Otherwise, let Claude Code use default model

    if (this.skipPermissions) {
      args.push('--dangerously-skip-permissions');
    }

    // Disable session persistence to avoid conflicts
    args.push('--no-session-persistence');

    return args;
  }

  /**
   * Execute claude CLI and capture output
   */
  private execClaude(args: string[]): Promise<{ success: boolean; stdout: string; stderr: string }> {
    return new Promise((resolve, reject) => {
      const proc = spawn(this.claudePath, args, {
        stdio: ['pipe', 'pipe', 'pipe'],
        env: { ...process.env },
      });

      // CRITICAL: Close stdin immediately - claude CLI waits for stdin to close
      proc.stdin.end();

      let stdout = '';
      let stderr = '';
      let settled = false;

      // Manual timeout handling (spawn timeout option doesn't work reliably)
      const timeoutId = setTimeout(() => {
        if (!settled) {
          settled = true;
          proc.kill('SIGTERM');
          reject(new Error(`Claude CLI timed out after ${this.timeout}ms`));
        }
      }, this.timeout);

      proc.stdout.on('data', (data: Buffer) => {
        stdout += data.toString();
      });

      proc.stderr.on('data', (data: Buffer) => {
        stderr += data.toString();
      });

      proc.on('close', (code: number | null) => {
        if (!settled) {
          settled = true;
          clearTimeout(timeoutId);
          resolve({
            success: code === 0,
            stdout,
            stderr,
          });
        }
      });

      proc.on('error', (error: Error) => {
        if (!settled) {
          settled = true;
          clearTimeout(timeoutId);
          reject(error);
        }
      });
    });
  }

  /**
   * Execute claude CLI with streaming output
   */
  private async *execClaudeStream(args: string[]): AsyncIterable<string> {
    const proc = spawn(this.claudePath, args, {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: { ...process.env },
    });

    // CRITICAL: Close stdin immediately
    proc.stdin.end();

    let buffer = '';
    let timedOut = false;

    // Set up timeout for streaming
    const timeoutId = setTimeout(() => {
      timedOut = true;
      proc.kill('SIGTERM');
    }, this.timeout);

    try {
      for await (const chunk of proc.stdout as AsyncIterable<Buffer>) {
        if (timedOut) {
          throw new Error(`Claude CLI streaming timed out after ${this.timeout}ms`);
        }
        buffer += chunk.toString();

        // Process complete lines
        const lines = buffer.split('\n');
        buffer = lines.pop() || '';

        for (const line of lines) {
          if (line.trim()) {
            yield line;
          }
        }
      }

      // Process remaining buffer
      if (buffer.trim()) {
        yield buffer;
      }
    } finally {
      clearTimeout(timeoutId);
    }
  }

  /**
   * Parse JSON response from claude CLI
   *
   * Expected format from `claude -p --output-format json`:
   * {
   *   "type": "result",
   *   "subtype": "success",
   *   "result": "The actual response content",
   *   "session_id": "...",
   *   "total_cost_usd": 0.123,
   *   "usage": {
   *     "input_tokens": 100,
   *     "output_tokens": 50,
   *     "cache_creation_input_tokens": 0,
   *     "cache_read_input_tokens": 0
   *   },
   *   "modelUsage": { ... }
   * }
   */
  private parseJsonResponse(output: string, model: string): LLMResponse {
    try {
      // Try to parse as JSON first
      const data = JSON.parse(output);

      // Extract input tokens (including cache tokens)
      const inputTokens = (data.usage?.input_tokens || 0) +
        (data.usage?.cache_creation_input_tokens || 0) +
        (data.usage?.cache_read_input_tokens || 0);

      const outputTokens = data.usage?.output_tokens || 0;

      // Get model from modelUsage keys if available
      const actualModel = data.modelUsage
        ? Object.keys(data.modelUsage)[0] || model
        : model;

      return {
        id: data.session_id || data.uuid || `claude-code-${Date.now()}`,
        model: actualModel,
        provider: 'claude-code',
        // Claude CLI uses "result" field for the response content
        content: data.result || data.content || data.message || '',
        usage: {
          promptTokens: inputTokens,
          completionTokens: outputTokens,
          totalTokens: inputTokens + outputTokens,
        },
        cost: {
          // Report actual cost from subscription usage
          promptCost: 0,
          completionCost: 0,
          totalCost: data.total_cost_usd || 0,
          currency: 'USD',
        },
        finishReason: data.is_error ? 'content_filter' : 'stop',
      };
    } catch {
      // Fallback: treat entire output as content
      const estimatedTokens = Math.ceil(output.length / 4);

      return {
        id: `claude-code-${Date.now()}`,
        model,
        provider: 'claude-code',
        content: output.trim(),
        usage: {
          promptTokens: 0,
          completionTokens: estimatedTokens,
          totalTokens: estimatedTokens,
        },
        cost: {
          promptCost: 0,
          completionCost: 0,
          totalCost: 0,
          currency: 'USD',
        },
        finishReason: 'stop',
      };
    }
  }

  /**
   * Parse streaming chunk
   */
  private parseStreamChunk(chunk: string): LLMStreamEvent | null {
    try {
      const data = JSON.parse(chunk);

      // Handle different event types
      if (data.type === 'content' || data.type === 'text') {
        return {
          type: 'content',
          delta: { content: data.content || data.text || data.delta?.content || '' },
        };
      }

      if (data.type === 'message_delta' || data.type === 'content_block_delta') {
        return {
          type: 'content',
          delta: { content: data.delta?.text || data.delta?.content || '' },
        };
      }

      if (data.type === 'done' || data.type === 'message_stop') {
        return {
          type: 'done',
          usage: data.usage,
        };
      }

      if (data.type === 'error') {
        return {
          type: 'error',
          error: new Error(data.error?.message || data.message || 'Unknown error'),
        };
      }

      // Unknown type - try to extract content
      if (data.content || data.text) {
        return {
          type: 'content',
          delta: { content: data.content || data.text },
        };
      }

      return null;
    } catch {
      // Not JSON - treat as raw text content
      if (chunk.trim()) {
        return {
          type: 'content',
          delta: { content: chunk },
        };
      }
      return null;
    }
  }

  /**
   * Clean up resources
   */
  destroy(): void {
    this.removeAllListeners();
    this.initialized = false;
  }
}
