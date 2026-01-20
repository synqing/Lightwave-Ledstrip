/**
 * Error handling tests for ClaudeCodeProvider
 */

import { ClaudeCodeProvider } from './dist/index.js';

async function testErrorHandling() {
  console.log('=== Error Handling Tests ===\n');

  // Test 1: Invalid claude path should fail initialization
  console.log('1. Testing invalid claude path...');
  try {
    const provider = new ClaudeCodeProvider({
      config: { provider: 'claude-code', model: 'sonnet' },
      claudePath: '/nonexistent/claude',
    });
    await provider.initialize();
    console.log('   ❌ Should have thrown an error');
  } catch (error) {
    if (error.message.includes('Claude CLI not found') || error.message.includes('ENOENT')) {
      console.log('   ✅ Correctly throws error for invalid path');
    } else {
      console.log(`   ⚠️  Unexpected error: ${error.message}`);
    }
  }

  // Test 2: Empty messages array
  console.log('\n2. Testing empty messages array...');
  try {
    const provider = new ClaudeCodeProvider({
      config: { provider: 'claude-code', model: 'default' },
      skipPermissions: true,
    });
    await provider.initialize();
    await provider.complete({ messages: [] });
    console.log('   ❌ Should have thrown an error');
    provider.destroy();
  } catch (error) {
    if (error.message.includes('at least one message')) {
      console.log('   ✅ Correctly throws error for empty messages');
    } else {
      console.log(`   ⚠️  Unexpected error: ${error.message}`);
    }
  }

  // Test 3: Very short timeout should timeout (skip this - too slow)
  console.log('\n3. Skipping timeout test (would take too long)');

  // Test 4: Provider re-initialization
  console.log('\n4. Testing re-initialization...');
  try {
    const provider = new ClaudeCodeProvider({
      config: { provider: 'claude-code', model: 'sonnet' },
      skipPermissions: true,
    });
    await provider.initialize();
    await provider.initialize(); // Should be idempotent
    console.log('   ✅ Re-initialization is safe (idempotent)');
    provider.destroy();
  } catch (error) {
    console.log(`   ❌ Error: ${error.message}`);
  }

  // Test 5: Request without initialization
  console.log('\n5. Testing auto-initialization on request...');
  try {
    const provider = new ClaudeCodeProvider({
      config: { provider: 'claude-code', model: 'default' },
      skipPermissions: true,
    });
    // Don't call initialize - should auto-init
    const response = await provider.complete({
      messages: [{ role: 'user', content: 'Say "auto"' }],
    });
    if (response.content) {
      console.log('   ✅ Auto-initializes on first request');
    }
    provider.destroy();
  } catch (error) {
    console.log(`   ❌ Error: ${error.message}`);
  }

  console.log('\n=== Error Handling Tests Complete ===');
}

testErrorHandling().catch(console.error);
