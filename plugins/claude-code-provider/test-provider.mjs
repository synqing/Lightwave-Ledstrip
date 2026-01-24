/**
 * Test script for ClaudeCodeProvider
 */

import { ClaudeCodeProvider } from './dist/index.js';

async function main() {
  console.log('=== Claude Code Provider Test ===\n');

  // Create provider (use default model - let Claude Code decide)
  const provider = new ClaudeCodeProvider({
    config: {
      provider: 'claude-code',
      model: 'default', // Let Claude Code use its default model
    },
    skipPermissions: true,
  });

  // Test 1: Initialize
  console.log('1. Testing initialization...');
  try {
    await provider.initialize();
    console.log('   ✅ Initialized successfully\n');
  } catch (error) {
    console.log(`   ❌ Failed: ${error.message}\n`);
    process.exit(1);
  }

  // Test 2: Health check
  console.log('2. Testing health check...');
  const health = await provider.healthCheck();
  console.log(`   Status: ${health.healthy ? '✅ Healthy' : '❌ Unhealthy'}`);
  console.log(`   Latency: ${health.latency}ms\n`);

  // Test 3: Simple completion
  console.log('3. Testing completion...');
  try {
    const response = await provider.complete({
      messages: [
        { role: 'user', content: 'Say "Hello from Claude Code Provider!" exactly, nothing else.' },
      ],
    });
    console.log(`   ✅ Response: ${response.content.substring(0, 150)}`);
    console.log(`   Tokens: ${response.usage.totalTokens}`);
    console.log(`   Model: ${response.model}`);
    console.log(`   Cost: $${response.cost?.totalCost?.toFixed(4) || 0}\n`);
  } catch (error) {
    console.log(`   ❌ Failed: ${error.message}\n`);
    console.log(`   Stack: ${error.stack}\n`);
  }

  // Skip streaming test for now (add back later if needed)
  console.log('4. Skipping streaming test (run manually if needed)\n');

  // Cleanup
  provider.destroy();
  console.log('=== Tests Complete ===');
}

main().catch(console.error);
