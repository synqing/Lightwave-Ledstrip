import { defineConfig, devices } from '@playwright/test';

/**
 * Playwright configuration for LightwaveOS Dashboard visual regression testing
 * @see https://playwright.dev/docs/test-configuration
 */
export default defineConfig({
  testDir: './e2e',
  testMatch: '**/*.visual.spec.ts',

  /* Run tests in files in parallel */
  fullyParallel: true,

  /* Fail the build on CI if you accidentally left test.only in the source code */
  forbidOnly: !!process.env.CI,

  /* Retry on CI only */
  retries: process.env.CI ? 2 : 0,

  /* Opt out of parallel tests on CI */
  workers: process.env.CI ? 1 : undefined,

  /* Reporter to use */
  reporter: 'html',

  /* Shared settings for all the projects below */
  use: {
    /* Base URL to use in actions like `await page.goto('/')` */
    baseURL: 'http://localhost:5173',

    /* Collect trace when retrying the failed test */
    trace: 'on-first-retry',

    /* Take screenshot on failure */
    screenshot: 'only-on-failure',
  },

  /* Screenshot comparison settings */
  expect: {
    toHaveScreenshot: {
      /* Allow up to 100 pixels difference for anti-aliasing */
      maxDiffPixels: 100,
      /* Disable animations for stable snapshots */
      animations: 'disabled',
      /* Mask dynamic content like timestamps */
      stylePath: './e2e/fixtures/screenshot-mask.css',
    },
  },

  /* Snapshot directory */
  snapshotDir: './.visual-baselines',

  /* Configure projects for major viewports */
  projects: [
    {
      name: 'Mobile',
      use: {
        ...devices['iPhone 12'],
        viewport: { width: 375, height: 667 },
      },
    },
    {
      name: 'Tablet',
      use: {
        viewport: { width: 768, height: 1024 },
      },
    },
    {
      name: 'Desktop',
      use: {
        viewport: { width: 1920, height: 1080 },
      },
    },
  ],

  /* Run local dev server before starting the tests */
  webServer: {
    command: 'npm run dev',
    url: 'http://localhost:5173',
    reuseExistingServer: !process.env.CI,
    timeout: 120 * 1000,
  },
});
