# Research Sources & Key Findings Index

**Complete bibliography organized by research domain**

---

## 1. Music Structure & Emotional Arcs

### Core Research

#### Tension Quantification & Modelling

**Source:** [TenseMusic: An automatic prediction model for musical tension - PLOS One](https://www.plosone.org/journal/pone/article?id=10.1371/journal.pone.0296385)

**Key Finding:**
> Tension = combined directional change of features over time. Model combines:
> - Loudness (RMS)
> - Onset frequency (rhythmic density)
> - Pitch height
> - Harmonic tension (distance from tonic)
> - Roughness/dissonance (sensory consonance level)
> - Tempo
> - Metrical structure

**Implication for LED System:**
Visual tension should mirror these drivers. When multiple drivers increase simultaneously, intensify LEDs (brightness + saturation + motion speed).

**Related Source:** [Musical tension is affected by metrical structure dynamically and hierarchically - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC11297889/)

---

#### Verse-Chorus Form & Emotional Arrival

**Source:** [MTO 28.3: Teleology in Verse–Prechorus–Chorus Form - Music Theory Online](https://www.mtosmt.org/issues/mto.22.28.3/mto.22.28.3.nobile.html)

**Key Findings:**
- Choruses signal arrival via anacrusis (upward melodic/harmonic pickup pointing to chorus downbeat)
- Non-harmonic elements (thematic, textural, lyrical) support sense of "initiation → buildup → arrival"
- Prechorus typically uses subdominant harmonies (harmonic instability leading to chorus)

**Melodic Strategy:**
- Chorus vocals sung 2-4 semitones HIGHER than verse
- Higher pitch generates more harmonics (brighter, cleaner sound)
- Longer held notes on key lyrics = more emotional impact

**Implication for LED System:**
Detect chorus arrival and shift LEDs to:
- Higher saturation (excitement)
- Potentially outward motion (expansion/release)
- Warmer hues (if chorus is in major mode)

**Related Source:** [Verse-Chorus Form – Open Music Theory](https://viva.pressbooks.pub/openmusictheory/chapter/verse-chorus-form/)

---

#### Harmonic Tension & Consonance/Dissonance

**Source:** [Cross-Modal Associations Between Harmonic Dissonance and Visual Roughness](https://journals.sagepub.com/doi/10.1177/20592043211055484)

**Key Finding:**
> Dissonant intervals increase perceived tension and activate expectation for resolution.
> Higher dissonance = higher perceived energy tension.

**Specific Research Quote:**
> "Tension increased not only with the sensory dissonance of a chord type (seventh chords are tenser than standard triads), but it was also affected by tonal hierarchies."

**Cross-Modal Insight:**
Dissonance (auditory) correlates with visual roughness/complexity. This suggests:
- Consonant = desaturated, simple colours
- Dissonant = saturated, complex colours

**Implication for LED System:**
Create a dissonance detector (MFCC-based or harmonic-function analysis) and map:
- Consonant sections → Low saturation (20-40%), muted colours
- Dissonant sections → High saturation (70-100%), vivid colours

**Related Source:** [Perception of musical tension in short chord sequences - Springer](https://link.springer.com/article/10.3758/BF03205482)

---

### Genre-Specific Structures

#### EDM: Buildup, Breakdown, and Drop

**Source:** [Buildup, Breakdown and Drop - An initial experimental approach for explaining the emotional impact of DJ performances in Electronic Dance Music (EDM) - ResearchGate](https://www.researchgate.net/publication/265596844_Buildup_Breakdown_and_Drop_An_initial_experimental_approach_for_explaining_the_emotional_impact_of_DJ_performances_in_Electronic_Dance_Music_EDM)

**Key Structure:**
```
Intro (32-64 bars)
    ↓ [minimal arrangement]
Build (16-32 bars)
    ↓ [add elements gradually: bass, melodic, hi-hats]
Drop (16-32 bars)
    ↓ [full beat/bass returns, tension released]
Breakdown (16-32 bars)
    ↓ [strip back, create contrast]
Build (repeat)
    ↓
Drop (repeat)
    ↓
Outro (32 bars)
```

**Emotional Arc:**
- Build = tension and anticipation
- Drop = ecstasy and collective euphoria
- Breakdown = catching breath, development

**Implication for LED System:**
- Build phase: Inward motion, increasing brightness/saturation, accelerating speed
- Drop: Explosive outward motion, full brightness, shift to warm/saturated colours
- Breakdown: Reduced motion, cooler hues, lower brightness

**Related Source:** [EDM Song Structure 101: Create Dynamic, Electrifying Tracks - Unison](https://unison.audio/edm-song-structure/)

---

#### Hip-Hop / R&B: Beat Switches & Vocal Intensity

**Source:** [RnB song structure - IllMuzik Community](https://www.illmuzik.com/threads/rnb-song-structure.1062/)

**Key Insights:**
- Structure less standardized than pop/EDM
- Emotion shifts via **beat switches** (dramatic production changes mid-song)
- **Vocal intensity increases during peak sections** (breaks Western convention of singing in comfortable range)
- **Key selection dominates emotional tone:** Minor keys heavily favored for somber quality

**Beat Switch Emotional Impact:**
> "Beat switches can shift from sparse, grief-stricken production to ghostly-sounding production with haunting vocal samples and light flute, creating sonic changes that give space to sit with emotion."

**Implication for LED System:**
- Detect beat switches as major narrative pivot points
- Trigger dramatic colour/motion changes at switches
- Track key (minor = cool colours, uncertain direction; major = warm colours, clear direction)
- Monitor vocal intensity as proxy for emotional peak

---

## 2. Audio Features & Emotion Recognition

### Spectral Analysis

#### Spectral Centroid (Sound Brightness)

**Source:** [Spectral Centroid - an overview | ScienceDirect Topics](https://www.sciencedirect.com/topics/engineering/spectral-centroid)

**Definition:**
> Spectral centroid = center of gravity of the spectrum. Represents the "brightness" of a sound. Higher centroid = brighter/sharper timbre.

**Correlation with Perception:**
Spectral centroid is correlated with major perceptual dimension of **timbre/sharpness**.

**Practical Ranges:**
- Sub-bass heavy (dub, hip-hop): 100-300 Hz centroid
- Balanced mix (pop, rock): 2000-4000 Hz
- Treble-rich (acoustic, strings): 4000-8000 Hz

**Implication for LED System:**
Direct mapping to hue temperature:
- 100 Hz → RED (warm)
- 2000 Hz → YELLOW (neutral)
- 8000 Hz → BLUE (cool)

---

#### Onset Strength & Rhythmic Complexity

**Source:** [The cognitive, affective, and motoric correlates of rhythmic complexity](https://www.tandfonline.com/doi/full/10.1080/09298215.2025.2453697)

**Key Finding:**
> Tension correlates with smaller rhythmic units and dense polyrhythmic structures. Rhythmic complexity measurable via:
> - Rhythmic syncopation
> - Inter-onset interval (IOI) histograms
> - Information entropy
> - Mathematical irregularity

**Implication for LED System:**
Onset density (spectral flux) directly maps to:
- Motion speed
- Perceived urgency
- Saturation intensity

High density = visual urgency, faster LEDs, more saturated colours.

---

#### RMS Energy (Loudness)

**Source:** [Audio Features — The GenAI Guidebook](https://ravinkumar.com/GenAiGuidebook/audio/audio_feature_extraction.html)

**Definition:**
Root Mean Square (RMS) Energy is measure of signal's loudness over time.

**Relationship to Emotion:**
Energy-based features frequently retained in emotion recognition systems because they provide **cues about emotional intensity and arousal**.

**Practical Use:**
Direct mapping: RMS → LED Brightness. Simplest and most intuitive audio-visual correspondence.

---

### Emotion Classification Models

#### Spotify/Thayer Model: Valence & Arousal

**Source:** [Deep Learning and Music: Mood Classification of Spotify Songs - Medium](https://mikemoschitto.medium.com/deep-learning-and-music-mood-classification-of-spotify-songs-b2dda2bf455)

**Two-Dimensional Emotion Space:**

**Valence (0.0-1.0):** "Musical positiveness"
- High valence (>0.7): Happy, euphoric, cheerful
- Low valence (<0.3): Sad, melancholic, dark

**Arousal/Energy (0.0-1.0):** "Intensity and activity"
- High arousal (>0.7): Fast, loud, noisy (energetic)
- Low arousal (<0.3): Slow, quiet, intimate

**Russell's Circumplex Model (8 Basic Emotions):**
```
                      Aroused
                        ↑
        Excited    |     Happy
          ↙        |        ↖
                   |
Afraid ←──────  Neutral ──────→ Content
                   |
          ↙        |        ↖
       Angry      |      Sad
                        ↓
                   Unaroused
```

**Implication for LED System:**
- High valence + high arousal (Excited/Happy): Yellow/warm, high saturation, fast motion, bright
- High valence + low arousal (Content/Calm): Yellow-green, moderate saturation, slow motion
- Low valence + low arousal (Sad/Depressed): Blue, desaturated, minimal motion
- Low valence + high arousal (Angry/Afraid): Red, highly saturated, aggressive motion

---

#### Feature Selection for Emotion Recognition

**Source:** [Music Emotion Recognition with Machine Learning Based on Audio Features](https://www.researchgate.net/publication/355832899_Music_Emotion_Recognition_with_Machine_Learning_Based_on_Audio_Features)

**Best Features (ranked by importance):**
1. **RMS Energy** (arousal indicator)
2. **Spectral Centroid** (brightness/valence indicator)
3. **Chromagram** (harmonic content/mode)
4. **MFCC coefficients** (timbre/instrument identity)
5. **Spectral contrast** (texture/dynamic range)
6. **Zero crossing rate** (noise/articulation)

**Implication for LED System:**
Focus on top 3 features:
- RMS → Brightness
- Spectral Centroid → Hue
- Chromagram → Saturation (if major mode = saturate; minor mode = desaturate)

---

## 3. Colour Psychology & Cross-Modal Perception

### Universal Colour-Music Associations

**Source:** [Color, Music, and Emotion: Bach to the Blues - Sage Journals](https://journals.sagepub.com/doi/10.1177/2041669518808535)

**Key Finding:**
> "Faster music in the major mode produced color choices that were more saturated, lighter, and yellower, whereas slower, minor music produced the opposite pattern (less saturated, darker, more bluish)."

**Cross-Modal Mechanism:**
Emotion mediates music-to-colour mappings. The emotional content of music (not just acoustic features) drives colour choice.

**Colour Emotion Associations (Cross-Cultural Robust):**

| Colour | Emotion | Audio Correlate |
|--------|---------|-----------------|
| Red | Energy, passion, urgency, anger | High energy + (low valence OR high valence) |
| Orange | Warmth, creativity, playfulness | Moderate-high energy, warm timbre |
| Yellow | Happy, optimistic, cheerful | High valence + bright timbre (major mode) |
| Green | Calm, balanced, natural | Moderate valence + stable harmonies |
| Blue | Calm, sad, cool, melancholic | Low valence OR (calm + ethereal) |
| Purple | Mysterious, dramatic | Harmonic complexity, dissonance, minor mode |

**Related Source:** [Colour Association with Music Is Mediated by Emotion - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC4671663/)

---

### Cross-Cultural Universals vs. Specifics

**Source:** [Cross-Cultural Biases of Emotion Perception in Music - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC12110013/)

**Universal Cues (Work Across All Cultures):**
- Fast tempo = arousal/excitement
- Slow tempo = calm/melancholy
- Loud = intensity
- High pitch = brightness/lift
- Low pitch = gravity/heaviness
- Consonance = stability/resolution
- Dissonance = tension/unease

**Culture-Specific Cues (Vary by Background):**
- Major mode = happy (Western training; not universal)
- Minor mode = sad (Western training; not universal)
- Key changes = emotional significance (Western; varies elsewhere)
- Harmonic function = emotional meaning (Western-specific system)

**Implication for LED System:**
- HARD-CODE universal mappings (tempo → speed, loudness → brightness, pitch → hue)
- CONFIGURABLE colour palettes for mode/key interpretation
- Provide "Western preset" and "alternative/neutral preset" options

**Related Source:** [Harmonic organisation conveys both universal and culture-specific cues for emotional expression in music - PLOS One](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0244964)

---

### Synesthetic Mapping (Advanced)

**Source:** [musicolors: Bridging Sound and Visuals For Synesthetic Creative Musical Experience](https://arxiv.org/html/2503.14220v1)

**Key Concept:**
Synesthesia (cross-sensory perception) can be designed/learned. Professional visualization tools (Ableton, Geisterwelt) train this mapping.

**Implementation in Ableton Live:**
**Source:** [Geisterwelt: Spectral Synesthesia with Max for Live - Ableton](https://www.ableton.com/en/blog/geisterwelt-max-live/)

> "Geisterwelt is a hybrid sampler/visualizer that lets you manipulate spectral parameters and visualize sounds as geometric shapes. Color scheme, style, and geometry react in real-time to playing, or can be manipulated individually via MIDI."

**Pattern Mapping Example:**
- Low frequencies = larger geometric shapes, warm colours, bottom of screen
- High frequencies = smaller geometric shapes, cool colours, top of screen
- Dissonance = rough/irregular shapes
- Consonance = smooth/regular shapes

---

## 4. Psychoacoustics & Spatial Perception

### Stereo Width & Spatial Spread

**Source:** [Creating a Wide and Immersive Mix: Advanced Stereo Imaging and Spatial - Carvin Audio](https://carvinaudio.com/blogs/audio-education/creating-a-wide-and-immersive-mix-advanced-stereo-imaging-and-spatial-effects)

**Key Concept:**
> Stereo field = perceived placement of sound sources across L-R spectrum. Width = perceived distance between sounds in left and right speakers.

**Measurement:**
Phase correlation between L/R channels:
- -1.0 (completely out of phase) = maximum width
- 0.0 (uncorrelated) = medium width
- +1.0 (perfectly in-phase, mono) = minimum width

**Perceptual Implication:**
Mono signals feel "centered" and "focused."
Stereo signals feel "spacious" and "expansive."

**Implication for LED System:**
Map stereo width to LED spatial distribution:
- Mono → Concentrate LEDs in center zone
- Stereo → Spread LEDs across full strip

---

### Sub-Bass & Infrasound

**Source:** [Infrasound, human health, and adaptation: an integrative overview - Springer Nature](https://link.springer.com/article/10.1007/s11069-013-0827-3)

**Definition:**
Infrasound = frequencies <20 Hz. Sub-bass = 20-60 Hz.

**Psychological Effects:**
- **Low to moderate levels:** Pleasure, relaxation, dopamine release
- **High levels:** Anxiety, unease, fear, foreboding (can cause nausea)
- **Physical perception:** Felt as vibration in chest/abdomen rather than heard

**Research Quote (Famous Experiment):**
> "In a well-known experiment ('Soundless Music'), an infrasound generator was incorporated into a live concert mix. Many people reported feelings of anxiety and foreboding, along with cold and tingling sensations."

**Implication for LED System:**
- Sub-bass heavy sections should darken or shift to "grounded" visual weight
- Emphasize lower LED zones when sub-bass energy is high
- Use red/orange (warm) hues to represent physical "gravity" and presence

**Related Source:** [The Sub-Bass Conundrum: Finding the Perfect Frequency - TheTechyLife](https://thetechylife.com/what-frequency-should-sub-bass-be/)

---

### Motion Perception & Speed

**Source:** [The speed and temporal frequency of visual apparent motion modulate auditory duration perception - Scientific Reports](https://www.nature.com/articles/s41598-023-38183-w)

**Key Finding:**
> Visual motion speed is encoded by neural activity measurable as MEG responses. Motion expands perceived time duration; this effect is function of speed rather than temporal frequency.

**Practical Meaning:**
Fast-moving LEDs feel more exciting and urgent.
Slow-moving LEDs feel calm and meditative.
Stationary LEDs feel static/ambient.

**Perceptual Thresholds:**
- <0.2 Hz (once per 5 seconds) = nearly imperceptible
- 0.5-2.0 Hz = natural, comfortable motion
- 3+ Hz = frantic, unnatural

**Implication for LED System:**
Motion speed should range 0.5-2.5 Hz for perceptual comfort.
Onset density (rhythm) naturally scales motion speed (faster rhythms = faster LEDs).

---

## 5. Concert Lighting & Immersive Design

### Professional Lighting Design Principles

**Source:** [How Stage Lighting Impacts Audience Emotion and Engagement - Innovation Lighting](https://www.innovationlighting.net/how-stage-lighting-impacts-audience-emotion-and-engagement/)

**Key Principle:**
> Lighting complements music's rhythm using different colors and beam shapes to amplify emotional and energetic impact. When lights pulse in time with beat, they create multi-sensory experience where sound and visuals enhance emotional impact.

**Tension & Release:**
> "When spotlight timing is late, tension breaks. The slow return to full stage lighting after an intense moment offers emotional release, like an exhale after holding your breath. Lighting cues can build anticipation, underscore tension, or celebrate a crescendo."

**Colour as Visual Metaphor:**
- Warm yellow = intimacy, coziness, home
- Cool blue/green = mystery, tension, unknown
- Juxtaposition of warm + cool = visual drama and tension

**Implication for LED System:**
- Use brightness modulation to reinforce musical rhythm
- Use colour temperature shifts to signal emotional transitions
- Use dimming/restoration cycles to mirror tension/release patterns

---

### Immersive Art Installation Design

**Source:** [The world's top immersive art experiences - blooloop](https://www.blooloop.com/technology/in-depth/immersive-art-experiences/)

**teamLab & Meow Wolf Approaches:**

**teamLab Vision:**
> "A collective of artists, programmers, engineers, CG animators, mathematicians, and architects investigating the confluence of art, science, technology, and the natural world. Goal is to trigger synesthesia experience with full sensory engagement."

**Meow Wolf Focus:**
> "Immersive and interactive experiences that take audiences into fantastic realms and stories that engage and include participation."

**Key Techniques:**
- Light ripples and patterns that change in real-time
- Colour fields that evoke emotional states
- Spatial orchestration: light guides attention through space
- Synesthesia design: simultaneous visual, auditory, proprioceptive responses

**Implication for LED System:**
Think of your LED strip as an immersive canvas for emotional storytelling, not just audio visualization.
- Create visual "journeys" from intro to outro
- Use darkness/contrast strategically
- Guide viewer attention through spatial patterns
- Aim for emotional resonance, not just technical accuracy

---

### Disney Imagineering: Emotional Arc Design

**Source:** [Disney Imagineers and Theme Park Designers Are Leading Experience Marketing - Skyword](https://www.skyword.com/contentstandard/disney-imagineers-and-theme-park-designers-are-leading-experience-marketing-into-a-whole-new-world-of-brand-storytelling/)

**Core Principle:**
> "All five senses are being manipulated to convince you that you are part of a living narrative. Every moment is part of a bigger narrative, making visitors feel like they're part of the magic."

**Emotional Investment Strategy:**
- Establish strong narrative structures
- Guide audiences through intentional emotional arcs
- Use multi-sensory reinforcement (sound, smell, architecture, light)
- Enable guest participation in shaping the narrative

**Implication for LED System:**
- Pre-plan emotional journey of each song/playlist
- Use audio-visual mapping to reinforce narrative intent
- Create distinctive "moments" (choruses, drops) that stand out
- Allow users to participate (configurability, personalization)

---

## 6. Music Information Retrieval (MIR)

### Beat Tracking & Temporal Structure

**Source:** [Segment Transformer: AI-Generated Music Detection via Music Structural Analysis](https://arxiv.org/html/2509.08283v1)

**Key Insight:**
> "A unified model jointly performs beat tracking, downbeat tracking, and functional structure segmentation/labeling, with ablation studies showing that concurrent learning of beats, downbeats, and segments enhances overall performance."

**Implication for LED System:**
- Use beat tracking to synchronize motion to pulse
- Use downbeat tracking to align visual transitions to bar boundaries
- Use segment detection to identify verse/chorus/bridge regions
- Combine signals for robust structure understanding

**Related Source:** [Beat Tracking - CMU](https://www.cs.cmu.edu/~rbd/bib-beattrack.html)

---

### Feature Extraction Frameworks

**Source:** [Music Feature Extraction - Librosa Documentation](https://librosa.org/)

**Available Features:**
- Chromagram (12 pitch classes)
- MFCC (Mel-frequency cepstral coefficients)
- Spectral contrast
- Tempogram (beat strength over time)
- Harmonic-percussive separation
- Onset detection
- Constant-Q transform

**Implication for LED System:**
- Use librosa for offline analysis (research/development)
- Use simplified subset for real-time extraction (FFT + RMS + onset)
- Cache pre-computed features for playlists

---

## 7. Key Synthesis: Practical Implications

### Summary of Research-Backed Mappings

**Evidence-Based Correlations:**

1. **RMS Energy ↔ Brightness:** ✓ Direct, universal, validated
2. **Spectral Centroid ↔ Hue Temperature:** ✓ Cross-culturally robust
3. **Harmonic Dissonance ↔ Saturation:** ✓ Experimental support
4. **Onset Density ↔ Motion Speed:** ✓ Perceptual psychology
5. **Valence ↔ Warm-Cool Hue Shift:** ✓ Multiple studies
6. **Arousal ↔ Saturation & Brightness:** ✓ Emotion recognition research
7. **Sub-Bass Power ↔ Visual Weight:** ✓ Infrasound research
8. **Stereo Width ↔ Spatial Spread:** ✓ Psychoacoustics
9. **Song Phase ↔ Movement Direction:** Theoretical (needs validation)

### Universal Principles (Hard-Code)

- Faster = more energetic visuals
- Louder = brighter visuals
- Higher pitch = cooler hues
- Lower pitch = warmer hues
- Consonant = stable, desaturated
- Dissonant = tense, saturated

### Culture-Specific (Make Configurable)

- Major mode interpretation (happy in West; varies elsewhere)
- Key change significance (structural in Western music; less so elsewhere)
- Rhythm syncopation interpretation

---

## 8. Research Gaps & Future Work

### Areas Needing More Research

1. **Long-term temporal dynamics:**
   Most studies focus on sub-second features. How should LED choreography respond to structure over 1-5 minute timescales?

2. **Narrative-aware choreography:**
   Limited research on how to intentionally design emotional visual arcs that amplify story.

3. **Real-time harmonic function detection:**
   Easy offline (harmonic analysis); hard real-time on ESP32.

4. **Cross-genre generalization:**
   Do mappings that work for pop work for classical, ambient, metal?

5. **Personalization & learning:**
   How should system adapt based on user preference feedback?

6. **Physical embodiment:**
   How does viewer's body (movement, proximity) enhance emotional response?

---

## 9. Reference Implementation Priorities

### Must-Have (Evidence-Strong)

1. RMS → Brightness
2. Spectral Centroid → Hue
3. Onset Density → Motion Speed
4. Basic smoothing (avoid flicker)
5. Perceptual range enforcement (10-100% brightness, etc.)

### Should-Have (Good Evidence)

1. Harmonic complexity → Saturation
2. Sub-bass → Visual weight
3. Stereo width → Spatial spread
4. Brightness-saturation coupling
5. Song phase detection

### Nice-to-Have (Theoretical/Experimental)

1. Movement direction (inward/outward)
2. Key/mode detection → Hue adjustment
3. Emotion classification (valence/arousal curve)
4. User preset system
5. Real-time vs. pre-computed hybrid mode

---

## Complete Source List (Alphabetical)

1. [Ableton Geisterwelt](https://www.ableton.com/en/blog/geisterwelt-max-live/)
2. [ARM CMSIS-DSP](https://arm-software.github.io/CMSIS_5/DSP/html/)
3. [Audio Features — GenAI Guidebook](https://ravinkumar.com/GenAiGuidebook/audio/audio_feature_extraction.html)
4. [Buildup, Breakdown and Drop - ResearchGate](https://www.researchgate.net/publication/265596844_Buildup_Breakdown_and_Drop_An_initial_experimental_approach_for_explaining_the_emotional_impact_of_DJ_performances_in_Electronic_Dance_Music_EDM)
5. [Carvin Audio: Stereo Width](https://carvinaudio.com/blogs/audio-education/creating-a-wide-and-immersive-mix-advanced-stereo-imaging-and-spatial-effects)
6. [CMU Beat Tracking Bibliography](https://www.cs.cmu.edu/~rbd/bib-beattrack.html)
7. [Color, Music, and Emotion: Bach to the Blues - Sage](https://journals.sagepub.com/doi/10.1177/2041669518808535)
8. [Colour Association with Music - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC4671663/)
9. [Cross-Cultural Biases - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC12110013/)
10. [Cross-Modal Associations: Dissonance & Visual Roughness](https://journals.sagepub.com/doi/10.1177/20592043211055484)
11. [Deep Learning: Mood Classification - Medium](https://mikemoschitto.medium.com/deep-learning-and-music-mood-classification-of-spotify-songs-b2dda2bf455)
12. [Disney Imagineering - Skyword](https://www.skyword.com/contentstandard/disney-imagineers-and-theme-park-designers-are-leading-experience-marketing-into-a-whole-new-world-of-brand-storytelling/)
13. [EDM Song Structure 101 - Unison](https://unison.audio/edm-song-structure/)
14. [Harmonic Organisation - PLOS One](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0244964)
15. [Hip-Hop Beat Switches - Complex](https://www.complex.com/music/a/dimassanfiorenzo/best-hip-hop-beat-switches-of-all-time)
16. [Infrasound: Health & Adaptation - Springer](https://link.springer.com/article/10.1007/s11069-013-0827-3)
17. [Innovation Lighting: Stage Impact](https://www.innovationlighting.net/how-stage-lighting-impacts-audience-emotion-and-engagement/)
18. [Librosa: Music Information Retrieval](https://librosa.org/)
19. [MTO 28.3: Verse-Chorus Teleology](https://www.mtosmt.org/issues/mto.22.28.3/mto.22.28.3.nobile.html)
20. [Music Emotion Recognition - ResearchGate](https://www.researchgate.net/publication/355832899_Music_Emotion_Recognition_with_Machine_Learning_Based_on_Audio_Features)
21. [Music Mood Prediction - ResearchGate](https://www.researchgate.net/publication/370450676_Music_Mood_Prediction_Based_on_Spotify's_Audio_Features_Using_Logistic_Regression)
22. [Musical Tension (TenseMusic) - PLOS One](https://www.plosone.org/journal/pone/article?id=10.1371/journal.pone.0296385)
23. [Musical Tension: Metrical Structure - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC11297889/)
24. [musicolors: Synesthetic Experience](https://arxiv.org/html/2503.14220v1)
25. [Perception of Tension in Chords - Springer](https://link.springer.com/article/10.3758/BF03205482)
26. [Rhythmic Complexity Research](https://www.tandfonline.com/doi/full/10.1080/09298215.2025.2453697)
27. [R&B Song Structure - IllMuzik](https://www.illmuzik.com/threads/rnb-song-structure.1062/)
28. [Segment Transformer - arXiv](https://arxiv.org/html/2509.08283v1)
29. [Spectral Centroid - ScienceDirect](https://www.sciencedirect.com/topics/engineering/spectral-centroid)
30. [Speed of Visual Motion - Scientific Reports](https://www.nature.com/articles/s41598-023-38183-w)
31. [Sub-Bass Frequency - TheTechyLife](https://thetechylife.com/what-frequency-should-sub-bass-be/)
32. [Tension in Chord Sequences - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC9117266/)
33. [The World's Top Immersive Art - blooloop](https://www.blooloop.com/technology/in-depth/immersive-art-experiences/)
34. [Verse-Chorus Form - Open Music Theory](https://viva.pressbooks.pub/openmusictheory/chapter/verse-chorus-form/)
35. [Visual Motion Perception - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC6872091/)

---

## Conclusion

This research provides a **scientific foundation** for mapping music's emotional journey to visual choreography. The mappings are grounded in:

- **Music theory** (tension/release, harmonic function)
- **Psychoacoustics** (spectral perception, spatial hearing)
- **Affective neuroscience** (emotion models, cross-modal perception)
- **Colour psychology** (universal and cultural associations)
- **Concert lighting design** (professional practice)
- **Immersive art installation** (experiential design)

**Core Principle:** Music creates emotional arcs. LED systems should make those arcs **visible and felt**, transforming music from an auditory experience into a multi-sensory emotional journey.

