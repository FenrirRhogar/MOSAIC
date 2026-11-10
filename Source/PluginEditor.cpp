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

// Small plain knobs (the LFO rate and depth): black body, cream pointer, no scale.
void MosaicLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto c = bounds.getCentre();
    const auto body = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (body.translated (0.0f, 2.5f).expanded (1.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d3e3b), c.x - r * 0.3f, c.y - r * 0.5f,
                                             juce::Colour (0xff111210), c.x + r * 0.4f, c.y + r, true));
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    juce::Path rim;
    rim.addCentredArc (c.x, c.y, r - 0.5f, r - 0.5f, 0.0f, -1.2f, 1.2f, true);
    g.strokePath (rim, juce::PathStrokeType (0.9f));
    const float a = startAngle + sliderPos * (endAngle - startAngle);
    g.setColour (slider.isEnabled() ? juce::Colour (0xfff1ead8) : Mosaic::textDim);
    g.drawLine ({ rackui::polar (c, r * 0.25f, a), rackui::polar (c, r * 0.82f, a) }, 2.0f);
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

    // Short fader: a recessed slot with a printed scale, and a machined cap with a lit index line.
    const float cy = (float) y + (float) h * 0.5f;
    const float left = (float) x + 6.0f, right = (float) (x + w) - 6.0f;
    const float pos = juce::jlimit (left, right, sliderPos);
    g.setColour (rack::colours::silk.withAlpha (0.3f));
    for (int i = 0; i <= 8; ++i)
    {
        const float tx = left + (right - left) * (float) i / 8.0f;
        g.drawLine (tx, cy - 7.0f, tx, cy - (i % 4 == 0 ? 10.0f : 8.5f), 1.0f);
    }
    rack::recess (g, { left - 2.0f, cy - 2.0f, right - left + 4.0f, 4.0f }, 2.0f, juce::Colour (0xff050506));

    const auto cap = juce::Rectangle<float> (9.0f, juce::jmin (16.0f, (float) h - 2.0f)).withCentre ({ pos, cy });
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (cap.translated (1.0f, 2.0f), 2.0f);
    g.setGradientFill (juce::ColourGradient (rack::colours::metalLight, cap.getX(), cap.getY(),
                                             rack::colours::metalDark.darker (0.3f), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (slider.isEnabled() ? Mosaic::accent : Mosaic::textDim);
    g.fillRect (cap.withSizeKeepingCentre (1.5f, cap.getHeight() - 4.0f));
}

// Drop-downs are small OLED windows with a caret.
void MosaicLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f, 1.0f);
    rackui::oled (g, b);
    juce::Path caret;
    const float cx = b.getRight() - 10.0f, cy = b.getCentreY();
    caret.startNewSubPath (cx - 3.0f, cy - 1.5f);
    caret.lineTo (cx, cy + 1.5f);
    caret.lineTo (cx + 3.0f, cy - 1.5f);
    g.setColour (Mosaic::accent.withAlpha (box.isMouseOver (true) ? 0.9f : 0.45f));
    g.strokePath (caret, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font MosaicLookAndFeel::getComboBoxFont (juce::ComboBox&)  { return rackui::font (rackui::Face::mono, 10.5f, 0.04f); }
juce::Font MosaicLookAndFeel::getLabelFont     (juce::Label& l)  { return l.getFont(); }

void MosaicLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, Mosaic::accent.interpolatedWith (juce::Colours::white, 0.45f));
}

juce::Label* MosaicLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    // Plain values under the knobs: no boxes.
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
    l->setJustificationType (juce::Justification::centred);
    l->setFont (rackui::font (rackui::Face::mono, 9.5f));
    l->setColour (juce::Label::textColourId, rackui::colour::inkSoft);
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
    // Buttons given their own colour (pad colour swatches, the learn button while learning) are
    // small lit lenses in that colour; everything else is a machined cap.
    if (button.isColourSpecified (juce::TextButton::buttonColourId))
    {
        const auto area = button.getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (area.expanded (1.0f), 3.0f);
        g.setGradientFill (juce::ColourGradient (backgroundColour.brighter (isMouseOver ? 0.4f : 0.25f), area.getCentreX(), area.getY(),
                                                 backgroundColour.darker (0.3f), area.getCentreX(), area.getBottom(), false));
        g.fillRoundedRectangle (area, 2.5f);
        return;
    }
    rackui::cap (g, button.getLocalBounds().toFloat().reduced (0.5f, 1.0f).withTrimmedBottom (1.5f), isMouseOver, isButtonDown);
}

void MosaicLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool isButtonDown)
{
    auto area = button.getLocalBounds().toFloat().withTrimmedBottom (1.5f);
    if (isButtonDown)
        area.translate (0.0f, 0.5f);
    const bool lens = button.isColourSpecified (juce::TextButton::buttonColourId);
    rackui::print (g, lens ? button.getButtonText() : button.getButtonText().toUpperCase(), area, rackui::Face::legend, 11.0f, 0.14f,
                   lens ? juce::Colour (0xff15130f) : rackui::colour::ink);
}

// Switches are an LED and a legend.
void MosaicLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isMouseOver, bool)
{
    const bool on = button.getToggleState();
    const auto r = button.getLocalBounds().toFloat();
    rackui::led (g, { 4.5f, r.getCentreY() }, 3.5f, Mosaic::accent, on);
    rackui::print (g, button.getButtonText().toUpperCase(), r.withTrimmedLeft (13.0f), rackui::Face::legend, 10.5f, 0.14f,
                   on || isMouseOver ? rackui::colour::ink : rackui::colour::inkSoft, juce::Justification::centredLeft);
}

juce::Button* MosaicLookAndFeel::createSliderButton (juce::Slider&, bool isIncrement)
{
    return new juce::TextButton (isIncrement ? "+" : "-");
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
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 28, 22);
        s.setIncDecButtonsMode (juce::Slider::incDecButtonsNotDraggable);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setFont (rackui::font (rackui::Face::textMedium, 8.5f, 0.18f));
        l.setColour (juce::Label::textColourId, rackui::colour::inkDim);
        l.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (l);
    };
    setupIntSlider (rowsSlider, rowsLabel, "ROWS");
    setupIntSlider (colsSlider, colsLabel, "COLS");

    followButton.setButtonText ("Follow last");
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
    inspectorTitle.setFont (rackui::font (rackui::Face::legendBold, 13.5f, 0.2f));
    inspectorTitle.setColour (juce::Label::textColourId, rackui::colour::ink);
    addAndMakeVisible (inspectorTitle);

    addKnob (volumeKnob,   "Volume",  ParamID::roleGain);
    addKnob (panKnob,      "Pan",     ParamID::rolePan);
    addKnob (pitchKnob,    "Pitch",   ParamID::rolePitch);
    addKnob (attackKnob,   "Attack",  ParamID::roleAttack);
    addKnob (releaseKnob,  "Release", ParamID::roleRelease);
    addKnob (speedKnob,    "Speed",   ParamID::roleSpeed);
    addKnob (startKnob,    "Start",   ParamID::roleStart);
    addKnob (endKnob,      "End",     ParamID::roleEnd);

    addKnob (grainKnob,    "Grain",   ParamID::roleGrain);   // Granular pitch mode only

    allKnobs = { &volumeKnob, &panKnob, &pitchKnob, &attackKnob,
                 &releaseKnob, &speedKnob, &startKnob, &endKnob, &grainKnob };

    // Faces and printed scales, in each parameter's own units.
    using M = std::vector<rackui::Mark>;
    const auto face = [] (LabelledKnob& k, bool metal, const char* unit, M m)
    {
        k.slider.configure (metal ? 54.0f : 46.0f, metal ? rackui::Knob::Variant::metal : rackui::Knob::Variant::black,
                            Mosaic::accent, k.baseName.toUpperCase(), unit, std::move (m));
    };
    face (volumeKnob,  true,  "GAIN",   M { { 0, "0" }, { 0.5, ".5" }, { 1, "1" }, { 2, "2" } });
    face (panKnob,     false, "",       M { { -1, "L" }, { 0, "C" }, { 1, "R" } });
    face (pitchKnob,   false, "st",     M { { -24, "-24" }, { 0, "0" }, { 24, "+24" } });
    face (attackKnob,  false, "ms",     M { { 0, "0" }, { 100, "100" }, { 2000, "2k" } });
    face (releaseKnob, false, "ms",     M { { 1, "1" }, { 200, "200" }, { 5000, "5k" } });
    face (speedKnob,   false, "x",      M { { 0.25, ".25" }, { 1, "1" }, { 4, "4" } });
    face (startKnob,   false, "%",      M { { 0, "0" }, { 0.5, "50" }, { 1, "100" } });
    face (endKnob,     false, "%",      M { { 0, "0" }, { 0.5, "50" }, { 1, "100" } });
    face (grainKnob,   false, "ms",     M { { 5, "5" }, { 60, "60" }, { 120, "120" } });

    for (auto* k : allKnobs)
        k->slider.onPopupMenu = [this, k] { showKnobLearnMenu (*k); };

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

    // Play and pitch modes are LED selectors on the panel; they drive the hidden drop-downs so the
    // attachments and the multi-pad mirroring stay exactly as they were.
    for (auto* c : { &modeBox, &pitchModeBox })
        c->setVisible (false);
    for (auto* l : { &modeLabel, &pitchModeLabel, &outBusLabel })
        l->setVisible (false);
    playSelect.onChange   = [this] (int i) { modeBox.setSelectedItemIndex (i, juce::sendNotificationSync); };
    engineSelect.onChange = [this] (int i) { pitchModeBox.setSelectedItemIndex (i, juce::sendNotificationSync); };
    addAndMakeVisible (playSelect);
    addAndMakeVisible (engineSelect);

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
    presetName.setFont (rackui::font (rackui::Face::mono, 12.5f, 0.06f));
    presetName.setColour (juce::Label::textColourId, Mosaic::accent.interpolatedWith (juce::Colours::white, 0.45f));
    presetName.setJustificationType (juce::Justification::centredLeft);
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
    lfoTitle.setVisible (false);
    lfoTitle.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold).withKerningFactor (0.18f)));
    lfoTitle.setColour (juce::Label::textColourId, Mosaic::accent);
    addChildComponent (lfoTitle);
    for (int i = 0; i < Config::numLfos; ++i)
        setupLfoStrip (i);

    addAndMakeVisible (updateBadge);

    processor.addChangeListener (this);

    // The children were built before this editor's look and feel applied to them, so their
    // value boxes still carry the default style; rebuild them with ours.
    sendLookAndFeelChange();

    setSize (980 + 2 * rackui::earWidth, 690);
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
    k.slider.setTooltip ("Right-click to MIDI-learn a CC");
    addAndMakeVisible (k.slider);

    k.label.setText (name.toUpperCase(), juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.14f)));
    k.label.setColour (juce::Label::textColourId, Mosaic::text);   // the knob prints its own name

    // Bulk edit: turning a knob writes the value to every other selected pad.
    k.slider.onValueChange = [this, &k]
    {
        // Start stays before End: the knob stops at the other one instead of crossing it.
        const auto gap = (double) MosaicAudioProcessor::minTrimGap;
        if (&k == &startKnob && startKnob.slider.getValue() > endKnob.slider.getValue() - gap)
            return startKnob.slider.setValue (endKnob.slider.getValue() - gap);
        if (&k == &endKnob && endKnob.slider.getValue() < startKnob.slider.getValue() + gap)
            return endKnob.slider.setValue (startKnob.slider.getValue() + gap);
        mirrorToSelection (k.role, k.slider.getValue());
    };
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

        k->slider.setLegend (txt.toUpperCase(), learning ? Mosaic::accent2 : (cc >= 0 ? Mosaic::accent : rackui::colour::ink));
    }
}

//==============================================================================
void MosaicEditor::setupLfoStrip (int i)
{
    auto& apvts = processor.getAPVTS();
    auto& s = lfoStrips[i];

    s.name.setText ("LFO " + juce::String (i + 1), juce::dontSendNotification);
    s.name.setFont (rackui::font (rackui::Face::legendBold, 13.0f, 0.2f));
    s.name.setColour (juce::Label::textColourId, rackui::colour::ink);
    addAndMakeVisible (s.name);

    s.enable.setButtonText ("On");
    addAndMakeVisible (s.enable);
    s.sync.setButtonText ("Sync");
    addAndMakeVisible (s.sync);

    s.shape.addItemList (Config::lfoShapeChoices(), 1);
    s.syncRate.addItemList (Config::lfoSyncChoices(), 1);
    s.target.addItemList (Config::lfoTargetChoices(), 1);
    s.scope.addItemList (Config::lfoScopeChoices(), 1);
    addChildComponent (s.shape);   // driven by the LED shape selector
    s.shapeSelect.onChange = [&s] (int k) { s.shape.setSelectedItemIndex (k, juce::sendNotificationSync); };
    addAndMakeVisible (s.shapeSelect);
    addAndMakeVisible (s.syncRate);
    addAndMakeVisible (s.target);
    addAndMakeVisible (s.scope);

    for (auto* sl : { &s.rate, &s.depth })
    {
        sl->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 16);
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
    s.shapeSelect.setSelected (s.shape.getSelectedItemIndex());
}

// One LFO section: name, ON and SYNC; the shape selector; rate and depth knobs; then the sync
// division, target and scope windows.
void MosaicEditor::layoutLfoStrip (int i, juce::Rectangle<int> area)
{
    auto& s = lfoStrips[i];
    auto row = area.removeFromTop (20);
    s.name.setBounds (row.removeFromLeft (60));
    s.sync.setBounds (row.removeFromRight (58));
    s.enable.setBounds (row.removeFromRight (46));
    area.removeFromTop (8);
    s.shapeSelect.setBounds (area.removeFromTop (18));
    area.removeFromTop (10);
    auto knobs = area.removeFromTop (40);
    s.rate.setBounds (knobs.removeFromLeft (100));
    knobs.removeFromLeft (12);
    s.depth.setBounds (knobs.removeFromLeft (100));
    area.removeFromTop (10);
    auto windows = area.removeFromTop (22);
    const int w = (windows.getWidth() - 12) / 3;
    s.syncRate.setBounds (windows.removeFromLeft (w));
    windows.removeFromLeft (6);
    s.target.setBounds (windows.removeFromLeft (w));
    windows.removeFromLeft (6);
    s.scope.setBounds (windows);
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
    modeAttachment.reset();
    pitchModeAttachment.reset();

    for (auto* k : allKnobs)
        k->attachment = std::make_unique<SliderAttachment> (apvts, ParamID::forRole (k->role, selectedPad), k->slider);

    modeAttachment       = std::make_unique<ComboAttachment> (apvts, ParamID::playMode (selectedPad), modeBox);
    pitchModeAttachment  = std::make_unique<ComboAttachment> (apvts, ParamID::pitchMode (selectedPad), pitchModeBox);
    outBusBox.setSelectedId (processor.getPadOutputBus (selectedPad) + 1, juce::dontSendNotification);

    grainKnob.slider.setEnabled (pitchModeBox.getSelectedItemIndex() == Config::pitchGranular);
    playSelect.setSelected (modeBox.getSelectedItemIndex());
    engineSelect.setSelected (pitchModeBox.getSelectedItemIndex());

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

void MosaicEditor::refreshOutputMenu()
{
    int mask = 0;
    for (int b = 0; b < Config::numOutputBuses; ++b)
        if (auto* bus = processor.getBus (false, b); bus != nullptr && bus->isEnabled())
            mask |= 1 << b;
    if (mask == enabledOutputs)
        return;
    enabledOutputs = mask;

    // A pad sent to an output the host has not switched on plays on Out 1-2 instead; say so.
    for (int b = 1; b < Config::numOutputBuses; ++b)
    {
        const auto name = "Out " + juce::String (b * 2 + 1) + "-" + juce::String (b * 2 + 2);
        outBusBox.changeItemText (b + 1, (mask & (1 << b)) != 0 ? name : name + "  (not active: plays on 1-2)");
    }
    outBusBox.setTooltip ("Which output the pad plays on. Outputs 3-16 must be switched on in your host "
                          "(e.g. REAPER: the plugin's multichannel outputs); the Standalone app only has 1-2.");
}

void MosaicEditor::clearSelectionPads()
{
    for (int pad : selectedPads)
    {
        processor.clearPad (pad);
        if (juce::isPositiveAndBelow (pad, pads.size()))
            pads[pad]->refreshFromState();
    }
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
    refreshOutputMenu();

    // Presets and host automation move the drop-downs; the LEDs follow.
    playSelect.setSelected (modeBox.getSelectedItemIndex());
    engineSelect.setSelected (pitchModeBox.getSelectedItemIndex());
    for (auto& strip : lfoStrips)
        strip.shapeSelect.setSelected (strip.shape.getSelectedItemIndex());
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

// Everything static: plate, ears, nameplate, screen bezel, section legends and rules.
void MosaicEditor::paintBackground (juce::Graphics& g)
{
    using namespace rackui;
    const auto bounds = getLocalBounds().toFloat();
    const auto plateArea = bounds.reduced ((float) earWidth, 0.0f);
    plate (g, plateArea);
    ears (g, bounds);
    const float x = plateArea.getX() + 30.0f;
    nameplate (g, { x, 26.0f }, "FENRIKOS", "MOSAIC", "MODULAR PAD SAMPLER",
               rune ({ { 4.5f, 2, 4.5f, 18 }, { 15.5f, 2, 15.5f, 18 }, { 4.5f, 2, 15.5f, 10 }, { 15.5f, 2, 4.5f, 10 } }, 2.3f),
               juce::Colour (0xfff08a3e));
    oled (g, presetName.getBounds().toFloat());

    for (auto dx : topDividers)
        divider (g, (float) dx, 34.0f, 250.0f);

    // Pad screen: bezel, dark glass and a status line.
    const auto glassArea = bezel (g, screenArea.toFloat());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b1513), glassArea.getCentreX(), glassArea.getBottom(),
                                             juce::Colour (0xff060808), glassArea.getCentreX(), glassArea.getY(), true));
    g.fillRect (glassArea);
    oledText (g, "PADS  " + juce::String (visibleRows) + juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + juce::String (visibleCols),
              glassArea.reduced (10.0f, 0.0f).withHeight (20.0f), Mosaic::accent, 9.5f);

    // Right column legends.
    const float rx = (float) topDividers[1] + 20.0f;
    print (g, "GRID", { rx, 34.0f, 120.0f, 16.0f }, Face::legend, 11.5f, 0.22f, colour::ink, juce::Justification::centredLeft);
    print (g, "SELECT", { rx, 138.0f, 120.0f, 16.0f }, Face::legend, 11.5f, 0.22f, colour::ink, juce::Justification::centredLeft);

    rule (g, x, plateArea.getRight() - 30.0f, (float) topRuleY);
    rule (g, x, plateArea.getRight() - 30.0f, (float) lfoRuleY);

    // Inspector selectors legends.
    const auto legend = [&] (const char* t, juce::Component& c)
    {
        print (g, t, { (float) c.getX(), (float) c.getY() - 14.0f, 120.0f, 12.0f }, Face::textMedium, 8.5f, 0.18f, colour::inkDim,
               juce::Justification::centredLeft);
    };
    legend ("PLAY", playSelect);
    legend ("ENGINE", engineSelect);
    legend ("OUTPUT", outBusBox);

    // LFO knob legends.
    for (auto& strip : lfoStrips)
    {
        print (g, "RATE", { (float) strip.rate.getX() + 46.0f, (float) strip.rate.getY() - 1.0f, 60.0f, 12.0f }, Face::textMedium, 8.0f, 0.18f,
               colour::inkDim, juce::Justification::centredLeft);
        print (g, "DEPTH", { (float) strip.depth.getX() + 46.0f, (float) strip.depth.getY() - 1.0f, 60.0f, 12.0f }, Face::textMedium, 8.0f, 0.18f,
               colour::inkDim, juce::Justification::centredLeft);
    }

    print (g, "FENRIKOS", { x, bounds.getBottom() - 20.0f, 200.0f, 14.0f }, Face::textMedium, 8.0f, 0.3f, colour::inkFaint,
           juce::Justification::centredLeft);
}

void MosaicEditor::resized()
{
    const int left = rackui::earWidth + 30, right = getWidth() - rackui::earWidth - 30;

    // Top row: nameplate with preset window | pad screen | grid and selection controls.
    presetName.setBounds (left, 112, 206, 34);
    loadButton.setBounds (left, 156, 58, 22);
    saveButton.setBounds (left + 64, 156, 58, 22);
    topDividers[0] = left + 206 + 18;
    screenArea = { topDividers[0] + 20, 30, 540, 266 };
    topDividers[1] = screenArea.getRight() + 20;
    const int rx = topDividers[1] + 20;
    rowsLabel.setBounds (rx, 58, 40, 24);
    rowsSlider.setBounds (rx + 40, 58, 72, 24);
    colsLabel.setBounds (rx, 90, 40, 24);
    colsSlider.setBounds (rx + 40, 90, 72, 24);
    selectAllButton.setBounds (rx, 162, 52, 22);
    bulkMenuButton.setBounds (rx + 58, 162, 56, 22);
    followButton.setBounds (rx, 196, 120, 20);

    updateBadge.setBounds (right - 300, getHeight() - 20, 300, 14);

    // Inspector: header, nine knobs, then the mode selectors and output window.
    topRuleY = 316;
    inspectorTitle.setBounds (left, topRuleY + 14, 300, 20);
    int x = left - 26;
    for (auto* k : allKnobs)
    {
        const auto s = k->slider.preferredSize();
        k->slider.setBounds (x, topRuleY + 36, s.x, s.y);
        x += s.x - 38;
    }
    const int sx = right - 168;
    playSelect.setBounds (sx, topRuleY + 50, 168, 18);
    engineSelect.setBounds (sx, topRuleY + 90, 168, 18);
    outBusBox.setBounds (sx, topRuleY + 130, 150, 22);
    outBusBox.setVisible (true);

    // Modulation: three LFO sections side by side.
    lfoRuleY = topRuleY + 178;
    const int lfoW = (right - left - 2 * 30) / Config::numLfos;
    for (int i = 0; i < Config::numLfos; ++i)
        layoutLfoStrip (i, { left + i * (lfoW + 30), lfoRuleY + 16, lfoW, 160 });

    background = juce::Image (juce::Image::RGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), false);
    {
        juce::Graphics bg (background);
        paintBackground (bg);
    }

    // Pad grid, inside the screen under its status line.
    auto padArea = screenArea.reduced (7).withTrimmedTop (22).reduced (8, 0).withTrimmedBottom (8);
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
