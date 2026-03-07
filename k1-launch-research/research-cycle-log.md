# K1 Launch Research — Cycle Log

## Cycle: 2026-03-05 (Cowork scheduled task)
- Task worked on: Task 2 — Recognition Device Research
- Progress: Complete. Researched 6 hardware brands (Teenage Engineering, Nothing, Apple, Dyson, Analogue, Bang & Olufsen) for recognition device patterns. Identified 5 candidates for K1: The Glow (luminous surface), The Breath (centre-origin pulse), The Edge (thin profile angle), The Dark Field (consistent dark composition), The Colour Moment (hero effect selection). Recommended combination: A (Glow) + B (Breath) + D (Dark Field) as primary stack. Full analysis with execution requirements saved.
- Status: Complete
- Next: Task 3 — Brand Manifesto Sprint Plan (requires reading Quiet Confidence spec; blocked on K1 Launch Planning directory access — not mounted in current Cowork session)
- Note: Output saved to `k1-launch-research/` in Lightwave-Ledstrip workspace. Needs moving to `/Users/spectrasynq/SpectraSynq_K1_Launch_Planning/05-Brand-and-Marketing/research/` on Mac.

## Cycle: 2026-03-05 14:35 UTC (Cowork scheduled task)
- Task worked on: Task 4 — Competitive Landscape Monitoring (first cycle)
- Progress: Complete initial monitoring sweep. Searched 8 topic areas across aspirational peers and adjacent space. Key findings:
  - **Monar Canvas Speaker** launched on Kickstarter 3 March 2026 — closest new entrant to K1's space (audio-reactive visual hardware). Uses AI + screen, not physical light + DSP. Watch funding trajectory.
  - **Teenage Engineering** extremely active: EP-40 Riddim (culturally-themed edition), Field System Black (colour editions), OP-1 Field v1.6 (firmware-extending-hardware), OP-XY rumoured.
  - **Nothing** evolved Glyph Interface to "Glyph Matrix" (Phone 3) then to LED squares (Phone 4a). Still iterating; K1's LGP glow is more distinctive.
  - **No one shipping beat-tracking in consumer form factor.** K1's PipelineCore (onset + PLL) remains unmatched. WLED-SR has not added it. HyperCube doesn't have it. Monar uses AI.
  - **Synthstrom Audible** open-source firmware model producing community contributions — potential post-launch model for K1 effect SDK.
  - **Analogue 3D** absorbed $20 tariff price increase without backlash — creative hardware audience tolerates premium pricing.
- Status: Complete (first cycle — ongoing monitoring in future cycles)
- Next: Future cycles should track Monar Kickstarter funding, TE OP-XY announcement, and any WLED-SR audio pipeline developments.
- Output: `K1_Launch_Planning/02-Market-Intelligence/competitive-updates.md`

## Cycle: 2026-03-06 (Cowork scheduled task)
- Decision targeted: D5 (Pricing — EXPLORING, $449–$549)
- Research action: Validate/challenge Captain's pricing hypothesis against competitive price landscape. Searched 9 product categories and extracted specific pricing for direct and adjacent competitors.
- Findings:
  - [FACT] HyperCube (closest reactive LED competitor) tops out at $300. No reactive LED product ships above this.
  - [FACT] Monar Canvas Speaker: MSRP $1,299, early bird $799. Launched KS 3 March 2026. Only 8 backers after 3 days — weak early traction.
  - [FACT] TE OP-1 Field: $1,399–$1,999. "Set your own price" experiment confirms $1,400 floor. 15+ years brand equity.
  - [FACT] Analogue Pocket: $199 → $240 (tariffs). Community absorbed 20% increase without backlash.
  - [FACT] Divoom Pixoo Max: $100–$130. Basic pixel art, no audio intelligence.
  - [INFERENCE] K1 at $449–$549 sits in an empty gap between HyperCube ($300) and Monar ($799). No competitor occupies this space.
  - [INFERENCE] The gap is opportunity AND risk — no price anchor exists for buyers in this tier.
  - [HYPOTHESIS] K1's price viability depends on whether buyers perceive it as "music technology" (worth $449+) or "LED accessory" (HyperCube-adjacent, worth $300 max).
- Juxtapositions surfaced:
  1. $300 ceiling in reactive LED vs. K1's proposed $449+ (category-redefining price)
  2. Monar validates $800+ audio-visual hardware but has weak KS traction (cautionary)
  3. TE proves premium works WITH brand equity; K1 launches with none
  4. Technical moat (PLL, FFT) is invisible until experienced — price anchor problem
  5. The $300–$800 gap is entirely unoccupied — K1 would own it
- Decision register impact: D5 remains EXPLORING. Evidence is balanced — supports and challenges the hypothesis in roughly equal measure. Key blocker identified: K1 needs a perceptual reframe from "LED product" to "music technology" before price can hold.
- Next cycle should: Research HyperCube buyer reviews (what do they praise/criticise?), track Monar KS trajectory at week 1, search for any DTC LED art brands at $400+.
- Output: `K1_Launch_Planning/02-Market-Intelligence/d5-pricing-validation-cycle1.md`

## Cycle: 2026-03-06 ~08:15 UTC (Cowork scheduled task — D5 pricing cycle 2)
- **Decision targeted:** D5 (Pricing — EXPLORING, $449-549)
- **Research action:** Granular competitive price mapping with vendor-verified prices. Focused on products the previous cycle missed: LiveLightMagic (direct feature match), Nanoleaf panel kits (casual buyer ceiling), SoundSwitch (professional floor), Petru (audiophile beachhead).
- **Findings:**
  - [FACT] LiveLightMagic: $341 for 320 WS2812B LEDs, 100+ effects, browser/mobile control. This is the closest feature-comparable product to K1 on paper. K1's premium over this baseline is 32-61%.
  - [FACT] HyperCube 15: $450 (was $500, currently discounted). Proves $450 payable for premium ambient light with basic sound reactivity and 95 patterns.
  - [FACT] Nanoleaf 15-panel kit: ~$350-400. Defines casual buyer ceiling with smart home integration and basic music sync.
  - [FACT] SoundSwitch Control One: $299 hardware + $8-20/mo software. Year-one cost: $395-539, directly overlapping K1's $449-549 range.
  - [FACT] Petru vinyl stand: $240, 192 LEDs, 24-band analysis. Design-object positioning for audiophiles.
  - [FACT] Audiorays Pro: $199, 384 LEDs, 24-band FFT. Best-in-class at the enthusiast tier.
  - [INFERENCE] K1 sits in pricing no-man's-land between consumer ambient ($200-400) and professional DJ ($400-600+/yr). This is either moat or gap.
  - [INFERENCE] K1's entire price premium over LiveLightMagic rests on audio intelligence (PLL beat tracking, 256-bin FFT, chroma) + Light Guide Plate form factor. If these aren't VISIBLE in the first demo experience, the premium evaporates.
  - [HYPOTHESIS] Petru's vinyl/audiophile community may be K1's natural beachhead — they already pay premiums for musical fidelity in physical objects.
- **Juxtapositions surfaced:**
  1. LiveLightMagic Gap: identical LED/effect count at $341 — K1's entire premium = audio intelligence + form factor
  2. HyperCube Anchor: $450 validated via visual mechanic (infinity mirrors), not musical intelligence — different buying motivation
  3. Nanoleaf Ceiling: $350-400 = casual limit. K1 must attract a fundamentally different buyer
  4. SoundSwitch Floor: at $549, K1 competes with professional DJ tools without brand equity
- **Decision register impact:** D5 remains EXPLORING. This cycle adds specificity to previous cycle's finding. Supports $449 floor (HyperCube anchor). Challenges $549 ceiling (SoundSwitch overlap). Key question sharpened: the first-run demo experience (D6) is the pricing gatekeeper.
- **Next cycle should:** Deep-dive HyperCube buyer reviews (what do $450 buyers praise/criticise?). Explore Petru/vinyl community as K1 beachhead. Investigate "first 10 seconds" demo requirements to justify price differential over LiveLightMagic.
- **Output:** `k1-launch-research/pricing/d5-pricing-landscape-2026-03-06.md`

## Cycle: 2026-03-07 ~scheduled (D5 pricing cycle 3)
- **Decision targeted:** D5 (Pricing — EXPLORING, $449–$549)
- **Research action:** Broadened competitive sweep beyond reactive-LED niche into smart lighting market (Nanoleaf, Govee, LIFX, Twinkly), WLED open-source ecosystem, and DMX professional tier. Goal: validate whether the $300–$500 gap identified in cycles 1–2 holds when the frame is widened.
- **Findings:**
  - [FACT] Audiorays Visualiser Pro: $199, 392 LEDs, spectrum-only analysis — still the best-in-class enthusiast benchmark
  - [FACT] Nanoleaf Skylight 3-panel: $249. Basic ambient colour sync, no beat tracking
  - [FACT] WLED audio-reactive is now default in v0.15.0+. ESP32-S3 board + I2S mic + level shifter = ~$15–40 total hardware cost
  - [FACT] Smart lighting market CAGR 18.5% through 2029 (Technavio). Cost remains #1 adoption barrier
  - [FACT] Pro DMX controllers routinely exceed $500 but deliver zero musical intelligence — protocol translators only
  - [FACT] NIQ: only a small % of consumers "fully willing" to pay premium for smart variants. Performance and experience are the leading purchase drivers
  - [INFERENCE] The $300–500 gap HOLDS across all three frames (reactive LED, smart lighting, professional DMX). No product combines integrated LEDs + musical intelligence + premium form factor in this range
  - [INFERENCE] K1 at $449 is a 10× multiplier over WLED DIY. The moat is PLL beat tracking + onset + chroma + Light Guide Plate form factor
  - [HYPOTHESIS] Category novelty (Light Guide Plate) may prevent price anchoring — buyers have no reference product to compare against
- **Juxtapositions surfaced:**
  1. "No Man's Land" at $300–500 confirmed across third search vector — opportunity or warning?
  2. K1 vs WLED: 10× price → moat depends entirely on buyer perceiving musical intelligence difference
  3. K1 vs Nanoleaf: $249 basic sync → $449 PLL intelligence = 2× premium for qualitative leap. Can the leap be demonstrated pre-purchase?
- **Decision register impact:** D5 remains EXPLORING. Three cycles now consistently find the same pattern: the price gap exists, the technical moat exists, but the CONVERSION MECHANISM (can buyer perceive the difference before purchase?) is the unresolved gatekeeper. This shifts focus to D6 (first-run choreography) as the pricing enabler.
- **Next cycle should:** Research ViVi II post-KS retail pricing, premium audio-visual art installations ($500–$1000), Founders Edition pricing precedents in hardware. Investigate whether PLL vs FFT is visually distinguishable in short-form video — this is now the critical D5/D6 intersection.
- **Output:** `k1-launch-research/2026-03-07-pricing-competitive-landscape.md`
