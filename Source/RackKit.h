#pragma once

#include <JuceHeader.h>
#include <map>

/// Hardware-rack drawing kit shared by the fenrikos plugins: brushed anodised plate with
/// rack ears, machined knobs, illuminated push buttons, bezels, recessed windows, screws
/// and silkscreen legends. Everything here is procedural; the static parts are meant to be
/// drawn once into cached images.
namespace rack
{
    namespace colours
    {
        const juce::Colour plateTop    { 0xff2b2d31 };
        const juce::Colour plateBottom { 0xff1d1f22 };
        const juce::Colour silk        { 0xffeae4d3 };   // silkscreen ink
        const juce::Colour silkDim     { 0xff9c978a };
        const juce::Colour glass       { 0xff08090b };
        const juce::Colour lcd         { 0xff0b0c0a };
        const juce::Colour metalLight  { 0xffd9dadc };
        const juce::Colour metalDark   { 0xff8d9095 };
    }

    static constexpr int earWidth = 34;

    inline juce::Point<float> polar (juce::Point<float> centre, float radius, float angle)
    {
        return centre.translated (radius * std::sin (angle), -radius * std::cos (angle));
    }

    /// Silkscreen legend with a hairline shadow.
    inline void silk (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                      juce::Justification just, juce::Colour colour = colours::silk)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawText (text, area.translated (0.0f, 1.0f), just, false);
        g.setColour (colour);
        g.drawText (text, area, just, false);
    }

    /// Panel LED in a chrome bezel: a domed lens that glows when lit.
    inline void led (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, bool on)
    {
        const auto bezel = juce::Rectangle<float> (radius * 2.0f + 3.0f, radius * 2.0f + 3.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff15161a), bezel.getCentreX(), bezel.getY(),
                                                 juce::Colour (0xff6a6d73), bezel.getCentreX(), bezel.getBottom(), false));
        g.fillEllipse (bezel);
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.5f), centre, colour.withAlpha (0.0f),
                                                     centre.translated (radius * 3.8f, 0.0f), true));
            g.fillEllipse (juce::Rectangle<float> (radius * 7.6f, radius * 7.6f).withCentre (centre));
        }
        const auto lens = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        g.setGradientFill (juce::ColourGradient (on ? juce::Colours::white.interpolatedWith (colour, 0.35f) : colour.darker (1.2f),
                                                 centre.x - radius * 0.3f, centre.y - radius * 0.35f,
                                                 on ? colour : colour.darker (3.0f), centre.x + radius, centre.y + radius, true));
        g.fillEllipse (lens);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.22f));
        g.fillEllipse (juce::Rectangle<float> (radius * 0.7f, radius * 0.45f).withCentre (centre.translated (-radius * 0.25f, -radius * 0.45f)));
    }

    /// Brushed aluminium with fine horizontal grain; deterministic so it never shimmers.
    inline void brushedMetal (juce::Graphics& g, juce::Rectangle<float> area,
                              juce::Colour top = colours::plateTop, juce::Colour bottom = colours::plateBottom, int seed = 7)
    {
        g.setGradientFill (juce::ColourGradient (top, area.getX(), area.getY(), bottom, area.getX(), area.getBottom(), false));
        g.fillRect (area);
        juce::Random random (seed);
        const auto lines = (int) (area.getHeight() * 2.2f);
        for (int i = 0; i < lines; ++i)
        {
            const auto y = area.getY() + random.nextFloat() * area.getHeight();
            const auto x = area.getX() + random.nextFloat() * area.getWidth() * 0.9f - area.getWidth() * 0.2f;
            const auto len = area.getWidth() * (0.2f + random.nextFloat() * 0.8f);
            const bool light = random.nextBool();
            g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (0.012f + random.nextFloat() * (light ? 0.025f : 0.05f)));
            g.fillRect (juce::Rectangle<float> (x, y, len, random.nextFloat() < 0.15f ? 1.0f : 0.5f).getIntersection (area));
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.045f), area.getX(), area.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), area.getCentreX(), area.getBottom(), false));
        g.fillRect (area);
    }

    /// Recessed cut-out: shadowed top lip, lit bottom lip.
    inline void recess (juce::Graphics& g, juce::Rectangle<float> area, float corner, juce::Colour fill = colours::glass)
    {
        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.0f).expanded (1.0f), corner + 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.fillRoundedRectangle (area.expanded (1.0f), corner + 1.0f);
        g.setColour (fill);
        g.fillRoundedRectangle (area, corner);
        const juce::Graphics::ScopedSaveState save (g);
        juce::Path shape;
        shape.addRoundedRectangle (area, corner);
        g.reduceClipRegion (shape);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, area.getY(),
                                                 juce::Colours::black.withAlpha (0.0f), 0.0f, area.getY() + 10.0f, false));
        g.fillRect (area.withHeight (10.0f));
    }

    /// Raised black bezel around a meter or screen.
    inline void bezel (juce::Graphics& g, juce::Rectangle<float> area, float corner)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (area.translated (0.0f, 3.0f).expanded (1.5f), corner + 1.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d4045), 0.0f, area.getY(),
                                                 juce::Colour (0xff111214), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area, corner);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1e2023), 0.0f, area.getY(),
                                                 juce::Colour (0xff2a2c30), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area.reduced (2.0f), juce::jmax (0.0f, corner - 2.0f));
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawLine (area.getX() + corner, area.getY() + 0.5f, area.getRight() - corner, area.getY() + 0.5f, 1.0f);
    }

    /// Inner shadow and a faint diagonal reflection for a screen; draw over the content.
    inline void glassOverlay (juce::Graphics& g, juce::Rectangle<float> screen, float corner)
    {
        const juce::Graphics::ScopedSaveState save (g);
        juce::Path shape;
        shape.addRoundedRectangle (screen, corner);
        g.reduceClipRegion (shape);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), 0.0f, screen.getY(),
                                                 juce::Colours::black.withAlpha (0.0f), 0.0f, screen.getY() + 12.0f, false));
        g.fillRect (screen.withHeight (12.0f));
        juce::Path sheen;
        sheen.startNewSubPath (screen.getX(), screen.getY());
        sheen.lineTo (screen.getX() + screen.getWidth() * 0.45f, screen.getY());
        sheen.lineTo (screen.getX() + screen.getWidth() * 0.25f, screen.getBottom());
        sheen.lineTo (screen.getX(), screen.getBottom());
        sheen.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), screen.getX(), screen.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), screen.getX() + screen.getWidth() * 0.3f, screen.getCentreY(), false));
        g.fillPath (sheen);
    }

    /// Phillips-head screw.
    inline void screw (juce::Graphics& g, juce::Point<float> centre, float radius, float angle = 0.6f)
    {
        const auto head = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (head.expanded (1.5f).translated (0.0f, 1.0f));
        g.setGradientFill (juce::ColourGradient (colours::metalLight, head.getX(), head.getY(),
                                                 juce::Colour (0xff55585e), head.getRight(), head.getBottom(), false));
        g.fillEllipse (head);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7d8086), head.getX(), head.getY(),
                                                 juce::Colour (0xffc4c6ca), head.getRight(), head.getBottom(), false));
        g.fillEllipse (head.reduced (radius * 0.18f));
        juce::Path cross;
        const auto arm = radius * 0.55f;
        for (int k = 0; k < 2; ++k)
        {
            const auto a = angle + (float) k * juce::MathConstants<float>::halfPi;
            const auto d = juce::Point<float> (std::cos (a), std::sin (a)) * arm;
            cross.startNewSubPath (centre - d);
            cross.lineTo (centre + d);
        }
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (cross, juce::PathStrokeType (radius * 0.28f), juce::AffineTransform::translation (0.4f, 0.6f));
        g.setColour (juce::Colour (0xff2a2c30));
        g.strokePath (cross, juce::PathStrokeType (radius * 0.28f));
    }

    /// Machined groove across the plate.
    inline void seam (juce::Graphics& g, float x1, float x2, float y)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (x1, y, x2 - x1, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (x1, y + 1.0f, x2 - x1, 1.0f);
    }

    /// Silkscreened section outline, the title printed in a gap in the top rule.
    inline void silkBox (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title = {},
                         juce::Colour titleColour = colours::silk, float corner = 6.0f)
    {
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold).withKerningFactor (0.18f));
        const auto textWidth = title.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), title) + 14.0f;
        {
            const juce::Graphics::ScopedSaveState save (g);
            if (textWidth > 0.0f)
                g.excludeClipRegion (juce::Rectangle<float> (area.getX() + 14.0f, area.getY() - 8.0f, textWidth, 16.0f).toNearestInt());
            g.setColour (colours::silk.withAlpha (0.38f));
            g.drawRoundedRectangle (area, corner, 1.0f);
        }
        if (textWidth > 0.0f)
            silk (g, title, { area.getX() + 21.0f, area.getY() - 8.0f, textWidth, 16.0f }, juce::Justification::centredLeft, titleColour);
    }

    /// Whole-faceplate background: brushed plate, rack ears with slotted screws, lit edges.
    inline void faceplate (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        brushedMetal (g, bounds);
        const auto ear = (float) earWidth;
        for (auto earArea : { bounds.withWidth (ear), bounds.withLeft (bounds.getRight() - ear) })
        {
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.fillRect (earArea);
            const bool left = earArea.getX() == bounds.getX();
            const auto foldX = left ? earArea.getRight() : earArea.getX();
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRect (foldX - (left ? 1.0f : 0.0f), bounds.getY(), 1.0f, bounds.getHeight());
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (foldX + (left ? 0.0f : -1.0f), bounds.getY(), 1.0f, bounds.getHeight());
            for (auto y : { bounds.getY() + 46.0f, bounds.getBottom() - 46.0f })
            {
                const auto slot = juce::Rectangle<float> (13.0f, 24.0f).withCentre ({ earArea.getCentreX(), y });
                recess (g, slot, 6.5f, juce::Colour (0xff050506));
                screw (g, { slot.getCentreX(), slot.getCentreY() + 3.0f }, 6.0f, y < bounds.getCentreY() ? 0.4f : 1.1f);
            }
        }
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (bounds.withHeight (1.0f));
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRect (bounds.withTop (bounds.getBottom() - 1.0f));
    }

    /// Small machined push button; `lit` adds an LED in `ledColour` at its left.
    inline void pushButton (juce::Graphics& g, juce::Rectangle<float> area, bool over, bool down, bool latched = false)
    {
        area = area.reduced (1.0f, 1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (area.translated (0.0f, 1.5f), 3.5f);
        if (down || latched)
            area.translate (0.0f, 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (latched ? 0xff2a2c30 : (over ? 0xff4a4d53 : 0xff404349)), 0.0f, area.getY(),
                                                 juce::Colour (latched ? 0xff1b1c1f : 0xff222428), 0.0f, area.getBottom(), false));
        g.fillRoundedRectangle (area, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (down || latched ? 0.05f : 0.16f));
        g.drawLine (area.getX() + 3.0f, area.getY() + 0.5f, area.getRight() - 3.0f, area.getY() + 0.5f, 1.0f);
    }

    /// Illuminated push button: square lens in a black collar that glows when engaged.
    inline void illuminatedButton (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour, bool lit,
                                   bool over, bool down, const juce::String& legend)
    {
        const auto collar = bounds.reduced (2.0f);
        if (lit)
        {
            g.setColour (colour.withAlpha (0.18f));
            g.fillRoundedRectangle (collar.expanded (2.0f), 6.0f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.fillRoundedRectangle (collar, 4.0f);
        auto lens = collar.reduced (2.5f);
        if (down)
            lens.translate (0.0f, 0.8f);
        if (lit)
            g.setGradientFill (juce::ColourGradient (colour.brighter (0.6f), lens.getCentreX(), lens.getCentreY(),
                                                     colour.darker (0.25f), lens.getX(), lens.getY(), true));
        else
            g.setGradientFill (juce::ColourGradient (colour.darker (1.6f).withSaturation (0.4f), 0.0f, lens.getY(),
                                                     colour.darker (2.6f).withSaturation (0.3f), 0.0f, lens.getBottom(), false));
        g.fillRoundedRectangle (lens, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (lit ? 0.45f : (over ? 0.2f : 0.1f)));
        g.drawLine (lens.getX() + 2.0f, lens.getY() + 0.7f, lens.getRight() - 2.0f, lens.getY() + 0.7f, 1.0f);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.15f));
        g.setColour (lit ? juce::Colour (0xff1b1208).withAlpha (0.8f) : colour.withAlpha (0.5f));
        g.drawText (legend, lens, juce::Justification::centred);
    }

    /// The parts of a knob that do not turn with it (lighting is fixed): shadow, skirt,
    /// knurled grip and spun-aluminium cap. Returns an image `side` points square.
    inline juce::Image renderKnobBody (float radius, float scale)
    {
        const auto pad = radius * 0.35f;
        const auto side = (radius + pad) * 2.0f;
        juce::Image image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (side * scale)), juce::jmax (1, juce::roundToInt (side * scale)), true);
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));
        const juce::Point<float> c (side * 0.5f, side * 0.5f);
        const auto circle = [&c] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c); };

        for (int i = 6; i >= 0; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.07f));
            g.fillEllipse (circle (radius + (float) i * radius * 0.035f).translated (radius * 0.05f, radius * 0.12f));
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c41), c.x, c.y - radius, juce::Colour (0xff0c0d0f), c.x, c.y + radius, false));
        g.fillEllipse (circle (radius));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1b1e), c.x, c.y - radius, juce::Colour (0xff26282c), c.x, c.y + radius, false));
        g.fillEllipse (circle (radius * 0.93f));

        const auto gripR = radius * 0.8f;
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (circle (gripR + 1.5f).translated (0.0f, 1.5f));
        g.setColour (juce::Colour (0xff141518));
        g.fillEllipse (circle (gripR));
        const int ridges = radius > 20.0f ? 48 : 32;
        for (int i = 0; i < ridges; ++i)
        {
            const auto a = (float) i / (float) ridges * juce::MathConstants<float>::twoPi;
            const auto light = 0.5f + 0.5f * std::cos (a - juce::MathConstants<float>::pi * 1.75f);
            g.setColour (juce::Colour::fromFloatRGBA (0.16f + 0.3f * light, 0.165f + 0.3f * light, 0.18f + 0.3f * light, 1.0f));
            juce::Path ridge;
            ridge.startNewSubPath (polar (c, gripR * 0.86f, a));
            ridge.lineTo (polar (c, gripR - 0.6f, a));
            g.strokePath (ridge, juce::PathStrokeType (juce::jmax (0.8f, gripR * 0.045f)));
        }

        const auto capR = gripR * 0.82f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (circle (capR + 1.0f).translated (0.0f, 1.0f));
        g.setColour (colours::metalDark.interpolatedWith (colours::metalLight, 0.5f).darker (0.15f));
        g.fillEllipse (circle (capR));
        constexpr int wedges = 180;
        for (int i = 0; i < wedges; ++i)
        {
            const auto a0 = (float) i / (float) wedges * juce::MathConstants<float>::twoPi;
            const auto a1 = (float) (i + 1) / (float) wedges * juce::MathConstants<float>::twoPi + 0.04f;
            const auto mid = 0.5f * (a0 + a1);
            const auto sheen = 0.5f + 0.26f * std::cos (2.0f * (mid - 0.75f)) + 0.12f * std::cos (mid - 5.5f);
            juce::Path wedge;
            wedge.addPieSegment (circle (capR), a0, a1, 0.0f);
            g.setColour (colours::metalDark.interpolatedWith (colours::metalLight, juce::jlimit (0.0f, 1.0f, sheen)).darker (0.15f));
            g.fillPath (wedge);
        }
        for (float rr = capR * 0.15f; rr < capR; rr += 1.3f)
        {
            g.setColour ((((int) (rr * 7.0f)) % 3 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.035f));
            g.drawEllipse (circle (rr), 0.6f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, c, juce::Colours::black.withAlpha (0.25f), c.translated (capR, 0.0f), true));
        g.fillEllipse (circle (capR));
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawEllipse (circle (capR - 0.5f), 0.8f);

        juce::Path rim;
        rim.addCentredArc (c.x, c.y, radius - 0.6f, radius - 0.6f, 0.0f, -1.2f, 1.2f, true);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.strokePath (rim, juce::PathStrokeType (1.0f));
        return image;
    }

    /// Knob bodies are shared between every knob of the same size.
    class KnobCache
    {
    public:
        const juce::Image& get (float radius, float scale)
        {
            const auto key = std::make_pair (juce::roundToInt (radius * 4.0f), juce::roundToInt (scale * 100.0f));
            auto it = images.find (key);
            if (it == images.end())
                it = images.emplace (key, renderKnobBody (radius, scale)).first;
            return it->second;
        }

    private:
        std::map<std::pair<int, int>, juce::Image> images;
    };

    /// A full machined knob: printed scale ticks, cached body, then the turning pointer lines.
    inline void knob (juce::Graphics& g, KnobCache& cache, juce::Point<float> centre, float ringR, float startAngle, float endAngle,
                      float pos, juce::Colour pointerColour, bool enabled, bool hover, int ticks = 11)
    {
        const auto r = ringR - 7.0f;
        const auto angleAt = [&] (float t) { return startAngle + t * (endAngle - startAngle); };
        const auto ink = colours::silk.withAlpha (enabled ? 0.85f : 0.35f);
        for (int i = 0; i < ticks; ++i)
        {
            const auto t = (float) i / (float) (ticks - 1);
            const bool major = i == 0 || i == ticks - 1 || i == ticks / 2;
            g.setColour (major ? ink : ink.withAlpha (ink.getFloatAlpha() * 0.55f));
            g.drawLine ({ polar (centre, ringR - (major ? 4.5f : 3.0f), angleAt (t)), polar (centre, ringR, angleAt (t)) }, major ? 1.3f : 1.0f);
        }

        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        const auto& body = cache.get (r, scale);
        const auto side = (float) body.getWidth() / scale;
        g.setOpacity (enabled ? 1.0f : 0.55f);
        g.drawImage (body, juce::Rectangle<float> (side, side).withCentre (centre));
        g.setOpacity (1.0f);

        const auto a = angleAt (pos);
        g.setColour (colours::silk.withAlpha (enabled ? 1.0f : 0.4f));
        g.drawLine ({ polar (centre, r * 0.84f, a), polar (centre, r * 0.98f, a) }, juce::jmax (1.4f, r * 0.06f));
        juce::Path pointer;
        pointer.startNewSubPath (polar (centre, r * 0.12f, a));
        pointer.lineTo (polar (centre, r * 0.6f, a));
        const auto width = juce::jmax (1.8f, r * 0.085f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (pointer, juce::PathStrokeType (width + 0.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (0.0f, 0.8f));
        g.setColour (enabled ? pointerColour.darker (0.1f) : colours::silkDim.darker (0.5f));
        g.strokePath (pointer, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (hover && enabled)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre));
        }
    }
}
