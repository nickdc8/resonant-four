# Resonant Four — circuit research and DSP plan

Prepared 6 November 2026, before DSP implementation. Independent EchoFlanger / PolyChorus-inspired instrument effect; not an EHX product or a measured replica of Cobain's unit.

## Evidence and revision boundaries

The primary circuit reference is the two-sheet **EH-7700 EchoFlanger, revision 157, 8 March 1978** scan. Both sheets were downloaded and visually inspected. They contain handwritten corrections and partly illegible values, so they cannot establish a uniquely calibrated factory response. The source links below are provided for research; this repository does not redistribute the schematic scans or handwritten-notes image.

1. [1978 control/clock/power schematic](https://www.experimentalistsanonymous.com/diy/Schematics/Flangers/Electro%20Harmonix%20Echo%20Flanger%201.jpg), linked source only.
2. [1978 audio-path schematic](https://www.experimentalistsanonymous.com/diy/Schematics/Flangers/Electro%20Harmonix%20Echo%20Flanger%202.jpg), linked source only.
3. [EHX Stereo PolyChorus XO manual](https://www.ehx.com/wp-content/uploads/2021/01/stereo-polychorus-manual.pdf). This is a **later revision**, used for operating terminology, not evidence of the 1978 component values.
4. [EHX: The Sound of Nirvana](https://www.ehx.com/blog/ehx-tone-tips-the-sound-of-nirvana/), including the pedal photograph and handwritten notes. EHX attributes use on all three requested songs to this pedal family. This does not prove the entire album sound came from one circuit or one setting.
5. [Handwritten settings image published by EHX](https://b2155914.assetcdn.net/2155914/wp-content/uploads/2021/03/ehx-tone-tips-3-kurt-notes.jpg), linked source only.

## What the schematic actually contributes

Sheet 1: LM324 oscillator/integrator, Rate pot marked 2M reverse log, Width 100K and Tune 100K networks, mode-controlled switching, LM311 clock generator, CD4013 divider and CD4049 clock drive. Width and Tune share the control-voltage network: independent arbitrary sine-depth and fixed delay would miss their interaction.

Sheet 2: input conditioning; NE570 compressor/expander; SAD1024A delay sections with selectable routing; reconstruction filters; feedback return through a 10K control; dry/wet output summing. S3 is explicitly labelled **BLEND**, not Sweep Filter. BBD clock rate controls delay, and recirculating filtered audio produces resonance. The scan's overwritten part values and switch annotations require caution.

This supports the architecture below, but not exact milliseconds, gain, calibration, pot taper, noise spectrum or saturation thresholds. We do not claim a transistor simulation, exact switch netlist solution or BBD chip emulation.

## Operating controls and modes

The later EHX manual describes Filter Matrix as a stationary comb, Flange as a sweeping comb, Chorus as a modulated echo and Double Track as an unmodulated echo. Rate is inactive in Matrix and Double Track. Tune moves towards shorter delay clockwise in the comb modes and longer delay in echo modes. Width interacts with Tune's range. Feedback can reach oscillation. Sweep Filter is described as making the effect rounder; this does not specify a transfer function.

**Revision decision:** retain a vintage Blend choice (mixed or wet only) and provide the requested Sweep Filter as a modern approximation: progressively blend a rounded sinusoid into the triangle clock-control sweep, and lower the wet/feedback bandwidth. Neither action is claimed to reproduce an identified 1978 Sweep Filter network. Keep this visible in the interface and guide.

## DSP architecture to implement

Reuse the Small Clone project's JUCE processor/APVTS/state and mono/stereo organisation, and its fractional-delay concept. Create a new project and plugin identity. Do not edit or build in Small Clone's tree. Implement a separate standard-C++ engine so its real-time path can be tested without a host.

Signal: input finite check → DC blocking → wet-path prefilter → bounded soft saturation at delay input → fractional delay → two-pole reconstruction low-pass → feedback DC blocker → feedback multiplier → delay input. Dry signal and wet output mix after the loop, followed by output trim and a sample-peak protection knee. Stereo uses independent audio loops with the same control phase, preserving mono compatibility; no invented stereo widening.

A rounded triangle is the starting LFO approximation to the oscillator/integrator. Use a reciprocal clock-inspired mapping: delay = 1 / interpolated reciprocal delay endpoints. Width sets excursion and also expands the available Tune range. This makes Rate, Tune and Width jointly control pitch displacement. Modes select short or long endpoint ranges; Matrix and Double Track remove LFO modulation, not manual delay control.

Proposed ranges (design choices, not measured): short family 0.65–12 ms; long family 5–100 ms; Rate 0.1–10 Hz; Width/Tune/Feedback 0–100%. Feedback maps 0–100% to 0–1.15, allowing bounded self-oscillation above unity. Delay-line input uses tanh saturation, the feedback return is DC blocked and low-pass filtered. Linear fractional interpolation is deliberately used inside the loop: its convex weights cannot overshoot stored values, unlike the Small Clone cubic interpolator. This trades some high-frequency detail for a simple stability guarantee. Maximum stored amplitude is bounded; maximum output sample magnitude is 0.98. This is not a true-peak limiter.

Smooth controls over ~30 ms, delay over ~10 ms, and bypass/mode gain over ~10 ms. Mode changes fade wet processing down, clear loop history, switch topology, then fade up. Dry passes during transition. No allocations, locks, file access or coefficient objects in the audio callback. Fault inputs become zero. Bypass fades and suppresses new loop input so disengaging a resonant preset is controlled.

## Preset provenance and initial digital translations

The handwritten note gives drawings rather than calibrated values. Its Heart-Shaped Box area contains both Chorus and Flange annotations; it is not an unambiguous numeric preset. The RFUS diagram is labelled Chorus. The Scentless Apprentice note says same with Double Track. The physical knob layout is Feedback upper left, Rate upper right, Width middle right, Tune lower right. The RFUS drawing suggests substantial feedback/width and high Tune. Readability and pot calibration prevent exact conversion to Hz or milliseconds.

| Preset (all labelled “inspired / estimated”) | Mode | Rate Hz | Width % | Tune % | Feedback % | Sweep Filter |
|---|---|---:|---:|---:|---:|---|
| Heart-Shaped Box (inspired) | Flange | 0.55 | 35 | 90 | 50 | Off |
| Radio Friendly Unit Shifter (inspired) | Chorus | 2.5 | 85 | 98 | 85 | Off |
| Scentless Apprentice (inspired) | Double Track | 2.5 (inactive) | 85 | 98 | 85 | Off |

*This table shows the original plan. Revision 2 below replaces the song presets.*

Mode choices follow the note's named possibilities; **every numerical value is an estimated voicing**, not a claimed transcription. Sweep Filter has no documented vintage setting here and defaults off. All presets use mixed Blend and -6 dB output trim. RFUS/Scentless share controls as suggested by the note. Tune and Width have different physical consequences in each mode; preset matching by knob position alone is unreliable. Guitar, distortion, amplifier and performance are outside this plugin. EHX suggests trying the effect before distortion for RFUS.

## What remains approximated or omitted

Exact BBD stage routing, chip charge transfer, clock leakage/aliasing, NE570 envelope law, component tolerances, calibrated nonlinear clock control, supply rails and original feedback polarity/gain are not reconstructed. The compander is represented only by wet-path level bounding; dynamic NE570 compression/expansion is **not implemented**. Filter cutoffs, delay ranges, triangle rounding, saturation, gain and smoothing are design choices. No recordings or hardware measurements have been used to validate a tonal match. The result is a circuit-informed behavioural instrument, with an explicit path to later calibration against hardware.

## Revision 2: Radio Friendly Unit Shifter listening fix (6 November 2026)

A listening test found the RFUS preset sounded nothing like the record. Measuring the engine showed why:

| RFUS preset measurement | Revision 1 | Revision 2 (variant B, same knob idea) |
|---|---|---|
| Delay sweep | 28–79 ms | 75–86 ms |
| Peak pitch wobble | ±9.2 semitones | ±0.74 semitones |
| Loop gain | 0.98 | 0.92 |

**Causes.**
1. **Delay law.** The reciprocal delay law is steepest at the long end, which is where Tune at 98% parks the delay. Width then swung it across 50 ms several times a second. Each repeat was bent again in pitch, producing a seasick echo cloud rather than a chorus.
2. **Dark repeats.** Three cutoff-tracking low-pass stages sat on the wet path, two of them inside the loop. That gave an effective bandwidth of about 1.6 kHz, darkening every repeat.
3. **Rate misread.** The sketch's Rate pointer is at about 1 o'clock, which is about 1.8 Hz on this plugin's taper, not 2.5 Hz.

**Changes** (all design approximations, not measurements):
- **Log delay scale.** Delay now moves on a log scale between the same endpoints, and Tune directions are unchanged.
- **Fixed sweep depth.** Full Width sweeps ±0.285 of the log range. At the ends of the Tune range, the sweep centre moves inward instead of the sweep collapsing, as a real oscillator would keep swinging the clock.
- **Pitch cap.** Pitch deviation |d(delay)/dt| is capped at Width × 1 semitone, whatever the Rate, Tune and Sweep settings, and a test enforces it.
- **Clock-linked bandwidth.** The wet bandwidth follows an assumed delay-chip clock: one 512-stage section in the comb modes, two chained in the echo modes. Bandwidth is 0.8 × clock Nyquist, capped at 9 kHz. Long echoes are therefore dark (about 2.5 kHz at 80 ms) and short delays stay bright. The input filter is now a fixed 9 kHz one-pole outside the loop.

**Re-reading the handwritten sketch.** Assuming a 7 o'clock–5 o'clock knob:
- **Feedback:** about 3 o'clock (~80%).
- **Rate:** about 1 o'clock.
- **Width:** about 4–5 o'clock (~85%).
- **Tune:** drawn at 6 o'clock, which is either fully down or fully up. The original plan assumed fully up.

A forum source ([Jag-Stang](https://www.jag-stang.com/forum/d/5492-nirvana-guitar-and-amp-settings), unverified) instead claims Flange mode with Width and Rate at about 11 o'clock, Tune at 12 and Feedback at 3. EHX advises placing the pedal before distortion for this song.

**Revision 2 presets** (all estimated):

| Preset | Mode | Rate Hz | Width % | Tune % | Feedback % | Result |
|---|---|---:|---:|---:|---:|---|
| Heart-Shaped Box (inspired) | Flange | 0.55 | 35 | 90 | 50 | 1.2–2.0 ms sweep |
| RFUS A - short ring | Chorus | 1.8 | 85 | 0 | 80 | 5–10 ms, ±0.4 st, metallic ring around 137 Hz |
| RFUS B - long | Chorus | 1.8 | 85 | 98 | 80 | 75–86 ms, ±0.74 st, dark repeats |
| RFUS C - flange | Flange | 0.52 | 40 | 50 | 80 | 2–4 ms resonant sweep |
| Scentless Apprentice (inspired) | Double Track | 1.8 (inactive) | 85 | 98 | 80 | static long echo |

The three RFUS variants were compared by ear against the record. **Variant B was closest to the intro effect**, so it ships as the single "Radio Friendly Unit Shifter (inspired)" preset, and variants A and C were removed. B was described as "very close" but not yet exact; any further refinement will be recorded here.

## Verification plan

Run deterministic standalone-engine tests at 44.1/48/96/192 kHz: all modes/control extremes; high input, silence, NaN/Inf rejection; automation and mode changes; bounded high-feedback tails; bypass; reset determinism; Rate inactivity in Matrix/Double Track; Width-zero Rate inactivity; mode distinction. Exercise processor state/preset recall and actual VST3 scan/render if a host or validator is available. Build a universal macOS VST3, sign and verify it, install under a separate name. Record actual tests and limits after implementation; do not describe a build as a listening comparison.
