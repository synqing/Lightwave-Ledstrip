#!/usr/bin/env node

/**
 * Inline Responsive Style Detection Script
 * 
 * Scans all source files for invalid inline responsive styles
 * (e.g., style="sm:width:12rem") that should use Tailwind classes instead.
 */

import { readFileSync, readdirSync } from 'fs';
import { join, extname } from 'path';
import { fileURLToPath } from 'url';
import { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const projectRoot = join(__dirname, '..');

// Pattern to match invalid inline responsive styles
const INVALID_PATTERN = /style\s*=\s*["'][^"']*(sm|md|lg|xl|2xl):[^"']*["']/gi;

// Recursively get all TypeScript/JavaScript/HTML files
function getAllFiles(dir, extensions = ['.ts', '.tsx', '.js', '.jsx', '.html']) {
  const files = [];
  
  try {
    const entries = readdirSync(dir, { withFileTypes: true });
    
    for (const entry of entries) {
      const fullPath = join(dir, entry.name);
      
      // Skip node_modules, dist, and other build directories
      if (entry.name === 'node_modules' || entry.name === 'dist' || 
          entry.name === '.git' || entry.name === 'scripts') {
        continue;
      }
      
      if (entry.isDirectory()) {
        files.push(...getAllFiles(fullPath, extensions));
      } else if (entry.isFile()) {
        const ext = extname(entry.name);
        if (extensions.includes(ext)) {
          files.push(fullPath);
        }
      }
    }
  } catch (err) {
    if (err.code !== 'EACCES') {
      console.warn(`Warning: Could not read directory ${dir}: ${err.message}`);
    }
  }
  
  return files;
}

// Find line number for a match in content
function getLineNumber(content, index) {
  return content.substring(0, index).split('\n').length;
}

// Extract context around match
function getContext(content, index, contextLines = 2) {
  const lines = content.split('\n');
  const lineNum = getLineNumber(content, index) - 1;
  const start = Math.max(0, lineNum - contextLines);
  const end = Math.min(lines.length, lineNum + contextLines + 1);
  
  return {
    line: lineNum + 1,
    context: lines.slice(start, end).join('\n'),
  };
}

// Main detection
function checkInlineStyles() {
  console.log('🔍 Checking for invalid inline responsive styles...\n');
  
  const srcDir = join(projectRoot, 'src');
  const files = [
    join(projectRoot, 'index.html'),
    ...getAllFiles(srcDir),
  ];
  
  const violations = [];
  
  // Scan all files
  for (const file of files) {
    try {
      const content = readFileSync(file, 'utf-8');
      const matches = Array.from(content.matchAll(INVALID_PATTERN));
      
      for (const match of matches) {
        const { line, context } = getContext(content, match.index);
        violations.push({
          file,
          line,
          match: match[0],
          context,
        });
      }
    } catch (err) {
      console.warn(`Warning: Could not read ${file}: ${err.message}`);
    }
  }
  
  // Report results
  if (violations.length === 0) {
    console.log('✅ No invalid inline responsive styles found!\n');
    return 0;
  }
  
  console.error(`❌ Found ${violations.length} violation(s):\n`);
  
  for (const violation of violations) {
    const relativePath = violation.file.replace(projectRoot + '/', '');
    console.error(`File: ${relativePath}:${violation.line}`);
    console.error(`Match: ${violation.match}`);
    console.error(`Context:\n${violation.context}\n`);
    console.error('Fix: Replace inline style with Tailwind classes (e.g., className="w-40 sm:w-48")\n');
  }
  
  console.error('💡 Tip: Use Tailwind classes instead of inline responsive styles.');
  console.error('   Example: className="w-40 sm:w-48" instead of style="width:10rem; sm:width:12rem"\n');
  
  return 1;
}

// Run check
try {
  const exitCode = checkInlineStyles();
  process.exit(exitCode);
} catch (error) {
  console.error('❌ Check failed:', error.message);
  process.exit(1);
}

