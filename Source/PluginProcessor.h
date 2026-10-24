#pragma once

#include <JuceHeader.h>
#include "Parameters.h"
#include "SamplerEngine.h"

//==============================================================================
/*  Dynamic Modular Sampler processor.

    Responsibilities:
      - Owns the APVTS (all automatable per-pad DSP parameters + grid size).
      - Owns a PADS_META child ValueTree for non-automatable per-pad metadata
        (sample path, colour, learned MIDI note, output-bus routing). It is a
        child of apvts.state so it serialises with the rest of the state.
      - Configures multiple stereo output buses for per-pad routing.
      - Drives the SamplerEngine and handles MIDI-learn without touching
        parameters from the audio thread.

    It is a ChangeBroadcaster: any metadata change (including one applied from a
    MIDI-learn on the audio thread) pushes a refresh to the editor.
*/
class MosaicAudioProcessor : public juce::AudioProcessor,
                                       public juce::ValueTree::Listener,
                                       public juce::AsyncUpdater,
                                       public juce::ChangeBroadcaster
{
public:
    MosaicAudioProcessor();
    ~MosaicAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MOSAIC"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }   // LED feedback to the controller
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Preset files (.mosaic XML, sample paths referenced not embedded).
    static juce::File getPresetsDirectory();
    bool savePresetToFile  (const juce::File& file);
    bool loadPresetFromFile (const juce::File& file);

    //==========================================================================
    // Public API used by the editor (all message thread).
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    bool          setPadSample  (int padIndex, const juce::File& file);
    void          clearPad      (int padIndex);
    void          setPadColour  (int padIndex, juce::Colour colour);
    void          setPadOutputBus (int padIndex, int bus);
    void          startMidiLearn (int padIndex) noexcept { midiLearnTarget.store (padIndex); sendChangeMessage(); }
    void          cancelMidiLearn() noexcept             { midiLearnTarget.store (-1); sendChangeMessage(); }

    juce::Colour  getPadColour  (int padIndex) const;
    juce::String  getPadFilePath (int padIndex) const;
    int           getPadNote    (int padIndex) const { return engine.getMappedNote (padIndex); }
    int           getPadOutputBus (int padIndex) const { return engine.getOutputBus (padIndex); }
    bool          isPadLearning (int padIndex) const noexcept { return midiLearnTarget.load() == padIndex; }

    // Pad tap handling. padPressed applies the pad's trigger mode
    // (momentary / latching / looping); padReleased only stops momentary pads.
    void          padPressed  (int padIndex, float velocity) { engine.pressPad (padIndex, velocity); }
    void          padReleased (int padIndex)
    {
        if (engine.playMode (padIndex) == Config::modeMomentary)
            engine.releasePad (padIndex);
    }
    int           getPadPlayMode (int padIndex) const { return engine.playMode (padIndex); }
    bool          isPadActive (int padIndex) const { return engine.isPadActive (padIndex); }
    float         getPadPlayPosition (int padIndex) const { return engine.getPlayPosition (padIndex); }
    int           getLastPlayedPad() const { return engine.getLastPlayedPad(); }
    juce::uint32  getPlayCounter()   const { return engine.getPlayCounter(); }

    // MIDI-CC learn for inspector-knob roles. A mapping is stored per role, so a
    // learned hardware knob always drives the currently selected pad.
    void          startKnobLearn   (int role);
    void          cancelKnobLearn();
    void          clearKnobCC      (int role);
    bool          isKnobLearning   (int role) const;
    int           getCCForKnobRole (int role) const;
    void          setActivePad     (int padIndex) noexcept { activePad.store (padIndex); engine.setSelectedPad (padIndex); }
    int           getActivePad() const noexcept { return activePad.load(); }

    juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

    //==========================================================================
    // ValueTree::Listener — keeps engine atomics in sync and notifies the UI.
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded   (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    // AsyncUpdater — applies a MIDI-learn result on the message thread.
    void handleAsyncUpdate() override;

private:
    static BusesProperties makeBusesProperties();
    juce::ValueTree padMeta (int padIndex);
    juce::ValueTree padMeta (int padIndex) const;
    void   buildDefaultMetaTree();
    void   syncEngineFromMeta();      // push all metadata into engine atomics + reload samples

    void   writeCCMapToTree();        // persist CC map into ccMapTree
    void   rebuildCCMapFromTree();    // rebuild runtime CC map after a state load
    void   restoreFromXml (const juce::XmlElement& xml);   // shared by setState + preset load

    void   updatePadLedColour (int padIndex);              // recompute LED velocities from pad colour
    void   sendLedFeedback (juce::MidiBuffer& out);        // emit LED note-ons on state change

    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree metaTree;         // "PADS_META" child of apvts.state

    SamplerEngine engine;
    juce::AudioFormatManager formatManager;

    // MIDI note-learn: audio thread records, message thread applies.
    std::atomic<int> midiLearnTarget { -1 };
    std::atomic<int> pendingLearnPad { -1 };
    std::atomic<int> pendingLearnNote { -1 };

    // MIDI-CC → knob-role map (indexed by CC number) plus learn state. The CC
    // drives whichever pad is selected (activePad), via cached parameter pointers.
    std::atomic<int> ccToRole[128];
    std::atomic<int> knobLearnRole  { -1 };
    std::atomic<int> pendingLearnCC { -1 };
    std::atomic<int> pendingLearnRole { -1 };
    std::atomic<int> activePad { 0 };
    juce::RangedAudioParameter* roleParams[ParamID::numRoles][Config::maxPads] = {};
    juce::ValueTree ccMapTree;      // "CCMAP" child of apvts.state, for persistence

    // LED feedback: last velocity/note pushed to the controller per pad (audio
    // thread only), so we only send on change. -2 == "nothing sent yet".
    int lastLedVel[Config::maxPads];
    int lastLedNote[Config::maxPads];
    static constexpr int ledChannel = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MosaicAudioProcessor)
};
