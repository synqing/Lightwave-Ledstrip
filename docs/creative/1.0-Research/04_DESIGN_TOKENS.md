# PRISM.node Design Token Specification

> Complete design token reference for LightwaveOS dashboard implementation

## 1. Color Tokens

### 1.1 Background Colors

```css
:root {
  /* Canvas - Main application background */
  --color-bg-canvas: #0A0E14;

  /* Surface - Panels, sidebars, cards */
  --color-bg-surface: #1A1F2E;

  /* Elevated - Modals, dropdowns, hover states */
  --color-bg-elevated: #252B3D;

  /* Overlay - Modal backdrops */
  --color-bg-overlay: rgba(10, 14, 20, 0.95);
}
```

### 1.2 Text Colors

```css
:root {
  /* Primary - Main content text */
  --color-text-primary: #E8E8E8;

  /* Secondary - Labels, metadata */
  --color-text-secondary: #A0A0A0;

  /* Muted - Placeholders, disabled text */
  --color-text-muted: #6B7280;

  /* Inverse - Text on light backgrounds */
  --color-text-inverse: #0A0E14;
}
```

### 1.3 Accent Colors

```css
:root {
  /* Gold - Primary accent (CTAs, active states) */
  --color-accent-primary: #FFD700;
  --color-accent-primary-hover: #FFC700;
  --color-accent-primary-active: #FFB600;

  /* Cyan - Secondary accent (info, status) */
  --color-accent-secondary: #00CED1;
  --color-accent-secondary-hover: #00B8BB;
  --color-accent-secondary-active: #00A3A6;
}
```

### 1.4 Semantic Colors

```css
:root {
  /* Success */
  --color-success: #10B981;
  --color-success-bg: rgba(16, 185, 129, 0.1);

  /* Warning */
  --color-warning: #F59E0B;
  --color-warning-bg: rgba(245, 158, 11, 0.1);

  /* Error */
  --color-error: #EF4444;
  --color-error-bg: rgba(239, 68, 68, 0.1);

  /* Info */
  --color-info: #3B82F6;
  --color-info-bg: rgba(59, 130, 246, 0.1);
}
```

### 1.5 Border Colors

```css
:root {
  /* Default border */
  --color-border-default: #2D3748;

  /* Accent border (subtle) */
  --color-border-accent: rgba(255, 215, 0, 0.2);

  /* Focus ring */
  --color-border-focus: #FFD700;

  /* Error border */
  --color-border-error: #EF4444;
}
```

### 1.6 Zone Colors

```css
:root {
  /* Zone 1 - Gold */
  --color-zone-1: #FFD700;

  /* Zone 2 - Cyan */
  --color-zone-2: #00CED1;

  /* Zone 3 - Emerald */
  --color-zone-3: #10B981;

  /* Zone 4 - Amber */
  --color-zone-4: #F59E0B;
}
```

### 1.7 LED-Specific Colors

```css
:root {
  /* LED off state */
  --color-led-off: #1A1F2E;

  /* LED dim state */
  --color-led-dim: rgba(255, 215, 0, 0.3);

  /* CENTER origin marker */
  --color-led-center: #00CED1;
}
```

---

## 2. Typography Tokens

### 2.1 Font Families

```css
:root {
  /* UI text */
  --font-family-ui: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;

  /* Code and numeric values */
  --font-family-mono: 'JetBrains Mono', 'Fira Code', 'Consolas', monospace;
}
```

### 2.2 Font Sizes

```css
:root {
  --font-size-xs: 10px;
  --font-size-sm: 12px;
  --font-size-base: 14px;
  --font-size-lg: 16px;
  --font-size-xl: 20px;
  --font-size-2xl: 24px;
  --font-size-3xl: 32px;
}
```

### 2.3 Font Weights

```css
:root {
  --font-weight-normal: 400;
  --font-weight-medium: 500;
  --font-weight-semibold: 600;
  --font-weight-bold: 700;
}
```

### 2.4 Line Heights

```css
:root {
  --line-height-tight: 1.2;
  --line-height-snug: 1.3;
  --line-height-normal: 1.4;
  --line-height-relaxed: 1.5;
}
```

### 2.5 Text Styles (Composite)

```css
/* Heading 1 - Page titles */
.text-h1 {
  font-family: var(--font-family-ui);
  font-size: var(--font-size-3xl);
  font-weight: var(--font-weight-bold);
  line-height: var(--line-height-tight);
  letter-spacing: -0.02em;
}

/* Heading 2 - Section headers */
.text-h2 {
  font-family: var(--font-family-ui);
  font-size: var(--font-size-2xl);
  font-weight: var(--font-weight-semibold);
  line-height: var(--line-height-snug);
  letter-spacing: -0.01em;
}

/* Heading 3 - Card titles */
.text-h3 {
  font-family: var(--font-family-ui);
  font-size: var(--font-size-xl);
  font-weight: var(--font-weight-semibold);
  line-height: var(--line-height-normal);
}

/* Heading 4 - Subsection headers */
.text-h4 {
  font-family: var(--font-family-ui);
  font-size: var(--font-size-lg);
  font-weight: var(--font-weight-semibold);
  line-height: var(--line-height-normal);
}

/* Body - Primary content */
.text-body {
  font-family: var(--font-family-ui);
  font-size: var(--font-size-base);
  font-weight: var(--font-weight-normal);
  line-height: var(--line-height-relaxed);
}

/* Body Small - Captions, labels */
.text-body-sm {
  font-family: var(--font-family-ui);
  font-size: var(--font-size-sm);
  font-weight: var(--font-weight-normal);
  line-height: var(--line-height-normal);
}

/* Label - Form labels */
.text-label {
  font-family: var(--font-family-ui);
  font-size: var(--font-size-base);
  font-weight: var(--font-weight-medium);
  line-height: var(--line-height-normal);
  letter-spacing: 0.01em;
}

/* Code - Parameter values */
.text-code {
  font-family: var(--font-family-mono);
  font-size: var(--font-size-base);
  font-weight: var(--font-weight-normal);
  line-height: var(--line-height-relaxed);
}
```

---

## 3. Spacing Tokens

### 3.1 Base Scale

```css
:root {
  --space-0: 0px;
  --space-1: 4px;
  --space-2: 8px;
  --space-3: 12px;
  --space-4: 16px;
  --space-5: 20px;
  --space-6: 24px;
  --space-8: 32px;
  --space-10: 40px;
  --space-12: 48px;
  --space-16: 64px;
  --space-20: 80px;
}
```

### 3.2 Semantic Spacing

```css
:root {
  /* Component internal */
  --spacing-component-xs: var(--space-1);  /* 4px */
  --spacing-component-sm: var(--space-2);  /* 8px */
  --spacing-component-md: var(--space-3);  /* 12px */
  --spacing-component-lg: var(--space-4);  /* 16px */

  /* Section spacing */
  --spacing-section-sm: var(--space-4);    /* 16px */
  --spacing-section-md: var(--space-6);    /* 24px */
  --spacing-section-lg: var(--space-8);    /* 32px */

  /* Page margins */
  --spacing-page-sm: var(--space-4);       /* 16px */
  --spacing-page-md: var(--space-6);       /* 24px */
  --spacing-page-lg: var(--space-8);       /* 32px */
}
```

---

## 4. Border Radius Tokens

```css
:root {
  --radius-none: 0px;
  --radius-sm: 4px;
  --radius-md: 8px;
  --radius-lg: 12px;
  --radius-xl: 16px;
  --radius-full: 9999px;
}
```

### Semantic Radius

```css
:root {
  /* Buttons, inputs */
  --radius-button: var(--radius-sm);

  /* Cards, panels */
  --radius-card: var(--radius-md);

  /* Modals, large containers */
  --radius-modal: var(--radius-lg);

  /* Pills, badges */
  --radius-pill: var(--radius-full);
}
```

---

## 5. Shadow Tokens

```css
:root {
  /* No shadow */
  --shadow-none: none;

  /* Subtle - Hover states */
  --shadow-subtle: 0 2px 4px rgba(0, 0, 0, 0.3);

  /* Moderate - Cards */
  --shadow-moderate: 0 4px 12px rgba(0, 0, 0, 0.4);

  /* Elevated - Modals, dropdowns */
  --shadow-elevated: 0 8px 24px rgba(0, 0, 0, 0.5);

  /* Overlay - Full screen overlays */
  --shadow-overlay: 0 16px 48px rgba(0, 0, 0, 0.6);

  /* Glow - Active/focus states */
  --shadow-glow-gold: 0 0 12px rgba(255, 215, 0, 0.4);
  --shadow-glow-cyan: 0 0 12px rgba(0, 206, 209, 0.4);
}
```

---

## 6. Animation Tokens

### 6.1 Duration

```css
:root {
  --duration-instant: 100ms;
  --duration-fast: 200ms;
  --duration-normal: 300ms;
  --duration-slow: 500ms;
  --duration-slower: 700ms;
}
```

### 6.2 Easing

```css
:root {
  /* Standard easing */
  --ease-default: cubic-bezier(0.4, 0.0, 0.2, 1);

  /* Enter animations */
  --ease-in: cubic-bezier(0.4, 0.0, 1, 1);

  /* Exit animations */
  --ease-out: cubic-bezier(0.0, 0.0, 0.2, 1);

  /* Bounce/spring */
  --ease-bounce: cubic-bezier(0.175, 0.885, 0.32, 1.275);
}
```

### 6.3 Common Transitions

```css
/* Button hover */
.transition-button {
  transition: background-color var(--duration-fast) var(--ease-default),
              transform var(--duration-fast) var(--ease-default);
}

/* Panel slide */
.transition-panel {
  transition: transform var(--duration-normal) var(--ease-out),
              opacity var(--duration-normal) var(--ease-out);
}

/* Tab switch */
.transition-tab {
  transition: opacity var(--duration-fast) var(--ease-default);
}

/* Slider thumb */
.transition-slider {
  transition: transform var(--duration-fast) var(--ease-bounce),
              box-shadow var(--duration-fast) var(--ease-default);
}
```

---

## 7. Z-Index Tokens

```css
:root {
  --z-base: 0;
  --z-dropdown: 10;
  --z-sticky: 20;
  --z-fixed: 30;
  --z-modal-backdrop: 40;
  --z-modal: 50;
  --z-popover: 60;
  --z-tooltip: 70;
}
```

---

## 8. Breakpoint Tokens

```css
:root {
  /* Mobile first breakpoints */
  --breakpoint-sm: 640px;
  --breakpoint-md: 768px;
  --breakpoint-lg: 1024px;
  --breakpoint-xl: 1280px;
  --breakpoint-2xl: 1536px;
  --breakpoint-3xl: 1920px;
}
```

### Media Query Usage

```css
/* Tablet and up */
@media (min-width: 768px) { }

/* Laptop and up */
@media (min-width: 1280px) { }

/* Desktop and up */
@media (min-width: 1920px) { }
```

---

## 9. Component-Specific Tokens

### 9.1 Header

```css
:root {
  --header-height: 64px;
  --header-bg: var(--color-bg-surface);
  --header-border: 1px solid var(--color-border-default);
}
```

### 9.2 Tab Bar

```css
:root {
  --tab-bar-height: 48px;
  --tab-bar-bg: var(--color-bg-elevated);
  --tab-active-border: 4px solid var(--color-accent-primary);
}
```

### 9.3 LED Strip

```css
:root {
  --led-strip-height: 120px;
  --led-size: 4px;
  --led-gap: 1px;
  --led-strip-gap: 16px;
}
```

### 9.4 Sidebar

```css
:root {
  --sidebar-width: 264px;
  --sidebar-collapsed: 64px;
  --sidebar-bg: var(--color-bg-surface);
}
```

### 9.5 Cards

```css
:root {
  --card-padding: var(--space-4);
  --card-radius: var(--radius-md);
  --card-bg: var(--color-bg-surface);
}
```

### 9.6 Buttons

```css
:root {
  --button-height: 40px;
  --button-padding-x: var(--space-4);
  --button-radius: var(--radius-sm);
  --button-icon-size: 40px;
}
```

### 9.7 Inputs

```css
:root {
  --input-height: 40px;
  --input-padding-x: var(--space-3);
  --input-radius: var(--radius-sm);
  --input-border: 1px solid var(--color-border-default);
}
```

### 9.8 Sliders

```css
:root {
  --slider-track-height: 4px;
  --slider-thumb-size: 16px;
  --slider-thumb-hover-size: 20px;
  --slider-track-bg: var(--color-bg-surface);
  --slider-fill-bg: var(--color-accent-primary);
}
```

---

## 10. Complete CSS Custom Properties

```css
:root {
  /* Colors */
  --color-bg-canvas: #0A0E14;
  --color-bg-surface: #1A1F2E;
  --color-bg-elevated: #252B3D;
  --color-bg-overlay: rgba(10, 14, 20, 0.95);

  --color-text-primary: #E8E8E8;
  --color-text-secondary: #A0A0A0;
  --color-text-muted: #6B7280;
  --color-text-inverse: #0A0E14;

  --color-accent-primary: #FFD700;
  --color-accent-primary-hover: #FFC700;
  --color-accent-primary-active: #FFB600;
  --color-accent-secondary: #00CED1;
  --color-accent-secondary-hover: #00B8BB;
  --color-accent-secondary-active: #00A3A6;

  --color-success: #10B981;
  --color-warning: #F59E0B;
  --color-error: #EF4444;
  --color-info: #3B82F6;

  --color-border-default: #2D3748;
  --color-border-accent: rgba(255, 215, 0, 0.2);
  --color-border-focus: #FFD700;

  --color-zone-1: #FFD700;
  --color-zone-2: #00CED1;
  --color-zone-3: #10B981;
  --color-zone-4: #F59E0B;

  --color-led-off: #1A1F2E;
  --color-led-center: #00CED1;

  /* Typography */
  --font-family-ui: 'Inter', sans-serif;
  --font-family-mono: 'JetBrains Mono', monospace;

  /* Spacing */
  --space-1: 4px;
  --space-2: 8px;
  --space-3: 12px;
  --space-4: 16px;
  --space-6: 24px;
  --space-8: 32px;

  /* Border Radius */
  --radius-sm: 4px;
  --radius-md: 8px;
  --radius-lg: 12px;
  --radius-full: 9999px;

  /* Shadows */
  --shadow-subtle: 0 2px 4px rgba(0, 0, 0, 0.3);
  --shadow-moderate: 0 4px 12px rgba(0, 0, 0, 0.4);
  --shadow-elevated: 0 8px 24px rgba(0, 0, 0, 0.5);

  /* Animation */
  --duration-fast: 200ms;
  --duration-normal: 300ms;
  --ease-default: cubic-bezier(0.4, 0.0, 0.2, 1);
}
```
