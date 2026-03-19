# Proposed New Psychoacoustic Spatial Cues for XYZPan

Research-backed proposals for additional DSP cues to improve front/back discrimination,
above/below perception, and externalization. All cues selected for generalizability
across listeners (no individualized HRTF data required).

---

## FRONT/BACK DISCRIMINATION

### FB-1: Blauert Low-Frequency Front Band (300–500 Hz)

**What**: Blauert's directional bands show 300–500 Hz is strongly associated with frontal
perception. A subtle low-mid boost for frontal sources reinforces "in front" impression.

**Research**:
- Blauert, J. (1969/70). "Sound localization in the median plane." *Acustica*, 22, 205–213. — Narrowband noise at 500 Hz perceived as frontal by >80% of listeners. Identified "boosted bands" where perceived direction depends on center frequency, not physical source position. [1]
- Blauert, J. (1997). *Spatial Hearing: The Psychophysics of Human Sound Localization* (rev. ed.). MIT Press. — Comprehensive treatment of directional bands with population data. Frontal bands: 270–550 Hz and 2.7–5.5 kHz. [2]
- Morimoto, M. & Aokata, H. (1984). "Localization cues of sound sources in the upper hemisphere." *J. Acoust. Soc. Japan (E)*, 5(3), 165–173. — Confirmed directional bands generalize across sagittal planes, not just the median plane. [3]

Physical basis: head diffraction creates a natural LF boost for frontal sources due to constructive interference at the face.

**Implementation**: Bell filter at 400 Hz, Q=1.0, gain Y-mapped: +1.5 dB (front, Y=1)
to −1.5 dB (rear, Y=−1). Very subtle — this is a low-frequency "warmth" cue.

**Generalizability**: HIGH — head diffraction physics, not anatomy.

**Priority**: MEDIUM — subtle but adds a low-frequency dimension that all existing cues
lack (everything currently active is >1 kHz).

---

### FB-2: Rear HF Peak at 11 kHz (Blauert Rear HF Band)

**What**: Blauert's second rear directional band is 10–12 kHz. Distinct from the existing
rear shadow LPF (broad rolloff). A narrow boost at 11 kHz for rear sources adds the
specific "rear HF" spectral signature.

**Research**:
- Blauert, J. (1969/70). "Sound localization in the median plane." *Acustica*, 22, 205–213. — 10–12 kHz narrowband noise perceived as rear by majority of subjects. [1]
- Blauert, J. (1997). *Spatial Hearing* (rev. ed.). MIT Press. — Rear directional bands: 0.8–1.6 kHz and 10–12 kHz. [2]
- Morimoto, M. & Aokata, H. (1984). "Localization cues of sound sources in the upper hemisphere." *J. Acoust. Soc. Japan (E)*, 5(3), 165–173. — Confirmed rear bands across sagittal planes. [3]
- Hebrank, J. & Wright, D. (1974). "Spectral cues used in the localization of sound sources on the median plane." *JASA*, 56(6), 1829–1834. — Rear sources show small peak between 10–12 kHz with decreased energy above and below. [4]

Physical basis: pinna geometry creates a small resonant peak at ~11 kHz for rear-arriving sound (reflection off back of concha rim).

**Implementation**: Bell filter at 11 kHz, Q=2.0, gain Y-mapped: 0 dB (front) → +2 dB (rear).
Works *alongside* the rear shadow LPF — the LPF darkens overall, but this peak pokes through
at 11 kHz, creating a distinctive rear spectral signature rather than just "dark."

**Generalizability**: MEDIUM-HIGH — 10–12 kHz rear band consistent across Blauert's subjects,
but individual pinna geometry affects exact frequency.

**Priority**: MEDIUM — differentiates "rear" from just "muffled." The existing rear shadow alone
makes rear sources sound distant/occluded; adding a specific 11 kHz rear peak makes them sound
specifically *behind*.

---

### FB-3: Front-Back ILD Asymmetry (Frequency-Dependent)

**What**: The head shadow ILD is NOT symmetric front-to-back. For the same lateral angle,
the ILD pattern differs between front and rear because the pinna faces forward. Currently
XYZPan applies head shadow based purely on azimuth magnitude (|X|), ignoring Y.

**Research**:
- Macpherson, E.A. & Middlebrooks, J.C. (2002). "Listener weighting of cues for lateral angle: The duplex theory of sound localization revisited." *JASA*, 111(5), 2219–2236. — Demonstrated that ILD weighting is frequency-dependent and front-back asymmetric. [5]
- Macpherson, E.A. (2013). "Cue weighting and vestibular mediation of temporal dynamics in sound localization via head rotation." *Proc. Meetings on Acoustics*, 19, 050131. — Front-back ILD asymmetry ~2 dB below 4 kHz, increasing to ~6 dB at higher frequencies. [6]
- Shaw, E.A.G. (1974). "Transformation of sound pressure level from the free field to the eardrum in the horizontal plane." *JASA*, 56, 1848–1861. — Systematic measurements showing the pinna's forward-facing geometry amplifies ipsilateral HF more for frontal incidence. [7]
- Brown, C.P. & Duda, R.O. (1998). "A structural model for binaural sound synthesis." *IEEE Trans. Speech Audio Proc.*, 6(5), 476–488. — Structural HRTF model incorporating asymmetric pinna directivity. [8]

**Implementation**: Modify existing head shadow SVF cutoff calculation to incorporate Y. When
source is rear (Y<0), shift contralateral LPF cutoff ~15–20% lower than the pure-azimuth value.
This makes rear lateral sources slightly darker on the far ear than front lateral sources at
the same angle.

```
effectiveCutoff = baseCutoff * (1.0f - 0.15f * rearFactor)
// where rearFactor = std::max(0.0f, -Y)
```

**Generalizability**: HIGH — physics (pinna faces forward on all humans).

**Priority**: HIGH — zero CPU cost (modifies existing filter parameter), improves lateral
front/back discrimination which is currently weak (front/back cues mostly work near median plane).

---

### FB-4: Concha Shadow for Rear Sources (2–4 kHz Dip)

**What**: The concha acts as a waveguide funneling sound into the ear canal. For frontal
sources, it provides +10–15 dB gain in the 2–4 kHz range. For rear sources, this gain is
substantially reduced because sound enters at a poor angle. Partially captured by the
existing ear canal resonance filter (2.7 kHz, Q=2.0, Y-mapped) but the concha effect is
broader (2–4 kHz) and deeper.

**Research**:
- Shaw, E.A.G. (1974). "Transformation of sound pressure level from the free field to the eardrum in the horizontal plane." *JASA*, 56, 1848–1861. — Concha contributes ~10 dB gain at 4–5 kHz, strongly direction-dependent. [7]
- Lopez-Poveda, E.A. & Meddis, R. (1996). "A physical model of sound diffraction and reflections in the human concha." *JASA*, 100(5), 3248–3259. — Computational model: concha gain drops 8–12 dB for rear vs. front incidence. Validated against KEMAR measurements. [9]
- Musicant, A.D. & Butler, R.A. (1984). "The influence of pinnae-based spectral cues on sound localization." *JASA*, 75(4), 1195–1200. — Pinna occlusion (blocking concha) severely degrades front/back discrimination. [10]
- Spagnol, S., Geronazzo, M., & Avanzini, F. (2013). "On the relation between pinna reflection patterns and HRTF features." *IEEE Trans. ASLP*, 21(3), 508–519. — Ray-tracing on pinna contours: concha wall is primary reflecting surface for directional spectral features. [11]

**Implementation**: Broad bell filter at 3.5 kHz, Q=0.8 (wide), gain Y-mapped: 0 dB (front)
→ −5 dB (rear). Supplements the existing narrower ear canal peak (2.7 kHz, Q=2.0) with a
broader concha effect.

**Generalizability**: HIGH — all humans have a concha with similar directional properties.

**Priority**: MEDIUM-HIGH — broadens the existing 2.7 kHz cue into a more realistic
directional response.

---

## ELEVATION (ABOVE/BELOW)

### EL-1: Pinna P2 Peak (~8 kHz, Elevation-Dependent)

**What**: The existing P1 peak (5 kHz, fixed) serves as a spectral anchor. Research shows
a second pinna resonance peak (P2) at ~7–9 kHz that IS elevation-dependent and significantly
improves upper-hemisphere localization.

**Research**:
- Iida, K. & Ishii, Y. (2018). "Effects of adding a spectral peak generated by the second pinna resonance to a parametric model of head-related transfer functions on upper median plane sound localization." *Applied Acoustics*, 129, 239–247. — Adding P2 to N1+N2+P1 eliminated statistically significant differences from full measured HRTFs at ALL tested vertical angles (0°–180°). [12]
- Iida, K., Itoh, M., Itagaki, A., & Morimoto, M. (2007). "Median plane localization using a parametric model of the head-related transfer function based on spectral cues." *Applied Acoustics*, 68(8), 835–850. — Established N1+N2+P1 as the minimal parametric model. P2 was identified as the remaining gap. [13]
- Takemoto, H., Mokhtari, P., Kato, H., Nishimura, R., & Iida, K. (2012). "Mechanism for generating peaks and notches of head-related transfer functions in the median plane." *JASA*, 132(6), 3832–3841. — Physical mechanism: P2 arises from second-mode resonance of the concha cavity. [14]
- Spagnol, S. & Avanzini, F. (2015). "Frequency estimation of the first pinna notch in head-related transfer functions with a linear anthropometric model." *Proc. DAFx*, NTNU Trondheim. — Statistical analysis of pinna spectral features across databases. [15]

P2 frequency: ~7 kHz (below) → ~9 kHz (above). Represents a second concha/helix resonance mode.

**Implementation**: Bell filter, Z-mapped frequency: 7 kHz (Z=−1) → 9 kHz (Z=+1), gain +3 dB,
Q=2.0. Fills the spectral gap between P1 (5 kHz, fixed) and N1 (6.5–10 kHz, notch) — giving the
auditory system a second reference peak to triangulate against the notches.

**Generalizability**: MEDIUM-HIGH — P2 exists across measured HRTF databases, though exact
frequency varies ~2 kHz across individuals.

**Priority**: HIGH — Iida's research specifically shows this is the missing piece that brings
parametric models to full-HRTF accuracy. Current model has P1+N1+N2 but NOT P2.

---

### EL-2: Below-Horizon Spectral Darkening (Low Shelf Boost)

**What**: Beyond the existing pinna high shelf (+3 dB above at 4 kHz), elevation perception
correlates with broadband spectral centroid — the overall "center of gravity" of the spectrum.
This is a coarser cue than pinna notches but more robust across individuals.

**Research**:
- Zonooz, B., Arani, E., Körding, K.P., & Van Opstal, A.J. (2019). "Spectral weighting underlies perceived sound elevation." *Scientific Reports*, 9, 1642. — Trial-by-trial perceived elevation correlates strongly with spectral energy in 2–10 kHz band. Spectral centroid is a reliable elevation predictor across subjects. [16]
- Middlebrooks, J.C. (1992). "Narrow-band sound localization related to external ear acoustics." *JASA*, 92(5), 2607–2624. — Higher-frequency stimuli perceived at higher elevation. Systematic frequency-to-elevation mapping across listeners. [17]
- Hebrank, J. & Wright, D. (1974). "Spectral cues used in the localization of sound sources on the median plane." *JASA*, 56(6), 1829–1834. — "Above" cue: 1-octave peak between 7–9 kHz. "Below" cue: broad low-frequency emphasis. [4]
- Lladó, P., Hyvärinen, P., & Pulkki, V. (2022). "Spectral manipulation improves elevation perception with non-individualized head-related transfer functions." *JASA*, 145(3), EL222–EL228. — Spectral manipulation (brightness adjustments) improved elevation accuracy even with generic HRTFs. [18]

**Implementation**: Low shelf at 500 Hz, Z-mapped: 0 dB (above) → +2 dB (below). Darkens
below-horizon sources by adding low-frequency weight, shifting spectral centroid downward.
Combined with existing HF shelf, creates full spectral tilt: below = warm/dark, above = bright/airy.

**Generalizability**: HIGH — spectral centroid perception is universal.

**Priority**: MEDIUM — supplements existing elevation cues with broadband tilt that works even
if pinna notch frequencies don't match the listener.

---

### EL-3: Torso Shadow LPF (Below-Horizon Direct Path Darkening)

**What**: For sources well below horizontal, the torso physically shadows the ears, creating
broadband HF damping up to 20–25 dB. Distinct from existing chest/floor bounce delays (which
model reflections). Torso shadow is the direct path being blocked.

**Research**:
- Algazi, V.R., Duda, R.O., Duraiswami, R., Gumerov, N.A., & Tang, Z. (2002). "Approximating the head-related transfer function using simple geometric models of the head and torso." *JASA*, 112(5), 2053–2064. — Snowman model: torso diffraction creates shadow zone below about −20° elevation. Torso radius ~17 cm, neck height ~5.3 cm. [19]
- Lan, B., Arend, J.M., & Pörschmann, C. (2022). "Effect of torso reflections from simplified torso models on the simulation of head-related transfer functions." *Applied Acoustics*, 199, 109009. — Semi-ellipsoidal torso models show 15–25 dB HF damping at −45° to −90° elevation. More anatomically accurate than spherical models. [20]
- Algazi, V.R., Duda, R.O., Morrison, R.P., & Thompson, D.M. (2001). "Structural composition and decomposition of HRTFs." *Proc. IEEE WASPAA*, New Paltz, NY, 103–106. — Demonstrated that HRTFs can be decomposed into head, torso, and pinna components operating in distinct frequency ranges. [21]

**Implementation**: SVF low-pass, Z-mapped: 22 kHz (above/horizontal, transparent) → 3 kHz
(Z=−1, strong below-horizon darkening). Only activates for Z < −0.2. Analogous to how rear
shadow LPF works for Y, but for extreme below-horizon Z values.

**Generalizability**: HIGH — all humans have a torso that shadows below-horizon sound.

**Priority**: MEDIUM — the chest bounce delay already provides a below cue, but torso shadow
adds the direct-path darkening that's physically correct for sub-horizontal sources. Currently
below-horizon sources sound the same brightness as horizontal ones (just with added bounce).

---

### EL-4: Elevation-Dependent Comb Filter Tuning

**What**: The existing comb filter bank has fixed delays (0.21–1.5 ms) and is only Y-driven
(front/back). In reality, torso reflection delay varies with elevation — the path length from
source→shoulder→ear changes with elevation angle.

**Research**:
- Algazi, V.R. et al. (2002). "Approximating the HRTF using simple geometric models of the head and torso." *JASA*, 112(5), 2053–2064. — Torso reflection delay maximum ~0.7 ms (first comb null at ~700 Hz) when source is overhead (ear-shoulder-source aligned), decreases to zero entering torso shadow zone. [19]
- Brown, C.P. & Duda, R.O. (1998). "A structural model for binaural sound synthesis." *IEEE Trans. Speech Audio Proc.*, 6(5), 476–488. — Structural model includes elevation-dependent torso reflection path with parameterized delay. [8]
- Arend, J.M., Pörschmann, C., & Weinzierl, S. (2023). "A torso reflection model for dynamic rendering of head-related transfer functions." *Forum Acusticum 2023*, Turin. — Dynamic torso model: continuous delay variation with elevation, reflection amplitude peaks at shoulder-ear-source alignment (~+30° elevation). [22]

**Implementation**: Scale comb bank delay times by Z-dependent factor: Z=+1 (overhead) delays
at 1.0× (maximum torso reflection). Z=0 (horizontal) delays at 0.6×. Z=−1 (below) delays at
0.2× (minimal, torso shadow dominates). Also modulate comb wet mix with Z: peak wetness at
Z≈+0.3 (shoulder-ear alignment), tapering above and below.

**Generalizability**: HIGH — torso geometry is universal.

**Priority**: LOW-MEDIUM — the comb bank is already Y-driven; adding Z-modulation is a refinement.

---

## EXTERNALIZATION & REALISM

### EX-1: Interaural Decorrelation (L/R Allpass Divergence)

**What**: Already identified in spatial-cues.md as item #23. Subtle frequency-dependent phase
divergence between L/R ears makes sound appear outside the head rather than between the ears.

**Research**:
- Catic, J., Santurette, S., Buchholz, J.M., Gran, F., & Dau, T. (2013). "The effect of interaural-level-difference fluctuations on the externalization of sound." *JASA*, 134(2), 1232–1241. — ILD fluctuations from reverberation drive externalization above 1 kHz. Smooth ILD = in-head; fluctuating ILD = externalized. [23]
- Hassager, H.G., Gran, F., & Dau, T. (2016). "The role of spectral detail in the binaural transfer function on perceived externalization in a reverberant environment." *JASA*, 139(5), 2992–3000. — Spectral detail in binaural transfer function critical for externalization. Smoothing HRTF spectral features degrades externalization. [24]
- Hartmann, W.M. & Wittenberg, A. (1996). "On the externalization of sound images." *JASA*, 99(6), 3678–3688. — Early reflections with appropriate interaural decorrelation are essential for externalization. Direct HRTF alone produces in-head localization. [25]
- Werner, S., Klein, F., Mayenfels, T., & Brandenburg, K. (2016). "A summary on acoustic room divergence and its effect on externalization of auditory events." *Proc. IEEE QoMEX*, Lisbon. — Even minimal room divergence (L/R decorrelation from early reflections) dramatically improves externalization. [26]

**Implementation**: Short allpass chain (3–4 stages) with slightly different coefficients per
ear. Decorrelation amount scales with distance: close = high coherence (intimate), far = more
decorrelation (spacious). Very low CPU cost.

**Generalizability**: HIGH — universal perceptual phenomenon.

**Priority**: HIGH — #1 reported problem with binaural audio is in-head localization.

---

## Summary Table

| ID | Cue | Category | Generalizability | CPU Cost | Priority |
|----|-----|----------|-----------------|----------|----------|
| FB-3 | Front-back ILD asymmetry | Front/Back | HIGH | Zero (modify existing) | HIGH |
| EL-1 | Pinna P2 peak (7–9 kHz) | Elevation | MED-HIGH | 1 biquad | HIGH |
| EX-1 | Interaural decorrelation | Externalization | HIGH | 2 allpass chains | HIGH |
| FB-4 | Concha shadow (2–4 kHz dip) | Front/Back | HIGH | 1 biquad | MED-HIGH |
| FB-1 | Blauert front LF band (400 Hz) | Front/Back | HIGH | 1 biquad | MEDIUM |
| FB-2 | Rear HF peak (11 kHz) | Front/Back | MED-HIGH | 1 biquad | MEDIUM |
| EL-2 | Below-horizon spectral darkening | Elevation | HIGH | 1 biquad | MEDIUM |
| EL-3 | Torso shadow LPF | Elevation | HIGH | 1 SVF | MEDIUM |
| EL-4 | Elevation-dependent comb tuning | Elevation | HIGH | Zero (modify existing) | LOW-MED |

**Total new processing**: ~5 biquads + 1 SVF + 2 allpass chains + 2 parameter mods to existing filters.

---

## Cues Considered and Rejected

| Cue | Reason |
|-----|--------|
| Bone conduction | Max bone ITD = 0.2 ms vs 0.65 ms air. Negligible spatial info. |
| Freq-dependent ITD (Kuhn 3/2) | Requires dual delay lines per ear. Broadband ITD "close enough." Already in spatial-cues.md #24. |
| Dynamic cues (head movement) | Requires head tracking hardware. Already V2 in spatial-cues.md. |
| Spectral tilt for distance | Already covered by 2-stage air absorption. Marginal gain. |
| Cross-frequency ILD patterns | Already naturally produced by head shadow SVF. No separate implementation needed. |
| Precedence effect / early reflections | Already handled by chest/floor bounce + reverb FDN. More reflections = reverb territory. |

---

## References

[1] Blauert, J. (1969/70). "Sound localization in the median plane." *Acustica*, 22, 205–213.

[2] Blauert, J. (1997). *Spatial Hearing: The Psychophysics of Human Sound Localization* (rev. ed.). MIT Press.

[3] Morimoto, M. & Aokata, H. (1984). "Localization cues of sound sources in the upper hemisphere." *J. Acoust. Soc. Japan (E)*, 5(3), 165–173.

[4] Hebrank, J. & Wright, D. (1974). "Spectral cues used in the localization of sound sources on the median plane." *JASA*, 56(6), 1829–1834.

[5] Macpherson, E.A. & Middlebrooks, J.C. (2002). "Listener weighting of cues for lateral angle: The duplex theory of sound localization revisited." *JASA*, 111(5), 2219–2236.

[6] Macpherson, E.A. (2013). "Cue weighting and vestibular mediation of temporal dynamics in sound localization via head rotation." *Proc. Meetings on Acoustics*, 19, 050131.

[7] Shaw, E.A.G. (1974). "Transformation of sound pressure level from the free field to the eardrum in the horizontal plane." *JASA*, 56, 1848–1861.

[8] Brown, C.P. & Duda, R.O. (1998). "A structural model for binaural sound synthesis." *IEEE Trans. Speech Audio Proc.*, 6(5), 476–488.

[9] Lopez-Poveda, E.A. & Meddis, R. (1996). "A physical model of sound diffraction and reflections in the human concha." *JASA*, 100(5), 3248–3259.

[10] Musicant, A.D. & Butler, R.A. (1984). "The influence of pinnae-based spectral cues on sound localization." *JASA*, 75(4), 1195–1200.

[11] Spagnol, S., Geronazzo, M., & Avanzini, F. (2013). "On the relation between pinna reflection patterns and HRTF features." *IEEE Trans. ASLP*, 21(3), 508–519.

[12] Iida, K. & Ishii, Y. (2018). "Effects of adding a spectral peak generated by the second pinna resonance to a parametric model of head-related transfer functions on upper median plane sound localization." *Applied Acoustics*, 129, 239–247.

[13] Iida, K., Itoh, M., Itagaki, A., & Morimoto, M. (2007). "Median plane localization using a parametric model of the head-related transfer function based on spectral cues." *Applied Acoustics*, 68(8), 835–850.

[14] Takemoto, H., Mokhtari, P., Kato, H., Nishimura, R., & Iida, K. (2012). "Mechanism for generating peaks and notches of head-related transfer functions in the median plane." *JASA*, 132(6), 3832–3841.

[15] Spagnol, S. & Avanzini, F. (2015). "Frequency estimation of the first pinna notch in head-related transfer functions with a linear anthropometric model." *Proc. DAFx*, NTNU Trondheim.

[16] Zonooz, B., Arani, E., Körding, K.P., & Van Opstal, A.J. (2019). "Spectral weighting underlies perceived sound elevation." *Scientific Reports*, 9, 1642.

[17] Middlebrooks, J.C. (1992). "Narrow-band sound localization related to external ear acoustics." *JASA*, 92(5), 2607–2624.

[18] Lladó, P., Hyvärinen, P., & Pulkki, V. (2022). "Spectral manipulation improves elevation perception with non-individualized head-related transfer functions." *JASA*, 145(3), EL222–EL228.

[19] Algazi, V.R., Duda, R.O., Duraiswami, R., Gumerov, N.A., & Tang, Z. (2002). "Approximating the head-related transfer function using simple geometric models of the head and torso." *JASA*, 112(5), 2053–2064.

[20] Lan, B., Arend, J.M., & Pörschmann, C. (2022). "Effect of torso reflections from simplified torso models on the simulation of head-related transfer functions." *Applied Acoustics*, 199, 109009.

[21] Algazi, V.R., Duda, R.O., Morrison, R.P., & Thompson, D.M. (2001). "Structural composition and decomposition of HRTFs." *Proc. IEEE WASPAA*, New Paltz, NY, 103–106.

[22] Arend, J.M., Pörschmann, C., & Weinzierl, S. (2023). "A torso reflection model for dynamic rendering of head-related transfer functions." *Forum Acusticum 2023*, Turin.

[23] Catic, J., Santurette, S., Buchholz, J.M., Gran, F., & Dau, T. (2013). "The effect of interaural-level-difference fluctuations on the externalization of sound." *JASA*, 134(2), 1232–1241.

[24] Hassager, H.G., Gran, F., & Dau, T. (2016). "The role of spectral detail in the binaural transfer function on perceived externalization in a reverberant environment." *JASA*, 139(5), 2992–3000.

[25] Hartmann, W.M. & Wittenberg, A. (1996). "On the externalization of sound images." *JASA*, 99(6), 3678–3688.

[26] Werner, S., Klein, F., Mayenfels, T., & Brandenburg, K. (2016). "A summary on acoustic room divergence and its effect on externalization of auditory events." *Proc. IEEE QoMEX*, Lisbon.
