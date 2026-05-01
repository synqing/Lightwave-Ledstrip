import { test, expect } from './fixtures/test-fixtures';

/**
 * Visual regression tests for LightwaveOS Dashboard
 *
 * These tests capture screenshots of each tab at multiple viewports
 * to detect unintended visual changes. Run with:
 *
 *   npm run test:e2e              # Run tests
 *   npm run test:e2e:update       # Update baselines
 */

test.describe('Dashboard Visual Regression', () => {

  test.beforeEach(async ({ dashboard }) => {
    await dashboard.goto();
    await dashboard.waitForCanvasRender();
  });

  test('Control tab renders correctly', async ({ page, dashboard }) => {
    await dashboard.switchTab('Control');
    await expect(page).toHaveScreenshot('control-tab.png', {
      fullPage: true,
    });
  });

  test('Shows tab renders correctly', async ({ page, dashboard }) => {
    await dashboard.switchTab('Shows');
    await expect(page).toHaveScreenshot('shows-tab.png', {
      fullPage: true,
    });
  });

  test('Effects tab renders correctly', async ({ page, dashboard }) => {
    await dashboard.switchTab('Effects');
    await expect(page).toHaveScreenshot('effects-tab.png', {
      fullPage: true,
    });
  });

  test('System tab renders correctly', async ({ page, dashboard }) => {
    await dashboard.switchTab('System');
    await expect(page).toHaveScreenshot('system-tab.png', {
      fullPage: true,
    });
  });

});

test.describe('Component Visual Tests', () => {

  test.beforeEach(async ({ dashboard }) => {
    await dashboard.goto();
  });

  test('LED visualization renders', async ({ page, dashboard }) => {
    await dashboard.switchTab('Control');

    // Wait for canvas to be visible
    const canvas = page.locator('canvas').first();
    await expect(canvas).toBeVisible();

    await expect(canvas).toHaveScreenshot('led-visualization.png');
  });

  test('Slider controls render correctly', async ({ page, dashboard }) => {
    await dashboard.switchTab('Control');

    // Screenshot the slider group
    const sliderSection = page.locator('[class*="SliderControl"]').first();
    if (await sliderSection.isVisible()) {
      await expect(sliderSection).toHaveScreenshot('slider-control.png');
    }
  });

  test('Timeline tracks render correctly', async ({ page, dashboard }) => {
    await dashboard.switchTab('Shows');

    // Screenshot the timeline area
    const timeline = page.locator('[data-track]').first();
    if (await timeline.isVisible()) {
      await expect(timeline).toHaveScreenshot('timeline-track.png');
    }
  });

  test('Effects grid renders correctly', async ({ page, dashboard }) => {
    await dashboard.switchTab('Effects');

    // Screenshot the effects grid
    const grid = page.locator('.grid').first();
    await expect(grid).toHaveScreenshot('effects-grid.png');
  });

});

test.describe('Interactive State Tests', () => {

  test.beforeEach(async ({ dashboard }) => {
    await dashboard.goto();
  });

  test('Disconnected state renders correctly', async ({ page, dashboard }) => {
    await dashboard.toggleConnection();

    // Wait for disconnected state to render
    await page.waitForTimeout(100);

    await expect(page).toHaveScreenshot('disconnected-state.png', {
      fullPage: true,
    });
  });

  test('Effects filter empty state', async ({ page, dashboard }) => {
    await dashboard.switchTab('Effects');

    // Type a search that matches nothing
    const searchInput = page.getByPlaceholder('Search effects');
    await searchInput.fill('xyznonexistent');

    await expect(page).toHaveScreenshot('effects-empty-state.png', {
      fullPage: true,
    });
  });

  test('Effects category filter', async ({ page, dashboard }) => {
    await dashboard.switchTab('Effects');

    // Click a category filter
    const quantumFilter = page.getByText('Quantum', { exact: true });
    await quantumFilter.click();

    await expect(page).toHaveScreenshot('effects-quantum-filter.png', {
      fullPage: true,
    });
  });

});
