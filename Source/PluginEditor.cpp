#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cstring>

// ==============================================================================
// NATIVE VECTOR PANEL
// All geometry lives in rz1.lay layout coordinates (800x535). paint() scales
// the whole panel to the window, so the UI is drawn by JUCE as vector graphics
// and stays pixel-crisp at any DPI — no MAME bitmap rasterization to upscale
// (same approach as the Ensoniq-EPS-16-Plus sister project).
// ==============================================================================
namespace
{

const juce::Colour panelBg   (38, 28, 28);   // background 0.15/0.11/0.11
const juce::Colour panelTeal (73, 188, 173); // 0.29/0.74/0.68
const juce::Colour panelWhite(249, 242, 226);// 0.98/0.95/0.89
const juce::Colour panelDark (38, 28, 28);
const juce::Colour lcdBg     (138, 146, 148);// driver palette pen 0
const juce::Colour lcdFg     (92, 83, 88);   // driver palette pen 1 (pixel on)

struct PanelText
{
    int x, y, w, h;
    const char* text;
    juce::Colour colour;
};

// Teal top bar segments + the thin section rules at y=177 (from rz1.lay).
const juce::Rectangle<int> tealBarRects[] =
{
    { 11, 15, 40, 10 }, { 53, 15, 36, 10 }, { 91, 15, 36, 10 }, { 129, 15, 36, 10 },
    { 167, 15, 36, 10 }, { 205, 15, 36, 10 }, { 243, 15, 36, 10 }, { 281, 15, 36, 10 },
    { 319, 15, 36, 10 }, { 357, 15, 36, 10 }, { 395, 15, 36, 10 }, { 433, 15, 67, 10 },
    { 502, 15, 56, 10 }, { 560, 15, 56, 10 }, { 618, 15, 160, 10 },
    { 53, 177, 378, 1 }, { 502, 177, 56, 1 }, { 560, 177, 56, 1 },
};

// White LCD bezel bars surrounding the 115x20 display.
const juce::Rectangle<int> lcdBezel[] =
{
    { 319, 211, 161, 13 }, { 319, 211, 23, 47 }, { 319, 244, 161, 14 }, { 457, 211, 23, 47 },
};

// All text elements (auto-generated from rz1.lay: position, string, color).
const PanelText panelTexts[] =
{
    { 69, 16, 3, 7, "1", panelDark },
    { 107, 16, 3, 7, "2", panelDark },
    { 143, 16, 3, 7, "3", panelDark },
    { 183, 16, 3, 7, "4", panelDark },
    { 221, 16, 3, 7, "5", panelDark },
    { 259, 16, 3, 7, "6", panelDark },
    { 296, 16, 3, 7, "7", panelDark },
    { 334, 16, 3, 7, "8", panelDark },
    { 373, 16, 3, 7, "9", panelDark },
    { 409, 16, 6, 7, "10", panelDark },
    { 623, 46, 141, 31, "CASIO", panelWhite },
    { 607, 123, 184, 9, "DIGITAL SAMPLING RHYTHM COMPOSER", panelTeal },
    { 665, 134, 59, 24, "RZ-1", panelTeal },
    { 502, 160, 56, 7, "SAMPLING", panelWhite },
    { 53, 168, 378, 7, "INSTRUMENT LEVEL", panelWhite },
    { 502, 168, 56, 7, "LEVEL", panelWhite },
    { 560, 168, 56, 7, "VOLUME", panelWhite },
    { 518, 226, 14, 7, "1/32", panelWhite },
    { 555, 226, 14, 7, "1/48", panelWhite },
    { 592, 226, 14, 7, "1/96", panelWhite },
    { 524, 237, 3, 7, "7", panelDark },
    { 562, 237, 3, 7, "8", panelDark },
    { 599, 237, 3, 7, "9", panelDark },
    { 73, 258, 22, 7, "SONG", panelWhite },
    { 518, 258, 14, 7, "1/12", panelWhite },
    { 555, 258, 14, 7, "1/16", panelWhite },
    { 592, 258, 14, 7, "1/24", panelWhite },
    { 263, 266, 19, 7, "RESET", panelWhite },
    { 524, 269, 3, 7, "4", panelDark },
    { 562, 269, 3, 7, "5", panelDark },
    { 599, 269, 3, 7, "6", panelDark },
    { 115, 270, 14, 7, "EDIT", panelWhite },
    { 148, 270, 24, 7, "DELETE", panelWhite },
    { 187, 270, 22, 7, "INSERT", panelWhite },
    { 226, 270, 18, 7, "CHAIN", panelWhite },
    { 263, 273, 19, 7, "/COPY", panelWhite },
    { 347, 280, 11, 7, "MT", panelWhite },
    { 438, 280, 16, 7, "MIDI", panelWhite },
    { 69, 288, 30, 7, "PATTERN", panelWhite },
    { 520, 288, 11, 7, "1/4", panelWhite },
    { 557, 288, 11, 7, "1/6", panelWhite },
    { 594, 288, 11, 7, "1/8", panelWhite },
    { 326, 289, 14, 7, "SAVE", panelWhite },
    { 364, 289, 14, 7, "LOAD", panelWhite },
    { 415, 289, 24, 7, "CHANNEL", panelWhite },
    { 455, 289, 18, 7, "CLOCK", panelWhite },
    { 190, 296, 14, 7, "AUTO", panelWhite },
    { 263, 296, 19, 7, "RESET", panelWhite },
    { 524, 299, 3, 7, "1", panelDark },
    { 562, 299, 3, 7, "2", panelDark },
    { 599, 299, 3, 7, "3", panelDark },
    { 110, 300, 25, 7, "RECORD", panelWhite },
    { 148, 300, 24, 7, "DELETE", panelWhite },
    { 228, 300, 16, 7, "BEAT", panelWhite },
    { 184, 303, 30, 7, "COMPENSATE", panelWhite },
    { 263, 303, 19, 7, "/COPY", panelWhite },
    { 340, 315, 23, 7, "TEMPO", panelWhite },
    { 450, 318, 32, 7, "SAMPLING", panelDark },
    { 520, 319, 11, 7, "1/2", panelWhite },
    { 572, 319, 18, 7, "VALUE", panelWhite },
    { 327, 329, 14, 7, "\xe2\x96\xbc", panelWhite },   // ▼
    { 364, 329, 14, 7, "\xe2\x96\xb2", panelWhite },   // ▲
    { 524, 329, 3, 7, "0", panelDark },
    { 556, 329, 14, 7, "\xe2\x96\xbc", panelWhite },   // ▼
    { 593, 329, 14, 7, "\xe2\x96\xb2", panelWhite },   // ▲
    { 557, 341, 11, 7, "NO", panelWhite },
    { 594, 341, 11, 7, "YES", panelWhite },
    { 66, 378, 54, 9, "START/STOP", panelWhite },
    { 189, 379, 34, 9, "ACCENT", panelWhite },
    { 279, 379, 25, 9, "TOM 1", panelWhite },
    { 335, 379, 25, 9, "TOM 3", panelWhite },
    { 395, 379, 17, 9, "RIM", panelWhite },
    { 439, 379, 39, 9, "OPEN HH", panelWhite },
    { 500, 379, 29, 9, "CLAPS", panelWhite },
    { 550, 379, 41, 9, "COWBELL", panelWhite },
    { 608, 379, 42, 9, "SAMPLE 1", panelDark },
    { 664, 379, 42, 9, "SAMPLE 3", panelDark },
    { 71, 428, 46, 9, "CONTINUE", panelWhite },
    { 80, 436, 27, 9, "START", panelWhite },
    { 194, 436, 25, 9, "MUTE", panelWhite },
    { 279, 436, 25, 9, "TOM 2", panelWhite },
    { 340, 436, 14, 9, "B D", panelWhite },
    { 397, 436, 13, 9, "S D", panelWhite },
    { 434, 436, 48, 9, "CLOSED HH", panelWhite },
    { 504, 436, 21, 9, "RIDE", panelWhite },
    { 555, 436, 30, 9, "CRASH", panelWhite },
    { 608, 436, 42, 9, "SAMPLE 2", panelDark },
    { 664, 436, 42, 9, "SAMPLE 4", panelDark },
};

juce::Colour buttonColourFor (const juce::String& id)
{
    if (id == "btn_song" || id == "btn_pattern" || id == "btn_startstop" || id == "btn_continue")
        return juce::Colour (224, 204, 0);       // button_yellow
    if (id == "btn_sampling")
        return juce::Colour (94, 137, 221);      // button_darkblue
    if (id == "btn_accent" || id == "btn_mute")
        return juce::Colour (153, 153, 188);     // button_blue
    if (id == "btn_0" || id == "btn_1" || id == "btn_2" || id == "btn_3" || id == "btn_4"
        || id == "btn_5" || id == "btn_6" || id == "btn_7" || id == "btn_8" || id == "btn_9"
        || id == "btn_sample1" || id == "btn_sample2" || id == "btn_sample3" || id == "btn_sample4")
        return juce::Colour (211, 206, 193);     // button_white
    return juce::Colour (84, 84, 84);            // button_gray
}

void drawPanelText (juce::Graphics& g, const PanelText& t)
{
    // ▼ / ▲ are drawn as triangles so we never depend on font glyph coverage.
    const bool downArrow = (std::strcmp (t.text, "\xe2\x96\xbc") == 0);
    if (downArrow || std::strcmp (t.text, "\xe2\x96\xb2") == 0)
    {
        const float cx = t.x + t.w * 0.5f;
        juce::Path p;
        if (downArrow)
            p.addTriangle (cx, t.y + t.h - 1.0f, t.x, t.y, t.x + t.w, t.y);
        else
            p.addTriangle (cx, t.y, t.x, t.y + t.h, t.x + t.w, t.y + t.h);
        g.setColour (t.colour);
        g.fillPath (p);
        return;
    }

    const int n = static_cast<int> (std::strlen (t.text));
    float fh = t.h * 1.3f;
    if (n > 1)
        fh = juce::jmin (fh, t.w * 1.6f / n);
    fh = juce::jmax (4.0f, fh);

    g.setFont (juce::FontOptions().withName ("Verdana").withHeight (fh).withStyle ("Bold"));
    g.setColour (t.colour);
    g.drawText (juce::String (t.text), t.x - 4, t.y - 1, t.w + 8, t.h + 2,
                juce::Justification::centred, false);
}

void drawLed (juce::Graphics& g, int x, int y, int w, int h, int state, bool redOnly)
{
    juce::Colour c (12, 12, 12);
    if (redOnly)
        c = (state >= 1) ? juce::Colour (204, 12, 12) : juce::Colour (12, 12, 12);
    else
        c = (state >= 2) ? juce::Colour (204, 12, 12)
          : (state == 1) ? juce::Colour (12, 204, 12)
          : juce::Colour (12, 12, 12);
    g.setColour (c);
    g.fillRect (x, y, w, h);
}

} // namespace

EnsoniqSD1AudioProcessorEditor::EnsoniqSD1AudioProcessorEditor (EnsoniqSD1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setOpaque (false);
    setPaintingIsUnclipped (true);
    setResizable (true, true);
    setSize (1200, 802); // rz1.lay native view at 1.5x (labels read clearly)
    setResizeLimits (480, 321, 3840, 2568);
    startTimerHz (30);
    audioProcessor.nativePanel.store (true, std::memory_order_release);
}

EnsoniqSD1AudioProcessorEditor::~EnsoniqSD1AudioProcessorEditor() = default;

void EnsoniqSD1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::transparentBlack);

    // Native vector panel: scale the 800x535 layout space into the window so
    // the whole panel is drawn by JUCE at the display's resolution.
    const float s = juce::jmin (getWidth() / layoutW, getHeight() / layoutH);
    const float ox = (getWidth() - layoutW * s) * 0.5f;
    const float oy = (getHeight() - layoutH * s) * 0.5f;

    g.saveState();
    g.addTransform (juce::AffineTransform::scale (s).translated (ox, oy));
    drawPanel (g);
    g.restoreState();

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

void EnsoniqSD1AudioProcessorEditor::drawPanel (juce::Graphics& g)
{
    // Background
    g.fillAll (panelBg);

    // Teal bars
    g.setColour (panelTeal);
    for (const auto& r : tealBarRects)
        g.fillRect (r);

    // Separator lines below the instrument section
    g.setColour (juce::Colour (51, 51, 51));
    g.fillRect (1, 180, 798, 1);
    g.setColour (juce::Colours::black);
    g.fillRect (1, 181, 798, 1);

    // LCD bezel
    g.setColour (juce::Colour (211, 206, 193));
    for (const auto& r : lcdBezel)
        g.fillRect (r);

    // LCD display (16 chars published by the MAME thread)
    g.setColour (lcdBg);
    g.fillRect (342, 224, 115, 20);
    char lcd[17] = {};
    const uint64_t lo = audioProcessor.lcdCharsLo.load (std::memory_order_relaxed);
    const uint64_t hi = audioProcessor.lcdCharsHi.load (std::memory_order_relaxed);
    for (int i = 0; i < 16; ++i)
    {
        const uint8_t c = static_cast<uint8_t> (i < 8 ? (lo >> (i * 8)) : (hi >> ((i - 8) * 8)));
        lcd[i] = (c >= 0x20 && c < 0x7f) ? static_cast<char> (c) : ' ';
    }
    g.setColour (lcdFg);
    g.setFont (juce::FontOptions().withName (juce::Font::getDefaultMonospacedFontName())
                                   .withHeight (11.0f).withStyle ("Bold"));
    g.drawText (juce::String (lcd), 342, 224, 115, 20, juce::Justification::centred, false);

    // Buttons (flat, faithful to rz1.lay)
    for (const auto& btn : audioProcessor.rz1Buttons)
    {
        if (btn.w <= 0 || btn.h <= 0)
            continue;
        g.setColour (buttonColourFor (btn.paramID));
        g.fillRect (btn.x, btn.y, btn.w, btn.h);
    }

    // Pressed-button highlight
    if (pressedButtonIndex >= 0
        && pressedButtonIndex < static_cast<int> (audioProcessor.rz1Buttons.size()))
    {
        const auto& btn = audioProcessor.rz1Buttons[static_cast<size_t> (pressedButtonIndex)];
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRect (btn.x, btn.y, btn.w, btn.h);
    }

    // Text labels
    for (const auto& t : panelTexts)
        drawPanelText (g, t);

    // LEDs
    drawLed (g, 526, 35, 9, 4, audioProcessor.ledSampling.load (std::memory_order_relaxed), true);
    drawLed (g, 50, 271, 9, 4, audioProcessor.ledSong.load (std::memory_order_relaxed), false);
    drawLed (g, 50, 301, 9, 4, audioProcessor.ledPattern.load (std::memory_order_relaxed), false);
    drawLed (g, 50, 404, 9, 4, audioProcessor.ledStartStop.load (std::memory_order_relaxed), false);
}

void EnsoniqSD1AudioProcessorEditor::resized()
{
    // Keep MAME's render target in lockstep with the plugin window (same aspect as rz1.lay)
    if (getWidth() > 0 && getHeight() > 0)
    {
        audioProcessor.windowWidth.store (getWidth(), std::memory_order_release);
        audioProcessor.windowHeight.store (getHeight(), std::memory_order_release);
        audioProcessor.requestRenderResize.store (true, std::memory_order_release);
    }
}

void EnsoniqSD1AudioProcessorEditor::timerCallback()
{
    repaint();
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
