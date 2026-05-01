#!/usr/bin/env node

/**
 * Performance Benchmarking Script
 * 
 * Measures bundle size, build time, and other performance metrics
 * to track optimization progress.
 */

import { readdirSync, statSync } from 'fs';
import { join, extname } from 'path';
import { fileURLToPath } from 'url';
import { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const projectRoot = join(__dirname, '..');

// Recursively get all files in directory
function getAllFiles(dir) {
  const files = [];
  
  try {
    const entries = readdirSync(dir, { withFileTypes: true });
    
    for (const entry of entries) {
      const fullPath = join(dir, entry.name);
      
      if (entry.name === 'node_modules' || entry.name === '.git') {
        continue;
      }
      
      if (entry.isDirectory()) {
        files.push(...getAllFiles(fullPath));
      } else if (entry.isFile()) {
        files.push(fullPath);
      }
    }
  } catch {
    // Ignore permission errors
  }
  
  return files;
}

// Get file size in bytes
function getFileSize(filePath) {
  try {
    const stats = statSync(filePath);
    return stats.size;
  } catch {
    return 0;
  }
}

// Format bytes to human-readable
function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return `${(bytes / Math.pow(k, i)).toFixed(2)} ${sizes[i]}`;
}

// Analyze bundle
function analyzeBundle() {
  console.log('📊 Performance Benchmark\n');
  
  const distDir = join(projectRoot, 'dist');
  const v2DataDir = join(projectRoot, '../v2/data');
  
  // Check both possible output directories
  const checkDirs = [];
  
    try {
      if (statSync(distDir).isDirectory()) {
        checkDirs.push({ path: distDir, name: 'dist' });
      }
    } catch {
      // dist doesn't exist
    }
    
    try {
      if (statSync(v2DataDir).isDirectory()) {
        checkDirs.push({ path: v2DataDir, name: 'v2/data' });
      }
    } catch {
      // v2/data doesn't exist
    }
  
  if (checkDirs.length === 0) {
    console.log('⚠️  No build output found.');
    console.log('   Run "npm run build" first to generate build output.\n');
    return;
  }
  
  // Analyze each directory
  for (const { path: checkDir, name } of checkDirs) {
    console.log(`\n📦 Analyzing ${name} directory:\n`);
    
    const files = getAllFiles(checkDir);
    const fileStats = new Map();
    let totalSize = 0;
    
    // Categorize files
    for (const file of files) {
      const size = getFileSize(file);
      if (size === 0) continue;
      
      const ext = extname(file);
      const category = ext === '.js' ? 'JavaScript' :
                      ext === '.css' ? 'CSS' :
                      ext === '.html' ? 'HTML' :
                      ext === '.png' || ext === '.jpg' || ext === '.svg' ? 'Images' :
                      'Other';
      
      if (!fileStats.has(category)) {
        fileStats.set(category, { files: [], totalSize: 0 });
      }
      
      const relativePath = file.replace(checkDir + '/', '');
      fileStats.get(category).files.push({ path: relativePath, size });
      fileStats.get(category).totalSize += size;
      totalSize += size;
    }
    
    // Report by category
    const categories = ['JavaScript', 'CSS', 'HTML', 'Images', 'Other'];
    
    for (const category of categories) {
      const stats = fileStats.get(category);
      if (!stats || stats.files.length === 0) continue;
      
      console.log(`${category}:`);
      console.log(`  Files: ${stats.files.length}`);
      console.log(`  Total: ${formatBytes(stats.totalSize)}`);
      
      // Show largest files
      const sortedFiles = stats.files.sort((a, b) => b.size - a.size);
      const topFiles = sortedFiles.slice(0, 3);
      
      if (topFiles.length > 0) {
        console.log(`  Largest files:`);
        topFiles.forEach(file => {
          const percentage = ((file.size / stats.totalSize) * 100).toFixed(1);
          console.log(`    - ${file.path}: ${formatBytes(file.size)} (${percentage}%)`);
        });
      }
      console.log('');
    }
    
    console.log(`Total bundle size: ${formatBytes(totalSize)}\n`);
    
    // Calculate gzipped estimate (rough: ~30% reduction)
    const estimatedGzip = totalSize * 0.7;
    console.log(`Estimated gzipped: ~${formatBytes(estimatedGzip)}\n`);
    
    // Targets
    console.log('🎯 Targets:');
    console.log('  - Bundle size reduction: 30% from baseline');
    console.log('  - JavaScript: < 500KB (gzipped)');
    console.log('  - CSS: < 50KB (gzipped)');
    console.log('  - Total: < 600KB (gzipped)\n');
  }
}

// Main
try {
  analyzeBundle();
} catch (error) {
  console.error('❌ Benchmark failed:', error.message);
  process.exit(1);
}

