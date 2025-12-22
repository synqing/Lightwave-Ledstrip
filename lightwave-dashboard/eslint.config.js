import js from '@eslint/js'
import globals from 'globals'
import reactHooks from 'eslint-plugin-react-hooks'
import reactRefresh from 'eslint-plugin-react-refresh'
import tseslint from 'typescript-eslint'
import { defineConfig, globalIgnores } from 'eslint/config'
import noInlineResponsiveStylesRule from './eslint-rules/no-inline-responsive-styles.js'

// Custom plugin for inline responsive style detection
const customPlugin = {
  meta: {
    name: 'custom/no-inline-responsive-styles',
    version: '1.0.0',
  },
  rules: {
    'no-inline-responsive-styles': noInlineResponsiveStylesRule,
  },
}

export default defineConfig([
  globalIgnores(['dist']),
  {
    files: ['**/*.{ts,tsx}'],
    extends: [
      js.configs.recommended,
      tseslint.configs.recommended,
      reactHooks.configs.flat.recommended,
      reactRefresh.configs.vite,
    ],
    languageOptions: {
      ecmaVersion: 2020,
      globals: globals.browser,
    },
    plugins: {
      custom: customPlugin,
    },
    rules: {
      'custom/no-inline-responsive-styles': 'error',
    },
  },
  {
    // Playwright fixtures are not React components; disable React Hook rules for e2e.
    files: ['e2e/**/*.{ts,tsx}'],
    rules: {
      'react-hooks/rules-of-hooks': 'off',
      'react-hooks/exhaustive-deps': 'off',
      'react-hooks/set-state-in-effect': 'off',
    },
  },
  {
    // UI components and hooks legitimately set state inside effects.
    files: ['src/components/ui/Listbox.tsx', 'src/hooks/useLedStream.ts'],
    rules: {
      'react-hooks/set-state-in-effect': 'off',
    },
  },
  {
    // This file exports provider + hook + types; Fast Refresh rule is not applicable here.
    files: ['src/state/v2.tsx'],
    rules: {
      'react-refresh/only-export-components': 'off',
    },
  },
])
