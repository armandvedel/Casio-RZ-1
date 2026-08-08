# TESTING.md - Casio RZ-1 VST3 Verification in Element

Phase 3.3 checklist for verifying the rebuilt VST3 in **Element** (Kushview),
which hosts VST3 plugins. AU/Logic testing is intentionally deferred.

## 0. Pre-flight (one-time)

1. Confirm the plugin bundle is installed and current:

   ```bash
   ls ~/Library/Audio/Plug-Ins/VST3/"Casio RZ-1.vst3"/Contents/MacOS/
   ls ~/Library/Audio/Plug-Ins/VST3/"Casio RZ-1.vst3"/Contents/Resources/plugins/
   ```

   Both must exist: the Mach-O binary and the MAME `plugins/` folder
   (`cheat`, `data`, etc.). If `plugins/` is missing, MAME's Lua self-check
   fails and the engine never boots (black panel).

2. Confirm the ROM set is present (5 files):

   ```bash
   ls ~/Documents/CasioRZ1/ROMs/rz1/
   # expected: hd44780_a00.bin, program.bin, sound_a.cm5, sound_b.cm6, upd7811g-120.bin
   ```

   Missing `hd44780_a00.bin` = machine won't start = black panel (recipe §2).
   Its checksum is the "old" 2018 dump (CRC 01d108e2) and MAME logs a
   checksum warning at boot - **this is expected** and does not block
   booting/playing. The plugin's MAME build skips the modal boot-warning
   screens (VST HACKED), so no "press any key" dialog appears even though the
   dump checksum differs (and the RZ-1's sound is flagged imperfectly
   emulated). See "Known issues" below.

3. Don't run a standalone MAME instance on the same ROM/NVRAM paths at the
   same time (recipe §5 file locking).

## 1. Load the plugin in Element

1. Launch **Element** (`/Applications/Element.app`).
2. If the plugin was installed while Element was running, force a rescan:
   `Settings -> Plugins -> Rescan` (or restart Element).
3. Add an instrument track and insert **Casio RZ-1** (VST3, category
   Instrument).
4. Arm the track for MIDI input and enable monitoring so you can hear the
   instrument while it boots.

The plugin window shows only the MAME-rendered panel (no status overlay). Boot
is confirmed visually:

5. Give it a few seconds. The window opens at the layout's native 800x535 and
   shows the full Casio RZ-1 panel art, with `CASIO RZ-1` appearing on the LCD
   once the firmware boots. (A red `ERROR: ROMs not found!` at the top-left is
   the only overlay that ever appears - only when the ROM set is missing.)

## 2. MIDI note map (the first thing to pin down)

The RZ-1's MIDI note map is **non-standard** (manual p.33) and lives in the
firmware, so it must be discovered empirically. The expected range is roughly
C1 through G2 (MIDI 36-51), but the exact per-drum assignment is not GM-style.

**Sweep procedure:**

1. Play one short note at a time, starting at MIDI 36 (C1), going up.
2. For each note, note which pad/sound fires (watch the editor highlight and
   the LCD, and listen).
3. Record the result below. The 12 factory drums + 4 sample slots are:

| Sound | MIDI note found | Sound | MIDI note found |
|---|---|---|---|
| Tom 1 | | Claps | |
| Tom 2 | | Ride | |
| Tom 3 | | Cowbell | |
| Bass Drum | | Crash | |
| Rim | | Sample 1 | |
| Snare | | Sample 2 | |
| Open HH | | Sample 3 | |
| Closed HH | | Sample 4 | |

4. If no notes in 36-51 respond, sweep the wider range 24-60 (C1 octave
   below through C4) - the map may be Yamaha-style ("Yamaha-style notes" is
   how the RZ-1's MIDI behavior is often described).
5. Once found, save the map in this file (or a DAW drum map) for reuse.
   All drums are monophonic-ish and share a single MIDI channel; the channel
   is set with the **MIDI CH** button on the panel.

## 3. Click-map validation (panel buttons)

Click each of these with the mouse. A successful hit flashes the button
rectangle (white overlay) and drives the emulated key matrix
(`ioport_field::set_value()`), so the LCD responds:

1. **Drum pads** - each of the 12 pads (Tom 1-3, BD, Rim, SD, Open/Closed HH,
   Claps, Ride, Cowbell, Crash) triggers its sound; the pad lights.
2. **Sample pads 1-4** - flash when pressed (no sound until samples are
   loaded).
3. **Transport** - Start/Stop and Continue/Start affect the sequencer.
4. **Tempo Up/Down** - LCD tempo display changes.
5. **Value pad** (1-9, 0, No, Yes) - enters numeric values in edit screens.
6. **Edit/Record, Delete, Insert/Auto-Comp, Chain/Beat, Reset/Copy** -
   switch LCD screens.
7. **MT Save / MT Load, MIDI CH, MIDI Clock, Sampling** - respond on LCD.

If clicks trigger the **wrong** button or nothing: stop here and report the
misaligned region. The click-transform is the newest code (two-stage
editor->buffer->layout mapping) and is the most likely thing to need
correction. See "Known issues" for the diagnostic.

## 4. Audio sanity

1. Trigger a Bass Drum note; Element's track meter should move and you should
   hear a drum hit.
2. Trigger several drums together (chord) - the plugin sums all 10 drum
   channels into the stereo mix with 1/8 headroom, so a chord should be
   audible but quieter per voice.
3. Run Element's transport and use the RZ-1's own sequencer (Start/Stop) to
   play a factory pattern - a pattern playing means CPU + audio are streaming
   in real time.
4. If the track is silent but the panel works: check the plugin's
   **Internal Buffer** parameter (Settings/APVTS - default 512) and confirm
   audio isn't muted in the mixer. A sustained 0/0 readout in a scope at the
   instrument output means the MAME->JUCE audio tap is failing - capture the
   editor overlay values (Buffer/Prims) for diagnosis.

## 5. Persistence

1. Set **Internal Buffer** to something other than the default (e.g. 256).
2. Resize the plugin window (drag corner), then close and reopen the editor.
3. Reload the Element session (or reopen the plugin).
4. Check `~/Documents/CasioRZ1/settings.xml` exists and that
   `buffer_size`, `window_width`, and `window_height` were written. The
   plugin should restore them on relaunch.

## 6. Stress checks

- Reload the session 3x; no crash, no freeze (a dead MAME thread used to
  freeze the whole DAW - the boot-failure guard is new).
- Load two instances on two tracks; both boot and both play.
- Resize the window while a pattern plays; audio must not glitch and the
  panel must re-letterbox correctly.
- Start/stop the Element transport several times; RZ-1 sequencer clock sync
  (MIDI clock) should follow if enabled.

## 6.5 RZ-1 usage notes (verified in standalone MAME)

The RZ-1 boots with **empty pattern memory** (MAME's `dataram` NVRAM starts
blank; there are no factory patterns), so **START/STOP does nothing visible
until a pattern has content** - this is correct hardware behavior, not a
plugin bug. Same for the numpad: the 0-9 keys are data-entry keys and do
nothing on their own; they act after PATTERN/SONG/EDIT-RECORD etc.

To hear the sequencer:

1. Press **PATTERN**, then a numpad key (e.g. `1`) - the pattern LED lights.
2. Press **EDIT/RECORD**, then **START/STOP** (starts recording).
3. Hit drum pads in time to program the pattern.
4. Press **START/STOP** to stop recording, then **START/STOP** again to play
   back - the START/STOP LED lights and the recorded hits loop.

Verified via `mame-mac-master/mame` (standalone CLI, built with homebrew SDL2):
pads trigger sound on their own channel, PATTERN+5 lights the pattern LED, and
recorded playback lights the START/STOP LED with the recorded hits looping.

## 6.6 Logic (AU) performance notes

- Logic's real-time scheduler is aggressive; the AU now defaults to **2048
  samples** of internal MAME buffer headroom (vs 512 for VST3), which stops
  the ring-buffer underruns that made audio choppy. This also raises the
  plugin's reported latency to ~67 ms - fine for playback/sequencing; if pads
  feel laggy when played live, lower Logic's I/O buffer setting or ask for a
  smaller default.
- If choppiness persists, raise Logic's **I/O Buffer Size** (Logic Settings →
  Audio) to 256/512 and make sure Low Latency Mode isn't forcing a tiny buffer
  while monitoring.
- auval (CommandLineTools 1.10.0) fails to find this plugin and the sibling
  SD-1 AU with "didn't find the component" even though the system registry can
  instantiate them - treat Logic itself as the validator.

## 7. Diagnostics if something fails

| Symptom | Likely cause | Check |
|---|---|---|
| Black panel / empty window | device ROM missing / engine not booting | `ls ~/Documents/CasioRZ1/ROMs/rz1/`; editor shows red error when ROMs are missing |
| Red `ERROR: ROMs not found!` | self-check failed | exact missing filenames are drawn under the error |
| Empty window, no red error | MAME boot failed or stuck | confirm no second MAME instance holds the ROM/NVRAM files; check `debugInitLog` in the source/debugger for `MAME exited with code N` |
| Silent audio, panel works | stride/stride mapping regression | confirm installed binary is newer than last `Source/` edit; check `pushAudioFromMame` RZ1_STRIDE == 11 |
| Wrong button fires on click | click-transform (two-stage mapping) | window aspect vs 800x535; render target size vs screen buffer size |
| "press any key" boot dialog stuck | unpatched MAME UI (startup warning screens) | fixed in current build: `display_startup_screens` VST HACK skips all startup dialogs |
| LCD checksum warning (log only) | `hd44780_a00.bin` is the 2018 dump (CRC 01d108e2) | expected; MAME 2025 wants CRC e459877c - cosmetic unless you need a clean `-verifyroms`. Note: even the corrected dump is flagged BAD_DUMP in MAME's driver, so `-verifyroms` stays "bad" regardless |

## 8. Where things live (reference)

```text
Plugin bundle (installed):  ~/Library/Audio/Plug-Ins/VST3/Casio RZ-1.vst3
Build output:               <repo>/build_vst/CasioRZ1_artefacts/VST3/Casio RZ-1.vst3
ROMs:                       ~/Documents/CasioRZ1/ROMs/rz1/
Global settings:            ~/Documents/CasioRZ1/settings.xml
NVRAM:                      ~/Documents/CasioRZ1/GlobalState/nvram/rz1/
Source:                     <repo>/Source/PluginProcessor.{h,cpp}, PluginEditor.{h,cpp}
```

## 9. Deferred items (not in this test pass)

- **AU (Logic)** - build exists at
  `build_vst/CasioRZ1_artefacts/AU/Casio RZ-1.component` but is not validated
  and its bundle lacks the MAME `plugins/` POST_BUILD step. Skipped per
  request.
- **Per-instrument outputs** - MAME already delivers each drum on its own
  channel (10 drum channels + cassette/line-in); only the summed stereo mix
  is wired to the VST output buses today.
- **Sample loading** via cassette/line-in (channel 10 is dropped from the mix
  on purpose until this is implemented).
- **Corrected `hd44780_a00.bin`** to silence the checksum warning.
