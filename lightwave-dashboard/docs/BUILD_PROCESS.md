# Build Process Documentation

**Version:** 1.0  
**Last Updated:** 2025-12-22  
**Purpose:** Comprehensive guide to the Lightwave Dashboard build system

---

## Overview

The Lightwave Dashboard uses a modern build pipeline with Vite, Tailwind CSS, and PostCSS to create optimized production bundles for embedded deployment on ESP32 devices.

---

## Build Modes

### Development Build

```bash
npm run dev
```

- **Hot Module Replacement (HMR)**: Instant updates during development
- **No CSS purging**: All Tailwind classes available
- **Source maps**: Enabled for debugging
- **Fast rebuilds**: Optimized for development speed

### Production Build

```bash
npm run build
```

- **CSS purging**: Unused Tailwind classes removed via PurgeCSS
- **Minification**: JavaScript and CSS minified
- **Tree-shaking**: Unused code eliminated
- **Optimized assets**: Images and fonts optimized
- **No source maps**: Reduced bundle size

---

## Build Pipeline

### Pre-Build Steps

1. **Class Validation** (`npm run validate:classes`)
   - Scans all source files for Tailwind classes
   - Validates class patterns
   - Catches invalid responsive prefixes
   - Exits with error if issues found

2. **TypeScript Compilation** (`tsc -b`)
   - Type checking
   - Generates type definitions
   - Validates component props and interfaces

### Build Steps

1. **Vite Build**
   - Bundles React components
   - Processes TypeScript/JSX
   - Handles asset imports
   - Generates optimized chunks

2. **PostCSS Processing**
   - **Tailwind CSS**: Generates utility classes
   - **Autoprefixer**: Adds vendor prefixes
   - **PurgeCSS** (production only): Removes unused CSS

3. **Output**
   - JavaScript: `assets/app.js`
   - CSS: `assets/styles.css`
   - Assets: `assets/[name][extname]`
   - Output directory: `../v2/data/` (for ESP32 LittleFS)

---

## PurgeCSS Configuration

### When It Runs

PurgeCSS only runs in production builds (`NODE_ENV === 'production'`).

### Configuration

**File**: `postcss.config.js`

```javascript
purgecss({
  content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
  defaultExtractor: (content) => content.match(/[\w-/:]+(?<!:)/g) || [],
  safelist: {
    standard: [/^animate-/, /^bg-/, /^text-/, /^border-/],
    deep: [/^data-/, /^aria-/],
  },
})
```

### Safelist Patterns

The safelist ensures these classes are never purged:

- **Animation classes**: `animate-*` (used dynamically)
- **Background classes**: `bg-*` (may be generated dynamically)
- **Text classes**: `text-*` (used in many contexts)
- **Border classes**: `border-*` (common utility)
- **Data attributes**: `data-*` (used for state)
- **ARIA attributes**: `aria-*` (accessibility)

### Troubleshooting PurgeCSS

**Issue**: Styles missing in production but present in development

**Solution**:
1. Check if class is in safelist
2. Verify class is used in scanned files
3. Check for dynamic class generation (may need safelist entry)
4. Review PurgeCSS extractor pattern

**Example**: If using `className={`bg-${color}`}`, add to safelist:
```javascript
safelist: {
  standard: [/^bg-red-/, /^bg-blue-/, /^bg-green-/],
}
```

---

## Class Validation

### Purpose

The `validate-classes` script catches common issues before build:

- Invalid responsive prefix combinations
- Template literal issues
- Suspicious class patterns

### Running Manually

```bash
npm run validate:classes
```

### Integration

Runs automatically as `prebuild` hook before every production build.

---

## Bundle Size Optimization

### Current Targets

- **Target**: 30% reduction from baseline
- **Measurement**: Use `npm run build` and check `dist/stats.html` (if bundle analyzer enabled)

### Optimization Strategies

1. **PurgeCSS**: Removes unused CSS (automatic in production)
2. **Tree-shaking**: Vite automatically eliminates unused imports
3. **Code splitting**: Large dependencies split into separate chunks
4. **Asset optimization**: Images compressed, fonts subset

### Measuring Bundle Size

```bash
npm run build
# Check dist/assets/ directory for file sizes
```

### Bundle Analysis

To analyze bundle composition:

1. Run `npm run benchmark` to see bundle size breakdown
2. Check `dist/assets/` or `v2/data/assets/` for file sizes
3. Use browser DevTools Network tab to see actual transfer sizes
4. Identify large dependencies
5. Optimize or remove unnecessary imports

**Note**: `rollup-plugin-visualizer` has compatibility issues with `rolldown-vite`.
Use the `benchmark` script and browser DevTools for bundle analysis instead.

---

## Troubleshooting

### Build Fails with Class Validation Error

**Error**: `Validation issues found`

**Solution**:
1. Review error output for specific file/class
2. Fix invalid class patterns
3. Check for template literal issues
4. Re-run validation

### CSS Missing in Production

**Symptom**: Styles work in dev but not production

**Solution**:
1. Check PurgeCSS safelist
2. Verify class is in scanned files
3. Check for dynamic class generation
4. Add to safelist if needed

### Build Output Too Large

**Symptom**: Bundle size exceeds target

**Solution**:
1. Run bundle analyzer
2. Identify large dependencies
3. Check for duplicate imports
4. Review tree-shaking effectiveness
5. Consider code splitting

### TypeScript Errors During Build

**Error**: `tsc -b` fails

**Solution**:
1. Fix TypeScript errors in source
2. Check `tsconfig.json` configuration
3. Verify type definitions are up to date
4. Review component prop types

---

## Development Workflow

### Typical Development Cycle

1. **Start dev server**: `npm run dev`
2. **Make changes**: Edit source files
3. **See updates**: HMR updates browser automatically
4. **Test**: Run `npm run test` or `npm run test:e2e`
5. **Build**: `npm run build` before deployment

### Pre-Commit Checklist

- [ ] Run `npm run lint`
- [ ] Run `npm run test:run`
- [ ] Run `npm run validate:classes`
- [ ] Verify build succeeds: `npm run build`

---

## CI/CD Integration

### Recommended Pipeline Steps

1. **Install dependencies**: `npm ci`
2. **Lint**: `npm run lint`
3. **Type check**: `tsc -b --noEmit`
4. **Validate classes**: `npm run validate:classes`
5. **Run tests**: `npm run test:all`
6. **Build**: `npm run build`
7. **Verify bundle size**: Check `dist/assets/` sizes
8. **Deploy**: Copy `dist/` to ESP32 LittleFS

### Environment Variables

- `NODE_ENV=production`: Enables PurgeCSS and optimizations
- `NODE_ENV=development`: Disables purging, enables HMR

---

## File Structure

```
lightwave-dashboard/
├── src/                    # Source files
├── scripts/                # Build scripts
│   └── validate-classes.js
├── docs/                   # Documentation
│   └── BUILD_PROCESS.md
├── postcss.config.js       # PostCSS configuration
├── tailwind.config.js      # Tailwind configuration
├── vite.config.ts          # Vite configuration
└── package.json            # Dependencies and scripts
```

---

## Additional Resources

- [Tailwind CSS Documentation](https://tailwindcss.com/docs)
- [Vite Documentation](https://vitejs.dev)
- [PostCSS Documentation](https://postcss.org)
- [PurgeCSS Documentation](https://purgecss.com)

---

## Changelog

### 2025-12-22
- Initial documentation
- Added PurgeCSS configuration
- Added class validation script
- Documented build pipeline

