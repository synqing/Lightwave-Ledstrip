---
abstract: "12 product strategy principles derived from Tab5 UI layout exercise. Maps each design principle to product equivalents with real-world examples, anti-patterns, JTBD/Kano analysis, and K1-specific ranking. Read when making roadmap, feature scoping, or prioritisation decisions for SpectraSynq K1."
---

# Product Decision Principles: From UI Layout to Product Strategy

The 12 principles captured in `DESIGN_DECISION_PROCESS.md` emerged from a UI parameter layout exercise on the Tab5 controller. They are not UI-specific. They are decision-making principles that apply at every level of product strategy -- from roadmap to release, from feature scoping to market positioning.

This document translates each principle into its product strategy equivalent, with real-world examples, the anti-pattern it prevents, and specific application to the SpectraSynq K1 visual instrument.

---

## Principle 1: Diagnose the Disease, Not the Symptom

**UI origin:** "The UI feels outgrown" was treated as a visual design problem. The actual problem was information architecture -- how 50+ params map to 16 encoders.

### Product Strategy Equivalent

**Decompose the metric before choosing the intervention.** When a product metric is underperforming -- churn is high, activation is low, NPS is dropping -- the instinct is to pick the most visible cause and start building. Resist. The visible symptom almost never points to the root cause.

"Users are churning" could mean:
- Onboarding is broken (they never experienced the value)
- The product solved a one-time problem (the job is done)
- A competitor is pulling them away (value gap, not quality gap)
- Pricing changed and they are price-sensitive, not dissatisfied
- Power users are fine but casual users never activated

Each of these demands a different intervention. Building a re-engagement email campaign for users who left because your pricing doubled is the product equivalent of redesigning fonts when the menu system is wrong.

### Real-World Example

**HipChat vs Slack (2013-2016).** HipChat was losing ground and responded by adding features -- video calling, screen sharing, integrations. They treated the symptom ("teams are switching to Slack") as a feature gap. Slack's insight was different: the disease was that team messaging felt like enterprise software. Slack fixed the emotional experience -- the speed, the tone, the seamlessness of conversation flow. HipChat shipped features; Slack fixed the actual problem. HipChat is dead.

### Anti-Pattern It Prevents

**Cosmetic intervention on structural problems.** Polishing the UI of a product with broken onboarding. Adding features to a product with a positioning problem. Running sales campaigns for a product with a retention problem. Each of these treats the symptom while the disease progresses.

### K1 Application

When "the LED effects don't feel musical enough," the disease could be: (a) the audio pipeline is dropping beats, (b) the effect is not mapping audio features to the right visual parameters, (c) the effect is mapping correctly but the LGP's light propagation masks subtle changes, or (d) the user's expectations are calibrated to screen-based visualisers that do not apply to edge-lit acrylic. Each demands a different fix. Investigating the effect code when the problem is the microphone's frequency response is the K1 equivalent of changing fonts when the menu system is wrong.

---

## Principle 2: Research the Right Domain

**UI origin:** Researching "dark mode colour palettes" was irrelevant. The right question was "how do 16 encoders manage 50+ params?"

### Product Strategy Equivalent

**Match your research to the decision you are actually making.** There are three types of product research, and most teams default to the wrong one:

1. **Competitor research** -- what others are building. Useful for: positioning, pricing, feature parity decisions. Dangerous when: it becomes "build what they have."
2. **User behaviour research** -- what users actually do. Useful for: identifying pain, measuring adoption, validating hypotheses. Dangerous when: it only reflects current users, not potential ones.
3. **Domain/technology research** -- how the underlying problem space works. Useful for: architecture decisions, feasibility, interaction models. Dangerous when: it substitutes for talking to users.

The Tab5 exercise needed domain research (how professional controllers handle deep parameter sets). Competitor research on "LED controller apps" would have been irrelevant because no competitor builds a visual instrument with 16 physical encoders driving 100+ effects through an LGP.

### Real-World Example

**Studying "LED controller market" vs studying "what performing musicians need from a visual instrument controller."** The LED controller market tells you about WiFi-controlled RGB strips, phone apps with colour wheels, and DMX interfaces. None of that is relevant to K1. K1's actual competitive frame is Elektron, Ableton Push, and Resolume Arena -- instruments where deep parameter control through limited physical inputs is the core UX challenge. Researching the wrong domain produces the wrong product.

### Anti-Pattern It Prevents

**Research theatre.** Thorough research on the wrong question creates false confidence. A 30-page competitive analysis of LED strip controllers would be impressively thorough and completely useless for K1 product decisions. The rigour of the research does not compensate for researching the wrong domain.

### K1 Application

K1 product decisions should draw from: (a) performing musician workflows -- how do artists control visual parameters during a live set?, (b) hardware controller UX research -- how do Elektron, Push, grandMA3 handle deep param sets?, and (c) light physics -- how does edge-lit acrylic propagate and diffuse LED output? The intersection of these three domains IS the K1 product. Research outside this intersection is noise.

---

## Principle 3: Propose Structure Before Visuals, But Render Structure Visually

**UI origin:** The architecture was correct as text but impossible to evaluate until rendered as HTML mockups.

### Product Strategy Equivalent

**Write the strategy before the spec, but make the strategy concrete enough to evaluate.** A product strategy document that says "we will improve onboarding" is text without structure. A strategy that says "we will reduce time-to-first-value from 12 minutes to 3 minutes by eliminating the WiFi configuration step and auto-discovering the K1" is structure -- evaluable, measurable, debatable.

But even structured strategy needs to be "rendered" for stakeholders. For a PM, "rendering" means: a user journey map showing the before/after flow, a prototype showing the 3-minute experience, a metrics dashboard mock-up showing what success looks like. Stakeholders cannot evaluate abstractions. They evaluate experiences.

### Real-World Example

**Amazon's "Working Backwards" process.** The press release and FAQ are written BEFORE the spec. This is "propose structure before visuals." But the press release IS a visual rendering of the strategy -- it makes the value proposition concrete enough for a VP to read it and say "I would not buy this product" or "this changes everything." The structure is rendered in a format stakeholders can evaluate.

### Anti-Pattern It Prevents

**Strategy documents nobody can evaluate.** A 20-page PRD full of requirements that nobody can visualise as a user experience. The requirements are technically correct but nobody -- including the PM -- can tell whether the resulting product will be good. The strategy was never "rendered."

### K1 Application

For K1, "rendering" product strategy means: (a) a video showing the intended performance workflow, not a requirements doc, (b) a physical prototype with the Tab5 wired to K1 demonstrating the parameter control flow, (c) a 30-second clip of someone performing with K1 that makes the value proposition visceral. If you cannot show a musician using K1 and have them understand why it matters, the strategy is not rendered.

---

## Principle 4: Preserve What Works

**UI origin:** The header/footer frame was excellent. The card treatment was the problem. Early mockups threw away both.

### Product Strategy Equivalent

**Audit your product's strengths before planning changes. Protect what users already love.** Every product has features that users depend on, workflows they have internalised, and affordances they take for granted. A redesign that changes these creates negative value even if the new version is objectively "better" in isolation.

The discipline is: before any significant change, create an explicit inventory of (a) what users love and depend on -- do not touch, (b) what users tolerate but do not love -- evaluate, and (c) what users complain about or abandon -- fix. Most teams skip step (a) and assume everything is fair game.

### Real-World Example

**Twitter/X redesign (2023-present).** The core product -- a fast, text-first feed with replies and retweets -- was what users valued. The problems were: algorithmic timeline manipulation, bot/spam proliferation, and content moderation gaps. The redesign changed everything: name, branding, verification, feed mechanics, API access. Many of the changes destroyed what worked (fast text-first experience, developer ecosystem, blue-check trust signal) while not fixing what was broken (bots, content quality). The result was that alternatives (Bluesky, Threads) attracted users by offering what the old Twitter had.

### Anti-Pattern It Prevents

**The nuclear redesign.** Rebuilding everything because something needed fixing. The cost is twofold: you lose existing user trust in what worked, and you multiply engineering scope by 5x because you are rebuilding the good alongside the bad.

### K1 Application

K1's centre-origin rendering, the 100+ effect library, and the audio-reactive pipeline are what make K1 unique. If user feedback says "the Tab5 controller feels clunky," the fix is the controller UX -- not the effect rendering engine. If iOS users say "the app feels slow," the fix is network latency and state sync -- not redesigning the LED preview canvas that already works. Protect the core: audio-to-light mapping. Fix the periphery: control surfaces, onboarding, discoverability.

---

## Principle 5: Create Hierarchy Through Differentiation, Not Uniformity

**UI origin:** Same white borders on frame AND content destroyed visual hierarchy. Three distinct visual levels (frame / content / active) fixed it.

### Product Strategy Equivalent

**Your product needs a clear value hierarchy: primary, secondary, and supporting capabilities.** When every feature is presented with equal prominence -- in marketing, in the UI, in the pricing page -- users cannot determine what the product is FOR. The product feels like a feature list, not a solution to a problem.

The hierarchy:
- **Primary** (the reason users buy): the capability that solves the core job. This gets the most prominent position in marketing, onboarding, and the UI.
- **Secondary** (the reason users stay): capabilities that deepen engagement after the primary value is experienced. These appear after activation.
- **Supporting** (the reason users do not leave): parity features that prevent switching. These exist but do not headline.

### Real-World Example

**Notion's feature hierarchy.** Primary: flexible documents that combine text, databases, and workflows. Secondary: team collaboration, templates, sharing. Supporting: API, integrations, mobile app. Notion's marketing leads with "one tool for your team's docs, decisions, and knowledge" -- not "we have a mobile app and API." The hierarchy is clear. Compare this to Coda, which has nearly identical capabilities but presents them with equal weight. Users struggle to understand what Coda IS.

### Anti-Pattern It Prevents

**The feature soup product page.** "K1 has 100 effects, 75 palettes, 4 zones, 47 REST endpoints, 21 WebSocket commands, 16 encoders, and beat detection." This is a feature list, not a value proposition. It tells a musician nothing about why K1 matters to their performance. The features are all at the same "visual level" -- no hierarchy.

### K1 Application

K1's value hierarchy should be:
- **Primary** (why musicians buy): "Your music becomes visible. K1 listens and translates sound into light in real time." This is the centre-origin, audio-reactive, beat-synced light that no other product produces.
- **Secondary** (why musicians stay): Deep parameter control through Tab5, custom effect tuning, zone composition, preset banks. These reward investment.
- **Supporting** (why musicians do not leave): iOS app, web dashboard, API access, firmware updates. These are table stakes that prevent friction, not headline features.

Every piece of marketing, onboarding, and UI should reinforce this hierarchy. Leading with "47 REST endpoints" to a performing musician is like leading with "our database supports 14 join types" to a project manager.

---

## Principle 6: Explore Placement Exhaustively Before Committing

**UI origin:** "Sidebar" was chosen over horizontal tabs. Then "sidebar inside frame" was chosen over "sidebar outside frame." Each level was a separate decision.

### Product Strategy Equivalent

**Feature placement in the user journey has sub-decisions. Test each level.** Where a feature appears -- in onboarding, in the main flow, behind a menu, as a power-user shortcut -- is as important as whether the feature exists. And within each placement, there are sub-placements that change the experience.

"Add a quick-start tutorial" is not one decision. It is:
- When does it appear? (First launch? Every launch until completed? On demand?)
- Where does it live? (Modal overlay? Dedicated screen? Inline hints?)
- How does the user dismiss it? (Complete it? Skip it? It goes away on its own?)
- Does it come back? (If the user skips, can they find it later?)

Each sub-decision changes the experience enough to warrant explicit evaluation.

### Real-World Example

**Spotify's Discover Weekly placement.** Discover Weekly is a curated playlist. Where Spotify placed it -- as a dedicated "playlist" that appears in the user's library on Monday, not as a push notification, not as a banner, not as a separate tab -- was a deliberate sub-placement decision. It lives where playlists live, which makes it feel like the user's own music. A push notification would have felt like marketing. A separate tab would have felt like a feature demo. The sub-placement (where within "playlists," when it appears, how it refreshes) is what made it sticky.

### Anti-Pattern It Prevents

**The "just put it in settings" reflex.** When a feature does not have an obvious home, the default is to bury it in settings. This kills discoverability. The opposite failure is putting everything on the home screen, which kills focus. Neither is a considered placement -- they are defaults that avoid the sub-decision work.

### K1 Application

For K1, feature placement maps to: where does a capability live across the three control surfaces (Tab5, iOS, web)? A parameter that lives on the Tab5's persistent encoder row has fundamentally different discoverability than one buried in the iOS app's settings. Sub-decisions include: which Tab5 tab does it live on? Is it encoder-controlled or tap-to-cycle? Is it visible during performance or only during setup? Each of these changes the experience enough to warrant explicit A/B evaluation, not a default assumption.

---

## Principle 7: Generate More Options Than You Think You Need

**UI origin:** Six visual treatments were generated for the sidebar. The user explicitly said "Don't stop at 2-3, generate 5-6."

### Product Strategy Equivalent

**Test more approaches before committing to one.** The cost of exploring an additional option (concept test, paper prototype, landing page variant, pricing experiment) is almost always less than the cost of shipping the wrong approach and discovering it in production data 90 days later.

This applies to:
- **Positioning:** Test 5 value propositions, not 2. The winning message is often not the one the team would have guessed.
- **Pricing:** Test 3-4 pricing structures. Willingness-to-pay research with 2 options is barely better than guessing.
- **Onboarding:** Build 3 first-run experiences as clickable prototypes. User-test all three. One will be dramatically better.
- **Feature scope:** Sketch 4 versions at different scope levels (minimal, standard, deluxe, aspirational). Evaluate each against constraints before committing.

### Real-World Example

**Booking.com's experimentation culture.** Booking.com runs thousands of A/B tests per year -- not because they lack conviction, but because the cost of a wrong bet on a page seen by millions of users dwarfs the cost of running 5 variants for a week. They discovered that their best-performing designs were consistently NOT the ones the design team predicted. Generating more options and testing them was cheaper than trusting expert intuition.

Contrast this with a team that ships one landing page variant, watches it underperform for a quarter, then redesigns. The cost of the wrong commitment was 3 months of sub-optimal conversion. The cost of testing 5 variants upfront was one extra week of design time.

### Anti-Pattern It Prevents

**The first-idea-wins trap.** The team generates one solution, falls in love with it, and defends it against alternatives. By the time the solution ships and underperforms, the team has sunk cost that makes pivoting psychologically difficult. Generating 5 options early defuses attachment to any single one.

### K1 Application

K1 is entering a market that does not exist yet. "The world's first visual instrument" means there is no proven playbook for positioning, pricing, onboarding, or feature prioritisation. Every significant product decision should generate at least 3-5 approaches before committing. This includes: how the first-time experience works (unbox to first "wow" moment), how effects are browsed and discovered (flat list? curated sets? mood-based?), and how the Tab5/iOS/K1 relationship is explained to a buyer who has never seen a visual instrument.

---

## Principle 8: Every Visual Property Must Carry Information

**UI origin:** Colour per tab is not decoration -- it communicates "you are in a different context" without requiring the user to read the label.

### Product Strategy Equivalent

**Every product surface, feature, and communication must carry a purpose. If it does not inform, orient, or enable the user, it is noise that competes with things that do.** This applies to:

- **Onboarding steps:** Every step must deliver value or remove a blocker. "Welcome to K1!" with a logo animation is a step that carries zero information. "Connect to K1-XXXX WiFi network" carries information the user needs.
- **Dashboard widgets:** Every metric shown must be actionable or informative. A widget showing "total effects: 107" teaches nothing after the first viewing and steals space from something dynamic.
- **Notifications:** Every notification must require or enable an action. "Your firmware is up to date" is a notification with no information value -- the user's default assumption is already that their firmware is current.
- **Marketing copy:** Every sentence must advance the reader's understanding of why K1 matters to them. "Experience the future of visual performance" carries no information. "Your music becomes a light show that responds to every beat, melody, and drop -- in real time, from the centre of a glowing acrylic plate" carries information.

### Real-World Example

**Stripe's API dashboard.** Every element on Stripe's dashboard carries operational information: live vs test mode (colour-coded), last successful payment (recency signal), error rate (health signal), pending balance (financial signal). There is no decorative chrome. Compare this to early fintech dashboards that used gradient backgrounds, stock photos, and marketing taglines on the dashboard itself. Every decorative element competed with the data the user came to see.

### Anti-Pattern It Prevents

**Feature decoration.** Adding UI elements, marketing phrases, or product surfaces that look busy but carry no information. The product feels "full" without being useful. Every non-informational element raises the cognitive cost of finding the informational ones.

### K1 Application

On the Tab5 controller during a live performance, the musician has perhaps 1-2 seconds of visual attention to spare between looking at the audience. Every element on that 5" screen must communicate something in a glance: which effect is running (name), how the audio is being interpreted (gauges), which mode is active (colour). A decorative SpectraSynq logo in the header steals 40px of height from information that matters during performance. The colour-per-tab principle from the UI exercise is exactly right: colour IS information when attention is scarce.

---

## Principle 9: Use Theoretical Frameworks Exhaustively

**UI origin:** Colour harmony was resolved by testing all five colour wheel relationships (analogous, complementary, split-complementary, triadic, tetradic) rather than guessing "blue might look nice."

### Product Strategy Equivalent

**When a decision has a known analytical framework, use it completely rather than relying on intuition.** Product management has established frameworks for many common decisions:

- **Prioritisation:** RICE, ICE, Kano, MoSCoW, opportunity scoring -- pick one and apply it rigorously to the full backlog. Do not prioritise by gut feel and then retcon a framework onto the result.
- **Pricing:** Van Westendorp, Gabor-Granger, conjoint analysis -- these are established methods for determining willingness to pay. "Let's charge $199 because it feels right" is the equivalent of "blue might look nice."
- **Segmentation:** Jobs-to-be-Done, behavioural cohorts, firmographic clustering -- these identify who the product is for. "Musicians" is not a segment. "Performing electronic musicians who use hardware controllers and visual elements in live sets" is a segment.
- **Positioning:** Category design, against/for/to frameworks, competitive mapping -- these define how the product is understood in the market.

The discipline is: identify which framework applies to the decision, generate the full option set that the framework defines, evaluate each option, and choose from the complete set. Partial application of a framework is worse than not using one -- it creates false confidence in an incomplete analysis.

### Real-World Example

**Apple's pricing tiers for iPhone.** Apple does not guess at pricing. They use conjoint analysis and willingness-to-pay research to define the exact price points where each tier maximises revenue. The result -- $799, $999, $1199 -- is not arbitrary. It is the exhaustive output of a pricing framework applied to real purchase intent data. The three-tier structure itself (good, better, best) is a framework applied completely: each tier has a clear anchor feature that justifies the price delta.

### Anti-Pattern It Prevents

**Gut-feel strategy with post-hoc rationalisation.** The team decides "let's build X" based on intuition, then selectively cites data points that support the decision. The framework was never applied -- it was used as decoration on a conclusion already reached. This produces decisions that feel rigorous but are not.

### K1 Application

K1 pricing, tier structure, and launch market should be determined by framework, not feel. Van Westendorp pricing research with 50 target musicians would cost a few weeks and define the price range. Jobs-to-be-Done interviews with performing musicians would reveal whether the job is "make my set look professional," "create a unique visual identity," "give the audience something to watch during transitions," or something the team has not considered. Each job implies a different feature priority, pricing model, and marketing message. The framework reveals the answer; intuition guesses at it.

---

## Principle 10: Every Parameter Gets Exactly One Home

**UI origin:** EM Mode, Spatial, and Temporal appeared in both the mode buttons AND the EDGE/CLR tab. Three duplicates removed.

### Product Strategy Equivalent

**Every capability gets exactly one home in the product. Audit ruthlessly before adding anything.** When the same capability exists in multiple places with different UX, users face three problems:
1. **Discovery confusion:** "Where do I do this?" has multiple answers, which means it effectively has no answer.
2. **Trust erosion:** "Are these the same thing? Will changing one change the other?" Users lose confidence in the system's predictability.
3. **Maintenance burden:** Two implementations of the same capability will inevitably diverge. One gets updated; the other does not. Now users get different behaviour depending on which one they use.

This applies beyond features to: support channels (where do I get help?), documentation (where is the truth?), feedback mechanisms (where do I report bugs?), and settings (where do I configure this?).

### Real-World Example

**Google Messaging (2016-2024).** At its peak, Google maintained Allo, Duo, Hangouts, Messages, Chat, Meet, Voice, and RCS messaging. Each had overlapping capabilities with different UX. A user who wanted to send a message to a colleague could use at least four different apps, none of which interoperated cleanly. Google's own employees could not explain which app to use for which purpose. Apple, by contrast, had one: iMessage. One home for messaging. The simplicity was a product advantage.

### Anti-Pattern It Prevents

**Feature sprawl through addition without audit.** Each new feature request is evaluated in isolation ("should we build this? yes") without checking whether it duplicates something that already exists. Over time, the product accumulates redundant capabilities in different locations, and the user experience fragments.

### K1 Application

K1 has three control surfaces: Tab5, iOS app, and web dashboard. Every controllable parameter must have a clear primary home. If effect selection lives on both the Tab5 sidebar and the iOS main screen, that is fine -- but the interaction model must be consistent, the state must be synced, and neither should offer capabilities the other does not. If the Tab5 has a "Zones" tab and the iOS app has a "Zones" screen, they must show the same parameters in the same organisation. Divergence between control surfaces is the product-level equivalent of the duplicate EM Mode on the Tab5 -- it creates confusion about which surface is the source of truth.

The parameter inventory discipline from the UI exercise scales directly: before adding any parameter to any control surface, check where it already lives across all three surfaces. If it already has a home, the new location is a duplicate unless it serves a different context (e.g., setup vs performance).

---

## Principle 11: Let Constraints Kill Options

**UI origin:** "No hidden modes" killed encoder-responsive buttons. Physical encoder stacking killed certain encoder assignments. Each constraint eliminated options before detailed evaluation.

### Product Strategy Equivalent

**State all constraints before generating solutions. Let constraints eliminate options early, before emotional investment accumulates.** Product constraints include:

- **Technical constraints:** "We have 6 months" kills certain architectures. "ESP32-S3 has 2MB PSRAM" kills certain features. "K1 is AP-only" kills STA mode.
- **Market constraints:** "Our users are performing musicians" kills interaction patterns that require sustained visual attention. "Musicians perform in dark venues" kills UI designs that assume bright ambient light.
- **Business constraints:** "We need revenue within 12 months" kills free-tier-only strategies. "We are a 2-person team" kills enterprise sales motions.
- **Physical constraints:** "The LGP is 329mm of acrylic" constrains what visual effects are physically perceptible. "Encoders are stacked top/bottom" constrains parameter-to-encoder mapping.

The discipline is: list all constraints FIRST. Then generate only options that survive all constraints. An option that violates a hard constraint is dead on arrival -- do not waste time evaluating its merits.

### Real-World Example

**K1 WiFi STA mode -- 6+ failed attempts over multiple weeks.** The K1 team attempted to enable STA mode (connecting K1 to external WiFi routers) six times. Each attempt failed with AUTH_EXPIRE or AUTH_FAIL at the ESP-IDF 802.11 driver level. The constraint was real: the ESP32 WiFi stack shares encryption key pools between AP and STA, and AP operation corrupts STA auth state. The correct decision was to accept the constraint early and design an AP-only architecture. Instead, six mitigation attempts (compatibility profiles, BSSID scrubs, AP-stop-before-join, PMF optional) were tried and failed. Each attempt cost engineering time that could have been spent on features. The constraint was right. Fighting it was wrong.

This is the product equivalent of Principle 11: "K1 is AP-only" is not a limitation to work around. It is a filter that eliminates an entire class of architectural options and frees the team to optimise within the viable space.

### Anti-Pattern It Prevents

**Constraint denial.** The team generates an exciting option, discovers it violates a hard constraint, and then spends weeks trying to make the constraint go away instead of accepting it and generating options that work within it. The sunk cost of the "exciting" option creates emotional resistance to letting it die.

### K1 Application

K1's hard constraints are product-defining, not product-limiting:
- **AP-only networking** means: design for local-first, zero-internet-required operation. This is actually a feature for musicians performing at venues with unreliable WiFi.
- **Centre-origin rendering** means: every effect has a unique visual signature impossible with linear strips. This is the product's visual identity.
- **2.0ms render ceiling** means: effects must be computationally elegant. This prevents feature bloat in the effect system.
- **320 LEDs on acrylic LGP** means: the medium IS the message. Effects that look good on a screen may look wrong on diffused acrylic, and vice versa.

Each constraint eliminates a class of options and defines K1's identity. A product that tried to be all things (STA + AP, linear + centre-origin, high-compute effects + fast rendering) would be nothing.

---

## Principle 12: Match Control Precision to Parameter Useful Resolution

**UI origin:** Spread (0-60) has only 5-6 meaningful positions. Strength (0-255) is practically Off/Light/Medium/Full. Both became tap-to-cycle buttons instead of continuous encoders.

### Product Strategy Equivalent

**Match the product's configuration surface to the user's actual decision granularity.** Do not build a configuration panel for something that should be a toggle. Do not build a toggle for something that needs a configuration panel.

This manifests in:
- **Pricing tiers:** If users only meaningfully distinguish between "free," "affordable," and "premium," do not build a 7-tier pricing page with $5 increments. Three tiers match the user's decision granularity.
- **Settings depth:** If 90% of users want "auto" and 10% want fine control, the default is "auto" with a "show advanced" expansion. Do not force everyone through 12 configuration fields.
- **Onboarding:** If the user's actual decision is "music style" (electronic / acoustic / mixed), do not ask them to configure 8 individual audio parameters. Map the high-level choice to the low-level params automatically.
- **Effect browsing:** If users think in moods (energetic, ambient, aggressive, subtle) rather than effect family names (LGP Waveform Harmonic v2), the browse interface should match the user's vocabulary, not the engineer's.

### Real-World Example

**iPhone volume vs Android volume (2007-2015).** iPhone: two physical buttons, up and down. One volume. Android (early versions): a slider per channel -- media, ringtone, notification, alarm, system -- each independently adjustable. The user's actual decision granularity for volume is: louder or quieter. The iPhone matched this perfectly. Android exposed implementation detail (separate audio channels) as user-facing configuration. Later Android versions converged toward the iPhone model -- because the user's decision granularity was always "louder/quieter," not "media should be 72% and ringtone should be 45%."

### Anti-Pattern It Prevents

**Over-engineering configuration for expert self-image.** The team builds a configuration panel with 20 options because "power users want control." In practice, 95% of users either accept defaults or change 2-3 things. The 20-option panel makes the product feel complex without serving the vast majority. The 5% who want deep control are better served by a separate "advanced" path that does not burden the 95%.

### K1 Application

This principle is existentially important for K1. The product has 1,162 exposed parameters (per the k1-composer discovery). A performing musician cannot meaningfully interact with 1,162 parameters during a live set. The product's job is to map those 1,162 parameters down to the ~20 that a performer actually needs during performance, and make those 20 instantly accessible.

This is exactly what the Tab5 UI exercise did: 50+ params reduced to 8 persistent encoder gauges + 6 tap buttons + 3 contextual tabs. The useful resolution of "what a musician needs to touch during performance" is about 14-20 parameters. The remaining 1,142 are setup-time or effect-author-time parameters that belong behind a different interface (web dashboard, iOS advanced settings).

The product strategy equivalent: K1's performance mode should expose 14-20 parameters. K1's studio/setup mode should expose everything. The two modes should not be conflated. A musician on stage and a musician in their studio have different decision granularities, and the product must match both.

---

## Cross-Cutting Analysis

### Jobs-to-be-Done Alignment

Each K1 parameter has a "job" on the screen, just as each product feature has a "job" in the user's workflow. JTBD theory says: if two features are hired for the same job, one is redundant.

Applied to K1's parameter architecture:

| Job (what the user is trying to accomplish) | Parameters that serve this job | Redundancy check |
|---|---|---|
| "Make the light respond to bass" | Brightness (via RMS), band-reactive effects, bass-mapped zones | Distinct -- each serves a different aspect of the job |
| "Set the colour mood" | Colour mode, palette selection, EdgeMixer mode | Potentially redundant -- if Colour Mode and EdgeMixer Mode both determine "what colour am I seeing," they compete for the same job |
| "Match the visual energy to the musical energy" | Speed, brightness, beat-reactivity, onset sensitivity | These should be grouped -- they are all hired for the same job |
| "Make it look different from the last song" | Effect selection, zone layout, preset recall | Preset recall IS the fast path; effect selection is the slow path. Both serve the job but at different speeds. Not redundant -- complementary. |
| "Surprise the audience" | Auto-advance, random zone, transition triggers | These should be surfaced during performance, not hidden in setup |

The JTBD lens reveals: parameters should be grouped by the JOB they perform for the musician, not by the SYSTEM they belong to. "EdgeMixer settings" is a system grouping. "Colour mood controls" is a job grouping. The Tab5 UI exercise moved toward job grouping by putting related-job params in the same visual region -- the persistent mode buttons group "mood" controls together regardless of which firmware subsystem they control.

### Kano Model Mapping

The Kano model classifies features by user expectation:

| Kano Category | Definition | K1 Parameters | Implication |
|---|---|---|---|
| **Must-Be** (expected, not delighting) | Users do not notice when present, but are dissatisfied when absent | Brightness, on/off, effect selection, basic colour | These get the simplest, most reliable controls. An encoder for Brightness is correct -- it must always work, always be accessible, never require thought. |
| **One-Dimensional** (satisfaction scales linearly with quality) | More = better, less = worse | Speed, beat-reactivity sensitivity, zone count, transition smoothness | These justify fine-grained control. An encoder for Speed is correct -- the user perceives and values gradations across the full range. |
| **Attractive** (delighting, not expected) | Users are delighted when present but not dissatisfied when absent | EdgeMixer colour harmonies, auto-advance with style detection, zone composition presets | These should be highly discoverable but not occupy prime real estate. The Tab5 tap-to-cycle for EdgeMixer mode is correct -- it is instantly accessible, immediately rewarding, but does not consume a continuous encoder that One-Dimensional params need. |
| **Indifferent** (users do not care) | No impact on satisfaction either way | Gamma correction fine-tuning, temporal smoothing coefficient, spatial decay rate | These should NOT be on the performance surface. Studio/setup mode only. Exposing them during performance adds cognitive load for zero perceived value. |
| **Reverse** (presence causes dissatisfaction) | Feature actively annoys some users | Auto-rainbow cycling, aggressive auto-advance, unsolicited mode changes | K1 already follows this instinct: "no rainbows" is a hard constraint. The Kano model validates it -- rainbow cycling would be a Reverse feature for the target user (performing musicians who want visual control, not visual chaos). |

The Kano category determines where a parameter should live:
- **Must-Be:** persistent controls, always visible, always accessible
- **One-Dimensional:** encoder-controlled, fine-grained, on the primary performance surface
- **Attractive:** tap-to-cycle or discoverable shortcuts, designed for delight not daily use
- **Indifferent:** hidden behind an advanced settings gate, accessible but not prominent
- **Reverse:** do not build. If it exists, remove it.

### K1-Specific Principle Ranking

Not all 12 principles are equally critical for "the world's first visual instrument." The ranking reflects which failures would be most costly for K1 specifically.

**Tier 1 -- Existential (get these wrong and the product fails)**

1. **Principle 12: Match control to resolution.** (Rank: 1st) K1 has 1,162 parameters and a musician has 2 seconds of attention. The entire product viability depends on surfacing the right 14-20 controls during performance. Over-expose, and the instrument is unusable on stage. Under-expose, and it feels like a toy. This is the single most consequential product decision for K1.

2. **Principle 1: Diagnose the disease, not the symptom.** (Rank: 2nd) K1 is creating a new category. Every piece of user feedback will be ambiguous -- "it doesn't feel musical" could mean 5 different things. The team's ability to decompose feedback into root causes determines whether iteration improves the product or just changes it.

3. **Principle 11: Let constraints kill options.** (Rank: 3rd) K1's constraints ARE its identity. AP-only networking means local-first reliability. Centre-origin means unique visual signature. 2.0ms ceiling means computational elegance. A team that fights these constraints will build a generic LED controller. A team that embraces them will build a visual instrument.

**Tier 2 -- Strategic (get these wrong and growth stalls)**

4. **Principle 2: Research the right domain.** (Rank: 4th) Researching "LED controllers" produces the wrong product. Researching "how performing musicians control visual elements live" produces the right one. The research domain determines whether K1 is positioned against Philips Hue (wrong) or Ableton Push (right).

5. **Principle 10: One home per thing.** (Rank: 5th) Three control surfaces (Tab5, iOS, web) create constant duplication risk. Without discipline, the same parameter will appear in different places with different names and different behaviour. This will erode musician trust in the system.

6. **Principle 5: Hierarchy through differentiation.** (Rank: 6th) "100 effects" is a feature list. "Your music becomes visible" is a value proposition. K1's marketing, onboarding, and UI must make the primary value unmissable and push everything else to supporting roles.

7. **Principle 8: Every property carries information.** (Rank: 7th) On a 5" Tab5 screen during a live performance in a dark venue, every pixel must work. Decorative elements steal attention from operational ones. This principle determines whether the Tab5 is a usable instrument or a pretty distraction.

**Tier 3 -- Operational (get these wrong and execution slows)**

8. **Principle 4: Preserve what works.** (Rank: 8th) The audio pipeline, the centre-origin renderer, and the 100+ effect library are years of work that already deliver value. Product iterations must protect these.

9. **Principle 7: Generate more options.** (Rank: 9th) New category = no proven playbook. Every significant decision benefits from exploring 5 options rather than committing to the first idea.

10. **Principle 6: Test placement sub-decisions.** (Rank: 10th) Where a feature lives across Tab5/iOS/web is not one decision -- it is three decisions with interaction effects.

11. **Principle 3: Structure before visuals, rendered to evaluate.** (Rank: 11th) The Tab5 UI exercise proved this is necessary. It applies equally to marketing, onboarding, and feature design.

12. **Principle 9: Use frameworks exhaustively.** (Rank: 12th) Important for pricing, positioning, and colour harmony. Less critical than the others because frameworks are tools, not answers -- and K1's new-category status means some frameworks will not have reference data.

---

## Synthesis: The K1 Product Decision Heuristic

When facing any K1 product decision, apply this sequence:

1. **What is the actual problem?** (Principle 1) Decompose the symptom into root causes before proposing solutions.
2. **What constraints apply?** (Principle 11) List hard constraints. Kill options that violate them immediately.
3. **What does the user's useful resolution look like?** (Principle 12) How many meaningful choices does the user actually make? Match the product surface to that number.
4. **Where does this already live?** (Principle 10) Check all three control surfaces. If it has a home, do not create a second one.
5. **What domain should we research?** (Principle 2) Performing musician workflows, not LED controller markets.
6. **What is the value hierarchy?** (Principle 5) Is this primary (audio-to-light), secondary (deep control), or supporting (infrastructure)?
7. **Generate options.** (Principle 7) At least 3 for any significant decision.
8. **Evaluate placements and sub-placements.** (Principle 6) Where in the user journey, and where within that location?
9. **Does every element carry information?** (Principle 8) If not, remove it.
10. **What already works?** (Principle 4) Protect it. Fix what is broken. Do not nuke what is good.
11. **Is there a framework for this decision type?** (Principle 9) If yes, apply it completely.
12. **Render it to evaluate it.** (Principle 3) Make the strategy concrete enough for stakeholders to react.

This sequence moves from diagnosis to constraint filtering to option generation to evaluation -- the same pattern that produced the correct Tab5 parameter layout.

---

*Derived from: `tab5-encoder/docs/DESIGN_DECISION_PROCESS.md`*
*Application: SpectraSynq K1 product strategy and decision-making*

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-22 | agent:product-manager | Created -- translated 12 UI design principles to product strategy equivalents with JTBD, Kano, and K1-specific analysis |
