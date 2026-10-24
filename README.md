# MOSAIC

**A dynamic modular sampler plugin — map any hardware pad controller, drop in samples, and play.**

MOSAIC turns your Launchpad, LPD8 or any MIDI pad grid into a colour-coded, fully
modular sampler. Build a grid of up to 8×8 pads, drag a sample onto each, pick how
it triggers, shape it, modulate it, and route pads to separate outputs — all in a
compact, good-looking interface.

Formats: **VST3**, **AU**, **Standalone** · Windows & macOS (universal) · Linux (VST3/Standalone)

---

## Features

- **Dynamic pad grid** — resize from 1×1 up to 8×8; pads reflow automatically.
- **Drag & drop** `.wav` / `.mp3` / `.aif`, with waveform display and visual trim.
- **Three trigger modes per pad** — Momentary (gate), Latching (one-shot), Looping (tap to toggle).
- **Three pitch/time engines per pad:**
  - **Repitch** — classic tape/varispeed, pristine.
  - **Granular** — independent pitch & time, with a Grain-size control.
  - **Studio** — high-quality independent pitch/time (powered by Signalsmith Stretch).
- **Sound shaping** — Volume, Pan, Pitch (±24 st), Speed (0.25–4×), Attack, Release, with the envelope drawn over the waveform.
- **3 global LFOs** — Sine/Tri/Saw/Square/S&H, free or **tempo-synced**, routable to Volume / Pan / Pitch / Speed on all pads or the selected pad.
- **Multi-out routing** — send pads to up to 8 stereo outputs for per-pad mixing.
- **MIDI learn** — map a note to each pad; map hardware CC knobs to the inspector controls.
- **Hardware LED feedback** — lights your controller's pads in their assigned colours, and keeps looping pads lit.
- **Multi-select & bulk edit**, **custom pad colours**, **playback cursor**, **"follow last played"**, and a **preset** system.

## Download

Pre-built installers are produced by CI for every release:
- **Releases:** grab the `MOSAIC-Windows.zip` / `MOSAIC-macOS.zip` / `MOSAIC-Linux.zip` from the [Releases](../../releases) page.
- **Latest builds:** the artifacts attached to each run under the [Actions](../../actions) tab.

### Installing
- **Windows:** copy `MOSAIC.vst3` to `C:\Program Files\Common Files\VST3`.
- **macOS:** copy `MOSAIC.vst3` to `~/Library/Audio/Plug-Ins/VST3` and `MOSAIC.component` to `~/Library/Audio/Plug-Ins/Components`.
- Or just run the **Standalone** app.

## Build from source

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

JUCE and Signalsmith Stretch are fetched automatically by CMake (internet required on the first configure).

## Licensing

MOSAIC is released under the **GNU AGPLv3** (see [LICENSE](LICENSE)). It builds on
**JUCE** (used here under its AGPL option) and **Signalsmith Stretch** (MIT) —
see [THIRDPARTY.md](THIRDPARTY.md). Under AGPLv3 you may use, modify and sell
builds of MOSAIC, provided the complete corresponding source is made available
to recipients under the same licence.

© Ioannis Bouritis.
