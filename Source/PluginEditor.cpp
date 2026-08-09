#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
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

// Lavender background strips behind the sampling/sample labels (button_blue).
const juce::Rectangle<int> sampleLabelStrips[] =
{
    { 446, 317, 37, 9 },   // SAMPLING
    { 605, 379, 46, 9 },   // SAMPLE 1
    { 605, 436, 46, 9 },   // SAMPLE 2
    { 662, 379, 46, 9 },   // SAMPLE 3
    { 662, 436, 46, 9 },   // SAMPLE 4
};

// Faders filling the horizontal space above the section labels: 10 instrument
// faders evenly spaced and aligned with the 1..10 digits on the teal strip
// (the digits were nudged to the same even pitch), a sampling level fader
// under the SAMPLING/LEVEL area (x 530) and an overall volume fader under
// VOLUME (x 588).
// They start below the top bar AND the sampling LED, so they are a bit shorter.
constexpr int    faderCount  = 12;
constexpr float  faderCenters[faderCount] =
    { 70.5f, 108.5f, 146.5f, 184.5f, 222.5f, 260.5f, 298.5f, 336.5f, 374.5f, 412.5f,
      530.0f, 588.0f };
constexpr float  faderHalfCell = 19.0f;
constexpr float  faderY0     = 42.0f;  // below the top bar and the sampling LED
constexpr float  faderH      = 97.0f;  // y 42..139 (shorter, clear of the LED)
constexpr float  faderSlotW  = 30.0f;
constexpr float  faderCapH   = 12.0f;
constexpr float  faderLabelY = faderY0 + faderH + 2.0f; // 141, below the fader
constexpr float  faderLabelH = 14.0f;

struct FaderLabel { const char* line1; const char* line2; };

const FaderLabel faderLabels[faderCount] =
{
    { "TOM 1", nullptr }, { "TOM 2", nullptr }, { "TOM 3", nullptr }, { "B D", nullptr },
    { "RIM/SD", nullptr }, { "HIHAT", nullptr }, { "CLAPS", "RIDE" },
    { "COWBELL", "CRASH" }, { "SAMPLE", "1/2" }, { "SAMPLE", "3/4" },
    { "SAMPLING", nullptr }, { "VOLUME", nullptr },
};

// All text elements (auto-generated from rz1.lay: position, string, color).
const PanelText panelTexts[] =
{
    { 69, 16, 3, 7, "1", panelDark },
    { 107, 16, 3, 7, "2", panelDark },
    { 145, 16, 3, 7, "3", panelDark },
    { 183, 16, 3, 7, "4", panelDark },
    { 221, 16, 3, 7, "5", panelDark },
    { 259, 16, 3, 7, "6", panelDark },
    { 297, 16, 3, 7, "7", panelDark },
    { 335, 16, 3, 7, "8", panelDark },
    { 373, 16, 3, 7, "9", panelDark },
    { 409, 16, 6, 7, "10", panelDark },
    { 623, 46, 141, 31, "CASIO", panelWhite },
    { 607, 123, 184, 9, "DIGITAL SAMPLING RHYTHM COMPOSER", panelTeal },
    { 665, 134, 59, 24, "RZ-1", panelTeal },
    { 53, 168, 378, 7, "INSTRUMENT LEVEL", panelWhite },
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
    // All labels of the same height render at the same size so, e.g., "B D"
    // matches "TOM 2" and "SONG" matches "PATTERN". The width clamp only
    // shrinks genuinely long words in very narrow boxes; it never clips.
    // The global scale keeps every label comfortably inside its bounds.
    constexpr float labelSizeScale = 0.72f;
    float fh = t.h * 1.35f * labelSizeScale;
    if (n > 1)
        fh = juce::jmin (fh, t.w * 3.0f / n);
    fh = juce::jmax (4.0f, fh);

    g.setFont (juce::FontOptions().withName ("Verdana").withHeight (fh).withStyle ("Bold"));
    g.setColour (t.colour);
    // Generous draw rect centered on the box so the (possibly larger) glyphs
    // are never clipped vertically or horizontally.
    const float rectW = t.w + 16.0f;
    const float rectH = fh + 6.0f;
    g.drawText (juce::String (t.text),
                t.x - 8.0f, t.y + (t.h - rectH) * 0.5f, rectW, rectH,
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
    setResizeLimits (480, 321, 3840, 2568);

    // Lock the panel aspect ratio (800:535) so resizing never creates extra
    // space at the sides.
    boundsConstrainer = std::make_unique<juce::ComponentBoundsConstrainer>();
    boundsConstrainer->setFixedAspectRatio (800.0 / 535.0);
    boundsConstrainer->setMinimumSize (480, 321);
    boundsConstrainer->setMaximumSize (3840, 2568);
    setConstrainer (boundsConstrainer.get());

    // Restore the remembered window size straight from settings.xml (the DAW
    // state is unreliable for this), snapped to the locked aspect.
    int w = 0, h = 0;
    {
        const juce::File settingsFile =
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile ("CasioRZ1").getChildFile ("settings.xml");
        if (auto xml = juce::XmlDocument::parse (settingsFile))
        {
            w = xml->getIntAttribute ("window_width", 0);
            h = xml->getIntAttribute ("window_height", 0);
        }
    }
    if (w <= 0 || h <= 0) { w = 1200; h = 802; }   // default: 1.5x native view
    w = juce::jlimit (480, 3840, w);
    h = juce::roundToInt (w * 535.0f / 800.0f);
    h = juce::jlimit (321, 2568, h);
    w = juce::roundToInt (h * 800.0f / 535.0f);
    setSize (w, h);

    editorBirthTime = juce::Time::getMillisecondCounter();
    startTimerHz (30);
    audioProcessor.nativePanel.store (true, std::memory_order_release);
}

EnsoniqSD1AudioProcessorEditor::~EnsoniqSD1AudioProcessorEditor()
{
    if (sizeSettled)
        audioProcessor.saveGlobalSettings();
}

void EnsoniqSD1AudioProcessorEditor::paint (juce::Graphics& g)
{
    // Fill the whole window with the panel background so the artwork always
    // matches the window, even if the host gives us a slightly different aspect.
    g.fillAll (panelBg);

    // Native vector panel: scale the 800x535 layout space into the window so
    // the whole panel is drawn by JUCE at the display's resolution.
    const float s = juce::jmin (getWidth() / static_cast<float> (layoutW),
                                getHeight() / static_cast<float> (layoutH));
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

    // Lavender strips behind the sampling/sample labels
    g.setColour (juce::Colour (153, 153, 188));
    for (const auto& r : sampleLabelStrips)
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

    // Instrument level faders
    for (int i = 0; i < faderCount; ++i)
    {
        const float cx = faderCenters[i];
        const float slotX = cx - faderSlotW * 0.5f;

        // Slot (track)
        g.setColour (juce::Colour (51, 51, 51));
        g.fillRect (slotX, faderY0, faderSlotW, faderH);

        // Instrument label(s) below the fader (up to two lines)
        g.setFont (juce::FontOptions().withName ("Verdana").withHeight (7.0f).withStyle ("Bold"));
        g.setColour (panelWhite);
        if (faderLabels[i].line2 != nullptr)
        {
            g.drawText (juce::String (faderLabels[i].line1),
                        cx - faderHalfCell, faderLabelY, faderHalfCell * 2.0f, 7.0f,
                        juce::Justification::centred, false);
            g.drawText (juce::String (faderLabels[i].line2),
                        cx - faderHalfCell, faderLabelY + 7.0f, faderHalfCell * 2.0f, 7.0f,
                        juce::Justification::centred, false);
        }
        else
        {
            g.drawText (juce::String (faderLabels[i].line1),
                        cx - faderHalfCell, faderLabelY, faderHalfCell * 2.0f, faderLabelH,
                        juce::Justification::centred, false);
        }

        // Cap, positioned by the fader value (top = full).
        const float v = (i < faderCount) ? faderValues[i] : 1.0f;
        const float capY = faderY0 + (1.0f - v) * (faderH - faderCapH);
        g.setColour (juce::Colour (211, 206, 193));
        g.fillRect (slotX - 1.0f, capY, faderSlotW + 2.0f, faderCapH);

        // Grip groove on the cap
        g.setColour (juce::Colour (38, 28, 28));
        g.fillRect (slotX + 2.0f, capY + faderCapH * 0.5f - 0.5f, faderSlotW - 4.0f, 1.0f);
    }

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
    // Remember the window size for the next open (persisted on a debounce).
    audioProcessor.savedWindowWidth = getWidth();
    audioProcessor.savedWindowHeight = getHeight();
    if (sizeSettled)
        pendingSizeSave = true;

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
    if (!sizeSettled)
    {
        const juce::uint32 now = juce::Time::getMillisecondCounter();
        if (now - editorBirthTime >= 2000)
            sizeSettled = true;
    }

    if (pendingSizeSave)
    {
        const juce::uint32 now = juce::Time::getMillisecondCounter();
        if (now - lastSizeSaveTime >= 1000)
        {
            pendingSizeSave = false;
            lastSizeSaveTime = now;
            audioProcessor.saveGlobalSettings();
        }
    }
    repaint();
}

// ==============================================================================
// RZ-1 CLICK MAP (recipe §2: layout hit-test -> buttonParams -> ioport set_value)
// ==============================================================================

juce::Point<float> EnsoniqSD1AudioProcessorEditor::layoutFromEditor (juce::Point<int> editorPos) const
{
    // Inverse of the paint() transform: editor space -> 800x535 layout space.
    // This must not depend on the (now unused) MAME screen buffers, which
    // never resize once the native panel is active.
    const float s = juce::jmin (getWidth() / static_cast<float> (layoutW),
                                getHeight() / static_cast<float> (layoutH));
    const float ox = (getWidth() - layoutW * s) * 0.5f;
    const float oy = (getHeight() - layoutH * s) * 0.5f;
    return { (static_cast<float> (editorPos.x) - ox) / s,
             (static_cast<float> (editorPos.y) - oy) / s };
}

juce::Point<float> EnsoniqSD1AudioProcessorEditor::editorFromLayout (juce::Point<float> layoutPos) const
{
    const float s = juce::jmin (getWidth() / static_cast<float> (layoutW),
                                getHeight() / static_cast<float> (layoutH));
    const float ox = (getWidth() - layoutW * s) * 0.5f;
    const float oy = (getHeight() - layoutH * s) * 0.5f;
    return { layoutPos.x * s + ox, layoutPos.y * s + oy };
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

int EnsoniqSD1AudioProcessorEditor::faderIndexAt (juce::Point<float> layoutPos) const
{
    if (layoutPos.y < faderY0 || layoutPos.y > faderY0 + faderH)
        return -1;
    int best = -1;
    float bestDist = faderHalfCell;
    for (int k = 0; k < faderCount; ++k)
    {
        const float d = std::abs (layoutPos.x - faderCenters[k]);
        if (d < bestDist)
        {
            bestDist = d;
            best = k;
        }
    }
    return best;
}

void EnsoniqSD1AudioProcessorEditor::setFaderFromY (int idx, float layoutY)
{
    if (idx < 0 || idx >= faderCount)
        return;
    const float travel = faderH - faderCapH;
    float v = 1.0f - (layoutY - faderY0) / travel;
    v = juce::jlimit (0.0f, 1.0f, v);
    faderValues[idx] = v;
    if (idx >= 0 && idx < 10)
        audioProcessor.instrumentLevel[idx].store (v, std::memory_order_relaxed);
    else if (idx == 11)
        audioProcessor.masterVolume.store (v, std::memory_order_relaxed);
    // idx 10 (sampling level) is visual only — the emulation has no sampling input.
    repaint();
}

void EnsoniqSD1AudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const juce::Point<float> lp = layoutFromEditor (e.getPosition());

    // Faders take priority over buttons in the INSTRUMENT LEVEL strip.
    const int fader = faderIndexAt (lp);
    if (fader >= 0)
    {
        activeFader = fader;
        setFaderFromY (fader, lp.y);
        return;
    }

    const int idx = buttonIndexAt (lp);
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
    const juce::Point<float> lp = layoutFromEditor (e.getPosition());

    if (activeFader >= 0)
    {
        setFaderFromY (activeFader, lp.y);
        return;
    }

    const int idx = buttonIndexAt (lp);
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
    activeFader = -1;
    if (pressedButtonIndex >= 0 && pressedButtonIndex < static_cast<int> (audioProcessor.buttonParams.size()))
        audioProcessor.buttonParams[static_cast<size_t> (pressedButtonIndex)]->store (0.0f, std::memory_order_release);
    pressedButtonIndex = -1;
}
