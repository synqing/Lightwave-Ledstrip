/**
 * Importer Module
 *
 * Orchestrates the import of parsed conversations into claude-mem's SQLite database.
 * Handles batch processing, progress reporting, and FTS rebuilding.
 */

import { Scanner } from './Scanner.js';
import { ClaudeCLIParser } from './parsers/ClaudeCLIParser.js';
import { Normalizer } from './Normalizer.js';
import type {
  ImportOptions,
  ImportResult,
  ImportError,
  ImportSource,
  ParsedConversation,
  ProgressCallback
} from './types.js';

// Database types - we'll import from the parent package
interface SessionStore {
  importSdkSession(session: {
    claude_session_id: string;
    sdk_session_id: string;
    project: string;
    user_prompt: string | null;
    started_at: string;
    started_at_epoch: number;
    completed_at: string | null;
    completed_at_epoch: number | null;
    status: string;
  }): { imported: boolean; id: number };

  importObservation(obs: {
    sdk_session_id: string;
    project: string;
    text: string | null;
    type: string;
    title: string | null;
    subtitle: string | null;
    facts: string | null;
    narrative: string | null;
    concepts: string | null;
    files_read: string | null;
    files_modified: string | null;
    prompt_number: number | null;
    discovery_tokens: number;
    created_at: string;
    created_at_epoch: number;
  }): { imported: boolean; id: number };

  importUserPrompt(prompt: {
    claude_session_id: string;
    prompt_number: number;
    prompt_text: string;
    created_at: string;
    created_at_epoch: number;
  }): { imported: boolean; id: number };
}

export class Importer {
  private scanner: Scanner;
  private parser: ClaudeCLIParser;
  private normalizer: Normalizer;
  private store: SessionStore | null = null;

  constructor() {
    this.scanner = new Scanner();
    this.parser = new ClaudeCLIParser();
    this.normalizer = new Normalizer();
  }

  /**
   * Set the database store (allows dependency injection)
   */
  setStore(store: SessionStore): void {
    this.store = store;
  }

  /**
   * Import conversations from specified source(s)
   */
  async import(
    options: ImportOptions,
    onProgress?: ProgressCallback
  ): Promise<ImportResult> {
    const startTime = Date.now();
    const result: ImportResult = {
      source: options.source === 'all' ? 'claude-cli' : options.source,
      sessionsImported: 0,
      sessionsSkipped: 0,
      observationsImported: 0,
      observationsSkipped: 0,
      promptsImported: 0,
      promptsSkipped: 0,
      errors: [],
      duration: 0,
      dryRun: options.dryRun || false
    };

    try {
      // Currently only Claude CLI is supported
      if (options.source === 'claude-cli' || options.source === 'all') {
        await this.importClaudeCLI(options, result, onProgress);
      }

      // TODO: Add other sources in Wave 2
      // if (options.source === 'cursor' || options.source === 'all') {
      //   await this.importCursor(options, result, onProgress);
      // }
    } catch (error) {
      result.errors.push({
        file: 'import',
        message: error instanceof Error ? error.message : String(error),
        error: error instanceof Error ? error : undefined
      });
    }

    result.duration = Date.now() - startTime;

    if (onProgress) {
      onProgress({
        phase: 'complete',
        current: 100,
        total: 100,
        message: `Import complete. ${result.sessionsImported} sessions, ${result.observationsImported} observations imported.`
      });
    }

    return result;
  }

  /**
   * Import Claude CLI conversations
   */
  private async importClaudeCLI(
    options: ImportOptions,
    result: ImportResult,
    onProgress?: ProgressCallback
  ): Promise<void> {
    // Scan for files
    if (onProgress) {
      onProgress({
        phase: 'scanning',
        current: 0,
        total: 100,
        message: 'Scanning for Claude CLI conversation files...'
      });
    }

    const files = await this.scanner.getClaudeCLIFiles();

    if (files.length === 0) {
      result.errors.push({
        file: 'scanner',
        message: 'No Claude CLI conversation files found in ~/.claude/projects/'
      });
      return;
    }

    // Parse each file
    let processedFiles = 0;
    const allConversations: ParsedConversation[] = [];

    for (const filePath of files) {
      try {
        if (onProgress) {
          onProgress({
            phase: 'parsing',
            current: processedFiles,
            total: files.length,
            currentFile: filePath,
            message: `Parsing file ${processedFiles + 1}/${files.length}`
          });
        }

        const conversations = await this.parser.parseFile(filePath);

        // Apply filters
        const filtered = this.applyFilters(conversations, options);
        allConversations.push(...filtered);

        processedFiles++;
      } catch (error) {
        result.errors.push({
          file: filePath,
          message: error instanceof Error ? error.message : String(error),
          error: error instanceof Error ? error : undefined
        });
      }
    }

    // Deduplicate
    if (onProgress) {
      onProgress({
        phase: 'normalizing',
        current: 0,
        total: allConversations.length,
        message: 'Deduplicating and normalizing...'
      });
    }

    const uniqueConversations = this.normalizer.deduplicateConversations(allConversations);

    // Import to database
    if (!options.dryRun && this.store) {
      await this.importToDatabase(uniqueConversations, result, onProgress);
    } else if (options.dryRun) {
      // In dry run mode, just count what would be imported
      for (const conv of uniqueConversations) {
        result.sessionsImported++;
        result.observationsImported += conv.messages.filter(m => m.role === 'assistant').length;
        result.promptsImported += conv.messages.filter(m => m.role === 'user').length;
      }
    }
  }

  /**
   * Apply filters to conversations
   */
  private applyFilters(
    conversations: ParsedConversation[],
    options: ImportOptions
  ): ParsedConversation[] {
    return conversations.filter(conv => {
      // Project filter
      if (options.project && conv.project !== options.project) {
        return false;
      }

      // Date range filter
      if (options.dateRange) {
        if (options.dateRange.start && conv.createdAt < options.dateRange.start) {
          return false;
        }
        if (options.dateRange.end && conv.createdAt > options.dateRange.end) {
          return false;
        }
      }

      return true;
    });
  }

  /**
   * Import conversations to database
   */
  private async importToDatabase(
    conversations: ParsedConversation[],
    result: ImportResult,
    onProgress?: ProgressCallback
  ): Promise<void> {
    if (!this.store) {
      throw new Error('Database store not initialized');
    }

    let processed = 0;
    const total = conversations.length;
    const batchSize = 100;

    for (const conv of conversations) {
      try {
        // Import session
        const session = this.normalizer.toSession(conv);
        const sessionResult = this.store.importSdkSession({
          claude_session_id: session.claude_session_id,
          sdk_session_id: session.sdk_session_id,
          project: session.project,
          user_prompt: session.user_prompt,
          started_at: session.started_at,
          started_at_epoch: session.started_at_epoch,
          completed_at: session.completed_at,
          completed_at_epoch: session.completed_at_epoch,
          status: session.status
        });

        if (sessionResult.imported) {
          result.sessionsImported++;
        } else {
          result.sessionsSkipped++;
        }

        // Import observations (only if session was imported or we want to add new ones)
        if (sessionResult.imported) {
          const observations = this.normalizer.toObservations(conv);
          for (const obs of observations) {
            const obsResult = this.store.importObservation({
              sdk_session_id: obs.sdk_session_id,
              project: obs.project,
              text: obs.text,
              type: obs.type,
              title: obs.title,
              subtitle: obs.subtitle,
              facts: obs.facts,
              narrative: obs.narrative,
              concepts: obs.concepts,
              files_read: obs.files_read,
              files_modified: obs.files_modified,
              prompt_number: obs.prompt_number,
              discovery_tokens: obs.discovery_tokens,
              created_at: obs.created_at,
              created_at_epoch: obs.created_at_epoch
            });

            if (obsResult.imported) {
              result.observationsImported++;
            } else {
              result.observationsSkipped++;
            }
          }

          // Import user prompts
          const prompts = this.normalizer.toUserPrompts(conv);
          for (const prompt of prompts) {
            const promptResult = this.store.importUserPrompt({
              claude_session_id: prompt.claude_session_id,
              prompt_number: prompt.prompt_number,
              prompt_text: prompt.prompt_text,
              created_at: prompt.created_at,
              created_at_epoch: prompt.created_at_epoch
            });

            if (promptResult.imported) {
              result.promptsImported++;
            } else {
              result.promptsSkipped++;
            }
          }
        }

        processed++;

        if (onProgress && processed % batchSize === 0) {
          onProgress({
            phase: 'importing',
            current: processed,
            total,
            message: `Imported ${processed}/${total} conversations`
          });
        }
      } catch (error) {
        result.errors.push({
          file: conv.id,
          message: error instanceof Error ? error.message : String(error),
          error: error instanceof Error ? error : undefined
        });
      }
    }
  }

  /**
   * Import a single file
   */
  async importFile(
    filePath: string,
    options: ImportOptions = { source: 'claude-cli' },
    onProgress?: ProgressCallback
  ): Promise<ImportResult> {
    const startTime = Date.now();
    const result: ImportResult = {
      source: 'claude-cli',
      sessionsImported: 0,
      sessionsSkipped: 0,
      observationsImported: 0,
      observationsSkipped: 0,
      promptsImported: 0,
      promptsSkipped: 0,
      errors: [],
      duration: 0,
      dryRun: options.dryRun || false
    };

    try {
      const conversations = await this.parser.parseFile(filePath, onProgress);
      const filtered = this.applyFilters(conversations, options);
      const unique = this.normalizer.deduplicateConversations(filtered);

      if (!options.dryRun && this.store) {
        await this.importToDatabase(unique, result, onProgress);
      } else if (options.dryRun) {
        for (const conv of unique) {
          result.sessionsImported++;
          result.observationsImported += conv.messages.filter(m => m.role === 'assistant').length;
          result.promptsImported += conv.messages.filter(m => m.role === 'user').length;
        }
      }
    } catch (error) {
      result.errors.push({
        file: filePath,
        message: error instanceof Error ? error.message : String(error),
        error: error instanceof Error ? error : undefined
      });
    }

    result.duration = Date.now() - startTime;
    return result;
  }

  /**
   * Preview import (dry run)
   */
  async preview(options: ImportOptions): Promise<ImportResult> {
    return this.import({ ...options, dryRun: true });
  }
}
