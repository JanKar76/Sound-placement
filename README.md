# Sound Placement

> A free VST3 plugin that lets you place a sound in a virtual room — drag a dot to pan left/right and push the source forward or back in depth.

![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)
![JUCE](https://img.shields.io/badge/JUCE-8-8DC63F.svg)
![CMake](https://img.shields.io/badge/CMake-3.22+-064F8C.svg)
![Formats](https://img.shields.io/badge/formats-VST3_|_Standalone-orange.svg)

![Screenshot](docs/screenshot.png)

## How it works

Your ear judges distance from several cues at once, so the **Distance** (Y) axis drives four of them simultaneously:

| Cue | Implementation |
|-----|----------------|
| Level | Up to −15 dB attenuation at the back of the room |
| Air absorption | Lowpass cutoff falls exponentially, 20 kHz → 1.8 kHz |
| Stereo width | Mid/side narrowing — distant sources sound more point-like |
| Room sound | Convolution reverb wet level rises with distance |

Two extra depth cues come for free: **early reflections** (a decorrelated multi-tap delay whose level scales with distance) and a **distance-linked predelay** — near sources get a ~30 ms gap before the room answers, far sources almost none.

**Pan** (X) uses a constant-power balanced pan law.

The reverb is `juce::dsp::Convolution` running a synthetic impulse response: exponentially decaying noise, progressively lowpassed through the tail to mimic air and wall absorption. **Room Size** maps to RT60 (0.4–3.0 s) and regenerates the IR on a background thread, glitch-free.

## Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| Pan | −1 … 1 | Left/right (constant-power) |
| Distance | 0 … 1 | Near … far |
| Room Size | 0 … 1 | RT60 0.4–3.0 s, also scales reflection times |
| Early Reflections | 0 … 1 | Multi-tap reflection level (distance-scaled) |
| Mid Level | −60 … +6 dB | Gain for the mid (L+R) signal |
| Side Level | −60 … +6 dB | Gain for the side (L−R) signal |
| Output | −24 … +12 dB | Output trim |
| Mix | 0 … 1 | Global dry/wet |
| Bypass | on/off | Also exposed to the host via `getBypassParameter()` |

Hover over the pad for a readout of the pan amount and the straight-line distance (in metres) from listener to source.

## Building

Requirements: CMake 3.22+ and a C++17 compiler (Visual Studio 2022, Xcode, or GCC/Clang). JUCE is fetched automatically via CMake `FetchContent` on first configure — no manual setup.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The VST3 ends up in `build/SoundPlacement_artefacts/Release/VST3/` and is copied to your system VST3 folder automatically. A Standalone build is also produced — handy for quick tests without a DAW.

On Linux you'll need the usual JUCE dependencies:

```sh
sudo apt install libasound2-dev libx11-dev libxext-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libfreetype6-dev libfontconfig1-dev
```

## Roadmap

- Load real impulse response files (WAV) instead of the synthetic IR
- Binaural HRTF mode for headphones
- Doppler when automating the position
- Automatic motion (LFO paths around the room)

Contributions and issues welcome.

## License

Copyright © 2026<img width="1250" height="1340" alt="SoundPlacement" src="https://github.com/user-attachments/assets/39bb652d-b150-4941-a920-50a4ac5b1773" />


This project is licensed under the **GNU Affero General Public License v3.0** — see [LICENSE](LICENSE).

It is built on the [JUCE framework](https://juce.com), used under its AGPLv3 option (JUCE is not included in this repository; it is downloaded at configure time). The VST3 SDK bundled with JUCE is used under Steinberg's GPLv3 option. VST is a trademark of Steinberg Media Technologies GmbH.
