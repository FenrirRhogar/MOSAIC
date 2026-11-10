#pragma once

#include <JuceHeader.h>
#include "RackKit.h"

/*  Visual language shared with the other fenrikos plugins: a black anodised rack
    plate with silkscreen legends, machined knobs and the rune mark. */
namespace Theme
{
    const juce::Colour plateTop    = rack::colours::plateTop;
    const juce::Colour plateBottom = rack::colours::plateBottom;
    const juce::Colour cardFill    { 0xff222428 };
    const juce::Colour cardEdge    { 0x14ffffff };
    const juce::Colour glass       = rack::colours::glass;
    const juce::Colour track       { 0xff3a3c41 };
    const juce::Colour button      { 0xff2e3035 };

    /// Stroke with a soft bloom, for lit arcs and tracks.
    inline void glow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width)
    {
        g.setColour (colour.withAlpha (0.16f));
        g.strokePath (path, juce::PathStrokeType (width * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    /// On the rack plate a panel is a silkscreened outline.
    inline void card (juce::Graphics& g, juce::Rectangle<float> area, float /*corner*/ = 12.0f, const juce::String& title = {})
    {
        rack::silkBox (g, area, title);
    }

    /// The MOSAIC mark: Mannaz, the M rune. Two upright staves whose tops are joined by
    /// a crossing X, as round-capped strokes inside a square box.
    inline juce::Path mannaz (juce::Rectangle<float> box)
    {
        const float size = juce::jmin (box.getWidth(), box.getHeight());
        const auto at = [&] (float x, float y) { return juce::Point<float> (box.getX() + x * size, box.getY() + y * size); };

        juce::Path lines;
        for (auto [from, to] : { std::pair { at (0.24f, 0.10f), at (0.24f, 0.90f) },
                                 { at (0.76f, 0.10f), at (0.76f, 0.90f) },
                                 { at (0.24f, 0.10f), at (0.76f, 0.50f) },
                                 { at (0.76f, 0.10f), at (0.24f, 0.50f) } })
        {
            lines.startNewSubPath (from);
            lines.lineTo (to);
        }

        juce::Path rune;
        juce::PathStrokeType (0.12f * size, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded)
            .createStrokedPath (rune, lines);
        return rune;
    }

    /// The fenrikos brand gradient for rune marks, amber to red.
    inline void fillRune (juce::Graphics& g, const juce::Path& rune)
    {
        const auto b = rune.getBounds();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffc45a), b.getCentreX(), b.getY(),
                                                 juce::Colour (0xffe84830), b.getCentreX(), b.getBottom(), false));
        g.fillPath (rune);
    }
}
