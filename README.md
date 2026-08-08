# Casio-RZ-1 (MAME to VST3/AU Instrument)

A software instrument that wraps MAME's **Casio RZ-1** driver (a 1986 sampling
drum machine) as a native arm64 **VST3** and **Audio Unit** plugin using JUCE.
The plugin embeds a headless MAME instance: the RZ-1 panel (800x535 `rz1.lay`)
is rendered into the plugin editor, clicks drive the emulated key matrix, and
the 10 drum voices are summed into the stereo output.

This repo was carved out of the original Ensoniq SD-1 adaptation project; only
what is needed to build the two plugin formats is kept.

## Status

- VST3 works in Element (panel, buttons, MIDI, LCD refresh, pattern
  record/playback).
- AU works in Logic (the AU defaults to a larger internal MAME buffer to avoid
  Logic's real-time scheduler underruns).
- `auval` from the CommandLineTools fails to enumerate this plugin (and the
  sibling SD-1 AU) with "didn't find the component" even though the system
  AudioComponent registry instantiates it fine - treat Logic as the validator.

## Requirements

- macOS (arm64; the build is hard-set to arm64 in `CMakeLists.txt`)
- CMake 3.22+, a C++20 toolchain
- SDL2 via Homebrew (`brew install sdl2`)
- The RZ-1 ROM set (see below - **not** shipped with this repo)

## Building

```bash
cd Casio-RZ-1
cmake -S . -B build -DJUCE_DIR=work/JUCE -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build -j8
```

The bundles land in `build/CasioRZ1_artefacts/`:

- `VST3/Casio RZ-1.vst3`
- `AU/Casio RZ-1.component`

The AU bundle gets the MAME `plugins/` resources installed and ad-hoc
codesigned automatically (Logic loads it locally).

To use them, copy the bundles into `~/Library/Audio/Plug-Ins/VST3/` and
`~/Library/Audio/Plug-Ins/Components/` (or the `/Library/...` equivalents),
then rescan/restart the host.

## ROMs (required, not included)

Place the RZ-1 machine ROMs in `~/Documents/CasioRZ1/ROMs/rz1/`:

```text
upd7811g-120.bin   (4 KB  - CPU microcode)
program.bin        (16 KB - main program ROM)
sound_a.cm5        (32 KB - drum waveforms part A)
sound_b.cm6        (32 KB - drum waveforms part B)
hd44780_a00.bin    (4 KB  - HD44780 LCD character-generator device ROM)
```

Missing `hd44780_a00.bin` = black panel (the plugin self-checks and shows a
red error). The 2018-era dump (CRC 01d108e2) boots with a checksum warning;
MAME 2025 expects CRC e459877c. The plugin skips MAME's modal boot-warning
screens (see below), so the mismatch is cosmetic.

## MAME patch

`Mame_Patches/ui.cpp` is the "VST HACKED" `display_startup_screens()` override
(skip all startup dialogs, go straight to in-game mode) and **is already
applied** to `mame-mac-master/src/frontend/mame/ui/ui.cpp` in this tree. Without
it, MAME's "press any key" ROM-audit warning blocks boot because the plugin has
no keyboard input path.

The prebuilt MAME libraries live in
`mame-mac-master/build/osx_clang/bin/x64/Release/` (the plugin links these
directly). The MAME source tree is kept so the libs can be rebuilt; see
`PHASE2-BUILD-ROADMAP.md` for context.

## Notes

- The RZ-1 boots with empty pattern memory (MAME NVRAM starts blank), so
  START/STOP does nothing until you record a pattern: PATTERN -> number ->
  EDIT/RECORD -> START/STOP -> hit pads -> START/STOP -> START/STOP.
- The numpad keys are data-entry keys: they act after PATTERN/SONG/EDIT-RECORD,
  not standalone.
- Plugin "Internal Buffer" default: 2048 samples for the AU (Logic stability),
  512 for VST3.
- See `TESTING.md` for the Element/Logic test checklists and diagnostics.

## Legal

No ROMs or disk images are shipped - the plugin is non-functional without them,
so the repo is distributable. MAME is GPL-2.0-or-later and JUCE is used under
its GPLv3 terms (see `LICENSE`). "Casio RZ-1" is MAME's own display name for
the emulated hardware (nominative use); this project is not affiliated with or
endorsed by Casio.
