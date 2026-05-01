# Focus Management Patterns

**Version:** 1.0  
**Last Updated:** 2025-12-22  
**Purpose:** Guidelines and patterns for managing focus in accessible components

---

## Overview

Proper focus management is critical for keyboard and screen reader accessibility. This document outlines the patterns and hooks available for managing focus in the Lightwave Dashboard.

---

## Available Hooks

### `useFocusTrap`

Traps focus within a modal or dialog, preventing focus from escaping to elements outside.

**When to use:**
- Modals and dialogs
- Dropdown menus (when open)
- Popover components

**Example:**
```tsx
import { useFocusTrap } from '../hooks/useFocusManagement';

function Modal({ isOpen, onClose }) {
  const { containerRef, initialFocusRef } = useFocusTrap(isOpen);

  return (
    <div ref={containerRef} className="modal">
      <button ref={initialFocusRef} onClick={onClose}>
        Close
      </button>
      <input type="text" />
      <button>Submit</button>
    </div>
  );
}
```

**Behavior:**
- Tab cycles through focusable elements within container
- Shift+Tab reverses direction
- When reaching last element, Tab wraps to first
- When reaching first element, Shift+Tab wraps to last
- Focus moves to `initialFocusRef` when trap activates

---

### `useRestoreFocus`

Restores focus to the element that had focus before a modal opened.

**When to use:**
- Modals and dialogs
- Any component that steals focus

**Example:**
```tsx
import { useRestoreFocus } from '../hooks/useFocusManagement';

function App() {
  const [isOpen, setIsOpen] = useState(false);
  const { returnFocusRef } = useRestoreFocus(!isOpen);

  return (
    <>
      <button ref={returnFocusRef} onClick={() => setIsOpen(true)}>
        Open Modal
      </button>
      {isOpen && <Modal onClose={() => setIsOpen(false)} />}
    </>
  );
}
```

**Behavior:**
- Stores currently focused element when `shouldRestore` becomes true
- Restores focus when `shouldRestore` becomes false
- If `returnFocusRef` is set, uses that instead of stored element

---

### `useRovingTabIndex`

Implements the roving tabindex pattern for keyboard navigation groups.

**When to use:**
- Toolbars
- Tab lists (though tabs have their own pattern)
- Radio button groups
- Menu bars

**Example:**
```tsx
import { useRovingTabIndex } from '../hooks/useFocusManagement';

function Toolbar() {
  const items = ['Bold', 'Italic', 'Underline'];
  const { activeIndex, getTabIndex, handleKeyDown } = useRovingTabIndex(items);

  return (
    <div role="toolbar">
      {items.map((item, index) => (
        <button
          key={item}
          tabIndex={getTabIndex(index)}
          onKeyDown={(e) => handleKeyDown(e, index)}
        >
          {item}
        </button>
      ))}
    </div>
  );
}
```

**Behavior:**
- Only active item has `tabIndex={0}`
- Other items have `tabIndex={-1}`
- Arrow keys navigate between items
- Home/End jump to first/last item
- Enter/Space should be handled by component (not included in hook)

---

## Focus Management Patterns

### Pattern 1: Modal Focus Trap

**Complete example:**
```tsx
function Modal({ isOpen, onClose }) {
  const { containerRef, initialFocusRef, returnFocusRef } = useFocusTrap(isOpen);
  const { returnFocusRef: restoreRef } = useRestoreFocus(!isOpen);

  // Combine return focus refs
  const combinedReturnRef = returnFocusRef || restoreRef;

  return (
    <>
      <button ref={combinedReturnRef} onClick={() => setIsOpen(true)}>
        Open
      </button>
      
      {isOpen && (
        <div ref={containerRef} className="modal-overlay">
          <div className="modal-content">
            <button ref={initialFocusRef} onClick={onClose}>
              Close
            </button>
            {/* Modal content */}
          </div>
        </div>
      )}
    </>
  );
}
```

---

### Pattern 2: Dropdown Menu

**Complete example:**
```tsx
function DropdownMenu() {
  const [isOpen, setIsOpen] = useState(false);
  const { containerRef } = useFocusTrap(isOpen);
  const triggerRef = useRef<HTMLButtonElement>(null);

  const handleEscape = (e: KeyboardEvent) => {
    if (e.key === 'Escape' && isOpen) {
      setIsOpen(false);
      triggerRef.current?.focus();
    }
  };

  useEffect(() => {
    document.addEventListener('keydown', handleEscape);
    return () => document.removeEventListener('keydown', handleEscape);
  }, [isOpen]);

  return (
    <>
      <button ref={triggerRef} onClick={() => setIsOpen(!isOpen)}>
        Menu
      </button>
      {isOpen && (
        <ul ref={containerRef} role="menu">
          <li role="menuitem">Option 1</li>
          <li role="menuitem">Option 2</li>
        </ul>
      )}
    </>
  );
}
```

---

### Pattern 3: Tab Navigation

**Note:** Tabs use a specialized pattern (see `DashboardShell.tsx` for implementation).

Key principles:
- Roving tabindex (only active tab has `tabIndex={0}`)
- Arrow keys navigate between tabs
- Home/End jump to first/last tab
- Enter/Space activates tab
- Focus moves to tabpanel content on activation

---

## Best Practices

### 1. Always Provide Focus Indicators

Use visible focus styles:
```tsx
className="focus:outline-none focus:ring-2 focus:ring-primary focus:ring-offset-2"
```

### 2. Manage Focus on Mount/Unmount

When opening a modal:
- Focus the first focusable element (or a specific element)
- Store the previously focused element

When closing a modal:
- Restore focus to the previously focused element

### 3. Prevent Focus Traps

Ensure users can always escape:
- Escape key closes modals
- Tab key can eventually leave (or wraps within trap)
- Click outside closes dropdowns

### 4. Skip Links for Main Content

Provide skip links for keyboard users:
```tsx
<a href="#main-content" className="skip-link">
  Skip to main content
</a>
```

### 5. Logical Focus Order

- Focus should follow visual order
- Use `tabIndex` sparingly (prefer DOM order)
- Only use `tabIndex={0}` for programmatic focus management

---

## Common Pitfalls

### ❌ Don't: Remove Focus Styles

```tsx
// Bad
className="focus:outline-none"
```

**Fix:** Always provide visible focus indicator:
```tsx
className="focus:outline-none focus:ring-2 focus:ring-primary"
```

### ❌ Don't: Trap Focus Permanently

```tsx
// Bad - no way to escape
<div onKeyDown={(e) => e.key === 'Tab' && e.preventDefault()}>
```

**Fix:** Use `useFocusTrap` which allows Escape and proper wrapping.

### ❌ Don't: Forget to Restore Focus

```tsx
// Bad - focus lost after modal closes
function Modal({ isOpen, onClose }) {
  return isOpen && <div>...</div>;
}
```

**Fix:** Use `useRestoreFocus`:
```tsx
const { returnFocusRef } = useRestoreFocus(!isOpen);
```

### ❌ Don't: Use tabIndex > 0

```tsx
// Bad - breaks natural tab order
<button tabIndex={5}>Button</button>
```

**Fix:** Use DOM order or `tabIndex={0}` for programmatic focus only.

---

## Testing Focus Management

### Manual Testing

1. **Keyboard Navigation:**
   - Tab through all interactive elements
   - Verify focus order is logical
   - Verify focus indicators are visible

2. **Modal Focus Trap:**
   - Open modal
   - Tab should cycle within modal
   - Escape should close and restore focus

3. **Screen Reader:**
   - Test with VoiceOver (macOS) or NVDA (Windows)
   - Verify focus announcements
   - Verify focus order matches reading order

### Automated Testing

Use Playwright to test focus:
```typescript
test('modal traps focus', async ({ page }) => {
  await page.click('button[aria-label="Open modal"]');
  await page.keyboard.press('Tab');
  // Verify focus is within modal
  const focused = await page.evaluate(() => document.activeElement);
  expect(focused.closest('.modal')).not.toBeNull();
});
```

---

## Resources

- [WAI-ARIA Authoring Practices: Focus Management](https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/)
- [MDN: Managing Focus](https://developer.mozilla.org/en-US/docs/Web/Accessibility/Keyboard-navigable_JavaScript_widgets)
- [WCAG 2.1: Focus Order](https://www.w3.org/WAI/WCAG21/Understanding/focus-order.html)

---

## Changelog

### 2025-12-22
- Initial documentation
- Added focus trap pattern
- Added focus restoration pattern
- Added roving tabindex pattern
- Added best practices and pitfalls

