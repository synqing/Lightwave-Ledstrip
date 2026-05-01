# Lightwave Dashboard

**React 19 + TypeScript + Vite + Tailwind CSS** external web dashboard for LightwaveOS ESP32-S3 LED controller.

## Quick Start

```bash
npm install
npm run dev          # Development server with HMR
npm run build        # Production build
npm run preview      # Preview production build
npm run test         # Unit tests
npm run test:e2e     # E2E tests (Playwright)
```

## Developer Onboarding

### For Human Developers

1. **Read the Architecture:** Start with [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) to understand the system
2. **Learn the Design System:** Review [`docs/UI_STANDARDS.md`](docs/UI_STANDARDS.md) for Glass V4 patterns
3. **Use Component Gallery:** Check [`src/components/ui/README.md`](src/components/ui/README.md) for copy-paste patterns
4. **Follow Center-Origin Principle:** All effects must originate from LEDs 79/80 (see Architecture doc)

### For AI Agents

**CRITICAL:** Before making any UI changes, you MUST:

1. **Read Architecture First:** [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
   - Understand React 19 + Vite + Tailwind stack
   - Know the V2Provider state flow
   - Understand Center-Origin Principle (LEDs 79/80)

2. **Follow UI Standards:** [`docs/UI_STANDARDS.md`](docs/UI_STANDARDS.md)
   - Use Glass V4 design patterns
   - Follow Tailwind class ordering convention
   - Use design tokens from `tailwind.config.js`
   - Never hardcode colors or spacing

3. **Copy from Component Gallery:** [`src/components/ui/README.md`](src/components/ui/README.md)
   - Use existing components when possible
   - Copy patterns, don't invent new ones
   - Maintain visual consistency

4. **Common Mistakes to Avoid:**
   - ❌ Don't create effects starting from LED 0 or 159 (must be 79/80)
   - ❌ Don't hardcode colors (use `text-primary`, `bg-surface`, etc.)
   - ❌ Don't skip responsive breakpoints (use `sm:`, `lg:`, `xl:`)
   - ❌ Don't ignore the Glass V4 aesthetic (backdrop blur, layered shadows)
   - ❌ Don't create new UI patterns without checking existing ones first

5. **Testing Checklist:**
   - [ ] Responsive at all breakpoints
   - [ ] Follows Glass V4 patterns
   - [ ] Uses design tokens
   - [ ] Touch targets meet 44px minimum
   - [ ] Hover/focus states implemented
   - [ ] Visual regression tests pass

## Project Structure

```
lightwave-dashboard/
├── src/
│   ├── components/        # React components
│   │   ├── ui/           # Reusable UI components
│   │   └── tabs/         # Tab components
│   ├── state/            # State management (V2Provider)
│   ├── services/         # API clients (V2Client, V2WsClient)
│   ├── hooks/            # Custom React hooks
│   └── types/            # TypeScript types
├── docs/
│   ├── ARCHITECTURE.md   # System architecture
│   └── UI_STANDARDS.md   # Design system documentation
└── package.json
```

## Technology Stack

- **React 19.2.0** - Latest React with concurrent features
- **TypeScript 5.9.3** - Full type safety
- **Vite 7.2.5** (rolldown-vite) - Fast build tool
- **Tailwind CSS 3.4.17** - Utility-first CSS
- **lucide-react** - Icon library
- **@dnd-kit** - Drag-and-drop
- **Playwright** - E2E testing
- **Vitest** - Unit testing

## Key Features

- **Real-time LED Visualization** - Live feed from ESP32
- **Effects Browser** - Browse and select center-radiating effects
- **Timeline Editor** - Drag-and-drop show creation
- **System Diagnostics** - Device status and metrics
- **Narrative Engine UI** - Control tension and phases

## Design System

The dashboard uses a **Glass V4** design system:
- Frosted glass aesthetics with backdrop blur
- Layered depth via subtle shadows
- Radial gradients for ambient lighting
- High contrast text on dark backgrounds
- Smooth animations for transitions

See [`docs/UI_STANDARDS.md`](docs/UI_STANDARDS.md) for complete design system documentation.

## API Integration

The dashboard connects to the ESP32 via:
- **REST API v2** - `/api/v1/*` endpoints
- **WebSocket** - Real-time updates and LED streaming
- **Type-safe clients** - Full TypeScript support

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for API details.

## Development

This template provides a minimal setup to get React working in Vite with HMR and some ESLint rules.

Currently, two official plugins are available:

- [@vitejs/plugin-react](https://github.com/vitejs/vite-plugin-react/blob/main/packages/plugin-react) uses [Babel](https://babeljs.io/) (or [oxc](https://oxc.rs) when used in [rolldown-vite](https://vite.dev/guide/rolldown)) for Fast Refresh
- [@vitejs/plugin-react-swc](https://github.com/vitejs/vite-plugin-react/blob/main/packages/plugin-react-swc) uses [SWC](https://swc.rs/) for Fast Refresh

## React Compiler

The React Compiler is not enabled on this template because of its impact on dev & build performances. To add it, see [this documentation](https://react.dev/learn/react-compiler/installation).

## Expanding the ESLint configuration

If you are developing a production application, we recommend updating the configuration to enable type-aware lint rules:

```js
export default defineConfig([
  globalIgnores(['dist']),
  {
    files: ['**/*.{ts,tsx}'],
    extends: [
      // Other configs...

      // Remove tseslint.configs.recommended and replace with this
      tseslint.configs.recommendedTypeChecked,
      // Alternatively, use this for stricter rules
      tseslint.configs.strictTypeChecked,
      // Optionally, add this for stylistic rules
      tseslint.configs.stylisticTypeChecked,

      // Other configs...
    ],
    languageOptions: {
      parserOptions: {
        project: ['./tsconfig.node.json', './tsconfig.app.json'],
        tsconfigRootDir: import.meta.dirname,
      },
      // other options...
    },
  },
])
```

You can also install [eslint-plugin-react-x](https://github.com/Rel1cx/eslint-react/tree/main/packages/plugins/eslint-plugin-react-x) and [eslint-plugin-react-dom](https://github.com/Rel1cx/eslint-react/tree/main/packages/plugins/eslint-plugin-react-dom) for React-specific lint rules:

```js
// eslint.config.js
import reactX from 'eslint-plugin-react-x'
import reactDom from 'eslint-plugin-react-dom'

export default defineConfig([
  globalIgnores(['dist']),
  {
    files: ['**/*.{ts,tsx}'],
    extends: [
      // Other configs...
      // Enable lint rules for React
      reactX.configs['recommended-typescript'],
      // Enable lint rules for React DOM
      reactDom.configs.recommended,
    ],
    languageOptions: {
      parserOptions: {
        project: ['./tsconfig.node.json', './tsconfig.app.json'],
        tsconfigRootDir: import.meta.dirname,
      },
      // other options...
    },
  },
])
```
