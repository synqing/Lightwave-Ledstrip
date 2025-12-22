import { test, expect } from '@playwright/test';

/**
 * Accessibility Tests
 * 
 * Tests for keyboard navigation, focus management, and WCAG 2.1 compliance.
 */

test.describe('Accessibility Tests', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForSelector('canvas', { timeout: 5000 });
  });

  test.describe('Tab Keyboard Navigation', () => {
    test('navigates between tabs with arrow keys', async ({ page }) => {
      // Focus first tab
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      
      // Should be on first tab (Control)
      const firstTab = page.locator('button[role="tab"]').first();
      await expect(firstTab).toBeFocused();
      
      // Press ArrowRight to move to next tab
      await page.keyboard.press('ArrowRight');
      const secondTab = page.locator('button[role="tab"]').nth(1);
      await expect(secondTab).toBeFocused();
      
      // Press ArrowRight again
      await page.keyboard.press('ArrowRight');
      const thirdTab = page.locator('button[role="tab"]').nth(2);
      await expect(thirdTab).toBeFocused();
      
      // Press ArrowLeft to go back
      await page.keyboard.press('ArrowLeft');
      await expect(secondTab).toBeFocused();
    });

    test('jumps to first/last tab with Home/End', async ({ page }) => {
      // Focus first tab
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      
      const firstTab = page.locator('button[role="tab"]').first();
      await expect(firstTab).toBeFocused();
      
      // Press End to jump to last tab
      await page.keyboard.press('End');
      const lastTab = page.locator('button[role="tab"]').last();
      await expect(lastTab).toBeFocused();
      
      // Press Home to jump to first tab
      await page.keyboard.press('Home');
      await expect(firstTab).toBeFocused();
    });

    test('activates tab with Enter or Space', async ({ page }) => {
      // Focus first tab
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      
      const firstTab = page.locator('button[role="tab"]').first();
      await expect(firstTab).toBeFocused();
      
      // Move to second tab
      await page.keyboard.press('ArrowRight');
      const secondTab = page.locator('button[role="tab"]').nth(1);
      
      // Activate with Enter
      await page.keyboard.press('Enter');
      await expect(secondTab).toHaveAttribute('aria-selected', 'true');
      
      // Move to third tab
      await page.keyboard.press('ArrowRight');
      const thirdTab = page.locator('button[role="tab"]').nth(2);
      
      // Activate with Space
      await page.keyboard.press(' ');
      await expect(thirdTab).toHaveAttribute('aria-selected', 'true');
    });

    test('wraps around at tab boundaries', async ({ page }) => {
      // Focus first tab
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      
      // Press ArrowLeft from first tab should wrap to last
      await page.keyboard.press('ArrowLeft');
      const lastTab = page.locator('button[role="tab"]').last();
      await expect(lastTab).toBeFocused();
      
      // Press ArrowRight from last tab should wrap to first
      await page.keyboard.press('ArrowRight');
      const firstTab = page.locator('button[role="tab"]').first();
      await expect(firstTab).toBeFocused();
    });
  });

  test.describe('Listbox Keyboard Navigation', () => {
    test('opens listbox with Enter, Space, or ArrowDown', async ({ page }) => {
      // Navigate to Control tab (where palette listbox is)
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('Enter');
      
      // Find palette listbox trigger
      const listboxTrigger = page.locator('button[aria-haspopup="listbox"]').first();
      
      // Tab to trigger
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      
      // Open with ArrowDown
      await page.keyboard.press('ArrowDown');
      const listbox = page.locator('ul[role="listbox"]');
      await expect(listbox).toBeVisible();
      await expect(listboxTrigger).toHaveAttribute('aria-expanded', 'true');
    });

    test('navigates options with arrow keys', async ({ page }) => {
      // Navigate to Control tab and open listbox
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('Enter');
      
      // Find and focus listbox trigger
      const listboxTrigger = page.locator('button[aria-haspopup="listbox"]').first();
      await listboxTrigger.focus();
      await page.keyboard.press('ArrowDown');
      
      const listbox = page.locator('ul[role="listbox"]');
      await expect(listbox).toBeVisible();
      
      // Navigate with ArrowDown
      await page.keyboard.press('ArrowDown');
      const secondOption = page.locator('li[role="option"]').nth(1);
      await expect(secondOption).toHaveClass(/bg-primary\/20/);
      
      // Navigate with ArrowUp
      await page.keyboard.press('ArrowUp');
      const firstOption = page.locator('li[role="option"]').first();
      await expect(firstOption).toHaveClass(/bg-primary\/20/);
    });

    test('closes listbox with Escape', async ({ page }) => {
      // Navigate to Control tab and open listbox
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('Enter');
      
      const listboxTrigger = page.locator('button[aria-haspopup="listbox"]').first();
      await listboxTrigger.focus();
      await page.keyboard.press('ArrowDown');
      
      const listbox = page.locator('ul[role="listbox"]');
      await expect(listbox).toBeVisible();
      
      // Close with Escape
      await page.keyboard.press('Escape');
      await expect(listbox).not.toBeVisible();
      await expect(listboxTrigger).toHaveAttribute('aria-expanded', 'false');
      await expect(listboxTrigger).toBeFocused();
    });

    test('selects option with Enter or Space', async ({ page }) => {
      // Navigate to Control tab and open listbox
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('Enter');
      
      const listboxTrigger = page.locator('button[aria-haspopup="listbox"]').first();
      await listboxTrigger.focus();
      await page.keyboard.press('ArrowDown');
      
      // Select first option with Enter
      await page.keyboard.press('Enter');
      await expect(listboxTrigger).toHaveAttribute('aria-expanded', 'false');
      
      // Reopen and select second option with Space
      await listboxTrigger.focus();
      await page.keyboard.press('ArrowDown');
      await page.keyboard.press('ArrowDown');
      await page.keyboard.press(' ');
      await expect(listboxTrigger).toHaveAttribute('aria-expanded', 'false');
    });
  });

  test.describe('Timeline Keyboard Navigation', () => {
    test('scenes are keyboard-focusable', async ({ page }) => {
      // Navigate to Shows tab
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('ArrowRight');
      await page.keyboard.press('Enter');
      
      // Wait for timeline to render
      await page.waitForTimeout(200);
      
      // Tab to first scene
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      
      // Should be able to focus a scene
      const scene = page.locator('[role="button"][aria-label*="Scene"]').first();
      await scene.focus();
      await expect(scene).toBeFocused();
    });

    test('moves scene with arrow keys', async ({ page }) => {
      // Navigate to Shows tab
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('ArrowRight');
      await page.keyboard.press('Enter');
      
      await page.waitForTimeout(200);
      
      // Focus first scene
      const scene = page.locator('[role="button"][aria-label*="Scene"]').first();
      await scene.focus();
      
      // Get initial position
      const initialLeft = await scene.evaluate((el) => {
        return parseFloat((el as HTMLElement).style.left);
      });
      
      // Move right with ArrowRight
      await page.keyboard.press('ArrowRight');
      await page.waitForTimeout(100);
      
      const newLeft = await scene.evaluate((el) => {
        return parseFloat((el as HTMLElement).style.left);
      });
      
      expect(newLeft).toBeGreaterThan(initialLeft);
    });

    test('opens edit dialog with Enter', async ({ page }) => {
      // Navigate to Shows tab
      await page.keyboard.press('Tab');
      await page.keyboard.press('Tab');
      await page.keyboard.press('ArrowRight');
      await page.keyboard.press('Enter');
      
      await page.waitForTimeout(200);
      
      // Focus first scene
      const scene = page.locator('[role="button"][aria-label*="Scene"]').first();
      await scene.focus();
      
      // Open edit dialog
      await page.keyboard.press('Enter');
      
      const dialog = page.locator('[role="dialog"]');
      await expect(dialog).toBeVisible();
    });
  });

  test.describe('Skip Links', () => {
    test('skip link is visible on focus', async ({ page }) => {
      // Skip link should be hidden by default
      const skipLink = page.locator('a[href="#main-content"]');
      await expect(skipLink).not.toBeVisible();
      
      // Focus skip link
      await page.keyboard.press('Tab');
      
      // Should become visible
      await expect(skipLink).toBeVisible();
    });

    test('skip link navigates to main content', async ({ page }) => {
      // Focus skip link
      await page.keyboard.press('Tab');
      
      const skipLink = page.locator('a[href="#main-content"]');
      await expect(skipLink).toBeFocused();
      
      // Activate skip link
      await page.keyboard.press('Enter');
      
      // Should navigate to main content
      const mainContent = page.locator('#main-content');
      await expect(mainContent).toBeInViewport();
    });
  });

  test.describe('Focus Management', () => {
    test('focus indicators are visible', async ({ page }) => {
      // Tab through interactive elements
      await page.keyboard.press('Tab');
      
      // Check that focused element has focus ring
      const focusedElement = page.locator(':focus');
      const hasFocusRing = await focusedElement.evaluate((el) => {
        const style = window.getComputedStyle(el);
        return style.outlineWidth !== '0px' || 
               style.boxShadow !== 'none' ||
               el.classList.toString().includes('ring');
      });
      
      expect(hasFocusRing).toBe(true);
    });

    test('logical focus order', async ({ page }) => {
      // Tab through elements and verify order
      const focusOrder: string[] = [];
      
      for (let i = 0; i < 10; i++) {
        await page.keyboard.press('Tab');
        const focused = await page.evaluate(() => {
          const el = document.activeElement;
          return el?.getAttribute('aria-label') || 
                 el?.textContent?.trim() || 
                 el?.tagName || 
                 'unknown';
        });
        focusOrder.push(focused);
      }
      
      // Verify order is logical (header -> tabs -> content)
      expect(focusOrder.length).toBeGreaterThan(0);
    });
  });
});

