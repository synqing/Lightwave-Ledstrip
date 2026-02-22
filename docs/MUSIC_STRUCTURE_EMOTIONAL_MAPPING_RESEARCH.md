# Music Structure, Emotional Arcs, and Visual Storytelling Research

**Research Synthesis for LED Choreography System**

---

## Executive Summary

This document provides research-backed parameter mappings between music structure, emotional arcs, and visual choreography for LED systems. The research spans music theory, psychoacoustics, colour psychology, immersive art design, and cross-cultural perception studies to create a scientifically grounded framework for making musical journeys VISIBLE.

**Key Principle:** Music creates emotional journeys through tension and release cycles. Visual systems should make these cycles visible by mapping measurable audio features to intuitive visual parameters (brightness, speed, colour temperature, spatial density, movement direction).

---

## Part 1: Music Structure and Emotional Arcs

### 1.1 How Musical Tension is Quantified

Research from the "TenseMusic" automatic prediction model identifies these measurable components of tension:

**Core Tension Drivers (hierarchically weighted):**
1. **Loudness (RMS Energy)** - Peak dynamic intensity
2. **Onset Frequency** - Density of rhythmic events
3. **Pitch Height** - Vertical melodic position
4. **Harmonic Tension (tonal distance from tonic)**
5. **Roughness/Dissonance** - Sensory consonance level
6. **Tempo** - Overall pace and rhythmic rate
7. **Metrical Structure** - How rhythm relates to underlying pulse

**Model Definition:** Tension = combined directional change of features over time, with slope computed over an "attentional window" (typically 2-4 seconds) and integrated with a "memory window" capturing preceding context.

*Sources:*
- [TenseMusic: An automatic prediction model for musical tension - PLOS One](https://www.plosone.org/journal/pone/article?id=10.1371/journal.pone.0296385)
- [Musical tension is affected by metrical structure dynamically and hierarchically](https://pmc.ncbi.nlm.nih.gov/articles/PMC11297889/)

---

### 1.2 The Anatomy of "Arrival" in Music: Chorus vs. Verse

Choruses signal emotional climax through multiple coordinated shifts:

**Melodic Strategies:**
- Vocals sung 2-4 semitones HIGHER than verse (creates brightness, increases harmonic content)
- Anacrusis (upward melodic pickup) pointing to chorus downbeat
- Longer held notes on emotionally significant lyrics
- Reduced melodic fragmentation (simpler, catchier shapes)

**Harmonic Strategies:**
- Pre-chorus uses subdominant harmonies (building tension toward chorus)
- Chorus often shifts to brighter harmonic areas (major vs. minor shift, or shift away from tonic)
- Harmonic rhythm slows (longer chord durations) for emphasis

**Rhythmic & Textural Strategies:**
- Fuller arrangement (added instruments/vocal harmonies)
- Increased percussion density or new drum patterns
- Thematic elements change to signal narrative "payoff"

**Lyrical Strategy:**
- Song title typically lands in chorus
- Chorus lyrics are emotive, simplified vs. descriptive verse lyrics
- Maximum emotional resonance concentrated in chorus

*Sources:*
- [MTO 28.3: Teleology in Verse–Prechorus–Chorus Form](https://www.mtosmt.org/issues/mto.22.28.3/mto.22.28.3.nobile.html)
- [Verse-Chorus Form – OPEN MUSIC THEORY](https://viva.pressbooks.pub/openmusictheory/chapter/verse-chorus-form/)
- [Tension experience induced by tonal and melodic shift at music phrase boundaries - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC9117266/)

---

### 1.3 Buildups: How to Create Anticipation

A buildup is the primary tension-creation tool in all music genres. It works via:

**Progressive Element Addition:**
- Start with stripped-down, reduced arrangement (often just kick drum)
- Add elements gradually: bass, melodic fragment, hi-hats, fills
- Each addition increases perceived density and energy

**Harmonic Progression Toward Dominant:**
- Move toward harmonic instability (distance from tonic increases)
- Seventh chords, tritones, and unresolved tensions activate anticipation
- Repetition of unstable harmonic motion creates forward momentum

**Rising Pitch/Brightness:**
- Spectral centroid gradually increases (higher frequencies added)
- Riser synths sweep upward (both literally and energetically)
- Filter sweeps from dark to bright

**Rhythmic Intensification:**
- Onset density increases (more notes, hits, fills per measure)
- Syncopation increases (predictability decreases)
- Drum rolls or rapid hi-hat patterns at peak

**Temporal Stretching:**
- Buildup typically lasts 16-32 bars (builds pressure over time)
- Listeners develop expectation for resolution

**The Cathartic Release (The Drop):**
- Sudden harmonic resolution (return to tonic or home key)
- Explosive bass/beat re-entry
- Full arrangement suddenly deployed
- Listeners experience ecstasy/collective joy response

*Sources:*
- [Buildup, Breakdown and Drop - ResearchGate](https://www.researchgate.net/publication/265596844_Buildup_Breakdown_and_Drop_An_initial_experimental_approach_for_explaining_the_emotional_impact_of_DJ_performances_in_Electronic_Dance_Music_EDM)
- [EDM Song Structure 101: Create Dynamic, Electrifying Tracks](https://unison.audio/edm-song-structure/)

---

### 1.4 Genre-Specific Structures

#### Popular Music (Verse-Chorus-Bridge)
- **Verse:** Setup (low energy, narrative detail)
- **Pre-Chorus:** Momentum building toward payoff
- **Chorus:** Peak (emotional, catchy, memorable)
- **Bridge:** Contrast and development (often key change, instrumental break, or perspective shift)
- **Final Chorus:** Often repetition with added intensity

#### EDM (Intro → Build → Drop → Breakdown → Build → Drop → Outro)
- **Intro (32-64 bars):** Establish groove, minimal arrangement
- **Build (16-32 bars):** Tension via element addition
- **Drop (16-32 bars):** Full release, beat re-enters
- **Breakdown (16-32 bars):** Strip back for contrast, drop tension
- **Second Build/Drop:** Repeat cycle with variations
- **Outro (32 bars):** Gradual wind-down

#### Hip-Hop / R&B (Beat Switch & Vocal Intensity)
- **Structure:** Less standardized; uses beat switches as emotional pivot points
- **Beat Switch:** Dramatic production change mid-song (different drum pattern, instruments, key, tempo)
- **Vocal Strategy:** Singers often START at higher pitches than they would in verse, breaking Western expectations
- **Key Selection:** Minor keys dominate (darker, introspective quality)
- **Emotion Markers:** Vocal intensity, beat aggression, lyrical depth shift to mark emotional sections

*Sources:*
- [The 25 Best Hip-Hop Beat Switches of All Time - Complex](https://www.complex.com/music/a/dimassanfiorenzo/best-hip-hop-beat-switches-of-all-time)
- [RnB song structure - IllMuzik Community](https://www.illmuzik.com/threads/rnb-song-structure.1062/)
- [How do u approach "producing" R&B? - Gearspace](https://gearspace.com/board/rap-hip-hop-engineering-and-production/603409-how-do-u-approach-quot-producing-quot-r-amp-b.html)

---

## Part 2: Audio Features to Measurable Parameters

### 2.1 Tension-Driving Audio Features

These features are measurable in real-time via FFT, onset detection, and pitch tracking:

| Feature | Metric | Real-Time Extraction | Emotional Association |
|---------|--------|---------------------|----------------------|
| **RMS Energy** | 0.0-1.0 dB scale | Frame-by-frame power | Loudness, intensity, arousal |
| **Spectral Centroid** | Hz (typically 0-8000 Hz) | FFT center of gravity | Brightness, sharpness, "treble" |
| **Onset Strength** | Events per bar | Peak detection on spectral flux | Rhythmic density, urgency |
| **Pitch Height** (mean) | MIDI note (36-96) | Pitch tracking output | Emotional altitude, lift |
| **Harmonic Complexity** | Consonance score (0-1) | Dissonance analysis | Tension level, unresolved energy |
| **Zero Crossing Rate (ZCR)** | Normalized | Short-term feature | Spectral profile, texture |
| **MFCC coefficients** | 13-dimensional vector | Mel-frequency cepstral analysis | Timbral texture, instrument richness |
| **Chromagram** | 12-dimensional (pitch classes) | Constant-Q transform | Harmonic content, tonal clarity |
| **Sub-Bass Energy** | 0-60 Hz power | Isolated frequency band | Physical presence, weight, gravity |
| **Stereo Width** | Phase correlation (-1 to 1) | Cross-correlation of L/R channels | Spatial expansiveness |

*Sources:*
- [Audio Features — The GenAI Guidebook](https://ravinkumar.com/GenAiGuidebook/audio/audio_feature_extraction.html)
- [Music Emotion Recognition with Machine Learning Based on Audio Features](https://www.researchgate.net/publication/355832899_Music_Emotion_Recognition_with_Machine_Learning_Based_on_Audio_Features)
- [Spectral Centroid - an overview | ScienceDirect Topics](https://www.sciencedirect.com/topics/engineering/spectral-centroid)

---

### 2.2 Spotify's Emotion Framework: Valence & Arousal

Spotify's audio analysis provides two key dimensions used across streaming platforms:

**Valence (0.0-1.0):** "Musical positiveness"
- High valence (>0.7): Happy, euphoric, cheerful, energetic
- Low valence (<0.3): Sad, depressed, melancholic, dark

**Arousal (Energy) (0.0-1.0):** "Intensity and activity"
- High energy (>0.7): Fast, loud, noisy (rock, EDM, electronic)
- Low energy (<0.3): Slow, quiet, acoustic, intimate

**Russell's Circumplex Model (Industry Standard):**
Emotions plotted on 2D grid (Valence × Arousal) producing 8 basic emotion categories:
- **Excited:** High arousal, high valence
- **Happy:** Moderate arousal, high valence
- **Content:** Low arousal, high valence
- **Calm:** Low arousal, moderate valence
- **Depressed:** Low arousal, low valence
- **Sad:** Moderate arousal, low valence
- **Afraid:** High arousal, low-moderate valence
- **Angry:** High arousal, low valence

*Sources:*
- [Deep Learning and Music: Mood Classification of Spotify Songs - Medium](https://mikemoschitto.medium.com/deep-learning-and-music-mood-classification-of-spotify-songs-b2dda2bf455)
- [Music Mood Prediction Based on Spotify's Audio Features Using Logistic Regression](https://www.researchgate.net/publication/370450676_Music_Mood_Prediction_Based_on_Spotify's_Audio_Features_Using_Logistic_Regression)
- [Emotion Color, Music, and Emotion: Bach to the Blues - PMC](https://journals.sagepub.com/doi/10.1177/2041669518808535)

---

## Part 3: Visual-Audio Parameter Mappings

### 3.1 Recommended Parameter Mappings (Research-Backed)

These mappings are based on cross-modal correspondence research and professional concert lighting design:

#### **Brightness / LED Intensity ↔ RMS Energy (Loudness)**

**Mapping:**
```
LED_Brightness = RMS_Energy * 255  [linear or gamma-corrected]
OR
LED_Brightness = normalize(RMS_Energy, lowpass_smoothed) with 1-2s lag
```

**Rationale:**
- RMS energy (loudness) is the most direct and intuitive audio → visual mapping
- Brighter = louder is universally understood
- Natural response in concert/club lighting: brighten during crescendos

**Configuration:**
- **Minimum brightness:** 10-20% (never go to 0%, maintain visibility)
- **Maximum brightness:** 100% at peak loudness
- **Smoothing:** Low-pass filter with 500-1000ms time constant to avoid jitter
- **Gamma correction:** Apply gamma 2.2 for perceptually linear brightness

*Research:*
- [How Stage Lighting Impacts Audience Emotion and Engagement](https://www.innovationlighting.net/how-stage-lighting-impacts-audience-emotion-and-engagement/)

---

#### **Colour Temperature (Hue) ↔ Spectral Centroid (Brightness)**

**Mapping:**
```
Hue = map_range(
  Spectral_Centroid,
  min=100Hz (dark red/warm),
  mid=2000Hz (yellow/neutral),
  max=8000Hz (blue/cool)
)
```

**Specific Colour Scheme:**
- **Red (0-60°):** Sub-bass dominant, warm, sensual (0-500 Hz centroid)
- **Orange (30-90°):** Warm bass, presence peak (500-1500 Hz)
- **Yellow (45-120°):** Bright, presence-rich (1500-3000 Hz)
- **Green (90-180°):** Upper midrange, clarity (3000-5000 Hz)
- **Cyan (180-240°):** Bright, airy, ethereal (5000-6500 Hz)
- **Blue (240-270°):** Very bright, treble-rich (6500-8000 Hz)
- **Purple (270-300°):** Extreme brilliance, air/presence (8000+ Hz)

**Rationale:**
- Research shows spectral centroid ("brightness" of sound) naturally maps to colour temperature
- Warm hues (red, orange, yellow) correspond to low-frequency dominated sounds
- Cool hues (blue, cyan) correspond to high-frequency brightness
- This is cross-culturally robust: major-mode + bright timbre = yellower choices

**Configuration:**
- **Smoothing:** 1-2s low-pass filter to avoid jitter
- **Saturation:** Increase saturation at high spectral centroid extremes (very bright = vivid colour)
- **Hysteresis:** Prevent rapid colour flicker during transients

*Research:*
- [Colour Association with Music Is Mediated by Emotion - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC4671663/)
- [Color, Music, and Emotion: Bach to the Blues - Sage Journals](https://journals.sagepub.com/doi/10.1177/2041669518808535)
- [musicolors: Bridging Sound and Visuals For Synesthetic Creative Musical Experience](https://arxiv.org/html/2503.14220v1)

---

#### **Motion Speed (LED Chase Speed) ↔ Onset Density (Rhythm Complexity)**

**Mapping:**
```
Motion_Speed = Onset_Density * Speed_Scale_Factor [typically 0.5-2.0]
OR
Motion_Speed = clamp(Beats_Per_Minute / 120, 0.2, 3.0)  [normalize to BPM]
```

**Specific Interpretation:**
- **Static or very slow:** Sparse rhythm, ballad-like (0.5 events/sec or lower)
- **Slow pulse (60-90 BPM):** Verse-like, contemplative
- **Medium tempo (90-120 BPM):** Groove-oriented, steady energy
- **Fast (120-150 BPM):** Build-up phase, increasing tension
- **Very fast (150+ BPM, syncopated):** Peak excitement, drop imminent

**Configuration:**
- **Onset detection:** Use spectral flux-based onset strength envelope
- **Smoothing:** 1-2s median filter (to reject spurious onsets while tracking real density)
- **Movement patterns:**
  - Slow speeds: Fade in/out, gradient sweep
  - Fast speeds: Chasing LEDs, rapid colour transitions, rotation

*Research:*
- [The cognitive, affective, and motoric correlates of rhythmic complexity](https://www.tandfonline.com/doi/full/10.1080/09298215.2025.2453697)
- [Perceived duration of visual motion increases with speed - JOV ARVO](https://jov.arvojournals.org/article.aspx?articleid=2122203)
- [The speed and temporal frequency of visual apparent motion modulate auditory duration perception](https://www.nature.com/articles/s41598-023-38183-w)

---

#### **Saturation (Colour Vibrancy) ↔ Harmonic Complexity / Dissonance**

**Mapping:**
```
Saturation = map_harmonic_tension(
  Consonance_Score,
  min_sat=20% (highly consonant, simple harmonies),
  max_sat=100% (highly dissonant, complex/unstable)
)
```

**Interpretation:**
- **Low saturation (20-40%):** Consonant, resolved, calm harmonies (major triads, perfect intervals)
- **Medium saturation (40-70%):** Mixed consonance (seventh chords, some dissonance)
- **High saturation (70-100%):** High tension, dissonant intervals (tritones, seconds, sevenths unresolved)

**Rationale:**
- Consonance = visual stability and simplicity (desaturated, clean colours)
- Dissonance = visual complexity and urgency (vivid, saturated, almost overwhelming)
- Research shows dissonance and visual roughness are cross-modally associated

**Configuration:**
- **Consonance scoring:** Use MFCC-based dissonance metrics or harmonic-function analysis
- **Smoothing:** 2-3s filter (harmonic changes are relatively slow)
- **Threshold:** Map 0-1 consonance score to saturation range

*Research:*
- [Cross-Modal Associations Between Harmonic Dissonance and Visual Roughness](https://journals.sagepub.com/doi/10.1177/20592043211055484)
- [Music Emotion Visualization through Colour](https://www.researchgate.net/publication/349985160_Music_Emotion_Visualization_through_Colour)

---

#### **Spatial Density (LED Spread) ↔ Stereo Width**

**Mapping:**
```
LED_Spread = (Stereo_Correlation + 1) / 2  [normalize -1 to 1 into 0 to 1]
```

**Interpretation:**
- **Mono/Narrow (low stereo width):** LEDs concentrated in center, tight focus
- **Wide stereo:** LEDs spread across entire strip, distributed energy
- **Extreme width:** LEDs at opposite ends, maximum spatial separation

**Pattern Variations:**
- Mono music → Center-focused pulse
- Stereo music → Ping-pong between left and right zones
- Distributed stereo → Entire strip shimmers

**Configuration:**
- **Channel analysis:** L/R phase correlation measurement (can do on ESP32 with dual-channel audio input)
- **Smoothing:** 1-2s filter
- **LED mapping:** 160 LEDs per half, use correlation to modulate which zones activate

*Research:*
- [Creating a Wide and Immersive Mix: Advanced Stereo Imaging and Spatial - Carvin Audio](https://carvinaudio.com/blogs/audio-education/creating-a-wide-and-immersive-mix-advanced-stereo-imaging-and-spatial-effects)
- [Guide to Panning and Stereo Width - Mastering the Mix](https://www.masteringthemix.com/blogs/learn/guide-to-panning-and-stereo-width)

---

#### **Movement Direction (Inward ↔ Outward) ↔ Musical Narrative Phase**

**Mapping (Research-Based Interpretation):**

In psychoacoustics and motion perception:
- **Centrifugal (outward):** Expansion, release, opening, relaxation
  - Perceptually associated with divergence, "letting go", dispersal
  - Used for drops, buildups reaching peak, moments of euphoria

- **Centripetal (inward):** Contraction, tension, focus, convergence
  - Perceptually associated with gathering energy, building anticipation, pulling together
  - Used for buildups leading into moments of tension, verses (introspection)

**Narrative Mapping:**
```
Verse/Build-Up Phase → INWARD motion (centre-outward suppressed)
  - Listeners drawn IN (focus, gathering energy)
  - Prediction: convergence to imminent "arrival"

Chorus/Drop Phase → OUTWARD motion (centre-outward emphasized)
  - Release of gathered tension
  - Expansion, celebration, payoff

Bridge/Breakdown → MIXED or SUSPENDED
  - Neither fully inward nor outward
  - Creates ambiguity and contrast
  - Prepares for next build
```

**Configuration:**
- **Structural detection:** Use beat/segment tracking to identify song phases
- **Smooth transition:** 4-8 bar interpolation between directional patterns
- **Alternative interpretation:** Could reverse these if user prefers (make inward = release)

*Research Note:* The specific psychological meaning of centripetal vs. centrifugal motion in visual preference is understudied. The mapping above is based on motion perception neuroscience and concert lighting design principles. Test with users and adjust based on emotional response.

*Research:*
- [Illusory Movement of Stationary Stimuli in the Visual Periphery - Journal of Neuroscience](https://www.jneurosci.org/content/33/10/4415)
- [The Role of Lighting in Storytelling and Mood Setting - BeTopperDJ](https://betopperdj.com/blogs/blogs/the-role-of-lighting-in-storytelling-and-mood-setting/)

---

#### **Visual "Weight" / Gravity ↔ Sub-Bass Energy**

**Mapping:**
```
Visual_Weight = Sub_Bass_Power(0-60Hz) / Total_Power
Visual_Weight_Effect = LEDs at bottom of strip get proportionally more energy
                      OR entire strip dims while bass emphasizes center
```

**Interpretation:**
- **High sub-bass (>40 dB at 20-60 Hz):** Heavy, grounded, physical presence
  - Visualization: LEDs lower on physical strip, darker/heavier colours (reds/blacks)
  - Pattern: Throbbing rather than shimmering

- **Low sub-bass:** Light, ethereal, floating
  - Visualization: LEDs upper regions, brighter, more movement
  - Pattern: Floating/hovering rather than grounded

**Rationale:**
- Infrasound (sub-bass) is felt more than heard
- Research shows sub-bass triggers emotional responses (pleasure at moderate levels, unease at extreme levels)
- Physical association: low frequencies = gravity, weight

**Configuration:**
- **Frequency isolation:** Band-pass filter around 20-60 Hz
- **Temporal lag:** Sub-bass effects should feel "anchored" (0.5-1s smoothing)
- **Optional spatial emphasis:** Could emphasize bottom LEDs (if physical arrangement allows)

*Research:*
- [Sub-Bass Conundrum: Finding the Perfect Frequency - TheTechyLife](https://thetechylife.com/what-frequency-should-sub-bass-be/)
- [Infrasound, human health, and adaptation - Springer Nature](https://link.springer.com/article/10.1007/s11069-013-0827-3)
- [Physiological and Psychological Effects of Infrasound on Humans - Henrik Møller](https://journals.sagepub.com/doi/10.1177/026309238400300101)

---

### 3.2 Summary Mapping Table

| Visual Parameter | Audio Feature | Range | Smoothing | Perceptual Meaning |
|------------------|---------------|-------|-----------|-------------------|
| Brightness | RMS Energy | 10-100% | 0.5-1.0s | Loudness, arousal intensity |
| Hue (colour temp) | Spectral Centroid | Red-Blue spectrum | 1-2s | Timbral brightness, frequency content |
| Saturation | Harmonic Complexity | 20-100% | 2-3s | Harmonic tension, dissonance |
| Motion Speed | Onset Density / BPM | 0.2x - 3.0x | 1-2s | Rhythmic urgency, density |
| Spatial Spread | Stereo Width | Narrow-Wide | 1-2s | Spatial expansiveness |
| Movement Direction | Song Phase | Inward-Outward | 4-8 bars | Narrative tension/release |
| Visual Weight | Sub-Bass Energy | 0-1.0 | 0.5-1.0s | Physical presence, gravity |

---

## Part 4: Colour Psychology and Cross-Cultural Universals

### 4.1 Research-Backed Colour Mappings

**Universal Emotion-to-Colour Associations (Cross-Cultural):**

Based on research across Western, East Asian, and diverse populations:

| Colour | Emotion Association | Audio Correlate | Universality |
|--------|-------------------|-----------------|--------------|
| **Red** | Energy, passion, urgency, anger, excitement | High energy + low valence (angry) OR high energy + high valence (excited) | Very high in Western cultures; somewhat universal |
| **Yellow** | Happy, warm, optimistic, cheerful | High valence + bright timbre (major mode, high spectral centroid) | Very high (sunshine association) |
| **Green** | Calm, balanced, natural, refreshed | Moderate valence + stable harmonies | High (nature association) |
| **Blue** | Calm, sad, serene, cool, melancholic | Low valence or high valence + low arousal (calm/peaceful) | Very high (sky/ocean association) |
| **Purple** | Mysterious, luxury, spiritual, dramatic | High harmonic complexity, dissonance, or minor mode + aesthetic beauty | Medium (cultural variability in East vs. West) |
| **Orange** | Warmth, creativity, playfulness, energy | High energy + warm timbre, medium valence | High (fire/sun association) |
| **Black** | Power, darkness, fear, mystery, sophistication | Low arousal, low valence, or very high harmonic complexity | High (darkness association) |
| **White** | Purity, clarity, simplicity, emptiness | Very high brightness (treble), minimal arrangement, consonance | High (light association) |

**Research Finding:** The major-mode bright timbre produces choices that are "more saturated, lighter, and yellower," while slow minor mode produces "opposite pattern" (desaturated, darker, bluer/redder).

### 4.2 Universal vs. Culturally-Specific Mappings

**Universals (Hard-Code):**
- Major mode + bright timbre = Yellow/warm direction
- Minor mode + dark timbre = Blue/cool direction
- Fast/rhythmically dense = motion and colour change
- Slow/sparse = stability and colour steadiness
- Loud = bright
- High pitch = brighter colours
- Low pitch/sub-bass = darker/warmer colours or grounded movement

**Culturally-Specific (Make Configurable):**
- Major-happy/minor-sad association is Western-trained; other cultures don't have this
- Colour symbolism varies: white = death in some East Asian cultures (Western: purity)
- Red = luck/prosperity in Chinese culture (Western: danger/passion)
- Key changes perceived as tension in Western listeners; less so in non-Western traditions

**Recommendation:** Provide a **base mapping** (universals) and allow **user-configurable colour palettes** (cultural/preference-based).

*Research:*
- [Color, Music, and Emotion: Bach to the Blues - Sage](https://journals.sagepub.com/doi/10.1177/2041669518808535)
- [Colour Association with Music Is Mediated by Emotion - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC4671663/)
- [Cross-Cultural Biases of Emotion Perception in Music - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC12110013/)
- [Harmonic organisation conveys both universal and culture-specific cues for emotional expression in music](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0244964)

---

## Part 5: Professional Concert Lighting Principles

### 5.1 How Concert Designers Create Emotional Arcs

**Key Principle: All Five Senses Reinforce Narrative**

Just as Disney Imagineers design immersive experiences by aligning architecture, sound, smell, and movement with emotional trajectory, concert lighting designers:

1. **Align lighting timing with musical moments:** Cues brighten during crescendos, dim during quiet passages
2. **Use warm-cool juxtaposition for visual tension:** Contrast warm yellow spotlight (on soloist) against cool blue stage wash (creates visual drama)
3. **Time transitions to beat:** Lighting changes sync to downbeats or on-beat moments, never off-grid (reinforces rhythm)
4. **Modulate intensity for release:** After intense moment, slow return to full stage lighting = emotional "exhale" after tension

**Tension ↔ Release Cycle in Lighting:**
```
Build Phase (0-30s):
  - Dimmer overall lighting
  - Focus narrows (fewer LEDs active, concentrated)
  - Colour saturates (more vivid hues)
  - Motion accelerates (faster chases/pulses)

Peak/Drop Moment:
  - Sudden full brightness
  - All LEDs activate simultaneously
  - Colour shift to complementary or shocking hue
  - Motion becomes explosive or stops entirely (freeze)

Release Phase (post-drop):
  - Gradual dimming from peak
  - Motion slows and spreads
  - Colour desaturates or shifts to cooler tones
  - Pattern becomes more meditative
```

### 5.2 Multi-Sensory Reinforcement (Immersive Art Principles)

**teamLab & Meow Wolf Insights:**

Immersive installations use:
- **Light patterns that change in real-time** in response to visitor movement and ambient audio
- **Colour fields that evoke emotional states** (warm = comfort, cool = mystery)
- **Spatial orchestration:** Light ripples across surfaces, guides attention through space
- **Synesthesia design:** Trigger simultaneous visual, auditory, and proprioceptive responses

**For LED Choreography:**
- Don't just visualize frequency content; create **spatial stories**
- Use darkness/contrast strategically: dim areas are as important as bright areas
- Consider entire journey from intro to outro as **narrative arc**, not isolated moments

*Research:*
- [Digital Art Installation - 5 Companies Raising the Bar](https://www.bridgewaterstudio.net/blog/digital-art-installation)
- [The world's top immersive art experiences - blooloop](https://www.blooloop.com/technology/in-depth/immersive-art-experiences/)

---

## Part 6: Implementation Framework

### 6.1 Recommended Firmware Architecture

**Real-Time Audio Feature Extraction (ESP32-S3):**

```
Audio Input Buffer (960 samples @ 48kHz = 20ms window)
    ↓
FFT (1024-point) → Spectral Centroid, RMS Energy, Sub-Bass Power
    ↓
Onset Detection (Spectral Flux) → Onset Strength, Density
    ↓
Pitch Tracking → Pitch Height, Harmonic Content
    ↓
Stereo Analysis → Correlation, Width
    ↓
Smoothing Filters (EMA or Moving Average, 0.5-3s window)
    ↓
Parameter Mapping (Tension Model)
    ↓
Visual Effect Driver (Brightness, Hue, Saturation, Speed, Direction)
```

**Feature Update Rate:** 20-50 Hz (every 20-50ms), adequate for visual perception

### 6.2 Key Algorithmic Components

1. **RMS Energy Extraction:**
   ```
   rms = sqrt(mean(audio_frame^2))
   brightness = clamp(rms * 255, min_brightness, max_brightness)
   ```

2. **Spectral Centroid Calculation:**
   ```
   spec_centroid = sum(freq_bin[i] * magnitude[i]) / sum(magnitude[i])
   hue = map_frequency_to_hue(spec_centroid, 100Hz→Red, 8000Hz→Blue)
   ```

3. **Onset Strength (Spectral Flux):**
   ```
   flux = sum(max(0, magnitude[i] - prev_magnitude[i]))
   onset_density = smoothed(flux)
   motion_speed = onset_density * scale_factor
   ```

4. **Tension Model (Simplified):**
   ```
   tension = 0.3 * normalized(RMS)
           + 0.2 * normalized(onset_density)
           + 0.2 * normalized(pitch_height)
           + 0.2 * normalized(dissonance)
           + 0.1 * normalized(tempo_deviation)
   saturation = tension * 100%
   ```

5. **Smoothing (Critical for Stability):**
   ```
   exponential_moving_average(new_sample, alpha=0.2)  // ~5 samples lag at 50Hz
   OR
   median_filter(buffer, window_size=10)  // robust to transient noise
   ```

### 6.3 Configuration Strategy

**User-Configurable Presets:**

1. **"Energy Mirror"** (Direct Mapping)
   - Brightness ← RMS Energy (1:1)
   - Hue ← Spectral Centroid (direct)
   - Speed ← Onset Density (direct)
   - Direction ← Manual or fixed pattern
   - Use case: DJ clubs, live concerts where direct visual feedback is desired

2. **"Emotional Journey"** (Narrative-Aware)
   - Requires pre-analysis of song structure (beats, segments)
   - Direction inward during verses, outward during choruses
   - Colour palette shifts to emphasize emotional arc
   - Use case: Curated playlist listening, storytelling

3. **"Tension & Release"** (Therapeutic)
   - Exaggerate tension dynamics (make buildups very tight/fast, drops very explosive)
   - Smoothed transitions (2-4s interpolation)
   - Desaturate during calm sections, saturate during peaks
   - Use case: Relaxation, meditative music, emotional processing

4. **"Synesthetic"** (Creative Mapping)
   - Custom user-defined mappings (e.g., major chords → specific hues)
   - Allow inversion of mappings
   - Configurable colour palette per mood
   - Use case: Artists, performers, installations

---

## Part 7: Cross-Cutting Considerations

### 7.1 Perceptual Thresholds

**Brightness Changes:** Humans perceive 5-10% changes in brightness
- **Smoothing:** Use 500-1000ms filter to avoid flicker
- **Minimum amplitude:** Stay above 10% for perceptibility

**Colour Hue Changes:** Noticeable at ~10° hue shift
- **Smoothing:** 1-2s filter; gradual transitions feel smooth
- **Avoid rapid cycling:** Can cause visual fatigue

**Motion Speed:** Speeds below 0.2 Hz and above 3 Hz feel unnatural
- **Range:** Constrain 0.5 Hz (once per 2 seconds) to 2 Hz (twice per second)

**Saturation:** Highly saturated colours can feel garish at high brightness
- **Coupling:** Reduce saturation when brightness exceeds 80%
- **Inverse relationship:** `saturation_adjusted = saturation * (1.0 - brightness/100 * 0.3)`

### 7.2 Temporal Dynamics

**Attentional Window:** 2-4 seconds
- Features changing faster than 2s feel jittery
- Features changing slower than 4s feel sluggish

**Memory Window:** 5-10 seconds
- System should "remember" previous context
- Avoid abrupt shifts unrelated to prior trend

**Latency Budget:** Aim for <100ms total latency
- Audio capture → DSP → LED driver pipeline must be tight
- Larger latencies feel "out of sync" with music

### 7.3 Failure Modes to Avoid

1. **Brightness flicker:** Insufficient smoothing causes strobing effect
   - Fix: Increase low-pass filter time constant to 1-2s

2. **Saturation overload:** All colours equally saturated → visual fatigue
   - Fix: Dynamically reduce saturation at extreme tension states
   - Or: Map extreme tension to BLACK instead of maximum saturation

3. **Direction thrashing:** LEDs constantly switching inward ↔ outward
   - Fix: Require 4-8 bar minimum duration per direction
   - Or: Only change direction on beat/segment boundaries

4. **Temporal lag between audio and LED:** DSP processing lag misaligns with beat
   - Fix: Implement buffering to ensure tight phase alignment
   - Target: <50ms lag from audio capture to LED update

---

## Part 8: Song Analysis Recommendations

### 8.1 Pre-Computed Features to Extract Offline

For maximum choreography quality, pre-analyse songs to extract:

1. **Beat & Segment Structure**
   - Beats, downbeats, bars
   - Segments (verse/chorus/bridge/intro/outro)
   - Key & harmonic function per segment
   - Use: Spotify/YouTube Music API or librosa (Python)

2. **Structural Peaks**
   - Where are choruses? When do they hit?
   - When do drops occur?
   - When is energy at maximum?
   - Allows "look-ahead" choreography

3. **Emotion Trajectory**
   - Pre-compute valence & arousal over time (use ML model or manual annotation)
   - Allows intentional colour palette design per section

4. **Key & Harmonic Content**
   - Tonic key, key changes, modulations
   - Chord progression sequences
   - Consonance/dissonance profile

**Tools:**
- Spotify Web API (segment labels, key, tempo, time_signature)
- librosa (beat tracking, chroma features, spectral analysis)
- MusicBrainz (structured metadata)
- Essentia (advanced music analysis, full feature extraction)

### 8.2 Real-Time vs. Pre-Computed

**Real-Time:** Responsive, works with live audio, requires lower DSP load
- Use for: DJing, live performances, real-time visualization

**Pre-Computed:** Higher quality, can use expensive algorithms, allows authoring
- Use for: Curated playlists, installations, choreographed performances
- Store: Metadata file alongside audio (JSON/protobuf with all features + effects timeline)

---

## Part 9: Validation & Testing

### 9.1 Validation Metrics

**Perceptual Alignment:**
- Does brightness increase during loud sections? (Should be >90% accuracy)
- Does colour shift to warm during low frequencies? (Should be >85% correct)
- Does motion speed increase during dense rhythms? (Should be >80% correct)

**Emotional Response:**
- Do users perceive choreography as "in sync" with music? (Target: >4/5 rating)
- Do users feel emotional arc is reinforced? (Target: >3.5/5 rating)
- Does choreography feel "natural" vs. "random"? (Target: >80% natural rating)

**Technical Metrics:**
- Latency from audio to LED update: <50ms
- Feature stability: smoothed features shouldn't jitter >5% per frame
- CPU load on ESP32: <30% (leave headroom for other tasks)

### 9.2 Test Playlist

Use diverse genres to test:

1. **Orchestral (slow, harmonic):** Rite of Spring - Stravinsky
2. **Pop (verse-chorus structure):** Blinding Lights - The Weeknd
3. **EDM (build-drop-drop):** Scary Monsters - Skrillex
4. **Hip-Hop (beat switches):** Levitating - Kendrick Lamar, Drake
5. **Ambient (static, meditative):** Music for Airports - Brian Eno
6. **Heavy Bass (sub-bass focus):** Death Grips - Bottomless Pit
7. **Acoustic (minimal density):** Fast Car - Tracy Chapman

**Expected Results:**
- Orchestral: Colour palette shifts with orchestration changes, motion stays smooth
- Pop: Clear brightness peaks at choruses, colour temperature matches instruments
- EDM: Speed increases leading to drop, sudden colour shift at drop moment
- Hip-Hop: Beat switches trigger mode changes, vocal intensity increases brightness
- Ambient: Motion minimal, colours shift slowly, saturation low
- Heavy Bass: Red/warm dominant throughout, visual weight down; motion grounded
- Acoustic: Limited palette, brightness follows vocal dynamics, motion minimal

---

## References Summary

**Core Music Structure & Emotion Research:**
- [TenseMusic - PLOS One](https://www.plosone.org/journal/pone/article?id=10.1371/journal.pone.0296385)
- [MTO 28.3: Verse-Chorus Form Analysis](https://www.mtosmt.org/issues/mto.22.28.3/mto.22.28.3.nobile.html)
- [Buildup, Breakdown and Drop - ResearchGate](https://www.researchgate.net/publication/265596844_Buildup_Breakdown_and_Drop_An_initial_experimental_approach_for_explaining_the_emotional_impact_of_DJ_performances_in_Electronic_Dance_Music_EDM)

**Audio Features & Emotion Recognition:**
- [Music Emotion Recognition with Machine Learning](https://www.researchgate.net/publication/355832899_Music_Emotion_Recognition_with_Machine_Learning_Based_on_Audio_Features)
- [The GenAI Guidebook: Audio Features](https://ravinkumar.com/GenAiGuidebook/audio/audio_feature_extraction.html)

**Colour Psychology & Cross-Modal Perception:**
- [Color, Music, and Emotion: Bach to the Blues - Sage](https://journals.sagepub.com/doi/10.1177/2041669518808535)
- [Colour Association with Music Is Mediated by Emotion - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC4671663/)
- [Cross-Modal Associations Between Harmonic Dissonance and Visual Roughness](https://journals.sagepub.com/doi/10.1177/20592043211055484)

**Psychoacoustics & Spatial Perception:**
- [Stereo Imaging and Spatial Audio - Carvin Audio](https://carvinaudio.com/blogs/audio-education/creating-a-wide-and-immersive-mix-advanced-stereo-imaging-and-spatial-effects)
- [Infrasound and Psychological Effects - Springer](https://link.springer.com/article/10.1007/s11069-013-0827-3)

**Concert Lighting & Immersive Design:**
- [How Stage Lighting Impacts Audience Emotion - Innovation Lighting](https://www.innovationlighting.net/how-stage-lighting-impacts-audience-emotion-and-engagement/)
- [Digital Art Installation Companies - Bridgewater Studio](https://www.bridgewaterstudio.net/blog/digital-art-installation)

**Cross-Cultural Emotion Recognition:**
- [Cross-Cultural Biases of Emotion Perception in Music - PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC12110013/)
- [Harmonic Organisation and Emotional Expression - PLOS One](https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0244964)

---

## Conclusion

Music creates emotional journeys through **tension and release cycles**. These cycles are created via:

1. **Harmonic tension** (movement away from tonic, dissonance)
2. **Rhythmic complexity** (onset density, syncopation)
3. **Dynamic intensity** (loudness, spectral brightness)
4. **Arrangement density** (number of instruments, layers)
5. **Melodic contour** (pitch height, direction)

A scientifically-grounded LED choreography system maps these measurable audio features to intuitive visual parameters:

- **RMS Energy → Brightness** (loudness → visibility)
- **Spectral Centroid → Hue** (high frequencies → cool/bright, low frequencies → warm/dark)
- **Onset Density → Motion Speed** (rhythm complexity → visual urgency)
- **Harmonic Complexity → Saturation** (dissonance → vivid, consonance → muted)
- **Stereo Width → Spatial Spread** (mono → centered, wide → distributed)
- **Song Phase → Direction** (intro/build → inward, drop/chorus → outward)
- **Sub-Bass Energy → Visual Weight** (low frequencies → grounded, high → ethereal)

**Key insight for your system:** Pre-analyse music structure offline to enable **narrative-aware choreography**. Don't just respond to real-time audio; understand WHERE THE SONG IS GOING. Then use real-time features to ADD TEXTURE to the pre-choreographed arc.

This approach creates **intentional visual storytelling** that amplifies the music's emotional journey, making the invisible made visible.

