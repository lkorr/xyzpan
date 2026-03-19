# Psychoacoustic Spatial Cues — XYZPan Reference

Comprehensive catalog of every spatial audio cue in XYZPan's generalized (non-HRTF) binaural engine.
Organized by perceptual category. Each entry documents the implementation, underlying research, and physical mechanism.

**Status key**: Active = implemented in DSP chain · Constants only = defined in Constants.h but not yet wired into processing · Not started = research candidate

---

## 1. AZIMUTH CUES (Left/Right)

### 1.1 Interaural Time Difference (ITD) — Active

**Implementation**: Geometric ear-distance model via Woodworth formula: `ITD = (a/c)(θ + sin θ)`, max 0.72 ms at 90°.

**Files**: `Constants.h:18-21`, `Engine.cpp:377-380,487-489`

**Research**:
- **Woodworth (1938)** — *Experimental Psychology* — Original ray-tracing ITD formula for a spherical head model.
- **Kuhn (1977)** — "Model for the interaural time differences in the azimuthal plane," *JASA 62*, 157–167. ITD is frequency-dependent: low-frequency ITD is 3/2× larger than high-frequency ITD due to diffraction. LF formula: `ITD_LF = 3a/c × sin(θ)`. Max ITD ≈800 μs (LF) vs ≈600 μs (HF). Minimum ITD occurs at 1.4–1.6 kHz.
- **Aaronson & Hartmann (2014)** — Confirmed Woodworth model accuracy for high frequencies.

**Mechanism**: Sound arrives at the far ear later. Dominant azimuth cue below ~1.5 kHz (duplex theory, Rayleigh 1907; the ~1.5 kHz crossover figure from Mills 1960). The auditory system extracts ITD from the temporal fine structure (low freq) and envelope (high freq).

**Potential improvement**: Frequency-dependent ITD — split into LF/HF bands with 1.5× multiplier for LF band (Kuhn's 3/2 ratio). Currently implemented as broadband. Would require crossover filter + dual delay lines per ear.

*Note*: Max ITD values (~800 μs LF, ~600 μs HF) are derived approximations consistent with Kuhn's 3/2 ratio, not exact figures from the paper.

---

### 1.2 Head Shadow (ILD via Low-Pass Filtering) — Active

**Implementation**: SVF LP on contralateral ear, 16 kHz (center) → 1.2 kHz (hard pan).

**Files**: `Constants.h:23-35`, `Engine.cpp:339-340,644-645,688-695`

**Research**:
- **Rayleigh (1907)** — Duplex theory: ILD dominates above ~1.5 kHz where wavelength < head diameter. (Rayleigh proposed ~500 Hz as the crossover; the ~1.5 kHz figure comes from Mills 1960.)
- **Shaw (1974)** — "Transformation of sound pressure level from the free field to the eardrum in the horizontal plane," *JASA 56*, 1848–1861. Systematic measurements of head diffraction, ear canal resonance, pinna contributions.
- **Brown & Duda (1998)** — "A structural model for binaural sound synthesis," *IEEE Trans. Speech Audio Proc. 6*, 476–488. Modeled head shadow as 1-pole/1-zero minimum-phase filter parameterized by azimuth. Efficient real-time implementation.
- **Duda & Martens (1998)** — Range-dependent head shadow model.

**Mechanism**: The head physically blocks high frequencies from reaching the far ear. ILD is negligible below ~500 Hz, reaches ~20 dB at 6 kHz for 60° azimuth. A single LP filter is a reasonable approximation; the actual shadow is frequency-dependent with non-monotonic behavior (a "bright spot" at certain angles).

---

### 1.3 Near-Field ILD (Low-Frequency Proximity Boost) — Active

**Implementation**: Low-shelf at 200 Hz, up to +6 dB on ipsilateral ear at close range + full azimuth (max +15 dB combined).

**Files**: `Constants.h:182-184`, `Engine.cpp:649-652,677-686`

**Research**:
- **Brungart & Rabinowitz (1999)** — "Auditory localization of nearby sources: Head-related transfer functions," *JASA 106(3)*, 1465–1479. ILD increases substantially below 1 m, even at low frequencies where it's normally negligible for distant sources. ITD remains roughly distance-independent.
- **Sridhar & Choueiri (2020)** — Analytical LF-ILD formula derived from rigid-sphere HRTF theory (AES Convention Paper 10412). Not a measurement dataset.

**Mechanism**: At close range (<1 m), the head's relative size compared to source distance becomes significant, causing ILD even at low frequencies. Up to ~20 dB ILD at very close range (~0.12–0.15 m). Effect diminishes beyond ~1 m.

---

## 2. FRONT/BACK DISCRIMINATION CUES

### 2.1 Comb Filter Array (Torso/Shoulder Reflections) — Active

**Implementation**: 10 feedback comb filters in series, delays 0.21–1.50 ms, feedback 0.13–0.16. Wet mix: 0% (front, Y=1) → 30% (rear, Y=−1).

**Files**: `Constants.h:70-94`, `Engine.cpp:504-510,1017,1347-1351`

**Research**:
- **Algazi et al. (2002)** — "Approximating the HRTF using simple geometric models of the head and torso," *JASA 112(5)*, 2053–2064. The "snowman model" (spherical head atop spherical torso). Torso reflections create comb-filter patterns below ~3 kHz. KEMAR anthropometry: head radius 8.7 cm, torso radius 16.9 cm, neck height 5.3 cm.
- **Brown & Duda (1998)** — Structural model includes torso reflection path.

**Mechanism**: Sound reflecting off the chest/shoulders arrives with a short delay (0.2–1.5 ms) creating comb-filter coloration. More prominent for rear sources where the torso is facing the sound. The irregular delay spacing avoids resonant peaks.

---

### 2.2 Presence Shelf (Front Brightness) — Active

**Implementation**: High shelf at 3 kHz, +4 dB (front) to −4 dB (rear), Y-mapped.

**Files**: `Constants.h:117-119`, `Engine.cpp:629,665-666`

**Research**:
- **Blauert (1969/70)** — "Sound localization in the median plane," *Acustica 22*, 205–213. Established "directional bands": ~4 kHz associated with frontal impression, ~1 kHz with rear, ~8 kHz with overhead. Narrowband noise perceived direction correlates with center frequency, not physical location.
- **Hebrank & Wright (1974)** — "Spectral cues used in the localization of sound sources on the median plane," *JASA 56(6)*, 1829–1834. Frontal cue = 1-octave notch between 4–10 kHz; this shelf creates the relative brightness that accompanies frontal sources.

**Mechanism**: Pinna geometry naturally brightens frontal sounds (~3–5 kHz) and darkens rear sounds. A high shelf is a coarse but effective approximation.

---

### 2.3 Ear Canal Resonance (Front/Back Coupling) — Active

**Implementation**: Peaking EQ at 2.7 kHz (Q=2.0), 0 dB (front) to −4 dB (rear), Y-mapped.

**Files**: `Constants.h:121-124`, `Engine.cpp:630,667-668`

**Research**:
- **Shaw (1974)** — Ear canal is a quarter-wave resonator at ~2.7 kHz, providing 15–17 dB gain. This resonance couples more effectively with frontal sound due to ear orientation.

**Mechanism**: The ear canal opening faces slightly forward. Sound from the front enters more directly; rear sound arrives at a less favorable angle, reducing the 2.7 kHz resonance coupling.

---

### 2.4 Rear Shadow (HF Rolloff for Rear Sources) — Active

**Implementation**: SVF LP, both ears, 22 kHz (front/neutral) → 4 kHz (Y=−1 rear).

**Files**: `Constants.h:46-50`, `Engine.cpp:158-160,646-648,698-699`

**Research**:
- **Langendijk & Bronkhorst (2002)** — "Contribution of spectral cues to human sound localization," *JASA 112(4)*, 1583–1596. Front-back cues are primarily in the 8–16 kHz band. Removal of 2-octave bands above 4 kHz makes localization virtually impossible.
- **Frank & Zotter (2018)** — DAGA 2018, IEM Graz. Noted that VLC media player internally applies a high-shelf at ~700 Hz with 7 dB attenuation for rear sources, which reduces front-back confusion. (Not a recommendation from the authors; describes VLC's implementation.)

**Mechanism**: Head/pinna geometry naturally attenuates high frequencies from rear sources. The pinna acts as a directional collector favoring frontal sound.

---

### 2.5 1 kHz Rear Boost (Blauert's Directional Band) — Constants Only

**Constants**: `kRearCue1kHz_FreqHz = 1000`, `Q = 1.5`, `MaxDb = +2.0`

**Files**: `Constants.h:231-233`

**Research**:
- **Blauert (1969/70)** — 1 kHz is a "rear" directional band. Narrowband noise centered at 1 kHz creates a rear spatial impression.
- **Nakamura & Iida (2024)** — Confirmed P0 boost at ~1 kHz across 118 ears as a rear localization cue.

**Mechanism**: Diffraction around the head and pinna creates a natural boost near 1 kHz for rear-arriving sound. A +2 dB bell filter when Y < 0 reinforces this.

---

### 2.6 Front-Back Notch (6.5 kHz) — Constants Only

**Constants**: `kFrontBackNotchFreqHz = 6500`, `Q = 2.0`, `MaxDb = -6.0`

**Files**: `Constants.h:236-238`

**Research**:
- **Hebrank & Wright (1974)** — Frontal cue includes a 1-octave notch between 4–10 kHz.
- **Macpherson & Middlebrooks (2003)** — "Vertical-plane sound localization probed with ripple-spectrum noise," *JASA 114*, 430–445. Focused on spectral resolution (ripple density) for vertical-plane localization. The 6–10 kHz front-back frequency range is more directly supported by Langendijk & Bronkhorst (2002).

**Mechanism**: The pinna creates a characteristic notch around 6.5 kHz for rear sources. Attenuating this band when Y < 0 reinforces the "not front" impression.

---

## 3. ELEVATION CUES (Up/Down)

### 3.1 Pinna N1 Notch (Primary Elevation Cue) — Active

**Implementation**: Peaking EQ notch, Z-mapped frequency: 6.5 kHz (below) → 8 kHz (ear level) → 10 kHz (above). Depth: −15 dB, Q=2.0.

**Files**: `Constants.h:99-105`, `Engine.cpp:632,669-670`

**Research**:
- **Iida et al. (2007)** — "Median plane localization using a parametric model of the HRTF based on spectral cues," *Applied Acoustics 68*, 835–850. N1+N2+P1 are sufficient for elevation localization equivalent to full HRTFs. N1 varies systematically with elevation.
- **Batteau (1967)** — "The role of the pinna in human localization," *Proc. Royal Soc. London B 168*, 158–180. First theory that pinna reflections create direction-dependent spectral features in the 4–12 kHz range.
- **Lopez-Poveda & Meddis (1996)** — "A physical model of sound diffraction and reflections in the human concha," *JASA 100*, 3248–3259. Computational model predicting first notch varies from ~6.5 kHz at −40° elevation to ~10 kHz at +60° elevation.
- **Spagnol et al. (2013)** — "On the relation between pinna reflection patterns and HRTF features," *IEEE Trans. ASLP 21(3)*, 508–519. Geometric ray-tracing on pinna contours predicts spectral notch frequencies. Reflecting surfaces: helix and concha edges.

**Mechanism**: Sound reflecting off the pinna's folds (helix, antihelix, concha) creates destructive interference at a frequency that depends on the reflection path length. As elevation changes, the effective path length changes, shifting the notch frequency. This is THE primary elevation cue, shared across virtually all human ears despite individual variation in exact frequencies.

---

### 3.2 Pinna N2 Notch (Secondary) — Active

**Implementation**: Peaking EQ at N1 + 3 kHz offset, −8 dB, Q=2.0.

**Files**: `Constants.h:107-110`, `Engine.cpp:635,671-672`

**Research**:
- **Iida et al. (2007)** — N2 is the second spectral notch, systematically higher than N1. Together with N1 and P1, provides full elevation discrimination.

**Mechanism**: A second reflection path in the pinna creates a second notch at a higher frequency. The N1–N2 separation is an additional elevation cue.

---

### 3.3 Pinna P1 Peak (Concha Resonance, Fixed) — Active

**Implementation**: Peaking EQ +4 dB at 5 kHz, Q=1.5 (not elevation-dependent).

**Files**: `Constants.h:112-115`, `Engine.cpp:512-513`

**Research**:
- **Shaw (1974)** — Concha provides significant boost in 5–9 kHz region.
- **Iida et al. (2007)** — P1 serves as spectral reference anchor for the auditory system to interpret N1 and N2.

**Mechanism**: The concha (bowl of the outer ear) acts as a resonant cavity, boosting ~5 kHz regardless of direction. The auditory system uses this fixed peak as a reference point to detect the N1/N2 notches.

---

### 3.4 Pinna High Shelf (Above = Brighter) — Active

**Implementation**: High shelf at 4 kHz, 0 dB (below) → +3 dB (above), Z-mapped.

**Files**: `Constants.h:126-127`, `Engine.cpp:633,673-674`

**Research**:
- **Hebrank & Wright (1974)** — "Above" cue is a 1/4-octave peak between 7–9 kHz.
- **Middlebrooks (1992)** — "Narrow-band sound localization related to external ear acoustics," *JASA 92(5)*, 2607–2624. Perceived elevation correlates with stimulus frequency: higher frequencies → perceived as higher elevation.

**Mechanism**: The pinna's geometry naturally passes more high-frequency energy for overhead sources. A gentle HF shelf captures this tendency.

---

### 3.5 Concha Notch (4 kHz) — Constants Only

**Constants**: `kConchaNotchFreqHz = 4000`, `Q = 3.0`, 0 dB (above) → −8 dB (below).

**Files**: `Constants.h:241-244`

**Research**:
- **Shaw (1974)** — Concha resonance interacts with elevation; below-horizon sources lose the concha resonance coupling.
- **Musicant & Butler (1984)** — "The influence of pinnae-based spectral cues on sound localization," *JASA 75(4)*, 1195–1200. Pinna occlusion severely degrades localization; concha is key structure.

**Mechanism**: The concha resonance (~4 kHz) is strongest for sources near or above ear level. Sources from below couple less effectively, creating a natural notch.

---

### 3.6 Upper Pinna Peak (12 kHz) — Constants Only

**Constants**: `kUpperPinnaFreqHz = 12000`, `Q = 2.0`, −4 dB (below) → +3 dB (above).

**Files**: `Constants.h:247-250`

**Research**:
- **Langendijk & Bronkhorst (2002)** — Up-down cues primarily in 6–12 kHz band.
- **Iida & Ishii (2018)** — P2 peak at ~10 kHz, nearly elevation-independent. Its role is enhancing N1 salience rather than serving as a direct independent elevation cue.

**Mechanism**: Higher harmonics of pinna reflections. The 12 kHz band is enhanced for overhead sources and attenuated for below-horizon sources.

---

### 3.7 Shoulder Reflection Peak (1.5 kHz) — Constants Only

**Constants**: `kShoulderPeakFreqHz = 1500`, `Q = 1.0`, 0 dB (above) → +2 dB (below horizon).

**Files**: `Constants.h:253-255`

**Research**:
- **Algazi et al. (2002)** — Torso/shoulder reflections provide elevation cues below 3 kHz. The snowman model quantifies these.

**Mechanism**: Sound from below reflects off the shoulders, arriving at the ears with a slight boost around 1.5 kHz. This low-frequency cue supplements the high-frequency pinna cues.

---

### 3.8 Chest Bounce Delay — Active

**Implementation**: Parallel filtered delay: 4× HP cascade at 700 Hz + 1× LP at 1 kHz. Delay: 0 ms (above) → 2 ms (below). Gain: −∞ dB (above) → −8 dB (below).

**Files**: `Constants.h:129-133`, `Engine.cpp:614-615,1029-1033,1279-1292`

**Research**:
- **Algazi et al. (2002)** — Chest reflection provides below-horizon cues with delays of 0.4–0.7 ms.
- **Wallach et al. (1949)** — Precedence effect: reflections within ~5 ms fuse with the direct sound perceptually for click stimuli (for complex sounds like speech/music, fusion extends to ~40 ms). The 5 ms bound is conservative/appropriate for the chest bounce use case.

**Mechanism**: Sound from below ear level reflects off the chest/torso, arriving as a filtered, delayed copy. Within the precedence-effect window, this fuses with the direct sound and adds spectral coloration signaling "below."

---

### 3.9 Floor Bounce Delay — Active

**Implementation**: Per-ear delay lines. Delay: 0 ms (above) → 20 ms (below). LP at 5 kHz. Gain: −∞ dB (above) → −5 dB (below).

**Files**: `Constants.h:135-142`, `Engine.cpp:615,1035-1039,1298-1309`

**Research**:
- **Wendt et al. (2017)** — "The influence of the floor reflection on the perception of sound elevation." IEM, University of Music, Graz. Floor reflections affect perceived elevation, particularly for speech. Adding room acoustics increased perceived elevation range.

**Mechanism**: Ground reflection for below-horizon sources. Longer delay (up to 20 ms) as the ground path is longer than the chest path. LP filter models HF absorption by the floor surface.

---

### 3.10 Tragus Notch (8.5 kHz) — Constants Only

**Constants**: `kTragusNotchFreqHz = 8500`, `Q = 3.5`, −5 dB max, Y+Z mapped.

**Files**: `Constants.h:258-260`

**Research**:
- **General pinna acoustics literature** — The tragus (small flap in front of ear canal) shadows rear-arriving sound, creating a notch around 8–10 kHz for rear sources. (Note: Hebrank & Wright 1974 describe the "behind" cue as a small peak at 10–12 kHz, not this notch. The tragus shadow concept derives from broader pinna acoustics research.)

**Mechanism**: The tragus partially occludes the ear canal entrance for rear/below sources. This creates a narrow notch at ~8.5 kHz that varies with both front/back (Y) and elevation (Z).

---

## 4. DISTANCE CUES

### 4.1 Inverse-Square Law Attenuation — Active

**Implementation**: `gain = clamp((distRef / distance)², 0, 2.0)`. Floor: −72 dB at sphere boundary. Max: +6 dB for close sources.

**Files**: `Constants.h:158-163`, `Engine.cpp:1009-1021,1424-1431`

**Research**:
- **Coleman (1963)** — "An analysis of cues to auditory depth perception in free space," *Psych. Bull. 60*, 302–315. Intensity is the most effective distance cue.
- **Zahorik et al. (2005)** — "Auditory distance perception in humans," *Acta Acustica 91(3)*, 409–420. Distance perception follows a compressive power function. Intensity is the primary cue; listeners systematically underestimate far, overestimate near.

**Mechanism**: Sound intensity decreases with the square of distance. This is the most salient and universal distance cue.

---

### 4.2 Air Absorption (HF Rolloff with Distance) — Active

**Implementation**: Two cascaded 1st-order LP filters. Stage 1: 22→8 kHz. Stage 2: 22→12 kHz. Combined: ~12 dB/oct at far distance.

**Files**: `Constants.h:170-180`, `Engine.cpp:658-661,701-705,1025-1027,1449-1453`

**Research**:
- **ISO 9613-1:1993** — "Acoustics — Attenuation of sound during propagation outdoors — Part 1." Standard for air absorption: ~0.03 dB/m at 4 kHz, ~0.09 dB/m at 8 kHz, ~0.3 dB/m at 16 kHz (20°C, 50% RH).
- **Harris (1966)** — "Absorption of sound in air versus humidity and temperature," *JASA 40*, 148–159. Early measurements of atmospheric absorption.

**Mechanism**: Air preferentially absorbs high frequencies. Over short distances this is negligible, but over larger distances the cumulative HF loss is clearly audible. Two cascaded LP stages approximate the frequency-dependent absorption curve.

---

### 4.3 Distance Delay + Doppler — Active

**Implementation**: Fractional delay with Hermite interpolation. 2 ms (close) → 300 ms (far). Smoother: 150 ms RC. NOT latency-compensated (creative effect).

**Files**: `Constants.h:154-156`, `Engine.cpp:1022-1023,1034-1046`

**Research**:
- **Rosenblum et al. (1987)** — "Relative effectiveness of three stimulus variables for locating a moving sound source," *Perception 16*, 175–186. Doppler-shifted sounds can inform about source velocity and approach angle.

**Mechanism**: Sound travels at ~343 m/s. Propagation delay provides a distance cue, and changes in delay produce Doppler pitch shift. The deliberate non-compensation makes distance feel physically real.

---

### 4.4 Reverb (DRR — Direct-to-Reverberant Ratio) — Active

**Implementation**: FDN reverb with distance-mapped pre-delay (0–50 ms) and wet mix.

**Files**: `Constants.h:189-196`

**Research**:
- **Zahorik (2002)** — "Assessing auditory distance perception using virtual acoustics," *JASA 111(4)*, 1832–1846. DRR is the second most important distance cue after intensity. JND for DRR is ~5–6 dB (Zahorik, "Direct-to-reverberant energy ratio sensitivity," *JASA 112(5)*, 2110–2117).
- **Bronkhorst & Houtgast (1999)** — "Auditory distance perception in rooms," *Nature 397*, 517–520. DRR modulates perceived distance.

**Mechanism**: In any enclosed space, the ratio of direct sound to reverberant energy decreases with distance. A source close to the listener has high direct-to-reverberant ratio; a distant source is dominated by reverb. Mapping reverb wet mix to distance creates a powerful distance percept.

---

## 5. ADDITIONAL CUES NOT YET IN CODEBASE (Research Candidates)

### 5.1 Interaural Coherence / Decorrelation — HIGH PRIORITY

**What**: Subtle frequency-dependent decorrelation between L/R ears.

**Research**:
- **Catic et al. (2013)** — "The effect of interaural-level-difference fluctuations on the externalization of sound," *JASA 134(2)*, 1232. ILD fluctuations from reverberation drive externalization above 1 kHz.
- **Hassager et al. (2016)** — "The role of spectral detail in the binaural transfer function on perceived externalization," *JASA 139(5)*, 2992–3000. Spectral detail in the direct sound is critical for externalization.

**Why generalizable**: All listeners externalize better with some L/R decorrelation; it mimics the natural decorrelation caused by room reflections.

**Implementation sketch**: Short allpass chains with slightly different L/R coefficients. Amount scales with distance (close = high coherence, far = more decorrelation). Very low CPU cost.

**Impact**: Primarily improves headphone externalization (sounds outside the head), not directional accuracy per se.

---

### 5.2 Frequency-Dependent ITD — MEDIUM PRIORITY

**What**: LF ITD is ~1.5× larger than HF ITD (Kuhn's 3/2 ratio).

**Research**: **Kuhn (1977)** — See §1.1 above.

**Why generalizable**: This is physics (head diffraction), not anatomy. Universal across all human heads.

**Implementation sketch**: Crossover filter (~1.5 kHz), apply 1.5× ITD multiplier to LF band. Requires dual delay lines per ear (one per band) or a split-band approach.

**Impact**: Subtle but physically correct. The current broadband ITD may already be "close enough" perceptually.

---

### 5.3 Spectral Tilt for Distance — LOW PRIORITY

**What**: Beyond air absorption, distant sounds also exhibit overall spectral tilt (less bright).

**Research**: **Zahorik et al. (2005)** — Spectral cues supplement intensity and DRR for distance.

**Impact**: Already partially covered by air absorption LPFs. Adding explicit spectral tilt would be a marginal improvement.

---

### 5.4 Dynamic Spectral Cues (Head Movement) — V2 ONLY

**What**: Small head movements provide dramatic front/back disambiguation.

**Research**:
- **Wallach (1940)** — "The role of head movements and vestibular and visual cues in sound localization," *J. Exp. Psych. 27*, 339–368. Head rotation resolves front-back ambiguity via changing ITD/ILD.
- **Perrett & Noble (1997)** — Head movements substantially reduce front-back confusions.

**Implementation**: Requires head tracking (OSC input) — already in v2 roadmap as EXT-03.

**Impact**: THE single most effective anti-confusion cue, but requires hardware.

---

## 6. CUE INVENTORY SUMMARY

| #  | Cue                          | Category          | Status         | Priority Research Paper          |
|----|------------------------------|-------------------|----------------|----------------------------------|
| 1  | ITD                          | Azimuth           | Active         | Kuhn 1977                        |
| 2  | Head Shadow LP               | Azimuth           | Active         | Brown & Duda 1998                |
| 3  | Near-Field ILD               | Azimuth           | Active         | Brungart & Rabinowitz 1999       |
| 4  | Pinna N1 Notch               | Elevation         | Active         | Iida et al. 2007                 |
| 5  | Pinna N2 Notch               | Elevation         | Active         | Iida et al. 2007                 |
| 6  | Pinna P1 Peak                | Elevation         | Active         | Shaw 1974                        |
| 7  | Pinna High Shelf             | Elevation         | Active         | Hebrank & Wright 1974            |
| 8  | Ear Canal Resonance          | Front/Back        | Active         | Shaw 1974                        |
| 9  | Comb Filter Array            | Front/Back        | Active         | Algazi et al. 2002               |
| 10 | Presence Shelf               | Front/Back        | Active         | Blauert 1969                     |
| 11 | Rear Shadow LP               | Front/Back        | Active         | Langendijk & Bronkhorst 2002     |
| 12 | Chest Bounce                 | Elevation         | Active         | Algazi et al. 2002               |
| 13 | Floor Bounce                 | Elevation         | Active         | Wendt et al. 2017                |
| 14 | Air Absorption               | Distance          | Active         | ISO 9613-1:1993                  |
| 15 | Inverse-Square Gain          | Distance          | Active         | Zahorik et al. 2005              |
| 16 | Distance Delay + Doppler     | Distance          | Active         | Rosenblum et al. 1987            |
| 17 | 1 kHz Rear Boost             | Front/Back        | Constants only | Blauert 1969, Nakamura 2024      |
| 18 | Front-Back Notch 6.5 kHz     | Front/Back        | Constants only | Hebrank & Wright 1974            |
| 19 | Concha Notch 4 kHz           | Elevation         | Constants only | Shaw 1974, Musicant & Butler 1984|
| 20 | Upper Pinna Peak 12 kHz      | Elevation         | Constants only | Langendijk & Bronkhorst 2002     |
| 21 | Shoulder Reflection 1.5 kHz  | Elevation         | Constants only | Algazi et al. 2002               |
| 22 | Tragus Notch 8.5 kHz         | Front/Back + Elev | Constants only | General pinna acoustics          |
| 23 | Interaural Decorrelation     | Externalization   | Not started    | Catic et al. 2013                |
| 24 | Frequency-Dependent ITD      | Azimuth           | Not started    | Kuhn 1977                        |

---

## 7. FULL BIBLIOGRAPHY

1. Algazi, V.R., Duda, R.O., Duraiswami, R., Gumerov, N.A., & Tang, Z. (2002). "Approximating the head-related transfer function using simple geometric models of the head and torso." *JASA*, 112(5), 2053–2064.
2. Batteau, D.W. (1967). "The role of the pinna in human localization." *Proc. Royal Soc. London B*, 168, 158–180.
3. Blauert, J. (1969/70). "Sound localization in the median plane." *Acustica*, 22, 205–213.
4. Blauert, J. (1983). *Spatial Hearing: The Psychophysics of Human Sound Localization.* MIT Press.
5. Bronkhorst, A.W. & Houtgast, T. (1999). "Auditory distance perception in rooms." *Nature*, 397, 517–520.
6. Brown, C.P. & Duda, R.O. (1998). "A structural model for binaural sound synthesis." *IEEE Trans. Speech Audio Proc.*, 6, 476–488.
7. Brungart, D.S. & Rabinowitz, W.M. (1999). "Auditory localization of nearby sources: HRTFs." *JASA*, 106(3), 1465–1479.
8. Catic, J., Santurette, S., Buchholz, T., Gran, F., & Dau, T. (2013). "The effect of interaural-level-difference fluctuations on the externalization of sound." *JASA*, 134(2), 1232.
9. Coleman, P.D. (1963). "An analysis of cues to auditory depth perception in free space." *Psych. Bull.*, 60, 302–315.
10. Duda, R.O. & Martens, W.L. (1998). "Range dependence of the response of a spherical head model." *JASA*, 104, 3048–3058.
11. Frank, M. & Zotter, F. (2018). "Exploring the perceptual sweet area in ambisonics." DAGA 2018, IEM, University of Music and Performing Arts, Graz.
12. Harris, C.M. (1966). "Absorption of sound in air versus humidity and temperature." *JASA*, 40, 148–159.
13. Hartmann, W.M. (1983). "Localization of sound in rooms." *JASA*, 74, 1380–1391.
14. Hassager, H.G., Gran, F., & Dau, T. (2016). "The role of spectral detail in the binaural transfer function on perceived externalization." *JASA*, 139(5), 2992–3000.
15. Hebrank, J. & Wright, D. (1974). "Spectral cues used in the localization of sound sources on the median plane." *JASA*, 56(6), 1829–1834.
16. Iida, K. & Ishii, Y. (2018). "3D sound image control by individualized parametric head-related transfer functions." *Applied Acoustics*, 129, 401–407.
17. Iida, K., Itoh, M., Itagaki, A., & Morimoto, M. (2007). "Median plane localization using a parametric model of the HRTF based on spectral cues." *Applied Acoustics*, 68, 835–850.
18. ISO 9613-1:1993. "Acoustics — Attenuation of sound during propagation outdoors — Part 1: Calculation of the absorption of sound by the atmosphere."
19. Kuhn, G.F. (1977). "Model for the interaural time differences in the azimuthal plane." *JASA*, 62, 157–167.
20. Langendijk, E.H.A. & Bronkhorst, A.W. (2002). "Contribution of spectral cues to human sound localization." *JASA*, 112(4), 1583–1596.
21. Lopez-Poveda, E.A. & Meddis, R. (1996). "A physical model of sound diffraction and reflections in the human concha." *JASA*, 100, 3248–3259.
22. Macpherson, E.A. & Middlebrooks, J.C. (2002). "Listener weighting of cues for lateral angle." *JASA*, 111(5), 2219–2236.
23. Macpherson, E.A. & Middlebrooks, J.C. (2003). "Vertical-plane sound localization probed with ripple-spectrum noise." *JASA*, 114, 430–445.
24. Middlebrooks, J.C. (1992). "Narrow-band sound localization related to external ear acoustics." *JASA*, 92(5), 2607–2624.
25. Mills, A.W. (1960). "Lateralization of high-frequency tones." *JASA*, 32(1), 132–134.
26. Musicant, A.D. & Butler, R.A. (1984). "The influence of pinnae-based spectral cues on sound localization." *JASA*, 75(4), 1195–1200.
27. Nakamura, S. & Iida, K. (2024). Confirmation of P0 (~1 kHz) rear spectral cue across 118 ears.
28. Perrett, S. & Noble, W. (1997). "The contribution of head motion cues to localization of low-pass noise." *Perception & Psychophysics*, 59(7), 1018–1026.
29. Rayleigh, Lord (1907). "On our perception of sound direction." *Phil. Mag.*, 13, 214–232.
30. Rosenblum, L.D., Carello, C., & Pastore, R.E. (1987). "Relative effectiveness of three stimulus variables for locating a moving sound source." *Perception*, 16, 175–186.
31. Shaw, E.A.G. (1974). "Transformation of sound pressure level from the free field to the eardrum in the horizontal plane." *JASA*, 56, 1848–1861.
32. Spagnol, S., Geronazzo, M., & Avanzini, F. (2013). "On the relation between pinna reflection patterns and HRTF features." *IEEE Trans. ASLP*, 21(3), 508–519.
33. Sridhar, R. & Choueiri, E.Y. (2020). "An analytical formula for the low-frequency interaural level difference." AES Convention Paper 10412.
34. Wallach, H. (1940). "The role of head movements and vestibular and visual cues in sound localization." *J. Exp. Psych.*, 27, 339–368.
35. Wallach, H., Newman, E.B., & Rosenzweig, M.R. (1949). "The precedence effect in sound localization." *Am. J. Psych.*, 62, 315–336.
36. Wendt, F., Frank, M., & Zotter, F. (2017). "The influence of the floor reflection on the perception of sound elevation." IEM, University of Music, Graz.
37. Wightman, F.L. & Kistler, D.J. (1997). "Monaural sound localization revisited." *JASA*, 101, 1050–1063.
38. Woodworth, R.S. (1938). *Experimental Psychology.* Henry Holt & Co.
39. Zahorik, P. (2002a). "Assessing auditory distance perception using virtual acoustics." *JASA*, 111(4), 1832–1846.
40. Zahorik, P. (2002b). "Direct-to-reverberant energy ratio sensitivity." *JASA*, 112(5), 2110–2117.
41. Zahorik, P., Brungart, D.S., & Bronkhorst, A.W. (2005). "Auditory distance perception in humans." *Acta Acustica*, 91(3), 409–420.
42. Zonooz, B., Arani, E., Körding, K.P., & Van Opstal, A.J. (2019). "Spectral weighting underlies perceived sound elevation." *Scientific Reports*, 9, 1642.
