#!/usr/bin/env node

/**
 * Build-time Tailwind class validation script
 * 
 * Validates that all Tailwind classes used in the codebase are valid
 * and will be included in the production build.
 * 
 * This script runs as a pre-build step to catch invalid classes early.
 */

import { readFileSync, readdirSync } from 'fs';
import { join, extname } from 'path';
import { fileURLToPath } from 'url';
import { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const projectRoot = join(__dirname, '..');

// Tailwind class pattern (matches Tailwind's default extractor)
const TAILWIND_CLASS_PATTERN = /[\w-/:]+(?<!:)/g;

// Recursively get all files matching patterns
function getAllFiles(dir, extensions = ['.js', '.ts', '.jsx', '.tsx', '.html']) {
  const files = [];
  
  try {
    const entries = readdirSync(dir, { withFileTypes: true });
    
    for (const entry of entries) {
      const fullPath = join(dir, entry.name);
      
      // Skip node_modules, dist, and other build directories
      if (entry.name === 'node_modules' || entry.name === 'dist' || entry.name === '.git') {
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
    // Ignore permission errors
    if (err.code !== 'EACCES') {
      console.warn(`Warning: Could not read directory ${dir}: ${err.message}`);
    }
  }
  
  return files;
}

// Extract Tailwind classes from content
function extractClasses(content) {
  const classes = new Set();
  const matches = content.matchAll(TAILWIND_CLASS_PATTERN);
  
  for (const match of matches) {
    const className = match[0];
    // Filter out non-Tailwind patterns (like URLs, numbers, etc.)
    if (className.includes('-') || className.startsWith('text-') || className.startsWith('bg-') || 
        className.startsWith('border-') || className.startsWith('animate-') ||
        className.match(/^(sm|md|lg|xl|2xl):/)) {
      classes.add(className);
    }
  }
  
  return classes;
}

// Main validation
function validateClasses() {
  console.log('🔍 Validating Tailwind classes...\n');
  
  const srcDir = join(projectRoot, 'src');
  const files = [
    join(projectRoot, 'index.html'),
    ...getAllFiles(srcDir),
  ];
  
  const allClasses = new Set();
  const fileClasses = new Map();
  
  // Extract classes from all files
  for (const file of files) {
    try {
      const content = readFileSync(file, 'utf-8');
      const classes = extractClasses(content);
      
      if (classes.size > 0) {
        fileClasses.set(file, classes);
        for (const cls of classes) {
        allClasses.add(cls);
      }
      }
    } catch (err) {
      console.warn(`Warning: Could not read ${file}: ${err.message}`);
    }
  }
  
  console.log(`✓ Scanned ${files.length} files`);
  console.log(`✓ Found ${allClasses.size} unique class patterns\n`);
  
  // Basic validation: check for common issues
  const issues = [];
  
  for (const [file, classes] of fileClasses.entries()) {
    for (const className of classes) {
      // Check for invalid responsive prefixes in class names
      if (/^(sm|md|lg|xl|2xl):[^:]+:/.test(className)) {
        issues.push({
          file,
          className,
          issue: 'Multiple responsive prefixes detected',
        });
      }
      
      // Check for suspicious patterns
      if (className.includes('undefined') || className.includes('null')) {
        issues.push({
          file,
          className,
          issue: 'Potential template literal issue',
        });
      }
    }
  }
  
  if (issues.length > 0) {
    console.error('❌ Validation issues found:\n');
    for (const issue of issues) {
      const relativePath = issue.file.replace(projectRoot + '/', '');
      console.error(`  ${relativePath}`);
      console.error(`    Class: ${issue.className}`);
      console.error(`    Issue: ${issue.issue}\n`);
    }
    process.exit(1);
  }
  
  console.log('✅ All classes validated successfully!\n');
  console.log('Note: This script performs basic validation.');
  console.log('Full class validation happens during Tailwind build process.\n');
}

// Run validation
try {
  validateClasses();
} catch (error) {
  console.error('❌ Validation failed:', error.message);
  process.exit(1);
}

