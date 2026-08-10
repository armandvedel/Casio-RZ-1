#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class EnsoniqSD1AudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit EnsoniqSD1AudioProcessorEditor (EnsoniqSD1AudioProcessor&);
    ~EnsoniqSD1AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    // Draws the whole panel in rz1.lay layout coordinates (800x535) using JUCE
    // vector graphics, so it renders crisply at any DPI/window size.
    void drawPanel (juce::Graphics&);

    EnsoniqSD1AudioProcessor& audioProcessor;

    // rz1.lay view space (all button bounds in the matrix are expressed in this space)
    static constexpr int layoutW = 800;
    static constexpr int layoutH = 535;

    // Maps an editor-space mouse position to layout space, passing through the
    // screen buffer and MAME's internal aspect-preserving letterbox.
    juce::Point<float> layoutFromEditor (juce::Point<int> editorPos) const;

    // Inverse of layoutFromEditor (used to draw the pressed-button highlight).
    juce::Point<float> editorFromLayout (juce::Point<float> layoutPos) const;

    // Returns the rz1Buttons index under the given layout point, or -1.
    int buttonIndexAt (juce::Point<float> layoutPos) const;

    // Instrument level faders (10 drum voices) in the INSTRUMENT LEVEL strip.
    int faderIndexAt (juce::Point<float> layoutPos) const;
    void setFaderFromY (int idx, float layoutY);

    // Multi-key panel input: the mouse gesture can hold several buttons at
    // once (e.g. hold SAMPLING while tapping SAMPLE 1), and latchable keys
    // (SAMPLING, MUTE, ACCENT, DELETE) stay held after a click so a single
    // mouse can perform two-key combos. Both sets drive the pressed highlight.
    bool isButtonHeld (int idx) const;
    bool isLatchableButton (int idx) const;
    void pressButton (int idx);
    void releaseButton (int idx);

    // Buttons currently held by the mouse gesture (released on mouse up).
    std::vector<int> heldButtons;

    // Buttons latched by a click (toggle-hold), kept pressed until clicked
    // again. SAMPLING/MUTE/ACCENT/DELETE are latchable for their combos.
    std::vector<int> latchedButtons;

    // Fader values (0..1, default full) and which fader is being dragged.
    // 0-9 instruments, 10 sampling level (visual only), 11 overall volume.
    float faderValues[12] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    int activeFader = -1;

    // Locks the panel aspect ratio while resizing (no letterbox space).
    std::unique_ptr<juce::ComponentBoundsConstrainer> boundsConstrainer;

    // Debounced persistence of the window size (settings.xml).
    bool pendingSizeSave = false;
    juce::uint32 lastSizeSaveTime = 0;
    juce::uint32 editorBirthTime = 0;
    bool sizeSettled = false;   // becomes true ~2s after creation (host-driven
                                // startup resizes are ignored for persistence)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnsoniqSD1AudioProcessorEditor)
};
