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

    // Currently mouse-pressed button index (for highlight + release tracking)
    int pressedButtonIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnsoniqSD1AudioProcessorEditor)
};
