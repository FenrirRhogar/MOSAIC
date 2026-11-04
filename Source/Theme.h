#pragma once

#include <JuceHeader.h>

/*  Visual language shared with the other fenrikos plugins: dark graphite cards,
    hairline edges, glowing accent arcs and the rune mark. */
namespace Theme
{
    const juce::Colour plateTop    { 0xff1e2127 };
    const juce::Colour plateBottom { 0xff131519 };
    const juce::Colour cardFill    { 0xff1a1d22 };
    const juce::Colour cardEdge    { 0x10ffffff };
    const juce::Colour glass       { 0xff0c0e11 };
    const juce::Colour track       { 0xff2f333b };
    const juce::Colour button      { 0xff252930 };

    /// Stroke with a soft bloom, for lit arcs and tracks.
    inline void glow (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float width)
    {
        g.setColour (colour.withAlpha (0.16f));
        g.strokePath (path, juce::PathStrokeType (width * 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    /// Raised card: soft shadow, slightly lighter top edge and a hairline border.
    inline void card (juce::Graphics& g, juce::Rectangle<float> area, float corner = 12.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (area.translated (0.0f, 3.0f).expanded (1.0f), corner + 1.0f);
        g.setGradientFill (juce::ColourGradient (cardFill.brighter (0.07f), area.getTopLeft(),
                                                 cardFill, area.getBottomLeft(), false));
        g.fillRoundedRectangle (area, corner);
        g.setColour (cardEdge);
        g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
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
