# Accessibility Guidelines

**Version:** 1.0  
**Last Updated:** 2025-12-22  
**Target:** WCAG 2.1 Level A compliance

---

## Overview

The Lightwave Dashboard is designed to be accessible to all users, including those using keyboards, screen readers, and other assistive technologies. This document outlines accessibility features, keyboard shortcuts, and testing procedures.

---

## Keyboard Shortcuts

### Global Navigation

| Shortcut | Action |
|----------|--------|
| `Tab` | Move focus to next interactive element |
| `Shift+Tab` | Move focus to previous interactive element |
| `Enter` / `Space` | Activate focused element |
| `Escape` | Close modals, dropdowns, or cancel actions |

### Tab Navigation

| Shortcut | Action |
|----------|--------|
| `ArrowLeft` / `ArrowRight` | Navigate between tabs |
| `Home` | Jump to first tab |
| `End` | Jump to last tab |
| `Enter` / `Space` | Activate selected tab |

### Listbox/Dropdown

| Shortcut | Action |
|----------|--------|
| `Enter` / `Space` / `ArrowDown` | Open dropdown |
| `ArrowUp` / `ArrowDown` | Navigate options |
| `Home` | Jump to first option |
| `End` | Jump to last option |
| `Enter` / `Space` | Select option |
| `Escape` | Close dropdown |

### Timeline Editor

| Shortcut | Action |
|----------|--------|
| `Tab` | Focus scene elements |
| `Enter` / `Space` | Open scene edit dialog |
| `ArrowLeft` / `ArrowRight` | Move scene start time (1% increments) |
| `Shift+ArrowLeft` / `Shift+ArrowRight` | Move scene start time (5% increments) |
| `ArrowUp` / `ArrowDown` | Move scene to different zone |
| `Delete` | Remove scene (with confirmation) |

---

## Accessibility Features

### Keyboard Navigation

All interactive elements are keyboard accessible:

- **Tabs**: Full arrow key navigation with Home/End support
- **Listboxes**: Complete keyboard navigation with Escape support
- **Buttons**: Accessible via Tab and Enter/Space
- **Forms**: Logical tab order, labels associated with inputs
- **Timeline**: Keyboard alternative for drag-and-drop

### Screen Reader Support

- **ARIA Labels**: All interactive elements have descriptive labels
- **ARIA Roles**: Semantic roles (tab, tabpanel, listbox, option, dialog)
- **ARIA States**: aria-selected, aria-expanded, aria-disabled
- **Live Regions**: Dynamic content updates announced
- **Skip Links**: Jump to main content

### Focus Management

- **Visible Focus**: All focusable elements have visible focus indicators
- **Focus Trap**: Modals trap focus within dialog
- **Focus Restoration**: Focus returns to trigger after modal close
- **Logical Order**: Tab order follows visual layout

---

## Component Accessibility

### Tabs

**Implementation:** `DashboardShell.tsx`

- Uses `role="tablist"` and `role="tab"`
- Roving tabindex pattern (only active tab has `tabIndex={0}`)
- `aria-controls` links tabs to tabpanels
- `aria-selected` indicates active tab
- Arrow keys navigate, Enter/Space activates

### Listbox

**Implementation:** `components/ui/Listbox.tsx`

- Uses `role="combobox"` on trigger
- Uses `role="listbox"` and `role="option"` for options
- `aria-expanded` indicates open state
- `aria-activedescendant` tracks active option
- Full keyboard navigation support

### Timeline Scenes

**Implementation:** `components/tabs/ShowsTab.tsx`

- Scenes are keyboard-focusable (`tabIndex={0}`)
- `role="button"` with descriptive `aria-label`
- Arrow keys move scenes
- Enter opens edit dialog
- Delete removes scene (with confirmation)

### Modals

**Implementation:** Uses `useFocusTrap` hook

- Focus trapped within modal
- Escape closes modal
- Focus restored to trigger on close
- First focusable element receives focus on open

---

## Testing Procedures

### Manual Testing

#### Keyboard Navigation

1. **Tab through all interactive elements**
   - Verify logical order
   - Verify focus indicators visible
   - Verify no keyboard traps

2. **Test tab navigation**
   - Arrow keys navigate between tabs
   - Home/End jump to first/last
   - Enter/Space activates tab

3. **Test listbox navigation**
   - Arrow keys navigate options
   - Escape closes dropdown
   - Enter/Space selects option

4. **Test timeline keyboard editing**
   - Scenes are focusable
   - Arrow keys move scenes
   - Enter opens edit dialog

#### Screen Reader Testing

1. **VoiceOver (macOS)**
   - Enable VoiceOver (Cmd+F5)
   - Navigate with VO+Arrow keys
   - Verify announcements are clear

2. **NVDA (Windows)**
   - Install and enable NVDA
   - Navigate with arrow keys
   - Verify focus announcements

3. **Checklist:**
   - [ ] All interactive elements announced
   - [ ] Labels are descriptive
   - [ ] States are announced (selected, expanded, etc.)
   - [ ] Navigation order is logical

### Automated Testing

Run accessibility tests:

```bash
npm run test:e2e -- accessibility.spec.ts
```

Tests cover:
- Tab keyboard navigation
- Listbox keyboard interaction
- Timeline keyboard editing
- Focus management
- Skip links

---

## WCAG 2.1 Compliance

### Level A Requirements

- ✅ **2.1.1 Keyboard**: All functionality available via keyboard
- ✅ **2.1.2 No Keyboard Trap**: Focus can leave all components
- ✅ **2.4.3 Focus Order**: Logical focus order
- ✅ **4.1.2 Name, Role, Value**: Elements have accessible names

### Level AA Requirements

- ✅ **2.4.7 Focus Visible**: Focus indicator visible
- ⚠️ **1.4.3 Contrast**: Verify text contrast ratios (ongoing)
- ⚠️ **2.4.6 Headings and Labels**: Verify heading hierarchy (ongoing)

---

## Best Practices

### 1. Always Provide Labels

```tsx
// ✅ Good
<label htmlFor="input-id">Search</label>
<input id="input-id" />

// ✅ Also good (for icon-only buttons)
<button aria-label="Settings">
  <SettingsIcon />
</button>
```

### 2. Use Semantic HTML

```tsx
// ✅ Good
<button role="tab" aria-selected="true">Tab</button>

// ❌ Bad
<div onClick={handleClick} role="button">Tab</div>
```

### 3. Manage Focus Properly

```tsx
// ✅ Good - use focus management hooks
const { containerRef } = useFocusTrap(isOpen);

// ❌ Bad - manual focus manipulation
useEffect(() => {
  document.activeElement?.blur();
}, []);
```

### 4. Provide Keyboard Alternatives

```tsx
// ✅ Good - keyboard alternative for drag-and-drop
<div
  onKeyDown={handleKeyDown}
  tabIndex={0}
  role="button"
>
  Draggable item
</div>
```

### 5. Test with Screen Readers

- Test with VoiceOver (macOS) or NVDA (Windows)
- Verify all content is announced
- Verify navigation is logical

---

## Common Issues and Fixes

### Issue: Focus Not Visible

**Symptom:** Can't see which element has focus

**Fix:** Add focus styles:
```tsx
className="focus:outline-none focus:ring-2 focus:ring-primary"
```

### Issue: Keyboard Trap

**Symptom:** Can't Tab out of a component

**Fix:** Use `useFocusTrap` hook or ensure Escape key works

### Issue: Screen Reader Doesn't Announce

**Symptom:** Screen reader silent on interactions

**Fix:** Add ARIA labels and live regions:
```tsx
<button aria-label="Close dialog">×</button>
<div role="status" aria-live="polite">Updated</div>
```

### Issue: Focus Order Wrong

**Symptom:** Tab order doesn't match visual order

**Fix:** Reorder DOM elements or use `tabIndex={0}` sparingly

---

## Resources

- [WCAG 2.1 Guidelines](https://www.w3.org/WAI/WCAG21/quickref/)
- [WAI-ARIA Authoring Practices](https://www.w3.org/WAI/ARIA/apg/)
- [MDN: Accessibility](https://developer.mozilla.org/en-US/docs/Web/Accessibility)
- [WebAIM: Keyboard Accessibility](https://webaim.org/techniques/keyboard/)

---

## Changelog

### 2025-12-22
- Initial documentation
- Added keyboard shortcuts reference
- Added component accessibility details
- Added testing procedures
- Added WCAG compliance checklist

