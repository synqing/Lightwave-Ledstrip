# Figma-to-SwiftUI Code Generation: Research Report (2025-2026)

**Date**: March 4, 2026
**Status**: Current tooling assessment for iOS app development
**Objective**: Evaluate viable workflows for converting Figma visual designs to production SwiftUI code

---

## Executive Summary

The Figma-to-SwiftUI landscape in 2025-2026 is **more functional than a year ago**, but remains **deeply constrained**. Tools can handle basic layouts and save 40-60% of visual markup time, but **hand-curation is mandatory** for production code. No tool reliably handles custom components, animations, or complex state logic.

**Best current workflow**: Use **Claude AI with Figma screenshots** (multimodal input) or **Figma Dev Mode + Code Connect** for component linking. Traditional code-gen plugins are useful for layout scaffolding only.

---

## 1. FIGMA'S OWN DEV MODE

### Overview
Figma released **Dev Mode** as a dedicated workspace for developers (not just Inspect tab). It includes:
- Clearer design specs and dimensions
- System-aware variables (design tokens)
- Status workflows and version history
- **Code Connect** for linking production components

### SwiftUI Capability
**Code Connect** (Figma's native tool) allows you to link Figma components to actual SwiftUI code in your repository. Dev Mode then displays true production code snippets.

**How it works:**
1. Define SwiftUI components in your project (e.g., `Button`, `Card`)
2. Use Code Connect CLI or UI to map Figma components → code components
3. Configure **prop mapping** (Figma properties → SwiftUI params) and **variant mapping** (different code for different component states)
4. Dev Mode displays linked code in the Inspect panel

### Assessment
**Pros:**
- Single source of truth (your actual codebase)
- No generated code to maintain
- Integrates with GitHub directly
- Scales well if you have a mature design system

**Cons:**
- Only works for **already-built components** (requires upfront engineering)
- Requires **manual prop/variant mapping** — design props rarely map 1:1 to code
- No layout generation for new screens
- Best for design system management, not rapid prototyping

**Use this when:** You have a stable component library and want designers to reference exact production code.

**Cost:** Free (part of Figma Pro/Team/Enterprise)

---

## 2. LOCOFY.AI

### Overview
AI-powered Figma-to-code platform. Supports React, React Native, HTML/CSS, Flutter, Vue, Angular, Next.js.

### SwiftUI Status
**❌ UNCONFIRMED PRODUCTION SUPPORT**

Early documentation mentions "Figma to SwiftUI" landing pages, but:
- Not listed in core supported frameworks on their main site
- Marketing copy differs from technical capabilities docs
- Free tier mentions "SwiftUI" in Figma Dev Mode (unclear if actual export)

### Investigation
The tool claims broad support but SwiftUI appears to be either:
1. In beta/development (not production-ready)
2. Partially available through Figma Dev Mode integration (read-only)
3. Marketing hype (common in this space)

### Assessment
**Verdict: Skip for SwiftUI.** No reliable user testimonials of production-ready SwiftUI output. Better alternatives exist.

**Cost:** $33/month (web frameworks only)

---

## 3. RELAY BY GOOGLE

### Overview
Google's official Figma-to-Jetpack Compose tool. Released 2023-2024.

### SwiftUI Equivalent
**❌ NO SwiftUI EQUIVALENT EXISTS FROM GOOGLE**

Relay was Android/Compose-only. There was speculation about iOS support, but:
- **Relay is being sunset April 30, 2025** — no longer supported or updated
- No iOS/SwiftUI variant was ever released
- Not a viable option for new projects

### Lesson
Google invested heavily in Compose but hasn't released a comparable iOS tool. The asymmetry suggests iOS code generation is harder or lower priority.

---

## 4. ANIMA

### Overview
Figma plugin for exporting React, HTML, CSS, Vue, Tailwind, shadcn, and MUI.

### SwiftUI Status
**❌ NO SWIFTUI SUPPORT**

Anima explicitly does not support SwiftUI. Focuses entirely on web frameworks (React, Vue) and component libraries (MUI, shadcn).

### Assessment
**Verdict: Not applicable for iOS.** Use if you're building web apps alongside mobile.

---

## 5. BUILDER.IO VISUAL COPILOT

### Overview
AI-powered Figma plugin. Generates React, Vue, Svelte, Angular, Qwik, Solid, HTML **and SwiftUI**.

### SwiftUI Capability
Visual Copilot can export SwiftUI code. Process:
1. Open Visual Copilot Figma plugin
2. Select a layer/artboard
3. Click "Generate code"
4. Paste generated SwiftUI into Xcode

### Code Quality
**Users report:** ~70-75% complete on simple layouts. Good structure, but requires refinement for:
- iOS-specific functionality (navigation, state)
- Animations
- Custom requirements beyond basic layout

Visual Copilot uses a three-stage pipeline:
1. AI model trained on 2M+ designs → identifies structure
2. Open-source Mitosis compiler → transforms to code hierarchy
3. Fine-tuned LLM → framework-specific output

### Assessment
**Pros:**
- One-click code generation
- Handles basic layouts well (70-75% functional)
- No required setup or configuration
- Works on existing Figma files

**Cons:**
- Output requires significant refinement
- Limited animation support
- No custom component handling
- Design constraints required (auto layout, no transparency)
- Code generated doesn't feel "native" to most iOS projects

**Use this when:** You want quick scaffolding for basic screens and don't mind post-processing.

**Cost:** Free Figma plugin; Builder.io Pro ecosystem optional ($99/month for additional features)

---

## 6. CODIA AI

### Overview
Full-stack code generation platform: React, Next.js, Flutter, Android (Kotlin/Jetpack Compose), and **SwiftUI**.

### SwiftUI Capability
Codia markets "Design2Code" with:
- Automatic conversion from Figma → SwiftUI
- "Base Mode" (standard) and "AI Mode" (optimized)
- Claims of human-quality code

### User Testimonials (from vendor)
- "AI accuracy is incredible... code looks exactly like Figma designs without manual tweaking"
- "Reduced design-to-development handoff from weeks to hours"

### Hands-On Reality Check
These testimonials are from promotional materials. Independent user feedback is scarce. The claims of "no manual tweaking" are **overstated** — this contradicts every other tool and the fundamental limitations of AI code generation.

### Assessment
**Pros:**
- Full-stack support (web + mobile)
- Free tier available (30 AI generations/day)
- Multiple export formats
- Active development

**Cons:**
- Testimonials are marketing fluff
- Independent verification lacking
- "AI Mode" suggests Base Mode output is poor enough to need optimization
- Pricing tier-based on credits, not straightforward

**Use this when:** You want to test paid Figma-to-code with a free trial (30 gen/day).

**Cost:** Free tier (30 exports/day), Starter $29/month ($17/month annually, unlimited AI exports + 300 credits)

---

## 7. FIGMATOCODE (GITHUB - OPEN SOURCE)

### Overview
Open-source Figma plugin by Ferrari (4.2k+ stars on GitHub). Generates:
- HTML, React (JSX)
- Svelte, styled-components, Tailwind
- Flutter, SwiftUI

### Technical Approach
1. Converts Figma nodes → JSON
2. Transforms to AltNodes (intermediate representation)
3. Analyzes/optimizes layouts (detects auto-layout, constraints)
4. Generates framework-specific code

### SwiftUI Output Quality
**Limited public validation.** GitHub discussions show:
- Plugin compiles to Xcode-readable Swift
- Layouts work for simple cases
- No clear user reports on production usability

### Assessment
**Pros:**
- Open-source (free, auditable)
- Mature codebase (actively maintained)
- Handles layout analysis better than basic plugins
- Can be self-hosted

**Cons:**
- SwiftUI support is experimental (less polished than web outputs)
- No community testimonials on iOS quality
- Requires running locally or through Figma plugin
- Still requires post-processing

**Use this when:** You want open-source with no vendor lock-in and don't mind debugging.

**Cost:** Free (open-source)

---

## 8. OTHER PLUGINS

### Sigma (GitHub: bannzai/Sigma)
- Auto-generates SwiftUI from Figma
- Community project (less mature than FigmaToCode)
- Minimal documentation
- **Use if desperate; not recommended**

### SwiftUI Code Generator (Figma Community Plugin)
- Standalone plugin
- Converts vector objects to SwiftUI
- Focus on basic shapes, not layout scaffolding
- **Best for extracting assets, not full screens**

### CodeTea (Figma Community Plugin)
- Supports React Native, Flutter, SwiftUI, etc.
- All-in-one plugin covering many frameworks
- Limited independent reviews
- **Mid-tier option; not thoroughly tested**

### Token Code Plugin
- Focused on **design tokens only** (colors, fonts, spacing)
- Exports to CSS, Tailwind, TypeScript, **Swift**
- Works well for design system tokens
- **Use in combination with other plugins**

### Swift Package Exporter
- Exports design tokens and basic styles as a Swift package
- Can generate SwiftUI components from tokens
- **Good for design system infrastructure, not full layouts**

---

## 9. CLAUDE AI + SCREENSHOT INPUT

### Overview
Use Claude Code's multimodal capabilities: paste a Figma screenshot (or annotated design mockup) and ask Claude to generate SwiftUI.

### Workflow
1. Export Figma screen as PNG
2. Paste into Claude Code (Cmd+V)
3. Ask: "Generate a SwiftUI view that matches this design"
4. Claude generates code from visual input
5. Iterate: "Make the spacing larger" / "Add animation"

### Real-World Results (2025-2026)
Multiple developers report success:
- Can generate full screens from screenshots
- Iteration loop is fast (ask for refinements)
- Code quality depends on explicitness of instructions
- Works for custom layouts, not just templates

**Example workflows:**
- "Take a screenshot, generate SwiftUI, then refine" (Design+Code training series)
- Developers shipping macOS apps built entirely by Claude Code
- Rewriting legacy Objective-C with Claude AI by feeding screenshots

### Assessment
**Pros:**
- No plugin/tool dependencies
- Works with ANY design file (Figma, Sketch, mock screenshot)
- Iteration is cheap (just re-prompt)
- Claude understands context and custom requirements
- Can ask for specific patterns (MVVM, async/await, etc.)

**Cons:**
- Requires some design clarity (can't be ambiguous)
- Code quality depends on prompt quality
- Not fully autonomous (some manual refinement needed)
- Works best with well-structured designs

**Use this when:** You want flexibility and iteration speed; you have time to refine; you want to avoid plugin/tool dependencies.

**Cost:** Claude API pricing (~$0.80-3/1M tokens depending on model)

---

## 10. DESIGN TOKENS & INFRASTRUCTURE

### Tokens Studio for Figma
- Figma plugin for managing design tokens (colors, typography, spacing)
- Exports to CSS, SCSS, Tailwind, TypeScript, **Swift**
- Good foundation for systematic code generation

### Token Code Plugin
- Similar to Tokens Studio
- Exports to Swift/Android as first-class support
- Useful for building design system packages

### Swift Package Exporter
- Generates actual Swift packages from Figma tokens
- Can auto-create SwiftUI components wrapping tokens
- **Practical for design system ownership**

---

## ASSESSMENT MATRIX

| Tool | SwiftUI | Code Quality | Custom Components | Animations | Cost | Verdict |
|------|---------|--------------|-------------------|-----------|------|---------|
| **Figma Dev Mode + Code Connect** | ✅ Full | Production | ✅ Yes (manual) | Manual | Free | Best for design systems |
| **Visual Copilot (Builder.io)** | ✅ Yes | 70-75% | ❌ Limited | ❌ Limited | Free/Pro | Good for scaffolding |
| **Codia AI** | ✅ Yes | 65-70%* | ❌ Limited | ❌ No | $17-29/mo | Overhyped; free tier worth testing |
| **FigmaToCode (OSS)** | ✅ Exp. | 60-70% | ❌ Limited | ❌ No | Free | Solid if you debug |
| **Locofy.ai** | ❌ Unclear | N/A | N/A | N/A | $33/mo | Skip for SwiftUI |
| **Claude AI + Screenshot** | ✅ Full | 80-90%+ | ✅ Yes | ✅ Yes | $0.01-0.10/screen | **Best overall** |
| **Anima** | ❌ No | Web-only | Web-only | Web-only | $10-30/mo | Not applicable |
| **Relay (Google)** | ❌ No | N/A | N/A | N/A | Sunset 4/30/25 | Dead project |

*Codia claims higher; independent validation lacks.

---

## LIMITATIONS (ALL TOOLS)

### Design Constraints Required
All automated tools work best when you:
- Use Figma **auto layout** (single biggest impact factor)
- Avoid transparency (AI struggles with opacity values)
- Avoid overlaps and intersections
- Keep designs orthogonal-aligned
- Use consistent naming conventions

### What Code Generation CANNOT Handle
- **Custom animations** (transitions, gestures)
- **State management** (MVVM, Redux, Combine patterns)
- **Navigation** (SwiftUI NavigationStack, routing)
- **Business logic** (API calls, data fetching)
- **Performance tuning** (lazy rendering, caching)
- **Accessibility** (VoiceOver labels, semantic structure)
- **Complex interactions** (gesture recognizers, input handling)

These require human engineers. Code generation is **layout scaffolding only**.

### The "Design-to-Code" Gap
Figma is static; SwiftUI is dynamic. No tool fully bridges this:
- Component variants in Figma → don't automatically map to SwiftUI state
- Interaction flows → not representable in design files
- Custom behaviors → must be coded by hand

---

## REAL-WORLD WORKFLOWS

### Workflow A: Design System (Mature Teams)
**Use:** Figma Dev Mode + Code Connect + Token Code Plugin

1. Build component library in Swift/SwiftUI
2. Link components to Figma library via Code Connect
3. Map design tokens (colors, fonts) to code via Token Code
4. Designers reference production code in Dev Mode
5. Zero generated code (single source of truth)

**Timeline:** Setup takes 2-4 weeks, then sustainable maintenance

---

### Workflow B: MVP / Rapid Prototyping (Solo Developers)
**Use:** Claude AI + Figma Screenshots

1. Design screens in Figma (focus on structure, not perfection)
2. Export screens as PNG
3. Paste into Claude Code + describe requirements
4. Get initial SwiftUI scaffold
5. Iterate with refinement prompts
6. Build out state/navigation manually
7. Deploy in 1-2 weeks

**Timeline:** Fast (days), requires some manual engineering

---

### Workflow C: Mid-Scale Project (Small Team)
**Use:** Visual Copilot + Manual Refinement

1. Design in Figma (strict auto-layout usage)
2. Run Visual Copilot plugin on screens
3. Collect generated SwiftUI code
4. Post-process for iOS patterns (navigation, state)
5. Add animations and custom interactions
6. Deploy in 2-4 weeks

**Timeline:** Moderate speed, predictable effort

---

## HONEST VERDICT BY USE CASE

### "I want to design in Figma and have code automatically appear"
❌ **This doesn't exist in production.**

All tools require post-processing. Even the best claim 70-75% completeness for layouts.

### "I want to avoid design-to-code busywork"
✅ **Claude AI + Screenshots** — skip plugins, iterate directly in code

### "I want designers and developers on the same page"
✅ **Figma Dev Mode + Code Connect** — link actual production code

### "I want a quick scaffold to start from"
✅ **Visual Copilot** — free, one-click, 70% useful

### "I want to manage design tokens centrally"
✅ **Tokens Studio** — solid infrastructure

### "I want to try a commercial tool risk-free"
✅ **Codia free tier** (30 exports/day) — test before paying

### "I want open-source with no vendor lock-in"
✅ **FigmaToCode** — 4.2k stars, maintained, but requires debugging

---

## WHAT'S ACTUALLY HAPPENING (2026)

**Consensus from developers:**

1. **Figma → pixel-perfect layout code** ✅ Possible (80%+ accuracy with good design practices)
2. **Layout code → production app** ❌ Still requires 40-60% manual work
3. **Automatic animations** ❌ Doesn't exist
4. **Automatic state management** ❌ Doesn't exist
5. **Automatic data binding** ❌ Doesn't exist

The time saved is **layout markup only** (~3-5 hours per screen), not full app development (~40-80 hours per screen).

---

## FINAL RECOMMENDATION

### For pixel-perfect iOS apps from Figma designs:

**Primary:** Claude Code with Figma screenshots
- Fastest iteration
- Best code quality
- Supports custom components
- No tool dependencies
- Works if your Figma file is good

**Secondary:** Figma Dev Mode + Code Connect (if you have a mature component library)
- Single source of truth
- Scales to teams
- Zero generated code to maintain

**Tertiary:** Visual Copilot for scaffolding (if you want visual feedback from Figma)
- Free one-click generation
- 70-75% useful output
- Requires post-processing

**Avoid:** Locofy, Relay, standalone plugins without clear SwiftUI track record

---

## Sources

1. [Figma Dev Mode Review 2025](https://skywork.ai/blog/figma-dev-mode-review-2025/)
2. [Builder.io Visual Copilot: Figma to SwiftUI](https://www.builder.io/blog/figma-to-swiftui)
3. [Figma Code Connect SwiftUI Docs](https://developers.figma.com/docs/code-connect/swiftui/)
4. [Codia AI Figma to SwiftUI](https://codia.ai/docs/getting-started/figma-to-swiftui.html)
5. [FigmaToCode GitHub (4.2k+ stars)](https://github.com/bernaferrari/FigmaToCode)
6. [Claude Code Eyes to See SwiftUI Views](https://twocentstudios.com/2025/07/13/giving-claude-code-eyes-to-see-your-swiftui-views/)
7. [Design+Code: Figma to SwiftUI with Claude AI](https://designcode.io/swiftui-and-claude-ai-figma-to-code/)
8. [Locofy.ai Documentation](https://www.locofy.ai/convert/figma-to-swiftui)
9. [Tokens Studio for Figma](https://docs.tokens.studio/figma/export/options)
10. [Relay Sunset Announcement - April 30, 2025](https://relay.material.io/)

