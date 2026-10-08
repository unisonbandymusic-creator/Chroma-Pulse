# Chroma-Pulse

**Him'z DSP** - a minimalist, set-and-forget bus finisher (VST3 / AU / Standalone) built with JUCE.

## Signal flow

1. **Micro-drift** (host rate): slow random-walk fluctuations in gain (up to about +/-0.6 dB) and stereo pan (about +/-0.8 dB L/R), before the saturator.
2. **Oversampled saturation** (2x / 4x / 8x, `juce::dsp::Oversampling`, linear-phase half-band FIR): asymmetric tanh waveshaper. *Character* blends odd (symmetric) toward even (biased) harmonics. Small-signal gain stays at unity and only the added harmonic content is DC-blocked.
3. **Output trim + analog noise floor** (host rate): optional ultra-quiet pink noise, roughly -96 to -60 dBFS.

Latency is reported to the host and follows the oversampling setting.

## Parameters

| Parameter | Range | Default |
|---|---|---|
| Drive | 0-100 % | 25 |
| Character (odd to even) | 0-100 % | 40 |
| Drift | 0-100 % | 30 |
| Drift Rate | 0.02-2 Hz | 0.15 |
| Noise Floor | 0-100 % (0 = off) | 0 |
| Output | -12 to +12 dB | 0 |
| Oversampling | 2x / 4x / 8x | 4x |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE 8.0.4 is fetched automatically. Built plugins land under `build/ChromaPulse_artefacts/`.
AU is built on macOS only.
