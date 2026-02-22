/**
 * Normalizer Module
 *
 * Converts parsed conversations from various sources into claude-mem's
 * database schema format. Handles type inference, deduplication, and
 * concept extraction.
 */

import * as crypto from 'crypto';
import type {
  ParsedConversation,
  ParsedMessage,
  ObservationType,
  NormalizedObservation,
  NormalizedSession,
  NormalizedUserPrompt
} from './types.js';

export class Normalizer {
  /**
   * Convert a parsed conversation to a normalized session
   */
  toSession(conversation: ParsedConversation): NormalizedSession {
    const firstUserMessage = conversation.messages.find(m => m.role === 'user');
    const lastMessage = conversation.messages[conversation.messages.length - 1];

    return {
      claude_session_id: conversation.id,
      sdk_session_id: `import-${conversation.id}`,
      project: conversation.project,
      user_prompt: firstUserMessage?.content.substring(0, 500) || null,
      started_at: conversation.createdAt.toISOString(),
      started_at_epoch: conversation.createdAt.getTime(),
      completed_at: conversation.updatedAt.toISOString(),
      completed_at_epoch: conversation.updatedAt.getTime(),
      status: 'completed'
    };
  }

  /**
   * Convert parsed messages to normalized observations
   * Each assistant message becomes an observation
   */
  toObservations(conversation: ParsedConversation): NormalizedObservation[] {
    const observations: NormalizedObservation[] = [];
    const sessionId = `import-${conversation.id}`;

    let promptNumber = 0;
    let lastUserMessage: ParsedMessage | null = null;

    for (const message of conversation.messages) {
      if (message.role === 'user') {
        lastUserMessage = message;
        promptNumber++;
        continue;
      }

      if (message.role === 'assistant' && message.content) {
        // Infer observation type from content
        const type = this.inferObservationType(message, lastUserMessage);

        // Extract title (first line or first 50 chars)
        const title = this.extractTitle(message.content);

        // Extract concepts from content
        const concepts = this.extractConcepts(message.content);

        // Extract files mentioned in content
        const files = this.extractFilesFromContent(message.content);

        // Calculate discovery tokens
        const tokens = message.metadata.tokens;
        const discoveryTokens = tokens
          ? (tokens.input || 0) + (tokens.output || 0)
          : 0;

        const observation: NormalizedObservation = {
          sdk_session_id: sessionId,
          project: conversation.project,
          text: message.content.substring(0, 2000), // Truncate for summary
          type,
          title,
          subtitle: lastUserMessage?.content.substring(0, 200) || null,
          facts: null, // Could be extracted with more sophisticated parsing
          narrative: message.content,
          concepts: concepts.length > 0 ? JSON.stringify(concepts) : null,
          files_read: files.length > 0 ? JSON.stringify(files) : null,
          files_modified: null,
          prompt_number: promptNumber,
          discovery_tokens: discoveryTokens,
          created_at: message.timestamp.toISOString(),
          created_at_epoch: message.timestamp.getTime()
        };

        observations.push(observation);
      }
    }

    return observations;
  }

  /**
   * Convert user messages to normalized user prompts
   */
  toUserPrompts(conversation: ParsedConversation): NormalizedUserPrompt[] {
    const prompts: NormalizedUserPrompt[] = [];
    let promptNumber = 0;

    for (const message of conversation.messages) {
      if (message.role === 'user' && message.content) {
        promptNumber++;

        prompts.push({
          claude_session_id: conversation.id,
          prompt_number: promptNumber,
          prompt_text: message.content,
          created_at: message.timestamp.toISOString(),
          created_at_epoch: message.timestamp.getTime()
        });
      }
    }

    return prompts;
  }

  /**
   * Infer observation type from message content
   */
  inferObservationType(
    message: ParsedMessage,
    userMessage?: ParsedMessage | null
  ): ObservationType {
    const content = message.content.toLowerCase();
    const userContent = userMessage?.content.toLowerCase() || '';

    // Bug fix indicators
    if (
      content.includes('fix') ||
      content.includes('bug') ||
      content.includes('error') ||
      content.includes('issue') ||
      userContent.includes('fix') ||
      userContent.includes('bug') ||
      userContent.includes('error')
    ) {
      return 'bugfix';
    }

    // Feature indicators
    if (
      content.includes('implement') ||
      content.includes('add ') ||
      content.includes('create ') ||
      content.includes('new feature') ||
      userContent.includes('implement') ||
      userContent.includes('add ') ||
      userContent.includes('create ')
    ) {
      return 'feature';
    }

    // Refactor indicators
    if (
      content.includes('refactor') ||
      content.includes('clean') ||
      content.includes('reorganize') ||
      content.includes('restructure') ||
      userContent.includes('refactor') ||
      userContent.includes('clean up')
    ) {
      return 'refactor';
    }

    // Discovery indicators
    if (
      content.includes('found') ||
      content.includes('discovered') ||
      content.includes('learned') ||
      content.includes('analyzed') ||
      content.includes('investigated')
    ) {
      return 'discovery';
    }

    // Decision indicators
    if (
      content.includes('decided') ||
      content.includes('chose') ||
      content.includes('will use') ||
      content.includes('recommend') ||
      content.includes('should use')
    ) {
      return 'decision';
    }

    // Default
    return 'change';
  }

  /**
   * Extract title from content (first line or first 50 chars)
   */
  extractTitle(content: string): string | null {
    if (!content) return null;

    // Try to get first meaningful line
    const lines = content.split('\n');
    for (const line of lines) {
      const trimmed = line.trim();
      // Skip empty lines and common prefixes
      if (
        trimmed &&
        !trimmed.startsWith('#') &&
        !trimmed.startsWith('```') &&
        trimmed.length > 5
      ) {
        return trimmed.length > 100
          ? trimmed.substring(0, 97) + '...'
          : trimmed;
      }
    }

    // Fallback to first 50 chars
    const cleaned = content.replace(/\s+/g, ' ').trim();
    return cleaned.length > 100
      ? cleaned.substring(0, 97) + '...'
      : cleaned;
  }

  /**
   * Extract concepts/topics from content
   */
  extractConcepts(content: string): string[] {
    const concepts: Set<string> = new Set();
    const contentLower = content.toLowerCase();

    // Technology keywords
    const techKeywords = [
      'react', 'typescript', 'javascript', 'python', 'rust', 'go',
      'api', 'database', 'sql', 'graphql', 'rest',
      'authentication', 'authorization', 'security',
      'testing', 'unit test', 'integration test',
      'docker', 'kubernetes', 'aws', 'gcp', 'azure',
      'git', 'github', 'ci/cd', 'deployment',
      'performance', 'optimization', 'caching',
      'error handling', 'validation', 'logging'
    ];

    for (const keyword of techKeywords) {
      if (contentLower.includes(keyword)) {
        concepts.add(keyword);
      }
    }

    // Extract code-related concepts
    if (contentLower.includes('function') || contentLower.includes('def ')) {
      concepts.add('functions');
    }
    if (contentLower.includes('class ') || contentLower.includes('interface ')) {
      concepts.add('oop');
    }
    if (contentLower.includes('async') || contentLower.includes('await')) {
      concepts.add('async');
    }

    return Array.from(concepts).slice(0, 10); // Limit to 10 concepts
  }

  /**
   * Extract file paths from content
   */
  extractFilesFromContent(content: string): string[] {
    const files: Set<string> = new Set();

    // Match common file path patterns
    const patterns = [
      // Unix-style paths
      /(?:^|\s)([\/~][\w\-\.\/]+\.\w{1,10})(?:\s|$|:|\)|,)/gm,
      // Relative paths
      /(?:^|\s)(\.{1,2}\/[\w\-\.\/]+\.\w{1,10})(?:\s|$|:|\)|,)/gm,
      // File names with extensions
      /(?:^|\s)([\w\-]+\.[a-z]{2,6})(?:\s|$|:|\)|,)/gm,
      // Backtick-quoted paths
      /`([^`]+\.[a-z]{2,6})`/g
    ];

    for (const pattern of patterns) {
      let match;
      while ((match = pattern.exec(content)) !== null) {
        const path = match[1];
        // Filter out common false positives
        if (
          !path.includes('http') &&
          !path.includes('www.') &&
          !path.startsWith('..')
        ) {
          files.add(path);
        }
      }
    }

    return Array.from(files).slice(0, 20); // Limit to 20 files
  }

  /**
   * Generate a hash for deduplication
   */
  generateConversationHash(conversation: ParsedConversation): string {
    // Hash based on session ID, project, and first/last message
    const firstMsg = conversation.messages[0]?.content || '';
    const lastMsg = conversation.messages[conversation.messages.length - 1]?.content || '';

    const data = `${conversation.id}:${conversation.project}:${firstMsg.substring(0, 100)}:${lastMsg.substring(0, 100)}`;

    return crypto.createHash('md5').update(data).digest('hex');
  }

  /**
   * Check if two conversations are duplicates
   */
  areDuplicates(a: ParsedConversation, b: ParsedConversation): boolean {
    return this.generateConversationHash(a) === this.generateConversationHash(b);
  }

  /**
   * Deduplicate a list of conversations
   */
  deduplicateConversations(conversations: ParsedConversation[]): ParsedConversation[] {
    const seen = new Set<string>();
    const result: ParsedConversation[] = [];

    for (const conv of conversations) {
      const hash = this.generateConversationHash(conv);
      if (!seen.has(hash)) {
        seen.add(hash);
        result.push(conv);
      }
    }

    return result;
  }
}
