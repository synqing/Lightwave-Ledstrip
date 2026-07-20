# Anti-Slop Review Rubric (web frontends)

**Status:** review aid, not an authority. **Authority rank:** subordinate.
**Senior authorities win on every conflict:**
- `lightwave-dashboard/tailwind.config.js` (locked tokens)
- `lightwave-dashboard/docs/UI_STANDARDS.md` (Glass V4 system, patterns, a11y)
- the relevant project's design lock (e.g. landing-page `DESIGN.md`)

If a rule below conflicts with a locked token or pattern, **drop the rule** — do not "fix" the locked system to satisfy this rubric. This document is a PR/second-opinion checklist for net-new web work; it is never a token source.

**Provenance.** Engineering and anti-pattern checks distilled from the open-source `taste-skill` v2 `SKILL.md` (`Leonxlnx/taste-skill`, fetched 2026-06-04). We extract the durable rules; we do **not** install the upstream skill as an authority. The upstream is experimental, single-maintainer, and self-declares churny wording — so we vendor the value and own it here.

**Scope.** Applies to web surfaces: `lightwave-dashboard`, `k1-composer`, and any net-new marketing/dashboard frontend. Does **not** apply to iOS (SwiftUI), Tab5 (LVGL/C), or firmware.

---

## How to use

Run as a review pass on net-new web views or PRs. Each item is **Pass / Fail / N-A** with a one-line justification. A Fail blocks merge until fixed or explicitly waived by Captain. Tune dials per surface (see Section D). On any clash with a senior authority, the senior authority wins and the rubric item is N-A.

---

## A. ADOPT — net-new checks (not already in `UI_STANDARDS.md`)

These add value because `UI_STANDARDS.md` does not currently state them as law.

### A1. Engineering guardrails
- [ ] **Full-height sections use `min-h-[100dvh]`, never `h-screen`.** `h-screen` jumps on iOS Safari when the URL bar collapses.
- [ ] **Animate only `transform` and `opacity`.** Never animate `top`/`left`/`width`/`height` (forces layout/paint). The dashboard's existing keyframes already obey this — hold the line on new code.
- [ ] **CSS Grid over flex percentage maths.** No `w-[calc(33%-1rem)]`; use `grid grid-cols-* gap-*`.
- [ ] **No silent dependency injection.** Check `package.json` before any new import. Do **not** add `framer-motion`, `gsap`, `@phosphor-icons/react`, or `@radix-ui/react-icons` to the dashboard — it ships CSS animations + `lucide-react` already. New runtime deps need Captain sign-off.
- [ ] **Perpetual/infinite animations are isolated + memoised** in their own leaf client component; they never trigger parent re-renders.
- [ ] **`prefers-reduced-motion` is honoured.** Real gap: the dashboard defines six infinite keyframes (`pulse-glow`, `shimmer`, `border-glow`, etc.). Any new perpetual motion must degrade gracefully under reduced-motion. (a11y — escalate to `UI_STANDARDS.md` if you want this promoted to standard.)

### A2. Layout anti-slop
- [ ] **No three-equal-card feature row by default.** Use 2-column zig-zag, asymmetric grid, or scroll-snap.
- [ ] **No centred hero/H1 when layout variance > 4.** Prefer split-screen or left-aligned-content / right-aligned-asset.
- [ ] **Bento grids: N items → N cells.** No empty filler cells.
- [ ] **No div-based fake product UI** (fake task lists, terminals, dashboards built from styled divs) on marketing surfaces. Use a real screenshot or generated asset. *(N-A for the dashboard itself — it IS the real product UI.)*

### A3. Content / "AI tells"
- [ ] **No generic placeholder names** ("John/Jane Doe", "Sarah Chan"). Use realistic, varied names.
- [ ] **No suspiciously round fake data** (`99.9%`, `50%`, `1234567`). Use organic values.
- [ ] **No filler copy verbs** ("Elevate", "Seamless", "Unleash", "Next-Gen"). Concrete verbs only.
- [ ] **No startup-slop brand names** ("Acme", "Nexus", "SmartFlow") in mockups.
- [ ] **No hotlinked Unsplash.** Use stable seeded placeholders or real assets.

### A4. Copy hygiene — **optional, house-aware**
- [ ] **Em-dash / en-dash check on UI output copy.** The upstream skill bans them outright. We **do not** adopt this wholesale: SpectraSynq house style and British-English docs use em-dashes deliberately. Apply this **only** to user-facing marketing UI strings where a hyphen reads cleaner, and **never** to docs, comments, or logs. Default: N-A unless the brief calls for it.

---

## B. ALREADY COVERED — do not re-litigate (redundancy check)

`UI_STANDARDS.md` / `tailwind.config.js` already own these. Don't raise them as "slop"; point the reviewer at the senior doc instead.

| Topic | Where it lives |
|---|---|
| Off-black background (`#0f1219`), not pure black | `tailwind.config.js` |
| Single brand accent (gold `#ffb84d`) + semantic status colours | `tailwind.config.js` |
| Card / container / glass patterns | `UI_STANDARDS.md` § Glass V4 Pattern Library |
| Tailwind class ordering | `UI_STANDARDS.md` § Class Ordering Convention |
| Responsive breakpoints, mobile collapse | `UI_STANDARDS.md` § Responsive Breakpoints |
| Touch targets, focus states, ARIA, semantic HTML | `UI_STANDARDS.md` § Accessibility Standards |
| Hover states, inline-style-for-colour ban | `UI_STANDARDS.md` § Common Anti-Patterns |
| Visual regression / manual checklist | `UI_STANDARDS.md` § Testing Visual Consistency |

---

## C. REJECT — Taste Skill rules that FIGHT the locked system

Do **not** apply these to the dashboard. An agent that "fixes" any of them is breaking a deliberate, function-driven decision.

| Upstream rule | Why we reject it here |
|---|---|
| "Inter is banned → use Geist/Satoshi" | Dashboard **locks** `Inter` (body) + `Bebas Neue` (display) in `tailwind.config.js`. Keep Inter. |
| "No neon/outer glows" | Dashboard uses **functional** status glows (`pulse-glow-red` = alarm state). Semantic, not decorative. Keep. |
| "Max 1 accent colour" | Dashboard uses **semantic** status accents (`cyan`/`green`/`red`) for state. Keep. |
| "Icons must be Phosphor or Radix" | Dashboard standardised on `lucide-react`. Keep lucide. |
| "Use Framer Motion / GSAP for motion" | Don't inject the dep. CSS keyframes + `@dnd-kit` already cover the need. |
| "Never use `#000000`" | Moot — dashboard is already off-black (`#0f1219`). |

---

## D. Dial settings per surface

The upstream defaults (`DESIGN_VARIANCE 8 / MOTION_INTENSITY 6 / VISUAL_DENSITY 4`) are tuned for artsy marketing pages and are **wrong for a control surface**. Use:

| Surface | Variance | Motion | Density | Note |
|---|---|---|---|---|
| `lightwave-dashboard` | 5–6 | 3–4 | **7–8** | Functional control UI, not an art gallery. Override the skill's density default. Reduced-motion aware. |
| `k1-composer` | 5–6 | 3–4 | 6–7 | Tool UI. Verify stack before applying — no `package.json` found at composer root as of 2026-06-04. |
| Net-new marketing microsite | 6–8 | 4–6 | 3–4 | Closer to upstream defaults, but still subordinate to brand lock. |

---

## Sources
- taste-skill v2 `SKILL.md` — https://raw.githubusercontent.com/Leonxlnx/taste-skill/main/skills/taste-skill/SKILL.md
- taste-skill docs — https://www.tasteskill.dev/docs
- Local senior authorities: `lightwave-dashboard/tailwind.config.js`, `lightwave-dashboard/docs/UI_STANDARDS.md`
