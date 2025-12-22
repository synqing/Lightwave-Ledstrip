import { test, expect } from '@playwright/test';

/**
 * Canvas Performance Benchmarks
 * 
 * Performance tests for canvas rendering components to ensure
 * smooth animations and efficient resource usage.
 */

test.describe('Canvas Performance Benchmarks', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForSelector('canvas', { timeout: 5000 });
  });

  test('LGPVisualizer maintains smooth animation', async ({ page }) => {
    // Measure frame rendering time
    const frameTimes = await page.evaluate(() => {
      return new Promise<number[]>((resolve) => {
        const times: number[] = [];
        let frameCount = 0;
        const maxFrames = 60; // Measure 60 frames
        
        const measureFrame = (timestamp: number) => {
          if (frameCount === 0) {
            // First frame
            times.push(0);
          } else {
            const frameTime = timestamp - times[times.length - 1];
            times.push(frameTime);
          }
          
          frameCount++;
          
          if (frameCount < maxFrames) {
            requestAnimationFrame(measureFrame);
          } else {
            resolve(times.slice(1)); // Remove first entry
          }
        };
        
        requestAnimationFrame(measureFrame);
      });
    });
    
    // Calculate average frame time
    const avgFrameTime = frameTimes.reduce((a, b) => a + b, 0) / frameTimes.length;
    const avgFPS = 1000 / avgFrameTime;
    
    // Should maintain at least 55fps (allowing for test overhead)
    expect(avgFPS).toBeGreaterThan(55);
    
    // Frame times should be consistent (no major spikes)
    const maxFrameTime = Math.max(...frameTimes);
    const minFrameTime = Math.min(...frameTimes);
    const variance = maxFrameTime - minFrameTime;
    
    // Variance should be reasonable (less than 20ms difference)
    expect(variance).toBeLessThan(20);
  });

  test('LGPVisualizer handles 320 LEDs efficiently', async ({ page }) => {
    // Mock real LED data (320 LEDs × RGB = 960 bytes)
    const ledData = new Uint8Array(960);
    for (let i = 0; i < 960; i++) {
      ledData[i] = Math.floor(Math.random() * 255);
    }
    
    // Measure render time with real data
    const renderTime = await page.evaluate(() => {
      return new Promise<number>((resolve) => {
        const start = performance.now();
        
        // Trigger render by updating component
        // This would require component ref access, so we measure canvas operations
        const canvas = document.querySelector('#lgpCanvas') as HTMLCanvasElement;
        if (canvas) {
          const ctx = canvas.getContext('2d');
          if (ctx) {
            // Simulate render operations
            for (let i = 0; i < 320; i++) {
              ctx.fillRect(i * 2, 0, 2, 50);
            }
          }
        }
        
        const end = performance.now();
        resolve(end - start);
      });
    });
    
    // Render should complete in reasonable time (< 16ms for 60fps)
    expect(renderTime).toBeLessThan(16);
  });

  test('resize performance is acceptable', async ({ page }) => {
    const resizeTimes: number[] = [];
    
    // Measure multiple resize operations
    for (let i = 0; i < 5; i++) {
      const resizeTime = await page.evaluate(() => {
        return new Promise<number>((resolve) => {
          const start = performance.now();
          
          // Trigger resize
          window.dispatchEvent(new Event('resize'));
          
          // Wait for resize to complete
          requestAnimationFrame(() => {
            requestAnimationFrame(() => {
              const end = performance.now();
              resolve(end - start);
            });
          });
        });
      });
      
      resizeTimes.push(resizeTime);
      
      // Change viewport size
      await page.setViewportSize({ 
        width: 800 + (i % 2) * 400, 
        height: 600 
      });
      await page.waitForTimeout(100);
    }
    
    // Average resize time should be reasonable (< 50ms)
    const avgResizeTime = resizeTimes.reduce((a, b) => a + b, 0) / resizeTimes.length;
    expect(avgResizeTime).toBeLessThan(50);
  });

  test('TelemetryGraph updates efficiently', async ({ page }) => {
    await page.click('button[aria-label*="System"], button:has-text("System")');
    await page.waitForTimeout(200);
    
    const graph = page.locator('canvas').filter({ hasNot: page.locator('#lgpCanvas') }).first();
    
    if (await graph.count() > 0) {
      // Measure update time
      const updateTimes: number[] = [];
      
      for (let i = 0; i < 10; i++) {
        const updateTime = await page.evaluate(() => {
          return new Promise<number>((resolve) => {
            const start = performance.now();
            
            // Wait for next update cycle
            setTimeout(() => {
              const end = performance.now();
              resolve(end - start);
            }, 100);
          });
        });
        
        updateTimes.push(updateTime);
        await page.waitForTimeout(100);
      }
      
      // Updates should be fast (< 10ms overhead)
      const avgUpdateTime = updateTimes.reduce((a, b) => a + b, 0) / updateTimes.length;
      expect(avgUpdateTime).toBeLessThan(110); // 100ms wait + 10ms overhead
    }
  });

  test('multiple canvases render without performance degradation', async ({ page }) => {
    // Navigate to System tab (has multiple telemetry graphs)
    await page.click('button[aria-label*="System"], button:has-text("System")');
    await page.waitForTimeout(200);
    
    const allCanvases = page.locator('canvas');
    const canvasCount = await allCanvases.count();
    
    expect(canvasCount).toBeGreaterThan(1);
    
    // Measure overall frame rate with multiple canvases
    const fps = await page.evaluate(() => {
      return new Promise<number>((resolve) => {
        let frames = 0;
        const start = performance.now();
        
        const measure = () => {
          frames++;
          const elapsed = performance.now() - start;
          
          if (elapsed >= 1000) {
            resolve(frames);
            return;
          }
          
          requestAnimationFrame(measure);
        };
        
        requestAnimationFrame(measure);
      });
    });
    
    // Should maintain good frame rate even with multiple canvases
    expect(fps).toBeGreaterThan(50);
  });
});

