# Phase 2: Build Roadmap for Casio RZ-1 VST Plugin

## Current Status Overview
**Adaptation Stage:** Infrastructure & Source Code (70% Complete)
- ✅ Core infrastructure adapted for RZ-1
- ✅ MAME driver integrated
- ✅ Compilation testing passed (VST3 + AU both build cleanly on arm64)
- ✅ VST3 + AU plugin bundles built and installed
- ⏳ DAW testing & verification pending (Element)
- ⏳ AU validation in Logic pending (auval check deferred)

## Completed Preparations ✅
- ✅ Driver registration updated (rz1 driver added to driver list in PluginProcessor.cpp)
- ✅ MAME boot target changed from "sd132" to "rz1" in PluginProcessor.cpp
- ✅ ROM file verification updated for **5** RZ-1 ROMs (incl. the `hd44780_a00.bin`
     HD44780 LCD device ROM - recipe §2: missing device ROM = black panel)
- ✅ ROM extraction/copy functions updated for RZ-1 paths (~/Documents/CasioRZ1/),
     including `rz1.zip` auto-extraction and `hd44780_a00.bin`
- ✅ Settings/state/NVRAM paths updated from EnsoniqSD1 to CasioRZ1
- ✅ ROM files copied to ~/Documents/CasioRZ1/ROMs/rz1/
- ✅ CMakeLists.txt configured for CasioRZ1 (plugin name, codes, paths)
- ✅ MAME libraries pre-compiled and available in mame_libs/ (59MB libemu.a, etc.)
- ✅ RZ-1 MAME driver present: mame-mame0289/src/mame/casio/rz1.cpp
- ✅ RZ-1 MAME layout present: mame-mame0289/src/mame/layout/rz1.lay
- ✅ **Audio stride fixed (recipe §3):** RZ-1 = **11 output channels** (verified via
     `-wavwrite` WAV header at 48 kHz). Plugin now sums the 10 mono drum voices into
     L/R (channel 10 = cassette/line-in is dropped). The old stride-5 (SD-1) read was
     the cause of the silent "L=0 R=0" audio.
- ✅ **RZ-1 button matrix (recipe §2):** SD-1 `:panel:buttons_*` table replaced with
     the real kc0..kc7/foot key-matrix buttons, with click rectangles transcribed from
     rz1.lay (800x535). Editor mouse hit-testing drives `ioport_field::set_value()`.
- ✅ **Boot failure handling (recipe §5):** `cli_frontend::execute()` return code is
     captured; non-zero marks the engine dead (no DAW freeze) and is logged.
- ✅ **Startup warning screens skipped (VST HACKED):** MAME's modal "press any
     key" boot dialogs (ROM audit / imperfect-sound warnings) cannot be
     dismissed from the plugin (no keyboard/mouse providers), which blocked
     boot. `Mame_Patches/ui.cpp`'s `display_startup_screens()` override (skip
     all startup screens, go straight to in-game mode) was ported to
     `mame-mac-master/src/frontend/mame/ui/ui.cpp`; `libfrontend.a` rebuilt
     incrementally and the plugin relinked.
- ✅ **Live LCD refresh (recipe §8):** the dirty-frame hash only covers prim
     geometry/pointers, not texture pixels, so HD44780 character changes never
     triggered a redraw and the LCD lagged until some other button forced one.
     `VstOsdInterface` now forces an immediate repaint on every button press
     AND release, plus a periodic redraw every 2 renderable frames, so the LCD
     (pattern numbers, tempo, edit screens) updates live.
- ✅ **VST3 + AU builds** (recipe §9): both bundle formats build and ad-hoc codesign.
     Installed to ~/Library/Audio/Plug-Ins/VST3 and ~/Library/Audio/Plug-Ins/Components.
- ⏳ **Corrected `hd44780_a00.bin`:** MAME replaced this BAD_DUMP font in mid-2025
     (CRC e459877c). The 2018 ROM pack carries the older dump (CRC 01d108e2); MAME
     boots and renders fine with a checksum warning, but `-verifyroms` reports "bad".
     Drop in the corrected file when available to silence it.

## Status of ROM Files
Location: `~/Documents/CasioRZ1/ROMs/rz1/`
- ✅ upd7811g-120.bin (4 KB) - RZ-1 CPU microcode (UPD7811G @ 1.2MHz)
- ✅ program.bin (16 KB) - Main program ROM (BIOS/OS)
- ✅ sound_a.cm5 (32 KB) - Sound ROM A (drum waveforms part A)
- ✅ sound_b.cm6 (32 KB) - Sound ROM B (drum waveforms part B)

## Next Steps: DAW Testing (Element)

### PHASE 3.3: VST3 Integration Test in Element (Current Priority)
**Status:** ⏳ Pending

The rebuilt VST3 is installed at `~/Library/Audio/Plug-Ins/VST3/Casio RZ-1.vst3`.
See **TESTING.md** for the step-by-step Element checklist (load, boot, MIDI notes,
panel clicks, persistence). Key verification points:

1. Plugin appears in Element's plugin browser (rescan if the old build is cached).
2. Editor shows the RZ-1 panel + "RZ-1 ACTIVE" and the "CASIO RZ-1" boot text on the LCD.
3. A MIDI note (start with C1..G2 = MIDI 36..51) triggers the corresponding drum.
4. Clicking a drum pad on the panel triggers the sound (kc0..kc1 matrix via set_value).
5. Tempo/value buttons, transport, and the 0-9 value pad respond on screen.

### PHASE 4: AU Build for Logic
**Status:** ⏳ Built, validation deferred

- AU bundle builds and ad-hoc codesigns: `build_vst/CasioRZ1_artefacts/AU/Casio RZ-1.component`.
- Installed to `~/Library/Audio/Plug-Ins/Components/`.
- Pending: `auval -v aumu Crz1 RZ1A` (run outside the Codex sandbox) + Logic smoke test.

### Optional Future Work
- **Per-instrument outputs:** the RZ-1 driver already delivers each drum on its own
  MAME channel (tom1..sample_3_4); expose them as individual VST3 output buses
  (like the MPC3000 port) instead of the summed mono mix.
- **Sample loading:** the driver supports the cassette / line-in path (channel 10);
  a UI to load `rz1_cass` software would unlock user samples (recipe §4 style work).
- **Corrected hd44780_a00.bin** to make `-verifyroms` fully clean (see above).

### PHASE 3.3: Audio & MIDI Integration Test
**Status:** ⏳ Pending after compilation
**Objective:** Verify MIDI input/output and audio streaming from RZ-1 emulation

**Test Checklist:**
- [ ] MIDI note input triggers RZ-1 drum sounds
- [ ] Audio output streams from VST plugin to DAW
- [ ] MIDI CC automation works on drum parameters
- [ ] Display updates respond to MIDI/UI changes

## PHASE 4: VST3 Plugin Build
**Status:** ⏳ After compilation issues fixed
**Objective:** Build the actual VST3 plugin binary

```bash
# Build VST3 plugin using CMake
cd /Users/jaychung/Documents/GitHub/Ensoniq-SD-1-32-Voice-VST-emulation-1.0.1
cd build_test

# Build release binary
cmake --build . --config Release -j4

# Plugin output location:
# ~/Library/Audio/Plug-Ins/VST3/Casio\ RZ-1.vst3
```

## PHASE 5: DAW Testing & Verification
**Status:** ⏳ After VST3 build completes
**Objective:** Verify plugin works in actual DAW

**Test Checklist:**
- [ ] Plugin loads without crashes
- [ ] MAME RZ-1 driver initializes (check console for "rz1 not found" errors)
- [ ] Display shows RZ-1 UI via rz1.lay
- [ ] MIDI keyboard input works (play notes, trigger drum sounds)
- [ ] Audio output is audible in DAW
- [ ] Preset save/load works
- [ ] Settings persist across plugin reload
- [ ] Multiple plugin instances can be loaded
- [ ] Resizing GUI works without artifacts

## Potential Issues & Fixes

### Issue 1: "driver_rz1 not found" at link time
**Symptoms:** Undefined reference to 'driver_rz1' during linking
**Fix:** 
- Verify mame-mame0289/src/mame/casio/rz1.cpp is in the build
- Check CMakeLists.txt MAME_INCLUDE_DIRS includes src/mame/casio
- Ensure libmame_mame.a contains the RZ-1 driver (check with nm command)

### Issue 2: Audio/MIDI not working
**Symptoms:** No sound, or MIDI events not reaching emulation
**Fix:**
- Check PluginProcessor.cpp for audio buffer handling (pushAudioFromMame)
- Verify VstMidiInputPort and VstMidiOutputPort are properly initialized
- Check sample rate matching between VST and MAME (typically 48kHz)
- Verify ring buffer thresholds are appropriate for RZ-1's audio output

### Issue 3: Display shows garbage or wrong dimensions
**Symptoms:** LCD display rendering incorrectly or showing SD-1 artifacts
**Fix:**
- Check segmentToAscii dictionary is built correctly for HD44780 (1x16 chars)
- Verify vfdSegments array size matches RZ-1's actual display (16 chars vs 80)
- Review MAME layout file rendering (rz1.lay)
- Check VFD/LCD emulation code in rz1_state::lcd_pixel_update()

### Issue 4: Build errors about missing headers
**Symptoms:** Cannot find emu.h, mame.h, or device headers
**Fix:**
- Verify CMakeLists.txt MAME_INCLUDE_DIRS paths are correct
- Check that mame-mame0289/src exists and contains the expected subdirectories
- Add missing include directories to CMakeLists.txt

### Issue 5: ROM loading fails at runtime
**Symptoms:** "Cannot find rz1 ROMs" message or RZ-1 won't boot
**Fix:**
- Verify ~/Documents/CasioRZ1/ROMs/rz1/ directory exists
- Check ROM filenames are exactly: upd7811g-120.bin, program.bin, sound_a.cm5, sound_b.cm6
- Verify file sizes match (4 KB, 16 KB, 32 KB, 32 KB)
- Check PluginProcessor.cpp ROM loading code references correct paths
- Test with `ls ~/Documents/CasioRZ1/ROMs/rz1/` command

## Key Architectural Differences: RZ-1 vs SD-1

### Processor Architecture
| Component | RZ-1 | SD-1 |
|-----------|------|------|
| Main CPU | UPD7811G @ 1.2 MHz | 68000 + multiple others |
| Sound Engine | UPD934G (2 units) | 68000-based |
| CPU Cores | 1 | 3-4 |
| Complexity | Simpler (drum machine) | More complex (synthesizer) |

### Display
| Component | RZ-1 | SD-1 |
|-----------|------|------|
| Display Type | HD44780 LCD | VFD (40x2) |
| Resolution | 1x16 characters | 2x40 characters |
| Rendering | esqpanel.h LCD pixel update | esqvfd.h 14-segment rendering |
| Integration | MAME handles automatically | Custom JUCE rendering |

### Audio Output
| Component | RZ-1 | SD-1 |
|-----------|------|------|
| Main Output | UPD934G synth output | 68000 audio synthesis |
| Sample Rate | Derived from 1.2 MHz clock | Derived from main CPU clock |
| Channel Count | 1-2 (mono/stereo) | 4 discrete outputs |
| Audio Quality | Drum machine style | High-quality synthesizer |

### MIDI Implementation
| Component | RZ-1 | SD-1 |
|-----------|------|------|
| MIDI In | Serial cassette port | Full MIDI (12 channels) |
| MIDI Out | Limited | Full MIDI implementation |
| Polyphony | Not polyphonic (drum machine) | 32-voice polyphonic |

## File Structure Reference
```
~/Documents/CasioRZ1/
├── ROMs/
│   └── rz1/
│       ├── upd7811g-120.bin     (4 KB - CPU microcode)
│       ├── program.bin           (16 KB - Main ROM)
│       ├── sound_a.cm5           (32 KB - Sound ROM A)
│       └── sound_b.cm6           (32 KB - Sound ROM B)
├── GlobalState/
│   └── nvram/
│       └── rz1/                  (Emulated NVRAM/battery backup)
└── settings.xml                  (Plugin global settings)

Project Structure:
├── Source/
│   ├── PluginProcessor.cpp       (RZ-1 adaptation: driver registration, ROM loading)
│   ├── PluginProcessor.h
│   ├── PluginEditor.cpp          (UI rendering)
│   └── PluginEditor.h
├── mame-mame0289/
│   └── src/mame/casio/
│       ├── rz1.cpp               (RZ-1 MAME driver - pre-existing in MAME)
│       └── rz1.lay               (RZ-1 display layout)
├── mame_libs/                    (Pre-compiled MAME static libraries)
│   ├── libmame_mame.a
│   ├── libemu.a
│   ├── libsound.a
│   └── ... (33 .a files total)
└── CMakeLists.txt                (RZ-1 plugin configuration)
```

## Estimated Time to Completion

| Phase | Task | Estimated Time |
|-------|------|-----------------|
| 3.1 | Compilation test & fixes | 30-60 min |
| 3.2 | UI/Display adaptation | 15-30 min |
| 3.3 | Audio/MIDI integration | 15-30 min |
| 4 | VST3 plugin build | 5-10 min |
| 5 | DAW testing & debug | 15-45 min |
| **Total** | **Complete plugin** | **1.5-3 hours** |

## Current Build Status
- MAME static libraries: ✅ Pre-compiled (in mame_libs/)
- Source code adaptation: ✅ ~70% complete
- Compilation testing: ❌ Not yet attempted
- VST3 binary: ❌ Not yet built
- DAW verification: ❌ Not yet tested

**Next immediate action:** Run CMake build to identify and fix remaining compilation issues.
