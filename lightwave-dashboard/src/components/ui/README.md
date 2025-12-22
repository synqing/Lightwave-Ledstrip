# UI Component Gallery

**Purpose:** Copy-pasteable component patterns to prevent style drift and ensure consistency.

---

## Quick Reference

| Component | File | Usage |
|-----------|------|-------|
| SliderControl | `SliderControl.tsx` | Parameter sliders (brightness, speed, etc.) |
| PresetButton | `PresetButton.tsx` | Preset selection buttons |
| InfoRow | `InfoRow.tsx` | Key-value information display |
| Listbox | `Listbox.tsx` | Accessible dropdown/listbox with keyboard navigation |

---

## SliderControl

**File:** `src/components/ui/SliderControl.tsx`

**Usage:**
```tsx
import { SliderControl } from './ui/SliderControl';

<SliderControl 
  label="Brightness" 
  value={brightness} 
  min={0} 
  max={255} 
  onChange={(v) => actions.setParameters({ brightness: v })}
  fillColor="from-primary to-primary-dark"
/>

<SliderControl 
  label="Speed" 
  value={speed} 
  min={1} 
  max={50} 
  onChange={(v) => actions.setParameters({ speed: v })}
  fillColor="from-primary to-primary-dark"
/>

<SliderControl 
  label="Blur" 
  value={blur} 
  min={0} 
  max={1} 
  step={0.01}
  onChange={setBlur} 
  format={(v) => v.toFixed(2)}
  fillColor="from-accent-cyan/80 to-accent-cyan"
/>
```

**Props:**
- `label: string` - Label text
- `value: number` - Current value
- `min: number` - Minimum value
- `max: number` - Maximum value
- `step?: number` - Step increment (default: 1)
- `onChange: (val: number) => void` - Change handler
- `format?: (val: number) => string` - Custom value formatter
- `icon?: React.ReactNode` - Optional icon
- `fillColor?: string` - Tailwind gradient classes (default: "from-primary/80 to-primary")

**Pattern:**
- Tabular numbers for consistent alignment
- Custom gradient track fill
- Custom thumb styling with shadow
- Hover effects via CSS classes

---

## PresetButton

**File:** `src/components/ui/PresetButton.tsx`

**Usage:**
```tsx
import { PresetButton } from './ui/PresetButton';

<PresetButton 
  label="Movie Mode" 
  color="from-purple-500 to-blue-600"
  onClick={() => {/* handler */}}
/>

<PresetButton 
  label="Relax" 
  color="from-teal-400 to-emerald-500"
/>

<PresetButton 
  label="Party" 
  color="from-pink-500 to-rose-500"
/>
```

**Props:**
- `label: string` - Button label
- `color: string` - Tailwind gradient classes (e.g., "from-purple-500 to-blue-600")
- `onClick?: () => void` - Optional click handler

**Pattern:**
- Gradient background with opacity
- Hover scale effect
- Active scale-down effect
- Color-coded for visual distinction

---

## InfoRow

**File:** `src/components/ui/InfoRow.tsx`

**Usage:**
```tsx
import { InfoRow } from './ui/InfoRow';

<div className="space-y-1">
  <InfoRow label="Chip" value={state.deviceInfo?.chipModel ?? 'unknown'} />
  <InfoRow label="SDK" value={state.deviceInfo?.sdkVersion ?? 'unknown'} />
  <InfoRow label="Flash" value={`${Math.round(flashSize / (1024 * 1024))} MB`} />
  <InfoRow label="CPU" value={`${cpuFreq} MHz`} />
</div>
```

**Props:**
- `label: string` - Left-side label
- `value: string` - Right-side value

**Pattern:**
- Two-column layout
- Monospace value styling
- Subtle borders between rows
- Last row has no border

---

## Common Patterns (Copy-Paste Ready)

### Glass V4 Card

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
  </div>
  
  {/* Card content */}
</div>
```

### Primary Button

```tsx
<button className="h-9 px-3 rounded-lg bg-primary text-background hover:brightness-110 transition-all flex items-center text-[11px] font-medium touch-target">
  <Icon className="w-3.5 h-3.5 mr-1" />
  <span>Button Text</span>
</button>
```

### Secondary Button

```tsx
<button 
  className="touch-target grid place-items-center rounded-xl btn-secondary"
  style={{ 
    width: '2.75rem', 
    height: '2.75rem', 
    border: '1px solid rgba(255,255,255,0.10)' 
  }}
  aria-label="Action"
>
  <Icon className="w-[18px] h-[18px] text-text-secondary" strokeWidth={1.5} />
</button>
```

### Status Badge

```tsx
<span className={`rounded px-2 py-0.5 text-[9px] ${
  isActive
    ? 'bg-accent-green/10 border border-accent-green/30 text-accent-green'
    : 'bg-primary/10 border border-primary/30 text-primary'
}`}>
  {isActive ? 'ACTIVE' : 'INACTIVE'}
</span>
```

### Page Header

```tsx
<div className="flex flex-col gap-2 lg:flex-row lg:items-end lg:justify-between mb-5">
  <div>
    <h1 className="uppercase tracking-tight font-display text-2xl" style={{ letterSpacing: '0.01em' }}>
      Page Title
    </h1>
    <p className="text-xs text-text-secondary mt-1">
      Page description or metadata
    </p>
  </div>
  <div className="flex items-center gap-2">
    {/* Action buttons */}
  </div>
</div>
```

### Section Header (Inside Card)

```tsx
<div className="flex items-center justify-between mb-4">
  <span className="uppercase tracking-tight font-display text-[0.9375rem]" style={{ letterSpacing: '0.02em' }}>
    Section Title
  </span>
  <Icon className="w-4 h-4 text-primary" />
</div>
```

### Input Field

```tsx
<input
  type="text"
  value={value}
  onChange={(e) => setValue(e.target.value)}
  placeholder="Placeholder text"
  className="w-full bg-[#2f3849] border border-white/10 rounded-lg px-3 py-2 text-xs text-text-primary focus:outline-none focus:border-primary/50 transition-colors"
/>
```

### Listbox

**File:** `src/components/ui/Listbox.tsx`

**Usage:**
```tsx
import { Listbox } from './ui/Listbox';

<Listbox
  label="Palette"
  options={[
    { value: 'aurora', label: 'Aurora Glass' },
    { value: 'ocean', label: 'Ocean Wave' },
  ]}
  value={selectedPalette}
  onChange={(value) => setSelectedPalette(value)}
  placeholder="Select palette"
/>
```

**Accessibility Features:**
- Full keyboard navigation (Arrow keys, Home/End, Enter/Space, Escape)
- ARIA attributes (role="combobox", role="listbox", role="option")
- Focus management (focus moves into listbox when opened)
- Screen reader support

---

### Select Dropdown (Native)

**Note:** For custom styling and enhanced accessibility, use `Listbox` component instead.

```tsx
<select
  value={selectedValue}
  onChange={(e) => setSelectedValue(e.target.value)}
  className="h-9 rounded-lg bg-[#2f3849] border border-white/10 px-2 text-[11px] text-text-primary focus:outline-none focus:border-primary/50"
>
  <option value="">Select option</option>
  <option value="1">Option 1</option>
  <option value="2">Option 2</option>
</select>
```

### Loading State

```tsx
<div className="flex items-center justify-center py-8">
  <div className="animate-spin rounded-full h-8 w-8 border-2 border-primary border-t-transparent"></div>
</div>
```

### Empty State

```tsx
<div className="flex flex-col items-center justify-center py-16 text-center">
  <Icon className="w-12 h-12 text-text-secondary/30 mb-4" />
  <p className="text-sm text-text-secondary">No items found</p>
  <p className="text-xs text-text-secondary/70 mt-1">
    Description of empty state
  </p>
</div>
```

### Grid Layout

```tsx
<div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
  {/* Grid items */}
</div>
```

### Responsive Flex Layout

```tsx
<div className="flex flex-col gap-2 lg:flex-row lg:items-end lg:justify-between">
  {/* Content */}
</div>
```

---

## Icon Usage

**Library:** `lucide-react`

**Import Pattern:**
```tsx
import { Settings, Sliders, Wand2, Cpu, Zap, Play } from 'lucide-react';
```

**Common Sizes:**
- `w-3.5 h-3.5` - Small icons (11px)
- `w-4 h-4` - Standard icons (16px)
- `w-[18px] h-[18px]` - Medium icons (18px)
- `w-5 h-5` - Large icons (20px)
- `w-[22px] h-[22px]` - Extra large (22px)

**Stroke Width:**
- `strokeWidth={1.5}` - Standard
- `strokeWidth={2}` - Bold

**Color Classes:**
- `text-text-primary` - Primary text color
- `text-text-secondary` - Secondary text color
- `text-primary` - Gold accent
- `text-accent-cyan` - Cyan accent
- `text-accent-green` - Success/connected
- `text-accent-red` - Error/disconnected

---

## Animation Classes

**Defined in `index.css`:**

- `animate-slide-down` - Page transition
- `card-hover` - Card hover effect
- `status-pulse-connected` - Connected status pulse
- `status-pulse-disconnected` - Disconnected status pulse
- `shimmer` - Loading shimmer effect
- `tab-underline` - Tab indicator animation

**Usage:**
```tsx
<div className="animate-slide-down">
  {/* Content */}
</div>

<div className="card-hover">
  {/* Card content */}
</div>
```

---

## Responsive Patterns

### Padding
```tsx
className="p-4 sm:p-5 lg:p-6 xl:p-8"
```

### Gap
```tsx
className="gap-2 sm:gap-3"
```

### Grid Columns
```tsx
className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3"
```

### Text Visibility
```tsx
className="hidden sm:inline"
```

### Flex Direction
```tsx
className="flex flex-col lg:flex-row"
```

---

## Color Gradient Patterns

**Common Gradients:**
- `from-primary to-primary-dark` - Gold gradient
- `from-accent-cyan/80 to-accent-cyan` - Cyan gradient
- `from-accent-green/80 to-accent-green` - Green gradient
- `from-purple-500 to-blue-600` - Purple-blue
- `from-teal-400 to-emerald-500` - Teal-emerald
- `from-pink-500 to-rose-500` - Pink-rose

**Usage:**
```tsx
<div className={`bg-gradient-to-br ${color}`}>
  {/* Content */}
</div>
```

---

## Accessibility Checklist

When creating new components:
- [ ] Minimum touch target: `touch-target` class or `min-w-[44px] min-h-[44px]`
- [x] ARIA labels for icon-only buttons
- [x] Focus states visible
- [x] Keyboard navigation supported
- [ ] Color contrast meets WCAG AA
- [ ] Semantic HTML elements

---

## References

- [UI Standards Documentation](../../docs/UI_STANDARDS.md)
- [Architecture Documentation](../../docs/ARCHITECTURE.md)
- [Tailwind CSS Docs](https://tailwindcss.com/docs)
- [Lucide Icons](https://lucide.dev)

---

**For AI Agents:** When implementing UI:
1. Copy patterns from this file
2. Use existing components when possible
3. Follow Glass V4 design system
4. Maintain responsive design
5. Test accessibility

