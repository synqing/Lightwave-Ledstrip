# LightwaveOS Dashboard V4 — UI/UX/Tech Audit Report

**Target:** `docs/LightwaveOS_Dashboard_v4.html`  
**Audit date:** 22 Dec 2025  
**Scope:** Visual/UI, UX, accessibility (WCAG 2.1), responsiveness, performance signals, technical implementation, comparison to documented requirements.

## Executive summary

The V4 HTML is a strong **visual prototype** of the “Glass V4” aesthetic: consistent card system, clear hierarchy, and a cohesive colour palette. However, it is **not production‑ready** for the embedded device context or the external dashboard goals because:

- **Critical:** It relies on external CDNs (Tailwind CDN + Iconify API + Google Fonts), which is incompatible with offline/local-network reliability expectations and increases latency/jank.
- **High:** Several interactive components are *mouse/touch only* (tabs, listbox, timeline drag) and are **not keyboard accessible** per WCAG 2.1.
- **High:** The Canvas rendering has a **scaling bug** (`ctx.scale` accumulates on resize) and uses APIs with cross‑browser risk (`roundRect`).
- **High:** Core functionality is largely **static/demo** (no real API bindings; skeletons/empty/error states are present but not integrated).

**Net:** Visually excellent foundation; requires accessibility, robustness, and integration work before it can be treated as a reference implementation.

---

## Evidence pack (screenshots)

Saved in `docs/audits/v4/`:

- `audit-v4-desktop-control.png`
- `audit-v4-desktop-dropdown-open.png`
- `audit-v4-desktop-shows.png`
- `audit-v4-desktop-effects.png`
- `audit-v4-desktop-system.png`
- `audit-v4-tablet.png`
- `audit-v4-mobile.png`
- `audit-v4-mobile-shows.png`
- `audit-v4-mobile-effects.png`

---

## 1) Visual inspection (layout, spacing, typography, iconography)

### Strengths
- **Layout & spacing:** Consistent rhythm: header → tabs → page header → hero visualiser → 3‑card grid. Padding scales cleanly across breakpoints.
- **Colour scheme:** Strong, consistent palette (dark surface + gold primary + cyan/green accents). Good “glass” depth via inset highlights and shadows.
- **Typography:** Clear hierarchy using Bebas Neue for display + Inter for body; good use of caps for section labels.
- **Iconography:** Lucide icons are consistent and legible; buttons generally meet minimum touch target size.

### Issues
- **Inline responsive styles:** There is invalid CSS in inline styles, e.g. `style="width:10rem; sm:width:12rem; ..."` on the Effects search box. `sm:width` is not valid CSS, so the intended responsive width does not apply.
  - Evidence: HTML line containing `sm:width:12rem`.

---

## 2) UX audit (navigation, flows, responsiveness, states)

### Navigation & information architecture
- **Tabs are clear** and labelled; the underline indicator is intuitive.
- **Control tab**: the most important functions are above the fold on desktop.
- **Shows tab**: the concept of zone tracks is understandable.

### Responsiveness
- Mobile layout stacks cards and keeps key controls visible.
- Tab bar scroll exists for small screens.

### Missing/Incomplete states
- **Loading states exist but are not integrated:** skeletons are present (`lgpSkeleton`, `effectsSkeleton`, `metricsSkeleton`) but never toggled by real loading logic.
- **Error states are mostly demo:** connection banner exists but is not triggered by real network health.
- **Empty states:** no proper “no results” empty state for Effects filter/search.

---

## 3) Accessibility (WCAG 2.1) audit

### What’s good
- Tabs use `role="tablist"`, `role="tab"`, `role="tabpanel"` with `aria-controls`/`aria-labelledby`.
- Buttons generally have accessible names (`aria-label` on icon buttons).
- Sliders have `aria-labelledby` and `aria-valuenow/min/max`.
- Progress bars include `role="progressbar"` with `aria-valuenow/min/max`.
- Focus-visible outline is present globally.

### Critical gaps
1. **Tabs are not keyboard-operable (WCAG 2.1.1 Keyboard)**
   - Expected: arrow keys to move between tabs; Enter/Space to activate; focus management.
   - Actual: only click handlers; ArrowRight did not change tabs.

2. **Palette dropdown listbox is not keyboard-operable (WCAG 2.1.1, 2.4.3 Focus order)**
   - Expected: Escape closes; Arrow keys change active option; Enter selects; focus is moved into listbox; `aria-activedescendant` or roving tabindex.
   - Actual:
     - Escape does **not** close the dropdown.
     - No keyboard navigation implemented.

3. **Effects “Favourite star” uses ARIA but not consistent semantics**
   - Some stars are `button` with `aria-pressed` (good), but other places use an icon with `aria-label` on the icon itself rather than the button.

4. **Drag-and-drop is mouse/touch only**
   - No keyboard alternative for timeline edits (WCAG 2.1.1, 2.5.1 Pointer gestures).

### Medium gaps
- The dropdown listbox lacks a clear “active option” visual/focus state for keyboard users.
- The search box has `aria-label`, but lacks associated `<label>` (acceptable, but label is preferable).

---

## 4) Technical implementation review

### HTML/CSS structure
- **Strength:** Clean semantic structure with header/nav/tabpanel sections.
- **Risk:** Heavy use of inline styles makes maintenance difficult and breaks Tailwind’s intended workflow.
- **Risk:** Tailwind loaded via CDN; browser console warns against production use.

### JavaScript patterns
- **Tab underline logic** uses DOM measurements and manual transitions; workable, but brittle.
- **Dropdown close logic** only closes on document click, not keyboard (Escape).
- **Slider bindings** are clean (single `bindSlider`), but there is no throttling/debounce for downstream API calls (currently demo-only).

### Cross-browser compatibility
- **High risk:** `CanvasRenderingContext2D.roundRect()` is not supported in older Safari/iOS versions.
- **High risk:** `ctx.scale(dpr, dpr)` inside `resizeCanvas()` accumulates on every resize; should reset the transform first.
- **Medium risk:** Touch drag uses `{ passive: true }` on `touchstart`/`touchmove`, preventing `preventDefault()` to stop page scrolling during drag.

### Performance & loading signals
Measured in-browser (local server; external resources still fetched):
- DOMContentLoaded: ~602ms
- Load event: ~649ms
- **External network dependencies:** 2 scripts + Google Fonts + multiple Iconify JSON fetches.

Network requests show:
- Tailwind CDN (`https://cdn.tailwindcss.com/` + `.../3.4.17`)
- Iconify runtime plus **multiple `api.iconify.design` fetches** for lucide icons
- Google Fonts CSS + font files

This creates a brittle “works on my laptop” prototype that will degrade on:
- captive portals
- poor WiFi
- device without internet

---

## 5) Comparison against design/spec documents

### Reference specs used
- `docs/PRD_WEBAPP_DASHBOARD.md`
- `docs/DASHBOARD_V3_INTEGRATION_PLAN.md`

### Key deviations / gaps

1. **Zone system not feature-complete** (High)
   - V4 has zone tabs and some zone UI, but lacks:
     - Zone count control (1–4)
     - Per-zone brightness/speed/palette
     - Zone mixer faders
     - Real zone boundary visualisation and editing

2. **Real-time sync is not implemented** (High)
   - PRD requires WebSocket reconnection logic and dynamic data.
   - V4 is demo-only (local simulated values).

3. **Effects browser is mostly static** (Medium)
   - Hardcoded cards/filters; no empty state.

4. **System tab shows demo telemetry** (Medium)
   - Graphs update with random values; no integration to real metrics.

---

## 6) Issue catalogue (priorities, recommendations, effort)

### Critical
- **C1 — External CDN dependencies**
  - Impact: offline/local-network failure; inconsistent performance.
  - Fix: bundle Tailwind locally; self-host icons/fonts or inline SVG.
  - Effort: 1–2 days.

- **C2 — Canvas resize scaling bug (`ctx.scale` accumulation)**
  - Impact: visual glitches after resize; incorrect rendering.
  - Fix: reset transform via `ctx.setTransform(1,0,0,1,0,0)` before scaling.
  - Effort: 0.5 day.

### High
- **H1 — Keyboard accessibility for tabs**
  - Fix: implement roving tabindex + ArrowLeft/ArrowRight and Enter/Space activation.
  - Effort: 0.5–1 day.

- **H2 — Dropdown listbox keyboard + Escape close**
  - Fix: Escape closes; Arrow keys move; Enter selects; manage focus properly.
  - Effort: 1 day.

- **H3 — Drag-and-drop accessibility**
  - Fix: provide non-drag edit UI (forms) or keyboard alternative for moving scenes.
  - Effort: 2–4 days.

### Medium
- **M1 — Invalid inline responsive style (`sm:width`)**
  - Fix: replace with Tailwind classes (e.g. `w-40 sm:w-48`).
  - Effort: 0.25 day.

- **M2 — `roundRect` cross-browser fallback**
  - Fix: implement fallback path drawing for older browsers.
  - Effort: 0.5 day.

- **M3 — Missing favicon**
  - Fix: add favicon file or remove request.
  - Effort: 0.1 day.

### Low
- **L1 — Consistency: icon accessibility labels**
  - Fix: ensure labels on the `button`, not the icon.
  - Effort: 0.25 day.

---

## 7) Suggested remediation roadmap

### Phase 0 (1–2 days): Reliability & correctness
- Bundle Tailwind locally (no CDN).
- Replace Iconify runtime (or cache self-hosted icon sprites).
- Fix canvas resizing transform.
- Remove invalid inline responsive styles.

### Phase 1 (2–4 days): Accessibility baseline
- Keyboard-operable tabs.
- Accessible listbox behaviour (Escape, arrows, Enter, focus).
- Establish a consistent focus management pattern.

### Phase 2 (1–2 weeks): Feature parity + integration
Align with PRD requirements:
- Zone count, per-zone controls, zone mixer.
- Real-time API integration (WS + REST).
- Proper loading/error/empty states.

### Phase 3 (ongoing): Hardening
- Visual regression tests (Playwright) for key breakpoints.
- Performance budget and bundle analysis.
- Cross-browser test matrix (Safari iOS, Chrome, Firefox).

---

## 8) Updated design specs (delta)

- Treat this V4 HTML as **visual reference only**.
- Canonical implementation should live in `lightwave-dashboard/` (React + bundled Tailwind) with:
  - a design-token layer (Tailwind config) rather than inline styles
  - an accessible component library (tabs/listbox/slider)
  - a real data layer (V2Client/V2WsClient) with loading/error handling

