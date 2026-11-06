#include "PluginEditor.h"

namespace
{
    // A ColourSelector that reports live colour changes through a callback
    // (used for applying one colour to a whole multi-pad selection).
    class BulkColourSelector : public juce::ColourSelector,
                               private juce::ChangeListener
    {
    public:
        explicit BulkColourSelector (std::function<void (juce::Colour)> cb)
            : juce::ColourSelector (showColourspace | showColourAtTop), apply (std::move (cb))
        {
            setSize (240, 280);
            addChangeListener (this);
        }
        ~BulkColourSelector() override { removeChangeListener (this); }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override { if (apply) apply (getCurrentColour()); }
        std::function<void (juce::Colour)> apply;
    };
}

//==============================================================================
// MosaicLookAndFeel
//==============================================================================
MosaicLookAndFeel::MosaicLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Mosaic::bg);
    setColour (juce::Slider::textBoxTextColourId,        Mosaic::text);
    setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,                Mosaic::text);
    setColour (juce::ComboBox::textColourId,             Mosaic::text);
    setColour (juce::ComboBox::backgroundColourId,       Mosaic::panelHi);
    setColour (juce::ComboBox::outlineColourId,          juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId,            Mosaic::accent);
    setColour (juce::PopupMenu::backgroundColourId,      Mosaic::panel);
    setColour (juce::PopupMenu::textColourId,            Mosaic::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Mosaic::accent.withAlpha (0.30f));
    setColour (juce::TextButton::buttonColourId,         Mosaic::panelHi);
    setColour (juce::TextButton::textColourOnId,         Mosaic::text);
    setColour (juce::TextButton::textColourOffId,        Mosaic::text);
    setColour (juce::ToggleButton::textColourId,         Mosaic::textDim);
    setColour (juce::ToggleButton::tickColourId,         Mosaic::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, Mosaic::panelHi);
}

void MosaicLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    // Fenrikos knob: dished body, glowing value arc, lit indicator.
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto centre = bounds.getCentre();
    const float ringR = juce::jmax (6.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f);
    const float r     = ringR - 5.0f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const bool active = slider.isMouseOverOrDragging() && slider.isEnabled();
    const auto colour = slider.isEnabled() ? Mosaic::accent : Mosaic::textDim;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::track);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Controls whose range crosses zero (pan, pitch) light from the centre.
    const auto range = slider.getRange();
    const float origin = range.getStart() < 0.0 && range.getEnd() > 0.0
                             ? (float) slider.valueToProportionOfLength (0.0) : 0.0f;
    const float originAngle = startAngle + origin * (endAngle - startAngle);
    if (std::abs (angle - originAngle) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f,
                             juce::jmin (originAngle, angle), juce::jmax (originAngle, angle), true);
        Theme::glow (g, value, active ? colour.brighter (0.25f) : colour, 3.0f);
    }

    const auto body = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0.0f, 2.5f).expanded (1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff40444d), body.getCentreX(), body.getY(),
                                             juce::Colour (0xff1a1c21), body.getCentreX(), body.getBottom(), false));
    g.fillEllipse (body);
    const auto face = body.reduced (r * 0.14f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff23262c), face.getCentreX(), face.getY(),
                                             juce::Colour (0xff30343b), face.getCentreX(), face.getBottom(), false));
    g.fillEllipse (face);
    g.setColour (juce::Colours::white.withAlpha (active ? 0.12f : 0.06f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    const auto polar = [&] (float radius) { return centre.translated (radius * std::sin (angle), -radius * std::cos (angle)); };
    juce::Path indicator;
    indicator.startNewSubPath (polar (r * 0.3f));
    indicator.lineTo (polar (r * 0.8f));
    Theme::glow (g, indicator, colour.brighter (0.3f), 2.5f);
}

void MosaicLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float sliderPos, float, float,
                                          juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    const float cy = (float) y + (float) h * 0.5f;
    const float left = (float) x + 4.0f, right = (float) (x + w) - 4.0f;
    juce::Path track, value;
    track.startNewSubPath (left, cy);
    track.lineTo (right, cy);
    g.setColour (Theme::track);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    value.startNewSubPath (left, cy);
    value.lineTo (juce::jlimit (left, right, sliderPos), cy);
    Theme::glow (g, value, slider.isEnabled() ? Mosaic::accent : Mosaic::textDim, 3.0f);

    g.setColour (Mosaic::text);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ juce::jlimit (left, right, sliderPos), cy }));
}

void MosaicLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const float corner = juce::jmin (8.0f, b.getHeight() * 0.5f);
    const bool over = box.isMouseOver (true);
    g.setColour (Theme::glass.brighter (over ? 0.08f : 0.0f));
    g.fillRoundedRectangle (b, corner);
    g.setColour (over ? Mosaic::accent.withAlpha (0.5f) : Theme::cardEdge.withAlpha (0.12f));
    g.drawRoundedRectangle (b, corner, 1.0f);

    juce::Path caret;
    const float cx = b.getRight() - 12.0f, cy = b.getCentreY();
    caret.startNewSubPath (cx - 3.5f, cy - 1.5f);
    caret.lineTo (cx, cy + 2.0f);
    caret.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (Mosaic::textDim);
    g.strokePath (caret, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font MosaicLookAndFeel::getComboBoxFont (juce::ComboBox&)  { return juce::Font (juce::FontOptions (12.5f)); }
juce::Font MosaicLookAndFeel::getLabelFont     (juce::Label& l)  { return l.getFont(); }

void MosaicLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Label* MosaicLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    // Plain values under the knobs: no boxes.
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
    l->setJustificationType (juce::Justification::centred);
    l->setFont (juce::Font (juce::FontOptions (11.0f)));
    l->setColour (juce::Label::textColourId, Mosaic::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::TextEditor::backgroundColourId, Theme::glass);
    l->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::TextEditor::focusedOutlineColourId, Mosaic::accent);
    return l;
}

void MosaicLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                              bool isMouseOver, bool isButtonDown)
{
    const auto area = button.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = juce::jmin (8.0f, area.getHeight() * 0.5f);

    // Buttons given their own colour (pad colour swatches, the learn button while learning) keep it.
    const auto base = button.isColourSpecified (juce::TextButton::buttonColourId) ? backgroundColour : Theme::button;
    g.setColour (base.brighter (isButtonDown ? 0.02f : (isMouseOver ? 0.12f : 0.0f)));
    g.fillRoundedRectangle (area, corner);
    g.setColour (isMouseOver ? Mosaic::accent.withAlpha (0.45f) : Theme::cardEdge);
    g.drawRoundedRectangle (area, corner, 1.0f);
}

void MosaicLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool isMouseOver, bool)
{
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold).withKerningFactor (0.06f));
    g.setColour (button.findColour (juce::TextButton::textColourOffId).withAlpha (isMouseOver ? 1.0f : 0.85f));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

void MosaicLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isMouseOver, bool)
{
    // Switches are pills with their label inside, lit in the accent while on.
    const auto area = button.getLocalBounds().toFloat().reduced (0.5f, 2.0f);
    const float corner = area.getHeight() * 0.5f;
    const bool on = button.getToggleState();

    g.setColour (on ? Mosaic::accent.withAlpha (0.2f) : Theme::button.brighter (isMouseOver ? 0.1f : 0.0f));
    g.fillRoundedRectangle (area, corner);
    g.setColour (on ? Mosaic::accent.withAlpha (isMouseOver ? 1.0f : 0.8f) : Theme::cardEdge);
    g.drawRoundedRectangle (area, corner, 1.0f);

    const auto dot = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ area.getX() + 11.0f, area.getCentreY() });
    g.setColour (on ? Mosaic::accent.brighter (0.4f) : Mosaic::textDim.withAlpha (0.5f));
    g.fillEllipse (dot);

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (on ? Mosaic::accent.brighter (0.4f) : Mosaic::textDim);
    g.drawText (button.getButtonText(), area.withTrimmedLeft (20.0f).withTrimmedRight (6.0f),
                juce::Justification::centredLeft, true);
}

void MosaicLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (Theme::cardEdge);
    g.drawRect (0, 0, width, height);
}

//==============================================================================
// Editor
//==============================================================================
MosaicEditor::MosaicEditor (MosaicAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    auto& apvts = processor.getAPVTS();

    // Grid size controls ----------------------------------------------------
    auto setupIntSlider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 34, 22);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions (11.0f)));
        l.setColour (juce::Label::textColourId, Mosaic::textDim);
        l.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    };
    setupIntSlider (rowsSlider, rowsLabel, "ROWS");
    setupIntSlider (colsSlider, colsLabel, "COLS");

    followButton.setButtonText ("Follow last played");
    followButton.setTooltip ("Automatically select the pad that was last triggered");
    followButton.setColour (juce::ToggleButton::textColourId, Mosaic::textDim);
    addAndMakeVisible (followButton);

    rowsAttachment = std::make_unique<SliderAttachment> (apvts, ParamID::gridRows, rowsSlider);
    colsAttachment = std::make_unique<SliderAttachment> (apvts, ParamID::gridCols, colsSlider);

    apvts.addParameterListener (ParamID::gridRows, this);
    apvts.addParameterListener (ParamID::gridCols, this);

    // Pads ------------------------------------------------------------------
    for (int i = 0; i < Config::maxPads; ++i)
    {
        auto* pc = new PadComponent (processor, i, thumbnailCache);
        pc->onSelected = [this] (int idx, const juce::ModifierKeys& mods) { handlePadClicked (idx, mods); };
        pads.add (pc);
        addChildComponent (pc);
    }

    // Inspector -------------------------------------------------------------
    inspectorTitle.setFont (juce::Font (juce::FontOptions (14.0f).withStyle ("Bold")));
    inspectorTitle.setColour (juce::Label::textColourId, Mosaic::accent);
    addAndMakeVisible (inspectorTitle);

    addKnob (volumeKnob,   "Volume",  ParamID::roleGain);
    addKnob (panKnob,      "Pan",     ParamID::rolePan);
    addKnob (pitchKnob,    "Pitch",   ParamID::rolePitch);
    addKnob (attackKnob,   "Attack",  ParamID::roleAttack);
    addKnob (releaseKnob,  "Release", ParamID::roleRelease);
    addKnob (speedKnob,    "Speed",   ParamID::roleSpeed);
    addKnob (startKnob,    "Start",   ParamID::roleStart);
    addKnob (endKnob,      "End",     ParamID::roleEnd);

    allKnobs = { &volumeKnob, &panKnob, &pitchKnob, &attackKnob,
                 &releaseKnob, &speedKnob, &startKnob, &endKnob };

    for (auto* k : allKnobs)
        k->slider.onPopupMenu = [this, k] { showKnobLearnMenu (*k); };

    // Grain knob (Granular pitch mode only) — not a CC role.
    addKnob (grainKnob, "Grain", -1);
    grainKnob.slider.onValueChange = [this]
    {
        if (suppressMirror || selectedPads.size() <= 1) return;
        auto& apvts = processor.getAPVTS();
        const double v = grainKnob.slider.getValue();
        for (int pad : selectedPads)
            if (pad != selectedPad)
                if (auto* p = apvts.getParameter (ParamID::grainSize (pad)))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
    };

    auto setupCombo = [this] (juce::ComboBox& c, juce::Label& l, const juce::String& name)
    {
        addAndMakeVisible (c);
        l.setText (name, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions (11.0f)));
        l.setColour (juce::Label::textColourId, Mosaic::textDim);
        l.setJustificationType (juce::Justification::centredLeft);
        l.attachToComponent (&c, false);
        addAndMakeVisible (l);
    };

    modeBox.addItemList (Config::playModeChoices(), 1);
    pitchModeBox.addItemList (Config::pitchModeChoices(), 1);
    for (int b = 0; b < Config::numOutputBuses; ++b)
        outBusBox.addItem ("Out " + juce::String (b * 2 + 1) + "-" + juce::String (b * 2 + 2), b + 1);

    setupCombo (modeBox,      modeLabel,      "PLAY MODE");
    setupCombo (pitchModeBox, pitchModeLabel, "PITCH MODE");
    setupCombo (outBusBox,    outBusLabel,    "OUTPUT");

    pitchModeBox.onChange = [this]
    {
        if (! suppressMirror && selectedPads.size() > 1)
            if (auto* mp = processor.getAPVTS().getParameter (ParamID::pitchMode (selectedPad)))
            {
                const float v = mp->getValue();
                for (int pad : selectedPads)
                    if (pad != selectedPad)
                        if (auto* p = processor.getAPVTS().getParameter (ParamID::pitchMode (pad)))
                            p->setValueNotifyingHost (v);
            }
        grainKnob.slider.setEnabled (pitchModeBox.getSelectedItemIndex() == Config::pitchGranular);
    };

    outBusBox.onChange = [this]
    {
        const int bus = outBusBox.getSelectedId() - 1;
        for (int pad : selectedPads)
            processor.setPadOutputBus (pad, bus);
    };

    modeBox.onChange = [this]
    {
        // Mirror the play mode across the selection, then refresh those tiles.
        if (! suppressMirror && selectedPads.size() > 1)
            if (auto* mp = processor.getAPVTS().getParameter (ParamID::playMode (selectedPad)))
            {
                const float v = mp->getValue();
                for (int pad : selectedPads)
                    if (pad != selectedPad)
                        if (auto* p = processor.getAPVTS().getParameter (ParamID::playMode (pad)))
                            p->setValueNotifyingHost (v);
            }

        for (int pad : selectedPads)
            if (auto* pc = pads[pad])
                pc->refreshFromState();
    };

    // Selection controls.
    selectAllButton.setTooltip ("Select all visible pads");
    selectAllButton.onClick = [this]
    {
        selectedPads.clear();
        const int n = juce::jmin (visibleRows * visibleCols, Config::maxPads);
        for (int i = 0; i < n; ++i) selectedPads.insert (i);
        if (! selectedPads.count (selectedPad)) selectedPad = 0;
        applySelection();
    };
    addAndMakeVisible (selectAllButton);

    bulkMenuButton.setTooltip ("Bulk actions on the selected pads");
    bulkMenuButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addSectionHeader (juce::String ((int) selectedPads.size()) + " pad(s) selected");
        m.addItem (1, "Set colour...");
        m.addItem (2, "Clear sample(s)");
        if (const int missing = processor.getNumMissingSamples(); missing > 0)
        {
            m.addSeparator();
            m.addItem (3, "Relink missing samples (" + juce::String (missing) + ")...");
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bulkMenuButton),
                         [this] (int r)
        {
            if (r == 3)
            {
                relinkMissingSamples();
                return;
            }
            if (r == 1)
            {
                auto sel = std::make_unique<BulkColourSelector> ([this] (juce::Colour c) { setColourForSelection (c); });
                sel->setCurrentColour (processor.getPadColour (selectedPad));
                juce::CallOutBox::launchAsynchronously (std::move (sel), bulkMenuButton.getScreenBounds(), nullptr);
            }
            else if (r == 2)
            {
                clearSelectionPads();
            }
        });
    };
    addAndMakeVisible (bulkMenuButton);

    // Presets -------------------------------------------------------------
    presetName.setText ("(unsaved)", juce::dontSendNotification);
    presetName.setJustificationType (juce::Justification::centredRight);
    presetName.setFont (juce::Font (juce::FontOptions (11.0f)));
    presetName.setColour (juce::Label::textColourId, Mosaic::textDim);
    addAndMakeVisible (presetName);

    saveButton.setTooltip ("Save a .mosaic preset");
    saveButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Save MOSAIC preset",
                        MosaicAudioProcessor::getPresetsDirectory(), "*.mosaic");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f == juce::File()) return;
                if (! f.hasFileExtension ("mosaic")) f = f.withFileExtension ("mosaic");
                if (processor.savePresetToFile (f))
                    presetName.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);
            });
    };
    addAndMakeVisible (saveButton);

    loadButton.setTooltip ("Load a .mosaic preset");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load MOSAIC preset",
                        MosaicAudioProcessor::getPresetsDirectory(), "*.mosaic");
        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile() && processor.loadPresetFromFile (f))
                {
                    presetName.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);
                    rebuildGridLayout();
                    selectPad (processor.getActivePad());
                    for (auto* pc : pads) pc->refreshFromState();
                }
            });
    };
    addAndMakeVisible (loadButton);

    // LFO panel ------------------------------------------------------------
    lfoTitle.setText ("MODULATION", juce::dontSendNotification);
    lfoTitle.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    lfoTitle.setColour (juce::Label::textColourId, Mosaic::accent);
    addAndMakeVisible (lfoTitle);
    for (int i = 0; i < Config::numLfos; ++i)
        setupLfoStrip (i);

    addAndMakeVisible (updateBadge);

    processor.addChangeListener (this);

    // The children were built before this editor's look and feel applied to them, so their
    // value boxes still carry the default style; rebuild them with ours.
    sendLookAndFeelChange();

    setResizable (true, true);
    setResizeLimits (860, 600, 1800, 1300);
    setSize (980, 800);
    rebuildGridLayout();
    selectPad (processor.getActivePad());   // restore selection (survives reopen/minimize)

    lastSeenPlayCounter = processor.getPlayCounter();   // don't jump on open
    startTimerHz (30);
}

MosaicEditor::~MosaicEditor()
{
    processor.getAPVTS().removeParameterListener (ParamID::gridRows, this);
    processor.getAPVTS().removeParameterListener (ParamID::gridCols, this);
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void MosaicEditor::addKnob (LabelledKnob& k, const juce::String& name, int role)
{
    k.baseName = name;
    k.role     = role;
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 15);
    k.slider.setTooltip ("Right-click to MIDI-learn a CC");
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (11.5f)));
    k.label.setColour (juce::Label::textColourId, Mosaic::textDim);
    addAndMakeVisible (k.label);

    // Bulk edit: turning a knob writes the value to every other selected pad.
    k.slider.onValueChange = [this, &k] { mirrorToSelection (k.role, k.slider.getValue()); };
}

void MosaicEditor::showKnobLearnMenu (LabelledKnob& k)
{
    const int role = k.role;
    if (role < 0)
        return;

    const int  cc       = processor.getCCForKnobRole (role);
    const bool learning = processor.isKnobLearning (role);

    juce::PopupMenu m;
    if (learning)
    {
        m.addItem (1, "Waiting for MIDI CC... (cancel)");
    }
    else
    {
        m.addItem (2, "MIDI Learn (all pads)");
        if (cc >= 0)
        {
            m.addSeparator();
            m.addSectionHeader ("Mapped to CC " + juce::String (cc));
            m.addItem (3, "Clear mapping");
        }
    }

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&k.slider),
                     [this, role] (int r)
    {
        if (r == 1)      processor.cancelKnobLearn();
        else if (r == 2) processor.startKnobLearn (role);
        else if (r == 3) processor.clearKnobCC (role);
        updateKnobDecorations();
    });
}

void MosaicEditor::updateKnobDecorations()
{
    for (auto* k : allKnobs)
    {
        if (k == nullptr || k->role < 0)
            continue;

        const int  cc       = processor.getCCForKnobRole (k->role);
        const bool learning = processor.isKnobLearning (k->role);

        juce::String txt = k->baseName;
        if (learning)     txt += "  LEARN";
        else if (cc >= 0) txt += "  CC" + juce::String (cc);

        if (k->label.getText() != txt)
            k->label.setText (txt, juce::dontSendNotification);

        k->label.setColour (juce::Label::textColourId,
                            learning ? Mosaic::accent2
                                     : (cc >= 0 ? Mosaic::accent : Mosaic::textDim));
    }
}

//==============================================================================
void MosaicEditor::setupLfoStrip (int i)
{
    auto& apvts = processor.getAPVTS();
    auto& s = lfoStrips[i];

    s.name.setText ("LFO " + juce::String (i + 1), juce::dontSendNotification);
    s.name.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    s.name.setColour (juce::Label::textColourId, Mosaic::accent);
    addAndMakeVisible (s.name);

    s.enable.setButtonText ("On");
    addAndMakeVisible (s.enable);
    s.sync.setButtonText ("Sync");
    addAndMakeVisible (s.sync);

    s.shape.addItemList (Config::lfoShapeChoices(), 1);
    s.syncRate.addItemList (Config::lfoSyncChoices(), 1);
    s.target.addItemList (Config::lfoTargetChoices(), 1);
    s.scope.addItemList (Config::lfoScopeChoices(), 1);
    addAndMakeVisible (s.shape);
    addAndMakeVisible (s.syncRate);
    addAndMakeVisible (s.target);
    addAndMakeVisible (s.scope);

    // Linear sliders fit the compact horizontal strip far better than rotaries.
    for (auto* sl : { &s.rate, &s.depth })
    {
        sl->setSliderStyle (juce::Slider::LinearHorizontal);
        sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 18);
        addAndMakeVisible (*sl);
    }
    s.rate.setTextValueSuffix (" Hz");

    s.enableAtt   = std::make_unique<ButtonAttachment> (apvts, ParamID::lfoEnable (i),   s.enable);
    s.syncAtt     = std::make_unique<ButtonAttachment> (apvts, ParamID::lfoSync (i),     s.sync);
    s.shapeAtt    = std::make_unique<ComboAttachment>  (apvts, ParamID::lfoShape (i),    s.shape);
    s.syncRateAtt = std::make_unique<ComboAttachment>  (apvts, ParamID::lfoSyncRate (i), s.syncRate);
    s.targetAtt   = std::make_unique<ComboAttachment>  (apvts, ParamID::lfoTarget (i),   s.target);
    s.scopeAtt    = std::make_unique<ComboAttachment>  (apvts, ParamID::lfoScope (i),    s.scope);
    s.rateAtt     = std::make_unique<SliderAttachment> (apvts, ParamID::lfoRate (i),     s.rate);
    s.depthAtt    = std::make_unique<SliderAttachment> (apvts, ParamID::lfoDepth (i),    s.depth);
}

void MosaicEditor::layoutLfoStrip (int i, juce::Rectangle<int> row)
{
    auto& s = lfoStrips[i];
    s.name.setBounds    (row.removeFromLeft (42));
    s.enable.setBounds  (row.removeFromLeft (50));  row.removeFromLeft (6);
    s.shape.setBounds   (row.removeFromLeft (80));  row.removeFromLeft (8);
    s.rate.setBounds    (row.removeFromLeft (128)); row.removeFromLeft (8);
    s.sync.setBounds    (row.removeFromLeft (62));  row.removeFromLeft (4);
    s.syncRate.setBounds(row.removeFromLeft (56));  row.removeFromLeft (10);
    s.depth.setBounds   (row.removeFromLeft (116)); row.removeFromLeft (10);
    s.target.setBounds  (row.removeFromLeft (82));  row.removeFromLeft (8);
    s.scope.setBounds   (row.removeFromLeft (86));
}

//==============================================================================
void MosaicEditor::selectPad (int padIndex)
{
    selectedPad = juce::jlimit (0, Config::maxPads - 1, padIndex);
    selectedPads = { selectedPad };
    applySelection();
}

void MosaicEditor::handlePadClicked (int pad, const juce::ModifierKeys& mods)
{
    pad = juce::jlimit (0, Config::maxPads - 1, pad);

    if (mods.isShiftDown())
    {
        // Range from the current primary to the clicked pad.
        const int lo = juce::jmin (selectedPad, pad);
        const int hi = juce::jmax (selectedPad, pad);
        for (int i = lo; i <= hi; ++i) selectedPads.insert (i);
        selectedPad = pad;
    }
    else if (mods.isCommandDown() || mods.isCtrlDown())
    {
        // Toggle this pad in/out of the selection.
        if (selectedPads.count (pad) && selectedPads.size() > 1)
            selectedPads.erase (pad);
        else
            selectedPads.insert (pad);
        selectedPad = selectedPads.count (pad) ? pad : *selectedPads.begin();
    }
    else
    {
        selectedPad  = pad;
        selectedPads = { pad };
    }

    applySelection();
}

void MosaicEditor::applySelection()
{
    processor.setActivePad (selectedPad);   // CC-mapped knobs / LFO scope follow the primary

    for (int i = 0; i < pads.size(); ++i)
        pads[i]->setSelected (selectedPads.count (i) > 0, i == selectedPad);

    auto& apvts = processor.getAPVTS();

    // Destroy old attachments before making the new ones (overlapping attachments
    // would cross-write the previously selected pad's parameters). suppressMirror
    // stops the attachments' initial-update from bulk-writing the whole selection.
    suppressMirror = true;

    for (auto* k : allKnobs)
        k->attachment.reset();
    grainKnob.attachment.reset();
    modeAttachment.reset();
    pitchModeAttachment.reset();

    for (auto* k : allKnobs)
        k->attachment = std::make_unique<SliderAttachment> (apvts, ParamID::forRole (k->role, selectedPad), k->slider);

    grainKnob.attachment = std::make_unique<SliderAttachment> (apvts, ParamID::grainSize (selectedPad), grainKnob.slider);
    modeAttachment       = std::make_unique<ComboAttachment> (apvts, ParamID::playMode (selectedPad), modeBox);
    pitchModeAttachment  = std::make_unique<ComboAttachment> (apvts, ParamID::pitchMode (selectedPad), pitchModeBox);
    outBusBox.setSelectedId (processor.getPadOutputBus (selectedPad) + 1, juce::dontSendNotification);

    grainKnob.slider.setEnabled (pitchModeBox.getSelectedItemIndex() == Config::pitchGranular);

    suppressMirror = false;

    inspectorTitle.setText (selectedPads.size() > 1
                                ? "PADS × " + juce::String ((int) selectedPads.size())
                                : "PAD " + juce::String (selectedPad + 1),
                            juce::dontSendNotification);

    updateKnobDecorations();
}

void MosaicEditor::mirrorToSelection (int role, double value)
{
    if (suppressMirror || selectedPads.size() <= 1)
        return;

    auto& apvts = processor.getAPVTS();
    for (int pad : selectedPads)
    {
        if (pad == selectedPad) continue;   // primary handled by its attachment
        if (auto* p = apvts.getParameter (ParamID::forRole (role, pad)))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) value));
    }
}

void MosaicEditor::setColourForSelection (juce::Colour c)
{
    for (int pad : selectedPads)
        processor.setPadColour (pad, c);
}

void MosaicEditor::relinkMissingSamples()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a folder that contains the missing samples",
                    juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto folder = fc.getResult();
            if (! folder.isDirectory())
                return;

            const int before = processor.getNumMissingSamples();
            const int found  = processor.relinkMissingSamples (folder);
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Relink samples",
                "Found " + juce::String (found) + " of " + juce::String (before) + " missing samples in "
                    + folder.getFileName() + ".", "OK", this);
        });
}

void MosaicEditor::clearSelectionPads()
{
    for (int pad : selectedPads)
        processor.clearPad (pad);
}

//==============================================================================
void MosaicEditor::parameterChanged (const juce::String& id, float)
{
    if (id == ParamID::gridRows || id == ParamID::gridCols)
    {
        gridDirty.store (true);
        triggerAsyncUpdate();
    }
}

void MosaicEditor::handleAsyncUpdate()
{
    if (gridDirty.exchange (false))
    {
        rebuildGridLayout();
        resized();
    }
}

void MosaicEditor::timerCallback()
{
    // Follow last-played: on a new trigger, select that pad (if it is visible).
    if (followButton.getToggleState())
    {
        const auto counter = processor.getPlayCounter();
        if (counter != lastSeenPlayCounter)
        {
            lastSeenPlayCounter = counter;
            const int lp = processor.getLastPlayedPad();
            const int visibleCount = juce::jmin (visibleRows * visibleCols, Config::maxPads);
            if (lp >= 0 && lp < visibleCount && lp != selectedPad)
                selectPad (lp);
        }
    }

    for (int i = 0; i < pads.size(); ++i)
        pads[i]->setPlayback (processor.isPadActive (i), processor.getPadPlayPosition (i));

    // Keep selected pads' envelope / waveform overlays in sync with live edits.
    for (int pad : selectedPads)
        if (juce::isPositiveAndBelow (pad, pads.size()))
            pads[pad]->repaint();

    updateKnobDecorations();
}

void MosaicEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    for (auto* pc : pads)
        pc->refreshFromState();

    outBusBox.setSelectedId (processor.getPadOutputBus (selectedPad) + 1, juce::dontSendNotification);
    updateKnobDecorations();
}

//==============================================================================
void MosaicEditor::rebuildGridLayout()
{
    auto& apvts = processor.getAPVTS();
    visibleRows = juce::jlimit (1, Config::maxRows, (int) apvts.getRawParameterValue (ParamID::gridRows)->load());
    visibleCols = juce::jlimit (1, Config::maxCols, (int) apvts.getRawParameterValue (ParamID::gridCols)->load());

    const int visibleCount = juce::jmin (visibleRows * visibleCols, Config::maxPads);

    for (int i = 0; i < pads.size(); ++i)
        pads[i]->setVisible (i < visibleCount);

    // Drop now-hidden pads from the selection.
    for (auto it = selectedPads.begin(); it != selectedPads.end(); )
        it = (*it >= visibleCount) ? selectedPads.erase (it) : std::next (it);

    if (selectedPad >= visibleCount || selectedPads.empty())
        selectPad (0);
    else
        applySelection();
}

//==============================================================================
void MosaicEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);
}

// Everything static: plate, header, cards and footer. Rendered once per resize.
void MosaicEditor::paintBackground (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (Mosaic::bg, 0.0f, 0.0f, Mosaic::bgDeep, 0.0f, bounds.getBottom(), false));
    g.fillAll();

    // Soft accent glow from the top edge.
    g.setGradientFill (juce::ColourGradient (Mosaic::accent.withAlpha (0.14f), bounds.getCentreX(), 0.0f,
                                             Mosaic::accent.withAlpha (0.0f), bounds.getCentreX(), 90.0f, false));
    g.fillRect (bounds.withHeight (90.0f));
    g.setColour (Mosaic::accent);
    g.fillRect (bounds.withHeight (2.0f));

    // Header: Mannaz mark, wordmark and tagline.
    Theme::fillRune (g, Theme::mannaz ({ 16.0f, 10.0f, 32.0f, 32.0f }));
    g.setFont (juce::FontOptions (23.0f, juce::Font::bold).withKerningFactor (0.2f));
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawText ("MOSAIC", juce::Rectangle<float> (56.0f, 9.0f, 200.0f, 26.0f), juce::Justification::centredLeft);
    g.setColour (Mosaic::text);
    g.drawText ("MOSAIC", juce::Rectangle<float> (56.0f, 8.0f, 200.0f, 26.0f), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (11.5f));
    g.setColour (Mosaic::textDim);
    g.drawText ("Modular pad sampler", juce::Rectangle<float> (57.0f, 33.0f, 240.0f, 14.0f), juce::Justification::centredLeft);

    g.setColour (Theme::cardEdge);
    g.fillRect (16.0f, 85.0f, bounds.getWidth() - 32.0f, 1.0f);

    for (auto panel : { gridPanel, inspectorPanel, lfoPanel })
        if (! panel.isEmpty())
            Theme::card (g, panel.toFloat(), 12.0f);

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.25f));
    const auto footer = bounds.withTop (bounds.getBottom() - 22.0f);
    g.setColour (Mosaic::textDim.withAlpha (0.8f));
    g.drawText ("FENRIKOS", footer.withTrimmedRight (20.0f), juce::Justification::centredRight);
}

void MosaicEditor::resized()
{
    auto area = getLocalBounds();

    // Header — two rows: branding + presets, then selection + grid size.
    auto header = area.removeFromTop (86);
    {
        auto row1 = header.removeFromTop (44).reduced (18, 0);
        row1.removeFromLeft (240);   // painted mark and wordmark

        auto presetBar = row1.removeFromRight (280).withSizeKeepingCentre (280, 24);
        saveButton.setBounds (presetBar.removeFromLeft (52));
        presetBar.removeFromLeft (6);
        loadButton.setBounds (presetBar.removeFromLeft (52));
        presetBar.removeFromLeft (8);
        presetName.setBounds (presetBar);

        auto row2 = header.removeFromTop (34).reduced (18, 0);
        auto ctl = row2.removeFromRight (240).withSizeKeepingCentre (240, 24);
        colsSlider.setBounds (ctl.removeFromRight (72));
        colsLabel.setBounds  (ctl.removeFromRight (36));
        ctl.removeFromRight (10);
        rowsSlider.setBounds (ctl.removeFromRight (72));
        rowsLabel.setBounds  (ctl.removeFromRight (36));

        auto left = row2.removeFromLeft (320).withSizeKeepingCentre (320, 24);
        selectAllButton.setBounds (left.removeFromLeft (44));
        left.removeFromLeft (6);
        bulkMenuButton.setBounds (left.removeFromLeft (62));
        left.removeFromLeft (12);
        followButton.setBounds (left.removeFromLeft (150));
    }

    updateBadge.setBounds (20, getHeight() - 22, getWidth() / 2, 22);
    area.removeFromBottom (14);   // footer

    // LFO panel (very bottom) + inspector panel + grid panel (middle).
    auto lfoOuter       = area.removeFromBottom (128);
    auto inspectorOuter = area.removeFromBottom (196);
    lfoPanel       = lfoOuter.reduced (12, 8);
    inspectorPanel = inspectorOuter.reduced (12, 8);
    gridPanel      = area.reduced (12, 8);

    {
        auto lp = lfoPanel.reduced (14, 8);
        lfoTitle.setBounds (lp.removeFromTop (18));
        lp.removeFromTop (2);
        const int rowH = juce::jmax (22, lp.getHeight() / Config::numLfos);
        for (int i = 0; i < Config::numLfos; ++i)
            layoutLfoStrip (i, lp.removeFromTop (rowH).reduced (0, 2));
    }

    // Inspector contents.
    {
        auto ins = inspectorPanel.reduced (16, 12);
        inspectorTitle.setBounds (ins.removeFromTop (20));
        ins.removeFromTop (4);

        auto knobRow = ins.removeFromTop (88);
        const int knobW = juce::jmax (52, knobRow.getWidth() / 9);
        auto placeKnob = [&knobRow, knobW] (LabelledKnob& k)
        {
            auto cell = knobRow.removeFromLeft (knobW).reduced (2, 0);
            k.label.setBounds (cell.removeFromTop (15));
            k.slider.setBounds (cell);
        };
        placeKnob (volumeKnob);   placeKnob (panKnob);      placeKnob (pitchKnob);
        placeKnob (attackKnob);   placeKnob (releaseKnob);  placeKnob (speedKnob);
        placeKnob (startKnob);    placeKnob (endKnob);      placeKnob (grainKnob);

        ins.removeFromTop (18);   // room for the combo labels (attached above)
        auto comboRow = ins.removeFromTop (26);
        const int comboW = juce::jmax (90, comboRow.getWidth() / 3 - 11);
        modeBox.setBounds      (comboRow.removeFromLeft (comboW));
        comboRow.removeFromLeft (16);
        pitchModeBox.setBounds (comboRow.removeFromLeft (comboW));
        comboRow.removeFromLeft (16);
        outBusBox.setBounds    (comboRow.removeFromLeft (comboW));
    }

    background = juce::Image (juce::Image::RGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), false);
    {
        juce::Graphics bg (background);
        paintBackground (bg);
    }

    // Pad grid.
    auto padArea = gridPanel.reduced (12);
    if (padArea.isEmpty())
        return;

    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    using Fr    = juce::Grid::Fr;

    for (int r = 0; r < visibleRows; ++r) grid.templateRows.add    (Track (Fr (1)));
    for (int c = 0; c < visibleCols; ++c) grid.templateColumns.add (Track (Fr (1)));
    grid.setGap (juce::Grid::Px (7));

    const int visibleCount = juce::jmin (visibleRows * visibleCols, Config::maxPads);
    for (int i = 0; i < visibleCount; ++i)
        grid.items.add (juce::GridItem (*pads[i]));

    grid.performLayout (padArea);
}
