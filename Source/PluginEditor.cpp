#include "PluginProcessor.h"
#include "PluginEditor.h"

EnsoniqSD1AudioProcessorEditor::EnsoniqSD1AudioProcessorEditor (EnsoniqSD1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setOpaque (false);
    setPaintingIsUnclipped (true);
    setResizable (true, true);
    setSize (800, 535); // rz1.lay native view size (no letterboxing at default aspect)
    setResizeLimits (320, 214, 2560, 1712);
    startTimerHz (30);
}

EnsoniqSD1AudioProcessorEditor::~EnsoniqSD1AudioProcessorEditor() = default;

void EnsoniqSD1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::transparentBlack);

    const int bufferIndex = audioProcessor.readyBufferIndex.load(std::memory_order_acquire);
    // Copy the Image handle (ref-counted, cheap) so a buffer resize on the MAME
    // thread can never free the pixels mid-paint.
    const auto screenBuffer = audioProcessor.screenBuffers[bufferIndex];
    
    // Draw the MAME rendered layout. The buffer is rasterized at physical
    // pixels, so on a 2x display this maps 1:1 to the screen; high-quality
    // resampling only kicks in when the host applies its own zoom.
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageWithin (screenBuffer, 0, 0, getWidth(), getHeight(),
                       juce::RectanglePlacement::stretchToFit);

    // Highlight the currently pressed button (maps its layout rect back into editor space)
    if (pressedButtonIndex >= 0 && pressedButtonIndex < static_cast<int> (audioProcessor.rz1Buttons.size()))
    {
        const auto& btn = audioProcessor.rz1Buttons[static_cast<size_t> (pressedButtonIndex)];
        if (btn.w > 0 && btn.h > 0)
        {
            const auto tl = editorFromLayout (juce::Point<float> (static_cast<float> (btn.x),
                                                                  static_cast<float> (btn.y)));
            const auto br = editorFromLayout (juce::Point<float> (static_cast<float> (btn.x + btn.w),
                                                                  static_cast<float> (btn.y + btn.h)));
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRect (juce::Rectangle<float> (tl.x, tl.y, br.x - tl.x, br.y - tl.y));
        }
    }

    // Only overlay an error when the ROM set is actually missing; the panel
    // itself is the whole UI otherwise.
    if (!audioProcessor.verifyRomFiles())
    {
        g.setColour (juce::Colours::red);
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("ERROR: ROMs not found!", 10, 10, 200, 20, juce::Justification::topLeft, true);
        g.setFont (juce::Font (10.0f));
        const juce::String err = audioProcessor.lastRomError.isNotEmpty()
            ? audioProcessor.lastRomError
            : audioProcessor.missingFilesList;
        g.drawText (err, 10, 30, 380, 80, juce::Justification::topLeft, true);
    }
}

void EnsoniqSD1AudioProcessorEditor::resized()
{
    updateRenderSize();
}

void EnsoniqSD1AudioProcessorEditor::timerCallback()
{
    // Re-push the render size if the device scale changed (e.g. the plugin
    // window moved to a display with a different scale factor).
    const float scale = getRenderScale();
    if (scale != lastRenderScale)
        updateRenderSize();

    repaint();
}

float EnsoniqSD1AudioProcessorEditor::getRenderScale() const
{
    if (auto* peer = getPeer())
    {
        const double s = peer->getPlatformScaleFactor();
        if (s >= 1.0)
            return static_cast<float> (s);
    }

    // No peer yet (editor not attached to a host window): fall back to the
    // primary display so the initial boot still picks the right density.
    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        return juce::jmax (1.0f, static_cast<float> (display->scale));

    return 1.0f;
}

void EnsoniqSD1AudioProcessorEditor::updateRenderSize()
{
    // Keep MAME's render target in lockstep with the plugin window, but at
    // physical pixel resolution (logical points × device scale) so the panel
    // is displayed 1:1 on Retina. Same aspect as rz1.lay.
    const float scale = getRenderScale();
    const int physW = juce::jmax (1, juce::roundToInt (static_cast<float> (getWidth()) * scale));
    const int physH = juce::jmax (1, juce::roundToInt (static_cast<float> (getHeight()) * scale));

    // Bound the render resolution: a maximized window on a 3x display would
    // otherwise allocate multi-hundred-MB buffers. The capped remainder is
    // downscaled smoothly in paint().
    constexpr int maxPhysicalDim = 4096;
    int cappedW = physW;
    int cappedH = physH;
    if (juce::jmax (physW, physH) > maxPhysicalDim)
    {
        const float k = static_cast<float> (maxPhysicalDim) / static_cast<float> (juce::jmax (physW, physH));
        cappedW = juce::jmax (1, juce::roundToInt (static_cast<float> (physW) * k));
        cappedH = juce::jmax (1, juce::roundToInt (static_cast<float> (physH) * k));
    }

    lastRenderScale = scale;
    if (getWidth() > 0 && getHeight() > 0)
    {
        audioProcessor.windowWidth.store (cappedW, std::memory_order_release);
        audioProcessor.windowHeight.store (cappedH, std::memory_order_release);
        audioProcessor.requestRenderResize.store (true, std::memory_order_release);
    }
}

// ==============================================================================
// RZ-1 CLICK MAP (recipe §2: layout hit-test -> buttonParams -> ioport set_value)
// ==============================================================================

juce::Point<float> EnsoniqSD1AudioProcessorEditor::layoutFromEditor (juce::Point<int> editorPos) const
{
    const int bufferIndex = audioProcessor.readyBufferIndex.load (std::memory_order_acquire);
    const auto& screenBuffer = audioProcessor.screenBuffers[bufferIndex];
    const float sw = static_cast<float> (juce::jmax (screenBuffer.getWidth(), 1));
    const float sh = static_cast<float> (juce::jmax (screenBuffer.getHeight(), 1));
    const float ew = static_cast<float> (juce::jmax (getWidth(), 1));
    const float eh = static_cast<float> (juce::jmax (getHeight(), 1));

    // Stage 1: editor -> screen buffer (inverse of drawImageWithin stretchToFit)
    const float s1 = juce::jmin (ew / sw, eh / sh);
    const float ox = (ew - sw * s1) * 0.5f;
    const float oy = (eh - sh * s1) * 0.5f;
    const float bx = (static_cast<float> (editorPos.x) - ox) / s1;
    const float by = (static_cast<float> (editorPos.y) - oy) / s1;

    // Stage 2: screen buffer -> layout space (MAME letterboxes the 800x535 view
    // centered inside the target bounds, preserving aspect).
    const float s2 = juce::jmin (sw / static_cast<float> (layoutW), sh / static_cast<float> (layoutH));
    const float lx = (sw - static_cast<float> (layoutW) * s2) * 0.5f;
    const float ly = (sh - static_cast<float> (layoutH) * s2) * 0.5f;

    return { (bx - lx) / s2, (by - ly) / s2 };
}

juce::Point<float> EnsoniqSD1AudioProcessorEditor::editorFromLayout (juce::Point<float> layoutPos) const
{
    const int bufferIndex = audioProcessor.readyBufferIndex.load (std::memory_order_acquire);
    const auto& screenBuffer = audioProcessor.screenBuffers[bufferIndex];
    const float sw = static_cast<float> (juce::jmax (screenBuffer.getWidth(), 1));
    const float sh = static_cast<float> (juce::jmax (screenBuffer.getHeight(), 1));
    const float ew = static_cast<float> (juce::jmax (getWidth(), 1));
    const float eh = static_cast<float> (juce::jmax (getHeight(), 1));

    // Stage 2 inverse
    const float s2 = juce::jmin (sw / static_cast<float> (layoutW), sh / static_cast<float> (layoutH));
    const float lx = (sw - static_cast<float> (layoutW) * s2) * 0.5f;
    const float ly = (sh - static_cast<float> (layoutH) * s2) * 0.5f;
    const float bx = layoutPos.x * s2 + lx;
    const float by = layoutPos.y * s2 + ly;

    // Stage 1 inverse
    const float s1 = juce::jmin (ew / sw, eh / sh);
    const float ox = (ew - sw * s1) * 0.5f;
    const float oy = (eh - sh * s1) * 0.5f;

    return { bx * s1 + ox, by * s1 + oy };
}

int EnsoniqSD1AudioProcessorEditor::buttonIndexAt (juce::Point<float> layoutPos) const
{
    for (size_t i = 0; i < audioProcessor.rz1Buttons.size(); ++i)
    {
        const auto& btn = audioProcessor.rz1Buttons[i];
        if (btn.w <= 0 || btn.h <= 0)
            continue;
        if (layoutPos.x >= static_cast<float> (btn.x) && layoutPos.x < static_cast<float> (btn.x + btn.w) &&
            layoutPos.y >= static_cast<float> (btn.y) && layoutPos.y < static_cast<float> (btn.y + btn.h))
            return static_cast<int> (i);
    }
    return -1;
}

void EnsoniqSD1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const int idx = buttonIndexAt (layoutFromEditor (e.getPosition()));
    if (idx >= 0 && idx < static_cast<int> (audioProcessor.buttonParams.size()))
    {
        // Momentary press: release anything held, then press the hit button.
        if (pressedButtonIndex >= 0 && pressedButtonIndex < static_cast<int> (audioProcessor.buttonParams.size()))
            audioProcessor.buttonParams[static_cast<size_t> (pressedButtonIndex)]->store (0.0f, std::memory_order_release);

        pressedButtonIndex = idx;
        audioProcessor.buttonParams[static_cast<size_t> (idx)]->store (1.0f, std::memory_order_release);
    }
}

void EnsoniqSD1AudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    const int idx = buttonIndexAt (layoutFromEditor (e.getPosition()));
    if (idx != pressedButtonIndex)
    {
        if (pressedButtonIndex >= 0 && pressedButtonIndex < static_cast<int> (audioProcessor.buttonParams.size()))
            audioProcessor.buttonParams[static_cast<size_t> (pressedButtonIndex)]->store (0.0f, std::memory_order_release);
        pressedButtonIndex = -1;

        if (idx >= 0 && idx < static_cast<int> (audioProcessor.buttonParams.size()))
        {
            pressedButtonIndex = idx;
            audioProcessor.buttonParams[static_cast<size_t> (idx)]->store (1.0f, std::memory_order_release);
        }
    }
}

void EnsoniqSD1AudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (pressedButtonIndex >= 0 && pressedButtonIndex < static_cast<int> (audioProcessor.buttonParams.size()))
        audioProcessor.buttonParams[static_cast<size_t> (pressedButtonIndex)]->store (0.0f, std::memory_order_release);
    pressedButtonIndex = -1;
}
