#pragma once

#include <JuceHeader.h>
#include <set>
#include "PluginProcessor.h"
#include "PadComponent.h"

//==============================================================================
/*  MOSAIC colour palette + custom look-and-feel. */
namespace Mosaic
{
    const juce::Colour bg       { 0xff121218 };
    const juce::Colour bgDeep   { 0xff0c0c11 };
    const juce::Colour panel    { 0xff1d1d27 };
    const juce::Colour panelHi  { 0xff29293a };
    const juce::Colour accent   { 0xff37d6c2 };   // teal
    const juce::Colour accent2  { 0xffff5e87 };   // pink
    const juce::Colour text     { 0xffe9e9f1 };
    const juce::Colour textDim  { 0xff888897 };
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
    juce::Font getLabelFont (juce::Label&) override;
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
    juce::Label   titleLabel, subtitleLabel;
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
    LabelledKnob  grainKnob;                     // Granular grain size (not CC-learnable)
    std::array<LabelledKnob*, 8> allKnobs {};    // role knobs only

    juce::ComboBox modeBox, pitchModeBox, outBusBox;
    juce::Label    modeLabel, pitchModeLabel, outBusLabel;
    std::unique_ptr<ComboAttachment> modeAttachment, pitchModeAttachment;

    // LFO panel -----------------------------------------------------------
    LfoStrip lfoStrips[Config::numLfos];
    juce::Label lfoTitle;
    juce::Rectangle<int> lfoPanel;

    std::atomic<bool> gridDirty { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MosaicEditor)
};
