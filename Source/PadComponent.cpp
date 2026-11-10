#include "PadComponent.h"
#include "RackKit.h"
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

    // A backlit silicone pad: the colour shows faintly through the rubber when idle and
    // floods it while sounding (and the whole time a Looping pad is running).
    const auto body = bounds.reduced (1.5f);
    if (playing)
    {
        g.setColour (padColour.withAlpha (0.35f));
        g.fillRoundedRectangle (bounds, 7.0f);
    }
    auto rubber = juce::Colour (0xff1e1f22).interpolatedWith (padColour, playing ? 0.6f : 0.17f);
    if (fileHover)
        rubber = rubber.brighter (0.25f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (body.translated (0.0f, 1.5f), 6.0f);
    g.setGradientFill (juce::ColourGradient (rubber.brighter (0.15f), 0.0f, body.getY(),
                                             rubber.darker (0.3f), 0.0f, body.getBottom(), false));
    g.fillRoundedRectangle (body, 6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), body.getCentreX(), body.getY(),
                                             juce::Colours::white.withAlpha (0.0f), body.getCentreX(), body.getCentreY(), false));
    g.fillRoundedRectangle (body.reduced (2.0f), 5.0f);
    g.setColour (padColour.withAlpha (playing ? 0.95f : 0.4f));
    g.drawRoundedRectangle (body.reduced (1.0f), 5.5f, playing ? 1.8f : 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawLine (body.getX() + 6.0f, body.getY() + 0.6f, body.getRight() - 6.0f, body.getY() + 0.6f, 1.0f);

    // Waveform.
    const auto wave = waveformArea();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (wave.toFloat(), 3.0f);

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

        // The sample's name, on a dark strip along the bottom of the waveform.
        const auto nameStrip = wave.withTop (wave.getBottom() - 16).toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (nameStrip);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::File (loadedPath).getFileNameWithoutExtension(), nameStrip.reduced (5.0f, 0.0f),
                    juce::Justification::centredLeft, true);
    }
    else if (processor.isPadSampleMissing (pad))
    {
        // The session refers to a file that is gone: say so, and name it for relinking.
        const auto warning = juce::Colour (0xffff7a59);
        g.setColour (warning.withAlpha (0.12f));
        g.fillRect (wave);
        g.setColour (warning);
        g.drawRect (wave, 1);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("MISSING", wave.withTrimmedBottom (wave.getHeight() / 2), juce::Justification::centredBottom);
        g.setColour (warning.withAlpha (0.8f));
        g.setFont (11.0f);
        g.drawText (juce::File (loadedPath).getFileName(), wave.withTrimmedTop (wave.getHeight() / 2).reduced (4, 0),
                    juce::Justification::centredTop, true);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.setFont (12.0f);
        g.drawText ("double-click or drop a sample", wave, juce::Justification::centred);
    }

    // Header: pad number + mode + mapped note.
    g.setColour (rack::colours::silk);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.08f));
    const char* modeTag = playModeCached == Config::modeMomentary ? "GATE"
                        : playModeCached == Config::modeLooping    ? "LOOP"
                                                                   : "1SHOT";
    juce::String header = "PAD " + juce::String (pad + 1) + "  " + modeTag;
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

    if (e.mods.isPopupMenu())
    {
        showPadMenu();
        return;
    }
    draggingHandle = handleHitTest (e.getPosition());
}

void PadComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && handleHitTest (e.getPosition()) == 0)
        chooseSample();
}

void PadComponent::showPadMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Pad " + juce::String (pad + 1));
    m.addItem (1, "Load sample...");
    m.addItem (2, "Clear", processor.getPadFilePath (pad).isNotEmpty());

    juce::Component::SafePointer<PadComponent> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
    {
        if (safe == nullptr) return;
        if (result == 1)      safe->chooseSample();
        else if (result == 2) { safe->processor.clearPad (safe->pad); safe->refreshFromState(); }
    });
}

void PadComponent::chooseSample()
{
    const juce::File current (processor.getPadFilePath (pad));
    chooser = std::make_unique<juce::FileChooser> ("Sample for pad " + juce::String (pad + 1),
                                                   current.existsAsFile() ? current.getParentDirectory()
                                                                          : juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.mp3;*.aif;*.aiff");
    juce::Component::SafePointer<PadComponent> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe] (const juce::FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult().existsAsFile())
                safe->processor.requestPadSample (safe->pad, fc.getResult());   // decodes in the background
        });
}

void PadComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingHandle == 0)
        return;

    const auto wave = waveformArea();
    const float norm = juce::jlimit (0.0f, 1.0f,
                                     (float) (e.position.x - wave.getX()) / (float) wave.getWidth());

    if (draggingHandle == 1)
        setParam (ParamID::start (pad), juce::jmin (norm, paramValue (ParamID::end (pad)) - MosaicAudioProcessor::minTrimGap));
    else
        setParam (ParamID::end (pad),   juce::jmax (norm, paramValue (ParamID::start (pad)) + MosaicAudioProcessor::minTrimGap));

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
            processor.requestPadSample (pad, file);   // decodes in the background, then refreshes
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
