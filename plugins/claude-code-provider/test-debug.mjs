/**
 * Debug test for ClaudeCodeProvider
 */

import { spawn } from 'node:child_process';

async function main() {
  console.log('=== Debug: Direct Claude CLI Test ===\n');

  const prompt = 'Say "Hello from Claude Code Provider!" exactly, nothing else.';
  const args = [
    '-p', prompt,
    '--output-format', 'json',
    '--dangerously-skip-permissions',
    '--no-session-persistence'
  ];

  console.log('Command: claude', args.join(' '));
  console.log('\nExecuting...\n');

  const proc = spawn('claude', args, {
    timeout: 60000,
    env: { ...process.env },
  });

  let stdout = '';
  let stderr = '';

  proc.stdout.on('data', (data) => {
    stdout += data.toString();
    console.log('[stdout chunk]:', data.toString().substring(0, 200));
  });

  proc.stderr.on('data', (data) => {
    stderr += data.toString();
    console.log('[stderr chunk]:', data.toString().substring(0, 200));
  });

  proc.on('close', (code) => {
    console.log('\n--- Results ---');
    console.log('Exit code:', code);
    console.log('Stdout length:', stdout.length);
    console.log('Stderr length:', stderr.length);

    if (stdout) {
      console.log('\nStdout (first 500 chars):', stdout.substring(0, 500));
    }
    if (stderr) {
      console.log('\nStderr:', stderr);
    }

    // Try to parse
    try {
      const data = JSON.parse(stdout);
      console.log('\n✅ Parsed JSON successfully');
      console.log('Result:', data.result?.substring(0, 100) || data.content?.substring(0, 100));
    } catch (e) {
      console.log('\n❌ Failed to parse JSON:', e.message);
    }
  });

  proc.on('error', (error) => {
    console.log('Process error:', error);
  });
}

main().catch(console.error);
