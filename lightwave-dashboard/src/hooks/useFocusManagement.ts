import { useEffect, useRef, useState, type RefObject } from 'react';

/**
 * Focus Management Hooks
 * 
 * Collection of hooks for managing focus in accessible components:
 * - Focus trap for modals
 * - Focus restoration after modal close
 * - Roving tabindex pattern for keyboard navigation
 */

/**
 * Hook for trapping focus within a modal or dialog
 * 
 * @param isActive - Whether the focus trap should be active
 * @returns Refs for container, initial focus element, and return focus element
 * 
 * @example
 * ```tsx
 * const { containerRef, initialFocusRef, returnFocusRef } = useFocusTrap(isOpen);
 * 
 * return (
 *   <div ref={containerRef}>
 *     <button ref={initialFocusRef}>First focusable</button>
 *     <button>Second</button>
 *   </div>
 * );
 * ```
 */
export function useFocusTrap(isActive: boolean): {
  containerRef: RefObject<HTMLElement | null>;
  initialFocusRef: RefObject<HTMLElement | null>;
  returnFocusRef: RefObject<HTMLElement | null>;
} {
  const containerRef = useRef<HTMLElement>(null);
  const initialFocusRef = useRef<HTMLElement>(null);
  const returnFocusRef = useRef<HTMLElement>(null);

  useEffect(() => {
    if (!isActive || !containerRef.current) return;

    const container = containerRef.current;

    // Get all focusable elements within container
    const getFocusableElements = (): HTMLElement[] => {
      const selector = [
        'a[href]',
        'button:not([disabled])',
        'textarea:not([disabled])',
        'input:not([disabled])',
        'select:not([disabled])',
        '[tabindex]:not([tabindex="-1"])',
      ].join(', ');

      return Array.from(container.querySelectorAll<HTMLElement>(selector)).filter(
        (el) => {
          // Filter out hidden elements
          const style = window.getComputedStyle(el);
          return style.display !== 'none' && style.visibility !== 'hidden';
        }
      );
    };

    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key !== 'Tab') return;

      const focusableElements = getFocusableElements();
      if (focusableElements.length === 0) return;

      const firstElement = focusableElements[0];
      const lastElement = focusableElements[focusableElements.length - 1];

      if (e.shiftKey) {
        // Shift + Tab: if on first element, move to last
        if (document.activeElement === firstElement) {
          e.preventDefault();
          lastElement.focus();
        }
      } else {
        // Tab: if on last element, move to first
        if (document.activeElement === lastElement) {
          e.preventDefault();
          firstElement.focus();
        }
      }
    };

    // Focus initial element if provided, otherwise focus first focusable
    if (initialFocusRef.current) {
      initialFocusRef.current.focus();
    } else {
      const focusableElements = getFocusableElements();
      if (focusableElements.length > 0) {
        focusableElements[0].focus();
      }
    }

    container.addEventListener('keydown', handleKeyDown);

    return () => {
      container.removeEventListener('keydown', handleKeyDown);
    };
  }, [isActive]);

  return {
    containerRef,
    initialFocusRef,
    returnFocusRef,
  };
}

/**
 * Hook for restoring focus to a previous element
 * 
 * @param shouldRestore - Whether focus should be restored
 * @returns Ref to the element that should receive focus
 * 
 * @example
 * ```tsx
 * const [isOpen, setIsOpen] = useState(false);
 * const { returnFocusRef } = useRestoreFocus(isOpen);
 * 
 * return (
 *   <>
 *     <button ref={returnFocusRef} onClick={() => setIsOpen(true)}>
 *       Open Modal
 *     </button>
 *     {isOpen && <Modal onClose={() => setIsOpen(false)} />}
 *   </>
 * );
 * ```
 */
export function useRestoreFocus(shouldRestore: boolean): {
  returnFocusRef: RefObject<HTMLElement | null>;
} {
  const returnFocusRef = useRef<HTMLElement>(null);
  const previousActiveElementRef = useRef<HTMLElement | null>(null);

  useEffect(() => {
    if (shouldRestore) {
      // Store the currently focused element
      previousActiveElementRef.current = document.activeElement as HTMLElement;
    } else {
      // Restore focus to the stored element or returnFocusRef
      const elementToFocus = returnFocusRef.current || previousActiveElementRef.current;
      if (elementToFocus) {
        // Use setTimeout to ensure DOM has updated
        setTimeout(() => {
          elementToFocus.focus();
        }, 0);
      }
    }
  }, [shouldRestore]);

  return {
    returnFocusRef,
  };
}

/**
 * Hook for implementing roving tabindex pattern
 * 
 * Only one item in a group has tabIndex={0}, all others have tabIndex={-1}.
 * Arrow keys navigate between items, Enter/Space activates.
 * 
 * @param items - Array of items to navigate between
 * @param initialIndex - Initial active index (default: 0)
 * @returns Active index, setter, tabIndex getter, and keyboard handler
 * 
 * @example
 * ```tsx
 * const items = ['Option 1', 'Option 2', 'Option 3'];
 * const { activeIndex, getTabIndex, handleKeyDown } = useRovingTabIndex(items);
 * 
 * return (
 *   <div role="toolbar">
 *     {items.map((item, index) => (
 *       <button
 *         key={item}
 *         tabIndex={getTabIndex(index)}
 *         onKeyDown={(e) => handleKeyDown(e, index)}
 *       >
 *         {item}
 *       </button>
 *     ))}
 *   </div>
 * );
 * ```
 */
export function useRovingTabIndex<T>(
  items: T[],
  initialIndex: number = 0
): {
  activeIndex: number;
  setActiveIndex: (index: number) => void;
  getTabIndex: (index: number) => number;
  handleKeyDown: (e: React.KeyboardEvent) => void;
} {
  const [activeIndex, setActiveIndex] = useState(initialIndex);

  const getTabIndex = (index: number): number => {
    return index === activeIndex ? 0 : -1;
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    switch (e.key) {
      case 'ArrowRight':
      case 'ArrowDown':
        e.preventDefault();
        setActiveIndex(prev => (prev + 1) % items.length);
        break;
      case 'ArrowLeft':
      case 'ArrowUp':
        e.preventDefault();
        setActiveIndex(prev => (prev - 1 + items.length) % items.length);
        break;
      case 'Home':
        e.preventDefault();
        setActiveIndex(0);
        break;
      case 'End':
        e.preventDefault();
        setActiveIndex(items.length - 1);
        break;
    }
  };

  return {
    activeIndex,
    setActiveIndex,
    getTabIndex,
    handleKeyDown,
  };
}

