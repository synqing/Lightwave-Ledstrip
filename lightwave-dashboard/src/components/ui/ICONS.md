# Icon System Documentation

**Version:** 1.0  
**Last Updated:** 2025-12-22  
**Library:** `lucide-react` v0.562.0

---

## Overview

The Lightwave Dashboard uses [Lucide React](https://lucide.dev) for all iconography. Lucide is a modern, tree-shakeable icon library that provides consistent, accessible icons optimized for React.

---

## Why Lucide React?

- ✅ **Tree-shakeable**: Only imported icons are bundled
- ✅ **TypeScript support**: Full type definitions included
- ✅ **Consistent design**: Unified visual language
- ✅ **Accessible**: Built with accessibility in mind
- ✅ **Lightweight**: Minimal bundle impact
- ✅ **No external dependencies**: Self-contained

---

## Usage Patterns

### Basic Import

```tsx
import { Settings, Play, Pause } from 'lucide-react';

function MyComponent() {
  return (
    <button>
      <Settings size={16} />
      Settings
    </button>
  );
}
```

### With Size and Color

```tsx
import { Zap } from 'lucide-react';

function IconExample() {
  return (
    <Zap 
      size={20} 
      className="text-primary" 
      strokeWidth={1.5}
    />
  );
}
```

### In Buttons

```tsx
import { Play, Pause } from 'lucide-react';

function PlayButton({ isPlaying, onClick }) {
  return (
    <button onClick={onClick} aria-label={isPlaying ? 'Pause' : 'Play'}>
      {isPlaying ? <Pause size={16} /> : <Play size={16} />}
    </button>
  );
}
```

---

## Available Icons

### Commonly Used Icons

The following icons are currently used in the dashboard:

#### Navigation & UI
- `Settings` - Settings/configuration
- `Zap` - Lightning/power
- `Play` - Play action
- `Pause` - Pause action
- `SkipBack` - Previous/back
- `SkipForward` - Next/forward
- `Repeat` - Loop/repeat
- `Search` - Search functionality
- `X` - Close/cancel
- `Plus` - Add/create

#### Status & Indicators
- `Activity` - Activity/performance
- `Wifi` - Network/connection
- `Cpu` - CPU usage
- `HardDrive` - Storage/memory
- `Shield` - Security/safety
- `PlugZap` - Power/energy

#### Effects & Content
- `Star` - Favorite/bookmark
- `Tag` - Category/tag
- `SearchX` - No results
- `TrendingUp` - Performance trend
- `RefreshCw` - Refresh/reload

### Full Icon List

For a complete list of available icons, visit:
- [Lucide Icons Gallery](https://lucide.dev/icons/)
- Search by name or category
- Copy import statement directly

---

## Size Guidelines

### Standard Sizes

| Size | Use Case | Value |
|------|----------|-------|
| Small | Inline text, compact buttons | `12-14px` |
| Medium | Standard buttons, headers | `16-18px` |
| Large | Hero sections, prominent actions | `20-24px` |
| Extra Large | Landing pages, marketing | `32px+` |

### Implementation

```tsx
// Small (inline)
<Settings size={14} />

// Medium (buttons)
<Play size={18} />

// Large (headers)
<Zap size={22} />
```

---

## Color Guidelines

### Using Tailwind Classes

Icons inherit text color from parent or use explicit classes:

```tsx
// Inherit from parent
<button className="text-primary">
  <Settings size={16} />
</button>

// Explicit color
<Settings size={16} className="text-accent-cyan" />

// Disabled state
<Settings size={16} className="text-text-secondary opacity-50" />
```

### Color Tokens

Use design system color tokens:

- `text-primary` - Primary text color
- `text-secondary` - Secondary/muted text
- `text-accent-cyan` - Cyan accent
- `text-accent-green` - Green accent
- `text-accent-red` - Red/error accent
- `text-primary` (gold) - Primary accent

---

## Accessibility

### Required: aria-label

Always provide accessible labels for icon-only buttons:

```tsx
// ✅ Good
<button aria-label="Settings">
  <Settings size={16} />
</button>

// ❌ Bad (no label)
<button>
  <Settings size={16} />
</button>
```

### With Text

If icon accompanies text, label the button, not the icon:

```tsx
// ✅ Good
<button aria-label="Open settings">
  <Settings size={16} />
  Settings
</button>

// ✅ Also good (icon is decorative)
<button>
  <Settings size={16} aria-hidden="true" />
  Settings
</button>
```

### Focus States

Icons in interactive elements should have visible focus indicators:

```tsx
<button 
  className="focus:outline-none focus:ring-2 focus:ring-primary"
  aria-label="Play"
>
  <Play size={16} />
</button>
```

---

## TypeScript Typings

### Icon Props

All Lucide icons accept these props:

```typescript
interface LucideIconProps {
  size?: number | string;      // Size in pixels or CSS value
  strokeWidth?: number;        // Stroke width (default: 2)
  color?: string;              // SVG fill/stroke color
  className?: string;          // Additional CSS classes
  'aria-label'?: string;       // Accessibility label
  'aria-hidden'?: boolean;     // Hide from screen readers
}
```

### Example with Types

```tsx
import { Settings, type LucideProps } from 'lucide-react';

interface IconButtonProps {
  icon: React.ComponentType<LucideProps>;
  label: string;
  size?: number;
}

function IconButton({ icon: Icon, label, size = 16 }: IconButtonProps) {
  return (
    <button aria-label={label}>
      <Icon size={size} />
    </button>
  );
}
```

---

## Performance

### Tree-Shaking

Lucide React is fully tree-shakeable. Only imported icons are bundled:

```tsx
// ✅ Good - only Settings is bundled
import { Settings } from 'lucide-react';

// ❌ Bad - entire library bundled
import * as Icons from 'lucide-react';
```

### Bundle Impact

- **Per icon**: ~1-2KB (gzipped)
- **Total library**: ~500KB (unused)
- **Best practice**: Import only what you need

### Verification

Run icon analysis to see which icons are actually used:

```bash
npm run analyze:icons
```

---

## Migration Guide

### From Iconify

If migrating from Iconify (used in V4 HTML prototype):

**Before (Iconify)**:
```html
<iconify-icon icon="lucide:settings" width="16" height="16"></iconify-icon>
```

**After (Lucide React)**:
```tsx
import { Settings } from 'lucide-react';

<Settings size={16} />
```

### Icon Name Mapping

Lucide icon names are PascalCase (e.g., `Settings`, `Play`, `SkipBack`).

Common mappings:
- `lucide:settings` → `Settings`
- `lucide:play` → `Play`
- `lucide:skip-back` → `SkipBack`
- `lucide:skip-forward` → `SkipForward`

---

## Best Practices

### ✅ Do

- Import only needed icons
- Provide `aria-label` for icon-only buttons
- Use consistent sizes within UI sections
- Match icon color to context (primary, accent, etc.)
- Use semantic icons (Settings for settings, Play for play)

### ❌ Don't

- Import entire library (`import * as Icons`)
- Use icons without accessibility labels
- Mix icon sizes arbitrarily
- Use icons that don't match their function
- Override stroke width unnecessarily (default is fine)

---

## Troubleshooting

### Icon Not Showing

**Issue**: Icon renders as blank space

**Solutions**:
1. Check import statement (PascalCase)
2. Verify icon name exists in Lucide
3. Check for TypeScript errors
4. Verify icon is not hidden by CSS

### Icon Too Large/Small

**Issue**: Icon size doesn't match design

**Solutions**:
1. Use `size` prop (number in pixels)
2. Use `className` with Tailwind size utilities
3. Check parent container constraints

### Bundle Size Too Large

**Issue**: Icons adding too much to bundle

**Solutions**:
1. Run `npm run analyze:icons` to see usage
2. Remove unused icon imports
3. Verify tree-shaking is working
4. Check for `import *` patterns

---

## Resources

- [Lucide Icons Gallery](https://lucide.dev/icons/)
- [Lucide React Documentation](https://lucide.dev/guide/packages/lucide-react)
- [Icon Usage Guidelines](./README.md#icon-usage)

---

## Changelog

### 2025-12-22
- Initial documentation
- Added usage patterns and guidelines
- Documented accessibility requirements
- Added migration guide from Iconify

