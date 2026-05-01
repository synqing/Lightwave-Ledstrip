import { test as base, expect, Page } from '@playwright/test';

/**
 * Animation freeze script - ensures stable screenshots by:
 * 1. Freezing requestAnimationFrame after 2 frames
 * 2. Disabling all CSS animations and transitions
 * 3. Mocking Date.now for consistent timestamps
 */
const ANIMATION_FREEZE_SCRIPT = `
  // Freeze RAF after 2 frames (allow initial render)
  let rafFrames = 0;
  const originalRAF = window.requestAnimationFrame;
  window.requestAnimationFrame = (callback) => {
    if (rafFrames++ < 2) {
      return originalRAF(callback);
    }
    return 0;
  };

  // Freeze canvas updates
  const originalGetContext = HTMLCanvasElement.prototype.getContext;
  let canvasFrames = 0;
  HTMLCanvasElement.prototype.getContext = function(...args) {
    const ctx = originalGetContext.apply(this, args);
    if (ctx && canvasFrames++ > 2) {
      // After 2 frames, make fillRect a no-op for stable canvas
      const originalFillRect = ctx.fillRect?.bind(ctx);
      if (originalFillRect) {
        ctx.fillRect = () => {};
      }
    }
    return ctx;
  };

  // Disable CSS animations
  const style = document.createElement('style');
  style.id = 'playwright-animation-freeze';
  style.textContent = \`
    *, *::before, *::after {
      animation-duration: 0s !important;
      animation-delay: 0s !important;
      transition-duration: 0s !important;
      transition-delay: 0s !important;
    }
  \`;
  document.head.appendChild(style);

  // Mock Date.now for consistent telemetry values
  const frozenTime = 1703174400000; // 2023-12-21T12:00:00Z
  Date.now = () => frozenTime;
`;

/**
 * Page object for dashboard interactions
 */
export class DashboardPage {
  constructor(private readonly page: Page) {}

  async goto() {
    await this.page.goto('/');
    await this.page.waitForLoadState('networkidle');
  }

  async switchTab(tabName: 'Control' | 'Shows' | 'Effects' | 'System') {
    // Tab buttons have text content, use getByText with exact match
    await this.page.getByText(tabName, { exact: true }).click();
    // Wait for tab content to render
    await this.page.waitForTimeout(100);
  }

  async waitForCanvasRender() {
    // Wait for LGPVisualizer canvas to have content
    await this.page.waitForFunction(() => {
      const canvas = document.querySelector('canvas');
      if (!canvas) return false;
      const ctx = canvas.getContext('2d');
      if (!ctx) return false;
      // Check if canvas has been drawn to
      const imageData = ctx.getImageData(0, 0, 1, 1);
      return imageData.data.some(v => v > 0);
    }, { timeout: 5000 }).catch(() => {
      // Canvas may not exist on all tabs, continue anyway
    });
  }

  async setSlider(label: string, value: number) {
    const slider = this.page.getByLabel(label);
    await slider.fill(value.toString());
  }

  async clickPreset(name: string) {
    await this.page.getByRole('button', { name }).click();
  }

  async toggleConnection() {
    await this.page.getByRole('button', { name: 'Connection Settings' }).click();
  }
}

/**
 * Extended test fixture with animation freezing and dashboard page object
 */
export const test = base.extend<{
  dashboard: DashboardPage;
}>({
  dashboard: async ({ page }, withDashboard) => {
    // Inject animation freeze script before page loads
    await page.addInitScript(ANIMATION_FREEZE_SCRIPT);

    const dashboard = new DashboardPage(page);
    await withDashboard(dashboard);
  },
});

export { expect };
