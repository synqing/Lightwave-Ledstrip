#!/usr/bin/env node

/**
 * Icon bundle analysis script
 * 
 * Analyzes which Lucide React icons are actually imported and used
 * in the codebase. Helps identify unused imports and verify tree-shaking.
 */

import { readFileSync, readdirSync } from 'fs';
import { join, extname } from 'path';
import { fileURLToPath } from 'url';
import { dirname } from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const projectRoot = join(__dirname, '..');

// Recursively get all TypeScript/JavaScript files
function getAllFiles(dir, extensions = ['.ts', '.tsx', '.js', '.jsx']) {
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

// Extract icon imports from file content
function extractIconImports(content) {
  const imports = new Set();
  
  // Match: import { Icon1, Icon2 } from 'lucide-react'
  const namedImportRegex = /import\s+{([^}]+)}\s+from\s+['"]lucide-react['"]/g;
  let match = namedImportRegex.exec(content);
  
  while (match !== null) {
    const importList = match[1];
    // Split by comma and extract icon names
    const icons = importList
      .split(',')
      .map(item => item.trim())
      .filter(item => item && !item.startsWith('type ')) // Filter out type imports
      .map(item => item.split(' as ')[0].trim()); // Handle aliases
    
    for (const icon of icons) {
      imports.add(icon);
    }
    
    match = namedImportRegex.exec(content);
  }
  
  // Match: import Icon from 'lucide-react/Icon'
  const defaultImportRegex = /import\s+(\w+)\s+from\s+['"]lucide-react\/[\w-]+['"]/g;
  match = defaultImportRegex.exec(content);
  while (match !== null) {
    imports.add(match[1]);
    match = defaultImportRegex.exec(content);
  }
  
  return Array.from(imports);
}

// Main analysis
function analyzeIcons() {
  console.log('🔍 Analyzing icon usage...\n');
  
  const srcDir = join(projectRoot, 'src');
  const files = getAllFiles(srcDir);
  
  const iconUsage = new Map(); // icon -> [files]
  const allIcons = new Set();
  
  // Scan all files for icon imports
  for (const file of files) {
    try {
      const content = readFileSync(file, 'utf-8');
      const icons = extractIconImports(content);
      
      if (icons.length > 0) {
        const relativePath = file.replace(projectRoot + '/', '');
        
        icons.forEach(icon => {
          allIcons.add(icon);
          if (!iconUsage.has(icon)) {
            iconUsage.set(icon, []);
          }
          iconUsage.get(icon).push(relativePath);
        });
      }
    } catch (err) {
      console.warn(`Warning: Could not read ${file}: ${err.message}`);
    }
  }
  
  // Report results
  console.log(`✓ Scanned ${files.length} files\n`);
  console.log(`📊 Icon Usage Summary:\n`);
  console.log(`Total unique icons used: ${allIcons.size}\n`);
  
  if (allIcons.size === 0) {
    console.log('⚠️  No icons found. Make sure you\'re importing from "lucide-react".\n');
    return;
  }
  
  // Sort icons by usage frequency
  const sortedIcons = Array.from(iconUsage.entries())
    .sort((a, b) => b[1].length - a[1].length);
  
  console.log('Icons by usage frequency:\n');
  sortedIcons.forEach(([icon, files]) => {
    console.log(`  ${icon.padEnd(20)} (${files.length} file${files.length > 1 ? 's' : ''})`);
  });
  
  // Show detailed usage
  console.log('\n📋 Detailed Usage:\n');
  sortedIcons.forEach(([icon, files]) => {
    console.log(`\n${icon}:`);
    files.forEach(file => {
      console.log(`  - ${file}`);
    });
  });
  
  // Bundle size estimate
  const estimatedSize = allIcons.size * 1.5; // ~1.5KB per icon (gzipped)
  console.log(`\n📦 Bundle Size Estimate:`);
  console.log(`  Icons: ~${estimatedSize.toFixed(1)}KB (gzipped)`);
  console.log(`  Note: Actual size depends on tree-shaking effectiveness\n`);
  
  // Recommendations
  console.log('💡 Recommendations:\n');
  
  const multiFileIcons = sortedIcons.filter(([_, files]) => files.length > 1);
  if (multiFileIcons.length > 0) {
    console.log('Icons used in multiple files (consider creating shared components):');
    multiFileIcons.forEach(([icon, files]) => {
      console.log(`  - ${icon} (${files.length} files)`);
    });
    console.log('');
  }
  
  console.log('✅ Analysis complete!\n');
  console.log('To verify tree-shaking, check your production bundle size.');
  console.log('Each icon should add ~1-2KB (gzipped) to the bundle.\n');
}

// Run analysis
try {
  analyzeIcons();
} catch (error) {
  console.error('❌ Analysis failed:', error.message);
  process.exit(1);
}

