#include "PadComponent.h"
#include "PluginProcessor.h"

//==============================================================================
void PadTriggerButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (juce::Colours::black.withAlpha (held ? 0.65f : 0.35f));
    g.fillRoundedRectangle (b, 3.0f);

    // Play triangle.
    auto tri = b.reduced (b.getWidth() * 0.28f, b.getHeight() * 0.24f);
    juce::Path p;
    p.addTriangle (tri.getX(),        tri.getY(),
                   tri.getX(),        tri.getBottom(),
                   tri.getRight(),    tri.getCentreY());
    g.setColour (held ? juce::Colours::white : juce::Colours::white.withAlpha (0.85f));
    g.fillPath (p);
}

//==============================================================================
PadComponent::PadComponent (MosaicAudioProcessor& proc,
                            int padIndex,
                            juce::AudioThumbnailCache& thumbCache)
    : processor (proc),
      pad (padIndex),
      thumbnail (512, proc.getFormatManager(), thumbCache)
{
    padColour = processor.getPadColour (pad);

    // Audition (press-and-hold). Selecting happens too so you can tweak while it plays.
    triggerButton.onPress = [this]
    {
        if (onSelected) onSelected (pad, juce::ModifierKeys());   // plain = single-select
        processor.padPressed (pad, 0.85f);
    };
    triggerButton.onRelease = [this] { processor.padReleased (pad); };
    addAndMakeVisible (triggerButton);

    learnButton.setClickingTogglesState (false);
    learnButton.setTooltip ("MIDI Learn: map the next incoming note to this pad");
    learnButton.onClick = [this] { toggleMidiLearn(); };
    addAndMakeVisible (learnButton);

    colourButton.setTooltip ("Pick pad colour");
    colourButton.onClick = [this] { openColourSelector(); };
    addAndMakeVisible (colourButton);

    thumbnail.addChangeListener (this);
    refreshFromState();
}

PadComponent::~PadComponent()
{
    thumbnail.removeChangeListener (this);
}

//==============================================================================
void PadComponent::setSelected (bool shouldBeSelected, bool isPrimary)
{
    if (selected != shouldBeSelected || primary != isPrimary)
    {
        selected = shouldBeSelected;
        primary  = isPrimary;
        repaint();
    }
}

void PadComponent::setPlayback (bool active, float pos)
{
    // Repaint if the active state flipped, or if the cursor moved while active.
    if (playing != active || (active && std::abs (playPos - pos) > 0.0005f))
    {
        playing = active;
        playPos = pos;
        repaint();
    }
}

void PadComponent::refreshFromState()
{
    padColour       = processor.getPadColour (pad);
    learnedNote     = processor.getPadNote (pad);
    playModeCached  = processor.getPadPlayMode (pad);
    learning        = processor.isPadLearning (pad);

    const auto path = processor.getPadFilePath (pad);
    if (path != loadedPath)
    {
        loadedPath = path;
        if (path.isNotEmpty())
        {
            const juce::File f (path);
            if (f.existsAsFile())
                thumbnail.setSource (new juce::FileInputSource (f));
            else
                thumbnail.clear();
        }
        else
        {
            thumbnail.clear();
        }
    }

    learnButton.setColour (juce::TextButton::buttonColourId,
                           learning ? juce::Colours::red : juce::Colour (0xff2a2a2a));
    colourButton.setColour (juce::TextButton::buttonColourId, padColour);
    repaint();
}

//==============================================================================
juce::Rectangle<int> PadComponent::waveformArea() const
{
    // Leave a header strip for the buttons / note label.
    return getLocalBounds().reduced (4).withTrimmedTop (22);
}

float PadComponent::paramValue (const juce::String& id) const
{
    if (auto* p = processor.getAPVTS().getRawParameterValue (id))
        return p->load();
    return 0.0f;
}

void PadComponent::setParam (const juce::String& id, float value01)
{
    if (auto* p = processor.getAPVTS().getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value01));
        p->endChangeGesture();
    }
}

int PadComponent::handleHitTest (juce::Point<int> pos) const
{
    const auto wave = waveformArea();
    if (! wave.contains (pos))
        return 0;

    const float sx = wave.getX() + paramValue (ParamID::start (pad)) * wave.getWidth();
    const float ex = wave.getX() + paramValue (ParamID::end   (pad)) * wave.getWidth();

    if (std::abs ((float) pos.x - sx) <= 6.0f) return 1;
    if (std::abs ((float) pos.x - ex) <= 6.0f) return 2;
    return 0;
}

//==============================================================================
void PadComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // The pad illuminates in its assigned RGB while sounding (and stays lit for
    // the whole time a Looping pad is running), and sits dim/unlit otherwise.
    // The colour identity is preserved when unlit so pads remain distinguishable.
    juce::Colour bodyCol = playing ? padColour.brighter (0.35f)
                                   : padColour.withMultipliedBrightness (0.30f)
                                              .withMultipliedSaturation (0.85f);
    if (fileHover)
        bodyCol = bodyCol.brighter (0.2f);

    g.setColour (bodyCol);
    g.fillRoundedRectangle (bounds, 6.0f);

    // Glow ring while playing.
    if (playing)
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 3.0f);
        g.setColour (padColour.brighter (0.8f).withAlpha (0.5f));
        g.drawRoundedRectangle (bounds.reduced (3.5f), 5.0f, 2.0f);
    }

    // Waveform.
    const auto wave = waveformArea();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (wave);

    if (thumbnail.getTotalLength() > 0.0)
    {
        // Waveform height tracks the pad's Volume, so louder pads look bigger.
        const float waveZoom = juce::jlimit (0.0f, 4.0f, paramValue (ParamID::gain (pad)));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        thumbnail.drawChannels (g, wave, 0.0, thumbnail.getTotalLength(), waveZoom);

        // Trim region overlay + handles.
        const float sx = wave.getX() + paramValue (ParamID::start (pad)) * wave.getWidth();
        const float ex = wave.getX() + paramValue (ParamID::end   (pad)) * wave.getWidth();

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRect (juce::Rectangle<float> ((float) wave.getX(), (float) wave.getY(),
                                            sx - wave.getX(), (float) wave.getHeight()));
        g.fillRect (juce::Rectangle<float> (ex, (float) wave.getY(),
                                            wave.getRight() - ex, (float) wave.getHeight()));

        g.setColour (juce::Colours::yellow);
        g.drawLine (sx, (float) wave.getY(), sx, (float) wave.getBottom(), 2.0f);
        g.setColour (juce::Colours::orange);
        g.drawLine (ex, (float) wave.getY(), ex, (float) wave.getBottom(), 2.0f);

        // Attack / Release envelope overlaid on the trimmed region. Attack ramps
        // up from the start point; release ramps down to the end point.
        const double totalLen  = thumbnail.getTotalLength();
        const double startNorm = paramValue (ParamID::start (pad));
        const double endNorm   = paramValue (ParamID::end   (pad));
        const double trimSec   = juce::jmax (1.0e-4, (endNorm - startNorm) * totalLen);
        const float  attackFr  = (float) juce::jlimit (0.0, 1.0, (paramValue (ParamID::attack  (pad)) * 0.001) / trimSec);
        const float  releaseFr = (float) juce::jlimit (0.0, 1.0, (paramValue (ParamID::release (pad)) * 0.001) / trimSec);

        const float top = (float) wave.getY() + 2.0f;
        const float bot = (float) wave.getBottom() - 1.0f;
        float aX = sx + attackFr  * (ex - sx);
        float rX = ex - releaseFr * (ex - sx);
        if (aX > rX) { const float mid = 0.5f * (aX + rX); aX = rX = mid; }   // avoid crossover

        juce::Path env;
        env.startNewSubPath (sx, bot);
        env.lineTo (aX, top);
        env.lineTo (rX, top);
        env.lineTo (ex, bot);

        const juce::Colour envCol (0xff37d6c2);
        g.setColour (envCol.withAlpha (0.13f));
        g.fillPath (env);
        g.setColour (envCol.withAlpha (0.9f));
        g.strokePath (env, juce::PathStrokeType (1.5f));

        // Playback cursor.
        if (playing)
        {
            const float cx = wave.getX() + juce::jlimit (0.0f, 1.0f, playPos) * wave.getWidth();
            g.setColour (juce::Colours::white);
            g.drawLine (cx, (float) wave.getY(), cx, (float) wave.getBottom(), 1.5f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillEllipse (cx - 3.0f, (float) wave.getY() - 1.0f, 6.0f, 6.0f);
        }
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (12.0f);
        g.drawText ("drop .wav / .mp3 / .aif", wave, juce::Justification::centred);
    }

    // Header: pad number + mode + mapped note.
    g.setColour (juce::Colours::white);
    g.setFont (12.0f);
    const char* modeTag = playModeCached == Config::modeMomentary ? "GATE"
                        : playModeCached == Config::modeLooping    ? "LOOP"
                                                                   : "1SHOT";
    juce::String header = "Pad " + juce::String (pad + 1) + "  " + modeTag;
    if (learnedNote >= 0)
        header += "  [" + juce::MidiMessage::getMidiNoteName (learnedNote, true, true, 4) + "]";
    else if (learning)
        header += "  [learn...]";
    g.drawText (header, getLocalBounds().reduced (6, 2).removeFromTop (18)
                            .withTrimmedLeft (22)     // room for the ▶ trigger button
                            .withTrimmedRight (44),   // room for learn / colour buttons
                juce::Justification::centredLeft);

    // Selection / hover border. Primary pad = bright solid ring; other selected
    // pads = dimmer ring (part of a multi-pad bulk-edit selection).
    const juce::Colour accent (0xff37d6c2);
    if (primary)       g.setColour (accent);
    else if (selected) g.setColour (accent.withAlpha (0.55f));
    else               g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, selected ? 2.5f : 1.0f);
}

void PadComponent::resized()
{
    auto header = getLocalBounds().reduced (4, 2).removeFromTop (18);
    triggerButton.setBounds (header.removeFromLeft (18));
    colourButton.setBounds (header.removeFromRight (18));
    header.removeFromRight (4);
    learnButton.setBounds (header.removeFromRight (22));
}

//==============================================================================
void PadComponent::mouseDown (const juce::MouseEvent& e)
{
    // Clicking the pad only selects it for editing — it does NOT audition.
    // Use the ▶ trigger button (or the hardware controller) to play.
    // Ctrl/Cmd and Shift extend the selection for bulk editing.
    if (onSelected) onSelected (pad, e.mods);

    draggingHandle = handleHitTest (e.getPosition());
}

void PadComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingHandle == 0)
        return;

    const auto wave = waveformArea();
    const float norm = juce::jlimit (0.0f, 1.0f,
                                     (float) (e.position.x - wave.getX()) / (float) wave.getWidth());

    if (draggingHandle == 1)
        setParam (ParamID::start (pad), juce::jmin (norm, paramValue (ParamID::end (pad)) - 0.001f));
    else
        setParam (ParamID::end (pad),   juce::jmax (norm, paramValue (ParamID::start (pad)) + 0.001f));

    repaint();
}

void PadComponent::mouseUp (const juce::MouseEvent&)
{
    draggingHandle = 0;   // audition is handled by the trigger button, not the body
}

//==============================================================================
bool PadComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;mp3;aif;aiff"))
            return true;
    return false;
}

void PadComponent::fileDragEnter (const juce::StringArray&, int, int) { fileHover = true;  repaint(); }
void PadComponent::fileDragExit  (const juce::StringArray&)           { fileHover = false; repaint(); }

void PadComponent::filesDropped (const juce::StringArray& files, int, int)
{
    fileHover = false;

    for (const auto& f : files)
    {
        const juce::File file (f);
        if (file.hasFileExtension ("wav;mp3;aif;aiff"))
        {
            processor.setPadSample (pad, file);   // triggers a state refresh
            break;
        }
    }

    repaint();
}

//==============================================================================
namespace
{
    // A ColourSelector that live-applies its colour to the pad while dragging.
    class PadColourSelector : public juce::ColourSelector,
                              private juce::ChangeListener
    {
    public:
        PadColourSelector (MosaicAudioProcessor& p, int padIndex, juce::Colour initial)
            : juce::ColourSelector (showColourAtTop | showColourspace | showSliders),
              proc (p), pad (padIndex)
        {
            setCurrentColour (initial);
            setSize (240, 300);
            addChangeListener (this);
        }

        ~PadColourSelector() override { removeChangeListener (this); }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            proc.setPadColour (pad, getCurrentColour());
        }

        MosaicAudioProcessor& proc;
        int pad;
    };
}

void PadComponent::openColourSelector()
{
    auto selector = std::make_unique<PadColourSelector> (processor, pad, padColour);
    juce::CallOutBox::launchAsynchronously (std::move (selector),
                                            getScreenBounds(), nullptr);
}

void PadComponent::toggleMidiLearn()
{
    if (processor.isPadLearning (pad))
        processor.cancelMidiLearn();
    else
        processor.startMidiLearn (pad);

    refreshFromState();
}
