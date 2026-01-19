/**
 * Claude Code Provider for Claude-Flow
 *
 * Enables Claude-Flow to use the `claude` CLI (Max subscription)
 * instead of requiring separate API keys.
 *
 * @module @lightwave/claude-code-provider
 */
import { EventEmitter } from 'node:events';
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
export declare class ClaudeCodeProvider extends EventEmitter {
    readonly name: LLMProvider;
    readonly capabilities: ProviderCapabilities;
    config: LLMProviderConfig;
    private claudePath;
    private timeout;
    private skipPermissions;
    private initialized;
    constructor(options: ClaudeCodeProviderOptions);
    /**
     * Initialize the provider
     */
    initialize(): Promise<void>;
    /**
     * Complete a request (non-streaming)
     */
    complete(request: LLMRequest): Promise<LLMResponse>;
    /**
     * Stream complete a request
     */
    streamComplete(request: LLMRequest): AsyncIterable<LLMStreamEvent>;
    /**
     * Perform health check
     * Uses --version instead of doctor for faster response
     */
    healthCheck(): Promise<HealthCheckResult>;
    /**
     * Build prompt string from messages
     */
    private buildPrompt;
    /**
     * Build CLI arguments
     *
     * Valid model aliases: sonnet, opus, haiku (or full model names)
     * If model is 'sonnet', 'opus', or 'haiku', pass it to --model
     * Otherwise, let Claude Code use its default
     */
    private buildArgs;
    /**
     * Execute claude CLI and capture output
     */
    private execClaude;
    /**
     * Execute claude CLI with streaming output
     */
    private execClaudeStream;
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
    private parseJsonResponse;
    /**
     * Parse streaming chunk
     */
    private parseStreamChunk;
    /**
     * Clean up resources
     */
    destroy(): void;
}
//# sourceMappingURL=claude-code-provider.d.ts.map