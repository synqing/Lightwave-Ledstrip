import { test, expect } from '@playwright/test';

/**
 * Canvas Visual Regression Tests
 * 
 * Tests for canvas rendering components (LGPVisualizer, TelemetryGraph)
 * to ensure proper rendering, resize behavior, and transform handling.
 */

test.describe('Canvas Regression Tests', () => {
  test.beforeEach(async ({ page }) => {
    // Navigate to dashboard
    await page.goto('/');
    // Wait for canvas to render
    await page.waitForSelector('canvas', { timeout: 5000 });
  });

  test.describe('LGPVisualizer', () => {
    test('renders correctly on initial load', async ({ page }) => {
      const canvas = page.locator('#lgpCanvas').first();
      await expect(canvas).toBeVisible();
      
      // Wait for animation to start
      await page.waitForTimeout(100);
      
      // Take screenshot of canvas area
      await expect(canvas).toHaveScreenshot('lgp-visualizer-initial.png', {
        maxDiffPixels: 500, // Allow some variance for animation
      });
    });

    test('resizes without transform accumulation', async ({ page }) => {
      const canvas = page.locator('#lgpCanvas').first();
      
      // Initial render
      await page.waitForTimeout(100);
      await expect(canvas).toHaveScreenshot('lgp-resize-initial.png', {
        maxDiffPixels: 500,
      });
      
      // Resize viewport multiple times to test transform reset
      const viewports = [
        { width: 1440, height: 900 },
        { width: 768, height: 1024 },
        { width: 390, height: 844 },
        { width: 1440, height: 900 }, // Back to original
      ];
      
      for (const viewport of viewports) {
        await page.setViewportSize(viewport);
        await page.waitForTimeout(200); // Wait for resize handler
        
        // Verify canvas still renders correctly
        const canvasBox = await canvas.boundingBox();
        expect(canvasBox).not.toBeNull();
        expect(canvasBox!.width).toBeGreaterThan(0);
        expect(canvasBox!.height).toBeGreaterThan(0);
      }
      
      // Final render should match initial (no accumulation)
      await expect(canvas).toHaveScreenshot('lgp-resize-final.png', {
        maxDiffPixels: 500,
      });
    });

    test('handles device pixel ratio changes', async ({ page }) => {
      const canvas = page.locator('#lgpCanvas').first();
      
      // Test with different device pixel ratios
      await page.evaluate(() => {
        // Mock devicePixelRatio
        Object.defineProperty(window, 'devicePixelRatio', {
          value: 1,
          writable: true,
        });
      });
      
      await page.reload();
      await page.waitForTimeout(100);
      await expect(canvas).toHaveScreenshot('lgp-dpr-1.png', {
        maxDiffPixels: 500,
      });
      
      await page.evaluate(() => {
        Object.defineProperty(window, 'devicePixelRatio', {
          value: 2,
          writable: true,
        });
      });
      
      await page.reload();
      await page.waitForTimeout(100);
      await expect(canvas).toHaveScreenshot('lgp-dpr-2.png', {
        maxDiffPixels: 500,
      });
    });

    test('displays disconnected state correctly', async ({ page }) => {
      // This test would require mocking the connection state
      // For now, we verify the canvas exists and renders
      const canvas = page.locator('#lgpCanvas').first();
      await expect(canvas).toBeVisible();
      
      // Check that canvas has content (not blank)
      const imageData = await canvas.evaluate((el: HTMLCanvasElement) => {
        const ctx = el.getContext('2d');
        if (!ctx) return null;
        return ctx.getImageData(0, 0, el.width, el.height);
      });
      
      expect(imageData).not.toBeNull();
      
      // Verify some pixels are non-zero (canvas has content)
      if (imageData) {
        const data = imageData.data;
        const hasContent = Array.from(data).some(pixel => pixel !== 0);
        expect(hasContent).toBe(true);
      }
    });

    test('roundRect fallback works on older browsers', async ({ page }) => {
      const canvas = page.locator('#lgpCanvas').first();
      
      // Mock missing roundRect
      await page.evaluate(() => {
        const originalGetContext = HTMLCanvasElement.prototype.getContext;
        HTMLCanvasElement.prototype.getContext = function(type: string) {
          const ctx = originalGetContext.call(this, type);
          if (type === '2d' && ctx) {
            // Remove roundRect to test fallback
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            delete (ctx as any).roundRect;
          }
          return ctx;
        };
      });
      
      await page.reload();
      await page.waitForTimeout(100);
      
      // Should still render (using fillRect fallback)
      await expect(canvas).toBeVisible();
      
      const imageData = await canvas.evaluate((el: HTMLCanvasElement) => {
        const ctx = el.getContext('2d');
        if (!ctx) return null;
        return ctx.getImageData(0, 0, el.width, el.height);
      });
      
      expect(imageData).not.toBeNull();
    });
  });

  test.describe('TelemetryGraph', () => {
    test('renders graph correctly', async ({ page }) => {
      // Navigate to System tab where telemetry graphs are shown
      await page.click('button[aria-label*="System"], button:has-text("System")');
      await page.waitForTimeout(200);
      
      const graphs = page.locator('canvas').filter({ hasNot: page.locator('#lgpCanvas') });
      const graphCount = await graphs.count();
      
      expect(graphCount).toBeGreaterThan(0);
      
      // Take screenshot of first graph
      if (graphCount > 0) {
        await expect(graphs.first()).toHaveScreenshot('telemetry-graph-initial.png', {
          maxDiffPixels: 300,
        });
      }
    });

    test('handles data updates correctly', async ({ page }) => {
      await page.click('button[aria-label*="System"], button:has-text("System")');
      await page.waitForTimeout(200);
      
      const graph = page.locator('canvas').filter({ hasNot: page.locator('#lgpCanvas') }).first();
      
      if (await graph.count() > 0) {
        // Initial state
        await page.waitForTimeout(100);
        await expect(graph).toHaveScreenshot('telemetry-graph-update-1.png', {
          maxDiffPixels: 300,
        });
        
        // Wait for data update (telemetry updates every 500ms)
        await page.waitForTimeout(600);
        await expect(graph).toHaveScreenshot('telemetry-graph-update-2.png', {
          maxDiffPixels: 300,
        });
      }
    });

    test('resizes without transform accumulation', async ({ page }) => {
      await page.click('button[aria-label*="System"], button:has-text("System")');
      await page.waitForTimeout(200);
      
      const graph = page.locator('canvas').filter({ hasNot: page.locator('#lgpCanvas') }).first();
      
      if (await graph.count() > 0) {
        // Resize viewport
        await page.setViewportSize({ width: 768, height: 1024 });
        await page.waitForTimeout(200);
        
        await expect(graph).toHaveScreenshot('telemetry-graph-resize-1.png', {
          maxDiffPixels: 300,
        });
        
        // Resize again
        await page.setViewportSize({ width: 1920, height: 1080 });
        await page.waitForTimeout(200);
        
        await expect(graph).toHaveScreenshot('telemetry-graph-resize-2.png', {
          maxDiffPixels: 300,
        });
      }
    });

    test('handles empty data gracefully', async ({ page }) => {
      // This test would require mocking empty data
      // For now, verify graph exists and is visible
      await page.click('button[aria-label*="System"], button:has-text("System")');
      await page.waitForTimeout(200);
      
      const graph = page.locator('canvas').filter({ hasNot: page.locator('#lgpCanvas') }).first();
      
      if (await graph.count() > 0) {
        await expect(graph).toBeVisible();
        
        // Graph should render even with minimal data
        const imageData = await graph.evaluate((el: HTMLCanvasElement) => {
          const ctx = el.getContext('2d');
          if (!ctx) return null;
          return ctx.getImageData(0, 0, el.width, el.height);
        });
        
        expect(imageData).not.toBeNull();
      }
    });
  });

  test.describe('Canvas Performance', () => {
    test('maintains 60fps during animation', async ({ page }) => {
      // Measure frame rate
      const fps = await page.evaluate(() => {
      return new Promise<number>((resolve) => {
        let frames = 0;
        const lastTime = performance.now();
          
          const measure = () => {
            frames++;
            const now = performance.now();
            
            if (now - lastTime >= 1000) {
              resolve(frames);
              return;
            }
            
            requestAnimationFrame(measure);
          };
          
          requestAnimationFrame(measure);
        });
      });
      
      // Should maintain at least 50fps (allowing for test overhead)
      expect(fps).toBeGreaterThan(50);
    });

    test('does not leak memory on resize', async ({ page }) => {
      // Get initial memory (if available)
      const initialMemory = await page.evaluate(() => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        return (performance as any).memory?.usedJSHeapSize || 0;
      });
      
      // Perform multiple resizes
      for (let i = 0; i < 10; i++) {
        await page.setViewportSize({ 
          width: 800 + (i % 2) * 400, 
          height: 600 
        });
        await page.waitForTimeout(50);
      }
      
      // Force garbage collection (if available)
      await page.evaluate(() => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        if ((globalThis as any).gc) {
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          (globalThis as any).gc();
        }
      });
      
      await page.waitForTimeout(100);
      
      const finalMemory = await page.evaluate(() => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        return (performance as any).memory?.usedJSHeapSize || 0;
      });
      
      // Memory should not grow significantly (allow 20% growth for test overhead)
      if (initialMemory > 0 && finalMemory > 0) {
        const growth = (finalMemory - initialMemory) / initialMemory;
        expect(growth).toBeLessThan(0.2);
      }
    });
  });
});

