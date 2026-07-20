# Taste Skill — Pilot Brief

**Decision under test:** does `taste-skill` v2 earn a place in the web workflow, or is the extracted rubric (`ANTI_SLOP_REVIEW_RUBRIC.md`) all the value there is? Measured, not vibes.

**Owner:** Captain · **Status:** ready to run · **Date:** 2026-06-04

---

## 1. Core question

The extract rubric already harvests the durable rules. This pilot tests the *remaining* hypothesis: that running the **live skill** on net-new views measurably reduces design fix-count versus not running it — enough to justify carrying an experimental, single-maintainer dependency in the loop.

If the live skill does not beat the control, the correct outcome is **EXTRACT-ONLY** (keep the rubric, drop the skill).

## 2. Scope

- **In:** `lightwave-dashboard` only. Verified stack (2026-06-04): React 19.2, Vite (rolldown-vite), Tailwind **v3**.4.17, `lucide-react`, `@dnd-kit`. No Framer Motion / GSAP installed. Mature CI: `validate:classes`, `lint:styles` (inline-style check), `verify:cdns`, Vitest, Playwright snapshots, Storybook.
- **Deferred:** `k1-composer` — no `package.json` at its root; verify it is a standard web app before including.
- **Excluded:** landing page (locked `DESIGN.md`, archived workspace) and all non-web surfaces. The skill is **never** run against the landing page as an authority.

## 3. Setup (do this once)

1. **Vendor a pinned snapshot.** Copy the specific `SKILL.md` into a scratch location. **Do not** run `npx skills add` against the live repo, and do not commit the skill file into the tree. We are testing, not adopting.
2. **Subordinate it.** Paste `UI_STANDARDS.md` + the `tailwind.config.js` token block at the **top** of the working skill text as the dominant source of truth. The skill explicitly defers to a pasted project style guide — use that.
3. **Tune the dials** (override upstream defaults): `DESIGN_VARIANCE 5–6`, `MOTION_INTENSITY 3–4`, `VISUAL_DENSITY 7–8`. Rationale: control surface, reduced-motion aware, not an art gallery.
4. **Load the reject list** from `ANTI_SLOP_REVIEW_RUBRIC.md` § C so the agent does not swap Inter, replace lucide, or inject motion libs.

## 4. Method

Pick **2–3 net-new** dashboard views/components that do not yet exist (so we are not redesigning locked patterns). For each:

- **Control:** generate with the normal prompt, no skill.
- **Variant:** same prompt + tuned skill + pasted standards.

Hold everything else constant. Count the manual edits required to bring each output to merge-ready (passes CI + matches `UI_STANDARDS.md`). Time-box to a single focused session.

## 5. Pass / fail (explicit)

**PASS (adopt as opt-in for net-new views) requires all of:**
- [ ] Zero violations of `tailwind.config.js` tokens or `UI_STANDARDS.md` patterns in the variant.
- [ ] Variant needs **fewer** manual fixes than control to reach merge-ready (count the edits).
- [ ] Variant introduces **zero** new runtime dependencies.
- [ ] Variant passes existing CI unchanged: `validate:classes`, `lint:styles`, `verify:cdns`, Playwright snapshots, a11y checks.

**FAIL (any one) → EXTRACT-ONLY or REJECT:**
- [ ] Swaps `Inter` for Geist/Satoshi, or replaces `lucide-react`.
- [ ] Adds `framer-motion`, `gsap`, `@phosphor-icons/*`, or `@radix-ui/react-icons`.
- [ ] Emits light-mode surfaces or strips functional status glows.
- [ ] Output fails `verify:cdns` or `validate:classes`.
- [ ] Fix-count is equal to or worse than control (the skill added no leverage).

## 6. Decision matrix

| Outcome | Meaning | Action |
|---|---|---|
| **ADOPT** | Live skill measurably beat control, no breakage | Keep skill as **opt-in** for net-new views; fold any new durable checks into the rubric |
| **EXTRACT-ONLY** | Rubric captures the value; live skill adds nothing | Drop the live skill; keep `ANTI_SLOP_REVIEW_RUBRIC.md` |
| **REJECT** | Skill fought the locked system | Bin both the skill and any skill-specific tooling |

## 7. Kill switch / drift guard

If at any point the skill overrides a **locked token** in generated output, **stop the pilot** — that is the authority-drift class the governance already guards against (sandbox-to-integration loss). Log it, do not paper over it.

## 8. Honest leverage note

The dashboard already enforces engineering hygiene via lint + CI, so the live skill's *only* marginal value here is the aesthetic / anti-slop **layout + content** layer. If that layer does not cut fix-count, EXTRACT-ONLY is the expected and correct result — and the rubric has already banked the win at zero ongoing cost.

---

## Sources
- `ANTI_SLOP_REVIEW_RUBRIC.md` (companion)
- taste-skill docs / guide — https://www.tasteskill.dev/docs · https://www.tasteskill.dev/guide
- Verified stack: `lightwave-dashboard/package.json`, `tailwind.config.js`, `docs/UI_STANDARDS.md`
