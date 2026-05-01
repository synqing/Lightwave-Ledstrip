#!/usr/bin/env node

/**
 * CDN Dependency Verification Script
 * 
 * Verifies that the production build contains no external CDN dependencies.
 * Checks for common CDN patterns in built files.
 */

import { readFileSync, readdirSync, statSync } from 'fs';
import { join, extname } from 'path';
import { fileURLToPath } from 'url';
import { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const projectRoot = join(__dirname, '..');

// CDN patterns to detect
const CDN_PATTERNS = [
  /https?:\/\/(cdn|unpkg|jsdelivr|cdnjs)\./gi,
  /https?:\/\/fonts\.googleapis\.com/gi,
  /https?:\/\/fonts\.gstatic\.com/gi,
  /https?:\/\/code\.iconify\.design/gi,
  /https?:\/\/api\.iconify\.design/gi,
  /https?:\/\/cdn\.tailwindcss\.com/gi,
];

// Recursively get all files in directory
function getAllFiles(dir, extensions = ['.html', '.js', '.css']) {
  const files = [];
  
  try {
    const entries = readdirSync(dir, { withFileTypes: true });
    
    for (const entry of entries) {
      const fullPath = join(dir, entry.name);
      
      // Skip node_modules and .git
      if (entry.name === 'node_modules' || entry.name === '.git') {
        continue;
      }
      
      if (entry.isDirectory()) {
        files.push(...getAllFiles(fullPath, extensions));
      } else if (entry.isFile()) {
        const ext = extname(entry.name);
        if (extensions.includes(ext) || extensions.length === 0) {
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

// Find line number for a match
function getLineNumber(content, index) {
  return content.substring(0, index).split('\n').length;
}

// Main verification
function verifyNoCDNs() {
  console.log('🔍 Verifying no CDN dependencies in build output...\n');
  
  const distDir = join(projectRoot, 'dist');
  const v2DataDir = join(projectRoot, '../v2/data');
  
  // Check both possible output directories
  const checkDirs = [];
  
    try {
      if (statSync(distDir).isDirectory()) {
        checkDirs.push({ path: distDir, name: 'dist' });
      }
    } catch {
      // dist doesn't exist, that's okay
    }
    
    try {
      if (statSync(v2DataDir).isDirectory()) {
        checkDirs.push({ path: v2DataDir, name: 'v2/data' });
      }
    } catch {
      // v2/data doesn't exist, that's okay
    }
  
  if (checkDirs.length === 0) {
    console.log('⚠️  No build output directories found.');
    console.log('   Run "npm run build" first to generate build output.\n');
    return 0;
  }
  
  const violations = [];
  
  // Check each directory
  for (const { path: checkDir, name } of checkDirs) {
    const files = getAllFiles(checkDir);
    
    console.log(`Checking ${name} directory (${files.length} files)...`);
    
    for (const file of files) {
      try {
        const content = readFileSync(file, 'utf-8');
        
        for (const pattern of CDN_PATTERNS) {
          const matches = Array.from(content.matchAll(pattern));
          
          for (const match of matches) {
            const line = getLineNumber(content, match.index);
            violations.push({
              file: file.replace(projectRoot + '/', ''),
              line,
              match: match[0],
              pattern: pattern.source,
            });
          }
        }
      } catch (err) {
        // Skip binary files or unreadable files
        if (err.code !== 'EISDIR') {
          console.warn(`Warning: Could not read ${file}: ${err.message}`);
        }
      }
    }
  }
  
  // Report results
  if (violations.length === 0) {
    console.log('\n✅ No CDN dependencies found in build output!\n');
    return 0;
  }
  
  console.error(`\n❌ Found ${violations.length} CDN reference(s):\n`);
  
  for (const violation of violations) {
    console.error(`File: ${violation.file}:${violation.line}`);
    console.error(`Match: ${violation.match}`);
    console.error(`Pattern: ${violation.pattern}\n`);
  }
  
  console.error('💡 Fix: Replace CDN references with local bundled assets.\n');
  console.error('   - Tailwind: Use local PostCSS build');
  console.error('   - Icons: Use lucide-react (already bundled)');
  console.error('   - Fonts: Self-host or use system fonts\n');
  
  return 1;
}

// Run verification
try {
  const exitCode = verifyNoCDNs();
  process.exit(exitCode);
} catch (error) {
  console.error('❌ Verification failed:', error.message);
  process.exit(1);
}

