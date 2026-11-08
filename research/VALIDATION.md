# Validation record

Last run: 6 November 2026, macOS (Apple silicon), universal (arm64 + x86_64) Release build against JUCE 9.0.5.

## Completed checks

### Engine (`tests/EngineTests.cpp`, run by `ctest`)

- **Stress:** 10,703,616 frames across 44.1, 48, 96 and 192 kHz, covering every mode and extreme control combination. Every sample is finite and within the 0.98 sample ceiling; the observed peak was 0.98.
- **Stereo:** identical processing on both channels with independent audio histories. Opposite-polarity inputs cancel exactly.
- **Static modes:** Rate has no effect in Filter Matrix and Double Track.
- **Width:** at zero, Width removes all modulation in Flange and Chorus; above zero, it adds modulation.
- **Tune direction:** clockwise shortens the delay in Matrix and lengthens it in Double Track. The two delay families stay distinct, with the long family more than four times the short.
- **Automation:** rapid parameter automation is deterministic and stays bounded.
- **Bypass:** low-level signals pass at unity gain in steady bypass.
- **Feedback:** a 30-second tail at maximum feedback stays finite and bounded and still sustains oscillation after excitation.
- **Pitch cap:** in Flange and Chorus, pitch deviation never exceeds Width × 1 semitone across Rate, Tune, Width and Sweep extremes (revision 2).
- **Reset:** two instances produce identical output after a reset.

### Built plugin (`tests/HostCheck.cpp`, built with `-DRESONANT_BUILD_HOST_CHECK=ON`)

The built `Resonant Four.vst3` is tested through JUCE's VST3 hosting:

- **Scan and load:** the scan finds exactly one plugin, and the plugin loads.
- **Presets:** three named presets are exposed. Each one loads, renders signal, and has its program and parameter state restored correctly in a second instance.
- **Rendering:** mono and stereo, with variable block sizes. Output is finite and bounded, and a restored instance renders identically to the original.
- **Buses:** mismatched input/output layouts are rejected.
- **Editor:** the editor is created.

### Other

- **Small Clone:** the sources match the SHA-256 checksums taken before work began (`small-clone-before.sha256`), and the installed Small Clone plugins are untouched.
- **Signing:** the plugin is ad-hoc signed, and `codesign --verify --deep` passes. It is not notarised.

## Fixes made after the first validation pass

- Preset names and the editor subtitle used non-ASCII characters (`—`, `•`) in plain `char*` literals. JUCE read these as non-UTF-8, so hosts showed garbled text such as `â`. They are now ASCII: "Heart-Shaped Box (inspired)", and so on.

## Revision 2 (RFUS listening fix)

A listening test showed the RFUS preset was a seasick echo: a 28–79 ms sweep with ±9 semitones of pitch movement. The delay law, wet-path filtering and presets were revised, as described in `RESEARCH.md` under Revision 2. All engine and plugin checks above pass on the revised build, which is installed.

**Listening result:** variant B (long) was closest to the record's intro, and it is now the only RFUS preset. It was judged "very close" but not yet exact.

## Known limits / not verified

- **Editor preview:** the test host's automatic snapshot comes out black, because it captures a native VST3 view through the wrapper. `research/editor-preview.png` is a screenshot from a real host instead. It shows the editor rendering correctly with the Heart-Shaped Box preset loaded, and that visual check passed on 6 November 2026.
- **Hosts:** no testing in specific DAWs (Logic needs AU; Ableton, Reaper and Cubase haven't been run). No pluginval or Steinberg validator run.
- **Hardware comparison:** none. No delay or clock measurements, feedback-threshold curves or matched recordings.
- **Not modelled:** the NE570 compander, BBD clock leakage, oversampling, and true-peak / intersample limiting. Saturation and fast modulation can alias.
- **Linux and Windows:** builds not verified.
- **Local build JUCE path:** the local `build-mac` directory uses the JUCE checkout at `/private/tmp/small-clone-vst3/build-mac/_deps/juce-src`, which macOS may clear. A fresh build directory without `JUCE_SOURCE_DIR` fetches JUCE 9.0.5 instead.
