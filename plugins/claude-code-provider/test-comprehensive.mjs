/**
 * Comprehensive test script for ClaudeCodeProvider
 * Tests edge cases, error handling, and all message types
 */

import { ClaudeCodeProvider } from './dist/index.js';

const TESTS = [];
let passed = 0;
let failed = 0;

function test(name, fn) {
  TESTS.push({ name, fn });
}

async function runTests() {
  console.log('=== Claude Code Provider Comprehensive Tests ===\n');

  for (const { name, fn } of TESTS) {
    try {
      await fn();
      passed++;
      console.log(`✅ ${name}`);
    } catch (error) {
      failed++;
      console.log(`❌ ${name}`);
      console.log(`   Error: ${error.message}\n`);
    }
  }

  console.log(`\n=== Results: ${passed} passed, ${failed} failed ===`);
  process.exit(failed > 0 ? 1 : 0);
}

// Test 1: Constructor with default options
test('Constructor accepts minimal config', () => {
  const provider = new ClaudeCodeProvider({
    config: { provider: 'claude-code', model: 'sonnet' },
  });
  if (provider.name !== 'claude-code') throw new Error('Wrong provider name');
  if (provider.config.model !== 'sonnet') throw new Error('Wrong model');
  provider.destroy();
});

// Test 2: Constructor with custom options
test('Constructor accepts custom options', () => {
  const provider = new ClaudeCodeProvider({
    config: { provider: 'claude-code', model: 'opus' },
    claudePath: '/custom/path/claude',
    timeout: 60000,
    skipPermissions: false,
  });
  if (provider.config.model !== 'opus') throw new Error('Wrong model');
  provider.destroy();
});

// Test 3: Capabilities are correct
test('Capabilities include all supported models', () => {
  const provider = new ClaudeCodeProvider({
    config: { provider: 'claude-code', model: 'sonnet' },
  });
  const caps = provider.capabilities;
  if (!caps.supportedModels.includes('sonnet')) throw new Error('Missing sonnet');
  if (!caps.supportedModels.includes('opus')) throw new Error('Missing opus');
  if (!caps.supportedModels.includes('haiku')) throw new Error('Missing haiku');
  if (caps.maxOutputTokens.sonnet !== 64000) throw new Error('Wrong max output for sonnet');
  if (caps.maxOutputTokens.haiku !== 8192) throw new Error('Wrong max output for haiku');
  provider.destroy();
});

// Test 4: Initialization works
test('Initialize succeeds with valid claude CLI', async () => {
  const provider = new ClaudeCodeProvider({
    config: { provider: 'claude-code', model: 'sonnet' },
    skipPermissions: true,
  });
  await provider.initialize();
  provider.destroy();
});

// Test 5: Health check works
test('Health check returns healthy status', async () => {
  const provider = new ClaudeCodeProvider({
    config: { provider: 'claude-code', model: 'sonnet' },
    skipPermissions: true,
  });
  await provider.initialize();
  const health = await provider.healthCheck();
  if (!health.healthy) throw new Error('Health check failed');
  if (typeof health.latency !== 'number') throw new Error('Missing latency');
  if (!(health.timestamp instanceof Date)) throw new Error('Invalid timestamp');
  provider.destroy();
});

// Test 6: Completion with system message
test('Completion handles system message', async () => {
  const provider = new ClaudeCodeProvider({
    config: { provider: 'claude-code', model: 'default' },
    skipPermissions: true,
  });
  await provider.initialize();
  const response = await provider.complete({
    messages: [
      { role: 'system', content: 'You are a helpful assistant. Respond with exactly "OK".' },
      { role: 'user', content: 'Are you ready?' },
    ],
  });
  if (!response.content) throw new Error('Empty response');
  if (!response.id) throw new Error('Missing response ID');
  if (!response.usage) throw new Error('Missing usage stats');
  provider.destroy();
});

// Test 7: Response has correct structure
test('Response contains all required fields', async () => {
  const provider = new ClaudeCodeProvider({
    config: { provider: 'claude-code', model: 'default' },
    skipPermissions: true,
  });
  await provider.initialize();
  const response = await provider.complete({
    messages: [{ role: 'user', content: 'Say "test"' }],
  });

  // Check required fields
  if (typeof response.id !== 'string') throw new Error('id must be string');
  if (typeof response.model !== 'string') throw new Error('model must be string');
  if (response.provider !== 'claude-code') throw new Error('provider must be claude-code');
  if (typeof response.content !== 'string') throw new Error('content must be string');
  if (typeof response.usage.promptTokens !== 'number') throw new Error('promptTokens must be number');
  if (typeof response.usage.completionTokens !== 'number') throw new Error('completionTokens must be number');
  if (typeof response.usage.totalTokens !== 'number') throw new Error('totalTokens must be number');

  provider.destroy();
});

// Test 8: Destroy cleans up
test('Destroy cleans up resources', async () => {
  const provider = new ClaudeCodeProvider({
    config: { provider: 'claude-code', model: 'sonnet' },
    skipPermissions: true,
  });
  await provider.initialize();
  provider.destroy();
  // Re-initialization should work after destroy
  await provider.initialize();
  provider.destroy();
});

// Test 9: Event emission
test('Provider emits events', async () => {
  const provider = new ClaudeCodeProvider({
    config: { provider: 'claude-code', model: 'default' },
    skipPermissions: true,
  });

  let initEventFired = false;
  let responseEventFired = false;

  provider.on('initialized', () => { initEventFired = true; });
  provider.on('response', () => { responseEventFired = true; });

  await provider.initialize();
  if (!initEventFired) throw new Error('initialized event not fired');

  await provider.complete({
    messages: [{ role: 'user', content: 'Say "hi"' }],
  });
  if (!responseEventFired) throw new Error('response event not fired');

  provider.destroy();
});

// Run all tests
runTests().catch(console.error);
