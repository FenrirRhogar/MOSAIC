#pragma once

#include <JuceHeader.h>
#include <set>
#include "PluginProcessor.h"
#include "PadComponent.h"
#include "Theme.h"
#include "UpdateCheck.h"

//==============================================================================
/*  MOSAIC colour palette + custom look-and-feel. */
namespace Mosaic
{
    // The fenrikos palette (see Theme.h), with MOSAIC's teal and pink accents.
    const juce::Colour bg       = rack::colours::plateTop;
    const juce::Colour bgDeep   = rack::colours::plateBottom;
    const juce::Colour panel    { 0xff1b1d20 };
    const juce::Colour panelHi  { 0xff2e3035 };
    const juce::Colour accent   { 0xff37d6c2 };   // teal
    const juce::Colour accent2  { 0xffff5e87 };   // pink
    const juce::Colour text     = rack::colours::silk;
    const juce::Colour textDim  = rack::colours::silkDim;
}

class MosaicLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MosaicLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getLabelFont (juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isMouseOver, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOver, bool isButtonDown) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool isMouseOver, bool isButtonDown) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

private:
    rack::KnobCache knobCache;
};

//==============================================================================
/*  The MOSAIC editor: a branded header, a dynamic pad grid, and an inspector
    panel whose knobs/combos re-point to the currently selected pad. */
class MosaicEditor : public juce::AudioProcessorEditor,
                                    public juce::ChangeListener,
                                    private juce::AudioProcessorValueTreeState::Listener,
                                    private juce::AsyncUpdater,
                                    private juce::Timer
{
public:
    explicit MosaicEditor (MosaicAudioProcessor&);
    ~MosaicEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;

    void rebuildGridLayout();
    void selectPad (int padIndex);                                   // single-select
    void handlePadClicked (int pad, const juce::ModifierKeys& mods); // +Ctrl/Shift multi-select
    void applySelection();                                           // sync UI + attachments
    void mirrorToSelection (int role, double value);                // bulk-edit broadcast
    void setColourForSelection (juce::Colour c);
    void clearSelectionPads();
    void refreshOutputMenu();                    // marks outputs the host has not switched on
    int  enabledOutputs = -1;                    // bit per output bus, as last shown
    void relinkMissingSamples();                 // asks for a folder, then searches it

    //==========================================================================
    using SliderAttachment  = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment   = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment  = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // One global LFO's controls + APVTS attachments (created once).
    struct LfoStrip
    {
        juce::Label        name;
        juce::ToggleButton enable, sync;
        juce::ComboBox     shape, syncRate, target, scope;
        juce::Slider       rate, depth;
        std::unique_ptr<ButtonAttachment> enableAtt, syncAtt;
        std::unique_ptr<ComboAttachment>  shapeAtt, syncRateAtt, targetAtt, scopeAtt;
        std::unique_ptr<SliderAttachment> rateAtt, depthAtt;
    };
    void setupLfoStrip (int i);
    void layoutLfoStrip (int i, juce::Rectangle<int> row);

    // A rotary slider that forwards right-clicks (for the MIDI-learn menu)
    // instead of treating them as value edits.
    struct LearnableSlider : juce::Slider
    {
        std::function<void()> onPopupMenu;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onPopupMenu) { onPopupMenu(); return; }
            juce::Slider::mouseDown (e);
        }
    };

    struct LabelledKnob
    {
        LearnableSlider slider;
        juce::Label     label;
        juce::String    baseName;
        int             role = -1;     // ParamID::KnobRole for CC-learn
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addKnob (LabelledKnob& k, const juce::String& name, int role);
    void showKnobLearnMenu (LabelledKnob& k);
    void updateKnobDecorations();

    MosaicLookAndFeel lnf;

    MosaicAudioProcessor& processor;

    // Header ---------------------------------------------------------------
    juce::Image   background;                   // everything static, redrawn only on resize
    void paintBackground (juce::Graphics& g);
    juce::Slider  rowsSlider, colsSlider;
    juce::Label   rowsLabel,  colsLabel;
    std::unique_ptr<SliderAttachment> rowsAttachment, colsAttachment;
    juce::ToggleButton followButton;             // select the last-played pad
    juce::uint32       lastSeenPlayCounter = 0;
    juce::TextButton   selectAllButton { "All" };
    juce::TextButton   bulkMenuButton  { "Edit..." };   // bulk ops on the selection
    juce::TextButton   saveButton { "Save" }, loadButton { "Load" };
    juce::Label        presetName;
    std::unique_ptr<juce::FileChooser> chooser;

    // Grid -----------------------------------------------------------------
    juce::AudioThumbnailCache thumbnailCache { Config::maxPads };
    juce::OwnedArray<PadComponent> pads;
    int visibleRows = Config::defaultRows;
    int visibleCols = Config::defaultCols;
    juce::Rectangle<int> gridPanel, inspectorPanel;   // for paint() backgrounds

    // Inspector ------------------------------------------------------------
    int selectedPad = 0;                 // the "primary" pad (drives the attachments)
    std::set<int> selectedPads { 0 };    // full selection (includes primary)
    bool suppressMirror = false;         // guard bulk-edit during attachment setup
    juce::Label   inspectorTitle;
    LabelledKnob  volumeKnob, panKnob, pitchKnob, attackKnob, releaseKnob, speedKnob;
    LabelledKnob  startKnob, endKnob;
    LabelledKnob  grainKnob;                     // Granular grain size
    std::array<LabelledKnob*, ParamID::numRoles> allKnobs {};

    juce::ComboBox modeBox, pitchModeBox, outBusBox;
    juce::Label    modeLabel, pitchModeLabel, outBusLabel;
    std::unique_ptr<ComboAttachment> modeAttachment, pitchModeAttachment;

    // LFO panel -----------------------------------------------------------
    LfoStrip lfoStrips[Config::numLfos];
    juce::Label lfoTitle;
    juce::Rectangle<int> lfoPanel;

    std::atomic<bool> gridDirty { true };

    // "MOSAIC 1.1.0" in the footer, or "UPDATE ... AVAILABLE" (UpdateCheck.h).
    fenrikos::UpdateBadge updateBadge { "MOSAIC " + juce::String (JucePlugin_VersionString),
                                        juce::FontOptions (10.0f, juce::Font::bold).withKerningFactor (0.25f),
                                        Mosaic::textDim.withAlpha (0.45f), Mosaic::accent, juce::Justification::centredLeft };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MosaicEditor)
};
