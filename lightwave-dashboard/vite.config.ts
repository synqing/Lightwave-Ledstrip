/// <reference types="vitest" />
import { defineConfig } from 'vitest/config'
import react from '@vitejs/plugin-react'
import path from 'path'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],

  // Build configuration for ESP32 LittleFS embedding
  build: {
    // Output directly to firmware data directory
    outDir: path.resolve(__dirname, '../v2/data'),

    // Clear old files before building (keeps designer/ if present)
    emptyOutDir: false,

    // Optimize for embedded systems
    rollupOptions: {
      output: {
        // Use consistent file names for easier firmware updates
        entryFileNames: 'assets/app.js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: (assetInfo) => {
          // Put CSS in assets folder with predictable name
          if (assetInfo.name?.endsWith('.css')) {
            return 'assets/styles.css'
          }
          return 'assets/[name][extname]'
        },
      },
    },

    // Target modern browsers (ESP32 WebUI accessed from modern devices)
    target: 'es2020',

    // Use default minification (oxc for rolldown-vite)
    minify: true,

    // Don't generate source maps for production firmware
    sourcemap: false,
  },

  // Base URL for assets (relative for embedded use)
  base: './',

  test: {
    globals: true,
    environment: 'jsdom',
    setupFiles: './src/test/setup.ts',
    exclude: ['e2e/**', 'node_modules/**'],
  },
})
