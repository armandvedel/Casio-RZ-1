/*
  ==============================================================================

    Ensoniq SD-1 MAME VST Emulation
    Open Source GPLv2/v3
    https://www.sojusrecords.com

  ==============================================================================
*/

#pragma once

// Uncomment to enable the debug rack panel (file manager + rack visible simultaneously)
//#define SD1_DEBUG_RACK_PANEL

// ==============================================================================
// MANDATORY MAME MACROS - MUST BE DEFINED BEFORE ANY MAME INCLUDES!
// These ensure compatibility with the MAME core data types and architectures.
// ==============================================================================
#ifndef PTR64
#define PTR64 1
#define LSB_FIRST 1
#define NDEBUG 1
#define __STDC_LIMIT_MACROS 1
#define __STDC_FORMAT_MACROS 1
#define __STDC_CONSTANT_MACROS 1
#endif

#include <JuceHeader.h>
#include <fstream> 

// MAME Core Includes
#include "emu.h"
#include "mame.h"

// Threading & Synchronization
#include <thread>
#include <atomic>
#include <mutex>

//==============================================================================

class EnsoniqSD1AudioProcessor : public juce::AudioProcessor,
    public juce::AudioProcessorValueTreeState::Listener
{
public:
    
    // --- VFD DISPLAY & LED HARDWARE STATES ---
        static constexpr int VFD_SIZE = 80; // 2 rows x 40 characters
        
        // Stores the raw 14-segment bitmask for each character
        std::atomic<uint16_t> vfdSegments[VFD_SIZE];
        
        // Stores the 32-bit integer where each bit represents a specific panel LED
        std::atomic<uint32_t> ledStateMask{ 0 };

        // --- NATIVE JUCE PANEL STATE (MAME thread -> UI thread) ---
        // Individual RZ-1 LED values (0 = off, 1 = green, 2 = red where
        // applicable), written by VstOsdInterface::update() on the MAME thread.
        std::atomic<int> ledSampling{ 0 };
        std::atomic<int> ledSong{ 0 };
        std::atomic<int> ledPattern{ 0 };
        std::atomic<int> ledStartStop{ 0 };
        // 16 HD44780 display characters packed as bytes (0..7 in Lo, 8..15 in Hi).
        std::atomic<uint64_t> lcdCharsLo{ 0 };
        std::atomic<uint64_t> lcdCharsHi{ 0 };
        // When true the editor draws the panel natively in JUCE, so MAME's
        // layout rasterization (screenBuffers) can be skipped entirely.
        std::atomic<bool> nativePanel{ false };

        // Instrument level faders (10 drum voices, 0..1). Written by the editor
        // (UI thread), read by pushAudioFromMame (MAME/audio thread). Default
        // is full level so sound is unchanged until a fader is moved.
        std::atomic<float> instrumentLevel[10];
        // Overall output volume fader (0..1, default full).
        std::atomic<float> masterVolume{ 1.0f };

        // MAME callback function triggered whenever a hardware output changes
        static void mameOutputNotifier(const char *outname, s32 value, void *param);
        
        // API for the Editor / File Manager to read the hardware state safely
        juce::String getHardwareVfdText();
        bool isHardwareLedOn(int ledBitIndex);

        // Dynamic dictionary to translate hardware bitmasks back to text
        std::unordered_map<uint16_t, char> segmentToAscii;
        void buildVfdDictionary();
    
    // New atomic flag to signal that the MAME engine is fully initialized and clocks are valid
        std::atomic<bool> mameIsFullyBooted{ false };

    // --- HOST TEMPO SYNC (RZ-1 MIDI CLOCK: FA/F8/FC, CLOCK=EXT) ---
    std::atomic<bool> hostSyncEnabled{ true };
    std::atomic<bool> hostSyncArmed{ false };  // set by OSD once CLOCK=EXT is applied
    // Audio-thread-only state (touched solely from processBlock):
    bool hostSyncLastPlaying = false;
    int64_t hostSyncLastTick = -1;
    double hostSyncLastPpq = -1.0;  // playhead ppq from the previous block (loop-wrap detection)
    bool hostSyncFaSent = false;
    
    // --- DEBUG: Primitive counter for GUI detection ---
        std::atomic<int> lastPrimitiveCount{ 0 };
        std::atomic<bool> renderTargetValid{ false };
    
    // --- SELF CHECK ---
        std::atomic<bool> isSelfCheckFailed{ false };
        juce::String selfCheckErrorMsg { "" };
        bool runSelfCheck();
    
    // --- ROM MANAGEMENT ---
        juce::String customRomPath { "" };
    
    // --- Last browsed ---
    juce::String lastBrowsedFolder;
    juce::String lastMediaFolder;
    juce::String lastRomFolder;
    juce::String myComputerPath;    // Browse Computer current directory
        
        // --- FOLDER BOOKMARKS (max 10, persisted in settings.xml) ---
        juce::StringArray bookmarkFolders;
        
        // --- FILE MANAGER STATE (survives Editor destroy/recreate) ---
        struct FileManagerState {
            bool visible = false;
            juce::String category;          // "INT (RAM)", "ROM0", "BOOKMARK:/path", etc.
            juce::String openedFilePath;    // full path of opened file (if external)
            bool viewingDiskBank = false;
            juce::String openedDiskBankName;
            int selectedRow = -1;           // contentList row (fallback only)
            int bankSelectedRow = -1;       // bankContentList row (fallback only)
            juce::String selectedName;      // actual item name in contentList (primary restore key)
            juce::String bankSelectedName;  // actual item name in bankContentList (primary restore key)
            int scrollPosition = 0;         // contentList top row
            int bankScrollPosition = 0;     // bankContentList top row
            juce::String activeBookmark;    // bookmark path active at save time (for song state)
            int viewBeforeBrowser = 0;      // panel view index to restore when closing file manager
            int fmWindowWidth = 1200;       // Dedicated width for File Manager
            int fmWindowHeight = 925;       // Dedicated height for File Manager
        };
        FileManagerState fileManagerState;
        std::atomic<bool> stateJustLoaded{ false };  // prevents Editor destructor from overwriting song state
        std::atomic<bool> isWarmBoot{ false }; // NEW INSTANCE OR LOAD STATE
        std::atomic<bool> requestFileManagerUIRefresh{ false }; // Notifies GUI to update File Manager after state load
        std::atomic<bool> showWelcomeMessage{ false };  // Flag for first-launch UX message
        void checkRomAndBootMame();
    
    // --- GLOBAL SETTINGS ---
        std::atomic<bool> requestGlobalSave{ false };
        void loadGlobalSettings();
        void saveGlobalSettings();
    
    // --- COMPARE STATE MANAGEMENT ---
    void forceCompareOff();
    std::atomic<double> scheduledCompareResetTime{ -1.0 };

    // --- MAME STATE MANAGEMENT ---
    // Used to safely orchestrate loading/saving states between the UI and the MAME thread
    std::atomic<bool> requestMameSave{ false };
    std::atomic<bool> requestMameLoad{ false };
    std::atomic<bool> mameStateIsReady{ false };
    juce::WaitableEvent mameStateEvent{ false };
    
    // Countdown timer (in samples) to trigger a delayed MIDI Panic after a state load
    std::atomic<int> panicDelaySamples{ 0 };

    // --- MEDIA HANDLING (FLOPPY/CARTRIDGE/SYSEX) ---
    std::atomic<bool> requestFloppyLoad{ false };
    std::atomic<bool> requestCartLoad{ false };
    std::string pendingFloppyPath;
    std::string pendingCartPath;
    std::mutex mediaMutex;
    
    // --- MEDIA STATE TRACKING ---
        std::atomic<bool> isFloppyLoaded{ false };
        std::atomic<bool> isCartLoaded{ false };
        juce::String loadedFloppyName{ "" };
        juce::String loadedCartName{ "" };

    // --- WINDOW SIZE PERSISTENCE ---
    // Stores the last window size set by the user to recall it upon project load
    int savedWindowWidth{ 0 };
    int savedWindowHeight{ 0 };

    // --- SYNCHRONIZATION ---
    // Throttle event used to prevent MAME from generating audio faster than the DAW consumes it
    juce::WaitableEvent mameThrottleEvent{ false };
    bool isMameRunningFlag() const { return isMameRunning.load(); }
    std::atomic<bool> isRomMissing{ false };

    
    // Flag to indicate if the zip file exists but contains invalid/missing ROMs
    std::atomic<bool> isRomInvalid{ false };

    std::atomic<bool> mameHasStarted{ false };
    std::atomic<double> initialSampleRate{ 0.0 };
    std::atomic<bool> sampleRateMismatch{ false };
    juce::Image lastRenderFrame;
    juce::CriticalSection renderFrameLock;
    
    // Flag to indicate if the plugin is running as an AU in an unsupported host (e.g., FL Studio, Ableton)
    std::atomic<bool> isUnsupportedAUHost{ false };
    bool isMaschineHost = false;  // Set once in prepareToPlay, read-only in processBlock
    bool maschineInFastRender = false; // True once WAV RENDER FIX confirms fast render; resets on stop
    
    uint64_t getTotalRead() const { return totalRead.load(std::memory_order_acquire); }
    uint64_t getTotalWritten() const { return totalWritten.load(std::memory_order_acquire); }

    //==============================================================================
    EnsoniqSD1AudioProcessor();
    ~EnsoniqSD1AudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    
            // FULL RZ-1 HARDWARE MATRIX DEFINITION
            // (inputtag, inputmask) come from the driver's INPUT_PORTS (src/mame/casio/rz1.cpp,
            // ports kc0..kc7 + foot). Click rectangles are transcribed from rz1.lay in layout
            // coordinates (view = 800x535, no <group> transforms). The editor maps mouse
            // positions through the same stretch-to-fit transform used to draw the render.
            // Bounds (0,0,0,0) = not clickable (param-only).
            struct RZ1ButtonDef {
                juce::String paramID;
                juce::String paramName;
                const char* ioportTag;
                uint32_t ioportMask;
                int x, y, w, h;
            };

            const std::vector<RZ1ButtonDef> rz1Buttons = {
                // --- TOP LEFT (kc4 / kc3) ---
                { "btn_song",           "Song",           "kc4", 0x02,  71, 267, 26, 10 },
                { "btn_pattern",        "Pattern",        "kc4", 0x01,  71, 298, 26, 10 },
                { "btn_editrecord",     "Edit/Record",    "kc4", 0x04, 109, 328, 26, 10 },
                { "btn_delete",         "Delete",         "kc4", 0x08, 147, 328, 26, 10 },
                { "btn_insertautocomp", "Insert/Auto-Comp","kc4", 0x10, 185, 328, 26, 10 },
                { "btn_chainbeat",      "Chain/Beat",     "kc4", 0x20, 223, 328, 26, 10 },
                { "btn_resetcopy",      "Reset/Copy",     "kc3", 0x20, 260, 328, 26, 10 },

                // --- TOP MIDDLE (kc5 / kc3) ---
                { "btn_save",       "MT Save",      "kc5", 0x01, 321, 298, 26, 10 },
                { "btn_load",       "MT Load",      "kc5", 0x02, 358, 298, 26, 10 },
                { "btn_tempodown",  "Tempo Down",   "kc3", 0x10, 321, 328, 26, 10 },
                { "btn_tempoup",    "Tempo Up",     "kc3", 0x08, 358, 328, 26, 10 },
                { "btn_channel",    "MIDI Channel", "kc5", 0x04, 414, 298, 26, 10 },
                { "btn_clock",      "MIDI Clock",   "kc5", 0x08, 452, 298, 26, 10 },
                { "btn_sampling",   "Sampling",     "kc3", 0x04, 452, 328, 26, 10 },

                // --- TOP RIGHT: VALUE PAD (kc6 / kc7) ---
                { "btn_7", "7 (1/32)", "kc7", 0x02, 513, 236, 24, 9 },
                { "btn_8", "8 (1/48)", "kc7", 0x04, 551, 236, 24, 9 },
                { "btn_9", "9 (1/96)", "kc7", 0x08, 588, 236, 24, 9 },
                { "btn_4", "4 (1/12)", "kc6", 0x10, 513, 268, 24, 9 },
                { "btn_5", "5 (1/16)", "kc6", 0x20, 551, 268, 24, 9 },
                { "btn_6", "6 (1/24)", "kc7", 0x01, 588, 268, 24, 9 },
                { "btn_1", "1 (1/4)",  "kc6", 0x02, 513, 298, 24, 9 },
                { "btn_2", "2 (1/6)",  "kc6", 0x04, 551, 298, 24, 9 },
                { "btn_3", "3 (1/8)",  "kc6", 0x08, 588, 298, 24, 9 },
                { "btn_0", "0 (1/2)",  "kc6", 0x01, 513, 328, 24, 9 },
                { "btn_no",  "No (Value Down)", "kc7", 0x20, 551, 328, 24, 9 },
                { "btn_yes", "Yes (Value Up)",  "kc7", 0x10, 588, 328, 24, 9 },

                // --- BOTTOM: TRANSPORT (kc3) ---
                { "btn_startstop", "Start/Stop",      "kc3", 0x01,  71, 393, 43, 27 },
                { "btn_continue",  "Continue/Start",  "kc3", 0x02,  71, 450, 43, 27 },

                // --- BOTTOM: ACCENT / MUTE (kc2) ---
                { "btn_accent", "Accent", "kc2", 0x20, 184, 393, 43, 27 },
                { "btn_mute",   "Mute",   "kc2", 0x10, 184, 450, 43, 27 },

                // --- BOTTOM: DRUM PADS (kc0 / kc1) ---
                { "btn_tom1",    "Tom 1",     "kc0", 0x01, 269, 393, 43, 27 },
                { "btn_tom2",    "Tom 2",     "kc1", 0x01, 269, 450, 43, 27 },
                { "btn_tom3",    "Tom 3",     "kc0", 0x02, 326, 393, 43, 27 },
                { "btn_bd",      "Bass Drum", "kc1", 0x02, 326, 450, 43, 27 },
                { "btn_rim",     "Rim",       "kc0", 0x04, 382, 393, 43, 27 },
                { "btn_sd",      "Snare",     "kc1", 0x04, 382, 450, 43, 27 },
                { "btn_openhh",  "Open HH",   "kc0", 0x08, 437, 393, 43, 27 },
                { "btn_closedhh","Closed HH", "kc1", 0x08, 437, 450, 43, 27 },
                { "btn_claps",   "Claps",     "kc0", 0x10, 493, 393, 43, 27 },
                { "btn_ride",    "Ride",      "kc1", 0x10, 493, 450, 43, 27 },
                { "btn_cowbell", "Cowbell",   "kc0", 0x20, 550, 393, 43, 27 },
                { "btn_crash",   "Crash",     "kc1", 0x20, 550, 450, 43, 27 },

                // --- BOTTOM: SAMPLES (kc2) ---
                { "btn_sample1", "Sample 1", "kc2", 0x01, 607, 393, 43, 27 },
                { "btn_sample2", "Sample 2", "kc2", 0x02, 607, 450, 43, 27 },
                { "btn_sample3", "Sample 3", "kc2", 0x04, 664, 393, 43, 27 },
                { "btn_sample4", "Sample 4", "kc2", 0x08, 664, 450, 43, 27 },

                // --- FOOT SWITCH (no layout element; param-only) ---
                { "btn_foot", "Foot Switch", "foot", 0x01, 0, 0, 0, 0 }
            };

        std::vector<std::atomic<float>*> buttonParams;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Core function to boot the headless MAME environment
    void runMameEngine();
    
    // Verifies the unzipped ROM files in the sd132 directory
    bool verifyRomFiles();
    // Extracts only the required .bin files from a user-provided zip
    bool extractRomsFromZip(const juce::File& zipFile);
    // Copies the required .bin files from a user-provided directory
    bool copyRomsFromFolder(const juce::File& sourceDir);
    // Stores the list of missing ROM files to be displayed on the UI
    juce::String missingFilesList;
    // Stores the last ROM verification error (displayed on the UI)
    juce::String lastRomError;

    // Callback to push generated audio from MAME into our ring buffers
    void pushAudioFromMame(const int16_t* pcmBuffer, int numSamples);

    // ========================================================
    // SAMPLING AUDIO INPUT (DAW -> MAME mic path)
    // ========================================================
    // Mono mix of the DAW input bus, keyed by absolute DAW sample position.
    // Written by the audio thread in processBlock; read by the OSD's
    // sound_stream_source_update on the MAME thread via the anchor mapping
    // (same t_anchor/s_anchor atomics the MIDI scheduler uses).
    static constexpr int INPUT_RING_SIZE = 65536;
    float inputRing[INPUT_RING_SIZE] = { 0.0f };
    // inputWritePos is the end (frontier) of the block just written;
    // inputBlockStart is its start (currentReadPos at capture time). The OSD
    // reads forward from inputBlockStart, which stays inside the written
    // region - reading at the frontier itself always misses by one block.
    std::atomic<uint64_t> inputWritePos{ 0 };
    std::atomic<uint64_t> inputBlockStart{ 0 };

    double getAnchorMameTime() const { return anchorMameTime.load(std::memory_order_relaxed); }
    uint64_t getAnchorDawSample() const { return anchorDawSample.load(std::memory_order_relaxed); }
    bool isAnchorPending() const { return needAnchorSync.load(std::memory_order_acquire); }
    uint64_t getInputBlockStart() const { return inputBlockStart.load(std::memory_order_relaxed); }

    // Direct boot-stage diagnostics (appended to mame_boot_log.txt regardless
    // of whether MAME's OSD update ever runs, so a stalled boot is traceable).
    void appendBootLog(const juce::String& line);

    // ========================================================
    // MIDI INPUT HANDLING (JUCE -> MAME)
    // ========================================================
    void pushMidiByte(uint8_t data, double targetMameTime);
    void clearMidiBuffer();
    bool pollMidiData();
    int readMidiByte();
    int findDeliverableMidiByte(double now) const;
    
    // --- MIDI OUTPUT (from SD-1 DUART TX → JUCE MIDI out) ---
    static constexpr int MIDI_OUT_BUFFER_SIZE = 16384;
    uint8_t midiOutBuffer[MIDI_OUT_BUFFER_SIZE];
    std::atomic<int> midiOutWritePos{ 0 };
    std::atomic<int> midiOutReadPos{ 0 };
    void pushMidiOutByte(uint8_t data);
    
    // MIDI output message assembler state
    std::vector<uint8_t> midiOutMsg;
    uint8_t midiOutRunningStatus = 0;
    bool midiOutInSysEx = false;

    // Pointer to the running MAME engine instance
    running_machine* mameMachine = nullptr;
    
    // OSD interface for MAME (keeps render target alive)
    void* mameOsd = nullptr;
    
    // Debug strings for GUI display
    juce::String debugInitLog;
    std::mutex debugLogMutex;

    // --- MOUSE EVENT INJECTION (JUCE -> MAME) ---
    void injectMouseMove(int x, int y);
    void injectMouseDown(int x, int y);
    void injectMouseUp(int x, int y);

    // Thread-safe containers for mouse coordinates and button states
    std::atomic<int> mouseX{ 0 };
    std::atomic<int> mouseY{ 0 };
    std::atomic<uint32_t> mouseButtons{ 0 };

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Triggered by the DAW when an automation parameter changes
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Dynamically adjustable buffer threshold for MAME processing
    std::atomic<int> mameBufferThreshold{ 1024 };

    // Dynamic offline buffer for sync
    std::atomic<int> maxOfflineBuffer{ 1024 };
                
        // --- RAM INJECTION BUFFERS ---
        juce::MemoryBlock pendingOsram;
        juce::MemoryBlock pendingSeqRam;
        std::atomic<bool> pendingRamInjection{ false };
    
        // --- BANK INJECTION (60-program bank → osram, no CPU reset) ---
        juce::MemoryBlock pendingBankData;          // interleaved 31800 bytes
        std::atomic<bool> pendingBankInjection{ false };
        
        // --- STATE LOAD COMPARE RESET ---
        /*std::atomic<bool> needsCompareReset{ false };*/
        
        // --- MIDI INPUT SUPPRESS (during Write Single Preset) ---
        std::atomic<bool> suppressMidiInput{ false };
        
        // --- MIDI OPERATION CANCEL (set by onClose to abort pending timers) ---
        std::atomic<bool> midiOpCancelled{ false };
    
        // AU COLD BOOT HACK
        std::atomic<bool> needsBootPreRoll { false };
    
        // --- DYNAMIC PANEL LAYOUT SELECTION ---
        // 0 = Compact, 1 = Full, 2 = Panel, 3 = Tablet
        std::atomic<int> requestedViewIndex{ 0 };
        std::atomic<bool> requestViewChange{ false };

        // --- PIXEL PERFECT RENDERING ---
        // Stores the exact physical pixel dimensions of the current JUCE window.
        // MAME will strictly render at this 1:1 resolution to save CPU and maximize sharpness.
        // Default matches the rz1.lay view aspect at 1.5x (1200x802).
        std::atomic<int> windowWidth{ 1200 };
        std::atomic<int> windowHeight{ 802 };
        std::atomic<bool> requestRenderResize{ false };

        std::atomic<bool>& getFrameFlag() { return newFrameAvailable; }
        double getHostSampleRate() const { return hostSampleRate.load(); }

        // --- DOUBLE BUFFERED VIDEO RENDERING ---
        // Increased to 2560x2560 to safely fit the maximum allowed VST window size
        juce::Image cachedTexture{ juce::Image::ARGB, 2560, 2560, true, juce::SoftwareImageType() };

        juce::Image screenBuffers[2]{
            juce::Image(juce::Image::ARGB, 2560, 2560, true, juce::SoftwareImageType()),
            juce::Image(juce::Image::ARGB, 2560, 2560, true, juce::SoftwareImageType())
        };

    // Indicates which screen buffer (0 or 1) is fully rendered and ready to be drawn by the UI
    std::atomic<int> readyBufferIndex{ 0 };
    
    // PendingAUMidi
    std::vector<std::pair<juce::MidiMessage, int>> pendingAUMidi;
    
    // AnchorSet for AU
    std::atomic<bool> auAnchorSet{ false };
    
    // --- MACRO STATE ---
    std::atomic<bool> isSaveMacroActive{ false };
    int saveMacroHeldBank = -1;                 // GUI-side: currently held bank (0-9), -1 = none
    std::atomic<int> macroBankToHold{ -1 };      // GUI → audio: bank to electronically hold via set_button
    std::atomic<int> detectedBankMask{ 0 };       // audio → GUI: bitmask of pressed banks (bit i = bank i)
    
    void shutdownMame();
    
private:

        // Member variables to replace the problematic 'static' variables in processBlock.
        // This ensures each plugin instance has its own independent state.
        bool lastIsPlaying = false;
        bool localLastOffline = false;
        double lastAuMidiTime = 0.0;
        uint64_t captureReadPos = 0;
    
    bool extractLegacyMameState(const juce::String& base64State, juce::MemoryBlock& outOsram, juce::MemoryBlock& outSeqram);
    std::thread mameThread;
    std::atomic<bool> isMameRunning{ false };

    std::atomic<uint64_t> totalRead{ 0 };
    std::atomic<double> hostSampleRate{ 44100.0 };
                
    int getInternalHardwareLatencySamples() const {
        double sr = hostSampleRate.load(std::memory_order_relaxed);
        // Measured MIDI->audio round trip on the MAME side with the raised
        // stream rate: delivery is <= ~1 ms (4 kHz poll), but the RZ-1's own
        // firmware MIDI handling (UART parse + voice trigger) adds a variable
        // 2.5-7.5 ms (mean ~4.5 ms, ~2 ms quantized; measured with the
        // midi_latency harness on the BD voice across phase offsets). The host
        // PDC compensates this reported value, so use the mean so hits land on
        // the grid on average.
        return static_cast<int>(0.0045 * sr);
    }

    // MAME's audio callback now delivers ~1 ms of audio per update (raised
    // STREAMS_UPDATE_FREQUENCY to 1000 Hz), so the emulator's lookahead over
    // the DAW read position oscillates by up to ~1 ms. MIDI/F8 targets are
    // scheduled this far past the nominal threshold so they are always in the
    // future and delivered at their exact time (previously a 20 ms buffer
    // caused "~1000 samples, quite random" timing).
    int getMidiLookaheadSamples() const {
        double sr = hostSampleRate.load(std::memory_order_relaxed);
        return static_cast<int>(0.005 * sr);   // 5 ms > ~2 ms max lookahead swing
    }

    // Audio Ring Buffers (Generously sized to prevent underruns)
    static constexpr int RING_BUFFER_SIZE = 65536;
    // One output bus per RZ-1 drum voice (MAME channel order, see
    // pushAudioFromMame): tom1, tom2, tom3, bd, rim_and_sd, hihat,
    // claps_and_ride, cowbell_and_crash, sample_1_and_2, sample_3_and_4.
    static constexpr int RZ1_DRUM_CHANNELS = 10;

    // --- MAIN OUT BUFFERS ---
    float ringBufferL[RING_BUFFER_SIZE] = { 0.0f };
    float ringBufferR[RING_BUFFER_SIZE] = { 0.0f };

    // --- PER-INSTRUMENT BUFFERS (one per drum voice; fader gain applied,
    // master volume not - the panel faders shape the Main mix and the
    // individual outputs alike, the master only affects Main) ---
    float instRing[RZ1_DRUM_CHANNELS][RING_BUFFER_SIZE] = { { 0.0f } };

    std::atomic<uint64_t> totalWritten{ 0 };

    // --- Timestamped MIDI ---
    std::atomic<bool> needAnchorSync{ true };
    std::atomic<bool> prepareWasCalled{ false }; // NEW: Prevents Logic AU double-reset
    std::atomic<double> anchorMameTime{ 0.0 };
    std::atomic<uint64_t> anchorDawSample{ 0 };

    // Double precision seconds
    struct TimestampedMidi {
        uint8_t data;
        double targetMameTime;
        bool consumed = false; // time-ordered delivery: bytes may be consumed out of FIFO order
    };

    static constexpr int MIDI_BUFFER_SIZE = 524288;
    TimestampedMidi midiBuffer[MIDI_BUFFER_SIZE];

    std::atomic<int> midiWritePos{ 0 };
    std::atomic<int> midiReadPos{ 0 };

    // Realtime-byte deferral (MAME-thread only): while a channel message is on
    // the wire, clock bytes (FA/F8/FC) must not be emitted between its bytes —
    // the RZ-1 firmware drops a note-on that follows a clock byte interleaved
    // into the preceding note-off. midiRealtimeBlockUntil is the machine time
    // until which realtime bytes are held back.
    double midiRealtimeBlockUntil = -1.0;

    std::atomic<bool> newFrameAvailable{ false };
    
    bool lastOfflineState = false;
    int64_t lastPlayheadPos = 0;

    // Render diagnostics (first blocks only): helps identify hosts that stop
    // calling processBlock, which stalls MAME's audio-driven boot.
    int pbDiagCount = 0;
    bool pbLoggedBootedY = false;
    
    juce::String instanceTempDir; // Unique sandbox directory for this plugin instance
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnsoniqSD1AudioProcessor)
};
