# Lightwave Dashboard UI Standards

**Version:** 1.0  
**Design System:** Glass V4  
**Last Updated:** 2025-01-XX  
**Purpose:** Single Source of Truth for UI Design Patterns and Tailwind Usage

---

## Design Philosophy: Glass V4

The Lightwave Dashboard uses a **"Glass V4"** design system characterized by:
- **Frosted glass aesthetics** with backdrop blur
- **Layered depth** via subtle shadows and borders
- **Radial gradients** for ambient lighting effects
- **High contrast** text on dark backgrounds
- **Smooth animations** for state transitions

---

## Design Tokens

### Color Palette

Defined in `tailwind.config.js`:

```javascript
colors: {
  background: '#0f1219',      // Deep dark base
  surface: '#252d3f',          // Elevated surfaces
  primary: {
    DEFAULT: '#ffb84d',       // Gold/amber accent
    dark: '#f59e0b',           // Darker gold variant
  },
  accent: {
    cyan: '#6ee7f3',           // Cyan accent
    green: '#22dd88',          // Success/connected state
    red: '#ef4444',            // Error/disconnected state
  },
  text: {
    primary: '#e6e9ef',        // Main text
    secondary: '#8b95a5',      // Secondary text
  },
}
```

### Typography

**Font Families:**
- **Display:** `'Bebas Neue'` - Used for headings, uppercase labels
- **Body:** `'Inter'` - Used for body text, descriptions
- **Mono:** `'JetBrains Mono'` - Used for code, values, technical data

**Font Sizes (Tailwind + Custom):**
- `text-[0.5625rem]` - 9px - Tiny labels, badges
- `text-[0.625rem]` - 10px - Small metadata
- `text-[0.6875rem]` - 11px - Secondary text, labels
- `text-xs` - 12px - Standard small text
- `text-sm` - 14px - Body text
- `text-[0.9375rem]` - 15px - Section headers
- `text-[1.375rem]` - 22px - Main logo text
- `text-2xl` - 24px - Page titles

**Letter Spacing:**
- `tracking-tight` - For display fonts (headings)
- `tracking-wide` - For uppercase labels
- Custom: `letterSpacing: '0.01em'` - Subtle spacing for logo
- Custom: `letterSpacing: '0.04em'` - Tab labels
- Custom: `letterSpacing: '0.06em'` - Section headers

### Spacing System

Follows Tailwind's default spacing scale:
- `p-3 sm:p-4 lg:p-6 xl:p-8` - Responsive padding pattern
- `gap-2 sm:gap-3` - Responsive gaps
- `mb-4` or `mb-5` - Standard vertical spacing between sections

### Border Radius

- `rounded-lg` - 8px - Standard cards, buttons
- `rounded-xl` - 12px - Main containers, large buttons
- `rounded-2xl` - 16px - Root container
- `rounded-full` - Pills, status indicators

---

## Glass V4 Pattern Library

### Main Container Pattern

**Usage:** Root section, main content areas

```tsx
<section 
  className="mx-auto max-w-screen-2xl w-full rounded-2xl overflow-hidden flex flex-col relative"
  style={{
    background: 'rgba(37,45,63,0.55)',
    border: '1px solid rgba(255,255,255,0.10)',
    boxShadow: '0 0 0 1px rgba(255,255,255,0.04) inset, 0 24px 48px rgba(0,0,0,0.4)',
    backdropFilter: 'blur(24px)'
  }}
>
  {/* Content */}
</section>
```

**Key Elements:**
- Semi-transparent background: `rgba(37,45,63,0.55)`
- Subtle border: `rgba(255,255,255,0.10)`
- Inset border highlight: `0 0 0 1px rgba(255,255,255,0.04) inset`
- Deep shadow: `0 24px 48px rgba(0,0,0,0.4)`
- Backdrop blur: `blur(24px)`

### Card Pattern

**Usage:** Content cards, control panels, information blocks

```tsx
<div 
  className="card-hover rounded-xl p-4 sm:p-5"
  style={{
    background: 'rgba(37,45,63,0.95)',
    border: '1px solid rgba(255,255,255,0.10)',
    boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset, 0 12px 32px rgba(0,0,0,0.25)'
  }}
>
  {/* Card content */}
</div>
```

**Key Elements:**
- More opaque background: `rgba(37,45,63,0.95)`
- Inset top highlight: `0 1px 0 rgba(255,255,255,0.08) inset`
- Card shadow: `0 12px 32px rgba(0,0,0,0.25)`
- Hover effect: `card-hover` class (see animations)

### Header Pattern

**Usage:** Section headers, page titles

```tsx
<header 
  className="relative z-50 flex items-center justify-between px-4 sm:px-6 lg:px-8"
  style={{
    height: '4.5rem',
    background: 'rgba(37,45,63,0.95)',
    borderBottom: '1px solid rgba(255,255,255,0.10)',
    boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset',
    backgroundImage: 'radial-gradient(circle at top left, rgba(255,255,255,0.06), transparent 50%)'
  }}
>
  {/* Header content */}
</header>
```

**Key Elements:**
- Fixed height: `4.5rem` (72px)
- Radial gradient accent: `radial-gradient(circle at top left, rgba(255,255,255,0.06), transparent 50%)`
- Inset top border highlight

### Button Patterns

#### Primary Button

```tsx
<button className="h-9 px-3 rounded-lg bg-primary text-background hover:brightness-110 transition-all flex items-center text-[11px] font-medium touch-target">
  {/* Button content */}
</button>
```

#### Secondary Button

```tsx
<button 
  className="touch-target grid place-items-center rounded-xl btn-secondary"
  style={{ 
    width: '2.75rem', 
    height: '2.75rem', 
    border: '1px solid rgba(255,255,255,0.10)' 
  }}
>
  {/* Icon */}
</button>
```

#### Status Button (Connection Indicator)

```tsx
<button 
  className="flex items-center gap-2 rounded-xl px-2 sm:px-3 py-2 transition-colors hover:bg-white/5 cursor-pointer"
  style={{ 
    background: 'rgba(47,56,73,0.65)', 
    border: '1px solid rgba(255,255,255,0.08)' 
  }}
>
  <span className={`rounded-full w-2 h-2 ${
    isConnected 
      ? 'bg-[#22dd88] status-pulse-connected' 
      : 'bg-[#ef4444] status-pulse-disconnected'
  }`}></span>
  <span className={`hidden sm:inline text-[0.6875rem] font-medium ${
    isConnected ? 'text-[#22dd88]' : 'text-[#ef4444]'
  }`}>
    {isConnected ? 'CONNECTED' : 'DISCONNECTED'}
  </span>
</button>
```

### Tab Navigation Pattern

```tsx
<nav 
  className="relative flex items-center gap-1 px-4 sm:px-6 lg:px-8 overflow-x-auto tab-scroll flex-shrink-0"
  style={{
    height: '3.25rem',
    background: 'rgba(37,45,63,0.75)',
    borderBottom: '1px solid rgba(255,255,255,0.08)'
  }}
  role="tablist"
>
  {tabs.map((tab) => (
    <button
      className={`tab-btn touch-target relative h-full px-3 sm:px-4 whitespace-nowrap transition-colors ${
        activeTab === tab.id ? 'text-primary' : 'text-text-secondary'
      }`}
      role="tab"
      aria-selected={activeTab === tab.id}
    >
      <span className="uppercase tracking-wide font-display text-sm" style={{ letterSpacing: '0.04em' }}>
        {tab.label}
      </span>
    </button>
  ))}
  
  {/* Sliding Underline Indicator */}
  <div 
    className="tab-underline absolute bottom-0 bg-primary h-[2px] rounded-[1px]"
    style={{
      left: `${indicatorStyle.left}px`,
      width: `${indicatorStyle.width}px`
    }}
  />
</nav>
```

### Page Header Pattern

```tsx
<div className="flex flex-col gap-2 lg:flex-row lg:items-end lg:justify-between mb-5">
  <div>
    <h1 className="uppercase tracking-tight font-display text-2xl" style={{ letterSpacing: '0.01em' }}>
      Page Title
    </h1>
    <p className="flex flex-wrap items-center gap-2 mt-1 text-xs text-text-secondary">
      <span>Description text</span>
      <span className="w-[3px] h-[3px] rounded-full bg-text-secondary"></span>
      <span>Metadata: <span className="text-[#22dd88]">Value</span></span>
    </p>
  </div>
</div>
```

### Info Row Pattern

```tsx
<div className="flex justify-between items-center py-2 border-b border-white/5 last:border-0">
  <span className="text-text-secondary">{label}</span>
  <span className="font-mono text-text-primary bg-surface/50 px-2 py-0.5 rounded border border-white/5">
    {value}
  </span>
</div>
```

---

## Tailwind Class Ordering Convention

**Standard Order:**
1. Layout (display, position, flex, grid)
2. Spacing (padding, margin, gap)
3. Sizing (width, height, min/max)
4. Typography (font, text, leading, tracking)
5. Colors (background, text, border)
6. Effects (shadow, opacity, backdrop)
7. Transitions/Animations
8. Responsive variants (sm:, lg:, xl:)
9. State variants (hover:, focus:, active:)

**Example:**
```tsx
<div className="
  flex items-center justify-between
  px-4 sm:px-6 lg:px-8
  h-12
  uppercase tracking-wide font-display text-sm
  bg-surface/50 border border-white/10
  rounded-xl
  transition-colors
  hover:bg-surface/70
">
```

---

## Responsive Breakpoints

Follow Tailwind's default breakpoints:
- **sm:** 640px - Small tablets, large phones
- **md:** 768px - Tablets
- **lg:** 1024px - Small laptops
- **xl:** 1280px - Desktops
- **2xl:** 1536px - Large desktops

**Common Responsive Patterns:**
```tsx
// Padding
className="p-4 sm:p-5 lg:p-6 xl:p-8"

// Grid columns
className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3"

// Text visibility
className="hidden sm:inline"

// Flex direction
className="flex flex-col lg:flex-row"
```

---

## Animation Patterns

### Custom Animations (defined in `index.css`)

**Status Pulse (Connected):**
```css
.status-pulse-connected {
  animation: pulse-glow 2s ease-in-out infinite;
}
```

**Status Pulse (Disconnected):**
```css
.status-pulse-disconnected {
  animation: pulse-glow-red 1s ease-in-out infinite;
}
```

**Card Hover:**
```css
.card-hover {
  transition: transform 0.2s ease, box-shadow 0.2s ease;
}
.card-hover:hover {
  transform: translateY(-2px);
  box-shadow: 0 20px 48px rgba(0,0,0,0.45);
}
```

**Tab Underline:**
```css
.tab-underline {
  transition: transform 0.25s ease-out, width 0.25s ease-out, left 0.25s ease-out;
}
```

**Slide Down (Page Transitions):**
```tsx
<div className="animate-slide-down">
  {/* Content */}
</div>
```

**Shimmer Effect:**
```tsx
<div className="shimmer">
  {/* Loading state */}
</div>
```

---

## Component-Specific Patterns

### Slider Control

See `src/components/ui/SliderControl.tsx` for the standard slider pattern.

**Key Features:**
- Custom gradient fill track
- Custom thumb styling
- Tabular numbers for values
- Icon support (optional)

### Preset Button

See `src/components/ui/PresetButton.tsx` for the preset button pattern.

**Key Features:**
- Gradient background with opacity
- Hover scale effect
- Color-coded presets

### Info Row

See `src/components/ui/InfoRow.tsx` for the info row pattern.

**Key Features:**
- Label/value layout
- Monospace value styling
- Subtle borders between rows

---

## Accessibility Standards

### Touch Targets

All interactive elements must meet minimum touch target size:
```tsx
className="touch-target"  // min-w-[44px] min-h-[44px]
```

### Focus States

```css
*:focus-visible {
  outline: 2px solid #ffb84d;
  outline-offset: 2px;
}
```

### ARIA Labels

```tsx
<button aria-label="Settings">
  <SettingsIcon />
</button>

<nav role="tablist">
  <button role="tab" aria-selected={isActive}>
    Tab Label
  </button>
</nav>
```

### Semantic HTML

- Use `<header>`, `<nav>`, `<main>`, `<section>` appropriately
- Use `<button>` for interactive elements (not `<div>`)
- Use proper heading hierarchy (`h1` → `h2` → `h3`)

---

## Common Anti-Patterns (DO NOT USE)

### ❌ Inline Styles for Colors
```tsx
// BAD
<div style={{ color: '#ffb84d' }}>

// GOOD
<div className="text-primary">
```

### ❌ Hardcoded Spacing Values
```tsx
// BAD
<div style={{ padding: '16px' }}>

// GOOD
<div className="p-4">
```

### ❌ Missing Responsive Variants
```tsx
// BAD
<div className="p-4">

// GOOD
<div className="p-4 sm:p-5 lg:p-6">
```

### ❌ Inconsistent Border Patterns
```tsx
// BAD
<div className="border border-gray-500">

// GOOD
<div style={{ border: '1px solid rgba(255,255,255,0.10)' }}>
```

### ❌ Missing Hover States
```tsx
// BAD
<button className="bg-primary">

// GOOD
<button className="bg-primary hover:brightness-110 transition-all">
```

---

## Style Override Guidelines

### When to Use Inline Styles

**Use inline styles for:**
- Glass V4 specific effects (backdrop-filter, complex shadows)
- Dynamic values (calculated positions, widths)
- Radial gradients with specific positioning
- Values not available in Tailwind config

**Example (Valid):**
```tsx
<div style={{
  background: 'rgba(37,45,63,0.55)',
  backdropFilter: 'blur(24px)',
  boxShadow: '0 0 0 1px rgba(255,255,255,0.04) inset, 0 24px 48px rgba(0,0,0,0.4)'
}}>
```

### When to Use Tailwind Classes

**Use Tailwind classes for:**
- Standard spacing, sizing, typography
- Color tokens from config
- Responsive breakpoints
- Standard transitions
- Layout utilities

---

## Component Layout Patterns

### Standard Tab Content

```tsx
<div className="p-4 sm:p-5 lg:p-6 xl:p-8 animate-slide-down">
  {/* Page Header */}
  <div className="flex flex-col gap-2 lg:flex-row lg:items-end lg:justify-between mb-5">
    <div>
      <h1 className="uppercase tracking-tight font-display text-2xl" style={{ letterSpacing: '0.01em' }}>
        Tab Title
      </h1>
      <p className="text-xs text-text-secondary mt-1">Description</p>
    </div>
  </div>
  
  {/* Content Grid */}
  <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
    {/* Cards */}
  </div>
</div>
```

### Card with Header

```tsx
<div 
  className="card-hover rounded-xl p-4 sm:p-5"
  style={{
    background: 'rgba(37,45,63,0.95)',
    border: '1px solid rgba(255,255,255,0.10)',
    boxShadow: '0 1px 0 rgba(255,255,255,0.08) inset, 0 12px 32px rgba(0,0,0,0.25)'
  }}
>
  <div className="flex items-center justify-between mb-4">
    <span className="uppercase tracking-tight font-display text-[0.9375rem]" style={{ letterSpacing: '0.02em' }}>
      Card Title
    </span>
    {/* Optional badge/icon */}
  </div>
  
  {/* Card content */}
</div>
```

---

## Testing Visual Consistency

### Visual Regression Testing

The project uses Playwright for visual regression testing:
```bash
npm run test:e2e
```

### Manual Checklist

Before submitting UI changes:
- [ ] Follows Glass V4 pattern library
- [ ] Uses design tokens from `tailwind.config.js`
- [ ] Responsive at all breakpoints (sm, md, lg, xl)
- [ ] Touch targets meet 44px minimum
- [ ] Hover states implemented
- [ ] Focus states visible
- [ ] Animations are smooth (60fps)
- [ ] Text contrast meets WCAG AA
- [ ] No hardcoded colors (use tokens)
- [ ] Consistent spacing (Tailwind scale)

---

## References

- [Architecture Documentation](./ARCHITECTURE.md)
- [Component Gallery](../src/components/ui/README.md)
- [Tailwind CSS Documentation](https://tailwindcss.com/docs)
- [React 19 Documentation](https://react.dev)

---

**For AI Agents:** When implementing UI components:
1. Copy patterns from this document
2. Use design tokens from `tailwind.config.js`
3. Follow the Glass V4 aesthetic
4. Maintain responsive design
5. Test visual consistency
6. Follow accessibility standards

