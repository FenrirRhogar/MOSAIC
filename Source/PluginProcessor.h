#pragma once

#include <JuceHeader.h>
#include <optional>
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
                                       private juce::Timer,
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

    // Two kinds of preset, kept apart so switching one never disturbs the other:
    //  - a kit (.mosaic): samples, colours, per-pad sound, modes, outputs, grid and LFOs;
    //  - a controller (.mosaicctl): which note plays each pad and which CC drives each knob.
    // Loading a kit keeps the current controller mapping (older .mosaic files that still carry
    // one load fine; their mapping is ignored), and loading a controller keeps the sound.
    static juce::File getKitsDirectory();
    static juce::File getControllersDirectory();
    bool saveKitToFile (const juce::File& file);
    bool loadKitFromFile (const juce::File& file);
    bool saveControllerToFile (const juce::File& file);
    bool loadControllerFromFile (const juce::File& file);
    void clearControllerMapping();
    juce::String getKitName() const;
    juce::String getControllerName() const;
    /// True when the sound (samples, pad settings, knobs) differs from the kit as it was loaded or
    /// saved, or the notes and CCs differ from the controller preset. Putting things back clears it.
    bool isKitModified() const;
    bool isControllerModified() const;

    //==========================================================================
    // Public API used by the editor (all message thread).
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    bool          setPadSample  (int padIndex, const juce::File& file);   // decodes now (state restore)
    /// Decodes in the background so the UI never freezes; if several loads overlap, the newest wins.
    void          requestPadSample (int padIndex, const juce::File& file);

    /// A pad whose sample file could not be found when the session or preset was opened.
    /// Its path is kept, so saving the session does not forget it.
    bool          isPadSampleMissing (int padIndex) const noexcept;
    int           getNumMissingSamples() const noexcept;
    /// Searches @p folder and its subfolders for missing samples by file name and loads them.
    /// Returns how many were found.
    int           relinkMissingSamples (const juce::File& folder);
    void          clearPad      (int padIndex);
    static constexpr float minTrimGap = 0.001f;   // smallest Start-to-End distance the knobs and CCs allow
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

    // The message-thread half of MIDI handling: finishes a note or CC learn and applies the CC
    // values the audio thread has collected. A timer calls it ~100 times a second, so the audio
    // thread only ever touches atomics (no listeners, locks or message-queue posts).
    void servicePendingMidi();

private:
    struct KitSnapshot
    {
        std::vector<float> values;
        juce::StringArray pads;
        bool operator== (const KitSnapshot& o) const
        {
            if (pads != o.pads || values.size() != o.values.size())
                return false;
            for (size_t i = 0; i < values.size(); ++i)
                if (std::abs (values[i] - o.values[i]) > 1.0e-5f)
                    return false;
            return true;
        }
    };
    struct ControllerSnapshot
    {
        std::array<int, Config::maxPads> notes {};
        std::array<int, 128> ccs {};
        bool operator== (const ControllerSnapshot& o) const { return notes == o.notes && ccs == o.ccs; }
    };
    KitSnapshot captureKitSnapshot() const;
    ControllerSnapshot captureControllerSnapshot() const;
    void markKitClean();
    void markControllerClean();
    mutable juce::CriticalSection cleanLock;
    KitSnapshot cleanKit;
    ControllerSnapshot cleanController;
    static BusesProperties makeBusesProperties();
    juce::ValueTree padMeta (int padIndex);
    juce::ValueTree padMeta (int padIndex) const;
    void   buildDefaultMetaTree();
    void   syncEngineFromMeta();      // push all metadata into engine atomics + reload samples

    void   writeCCMapToTree();        // persist CC map into ccMapTree
    juce::ValueTree captureController() const;                 // notes + CC map, as a standalone tree
    void   applyController (const juce::ValueTree& controller);
    void   rebuildCCMapFromTree();    // rebuild runtime CC map after a state load
    void   restoreFromXml (const juce::XmlElement& xml);   // shared by setState + preset load

    void   updatePadLedColour (int padIndex);              // recompute LED velocities from pad colour
    void   sendLedFeedback (juce::MidiBuffer& out);        // emit LED note-ons on state change
    static constexpr int maxLedEventsPerBlock = 16;        // the host owns the output buffer: keep bursts small

    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree metaTree;         // "PADS_META" child of apvts.state

    SamplerEngine engine;
    juce::AudioFormatManager formatManager;

    void timerCallback() override { servicePendingMidi(); }

    // CC values waiting for the message thread, per knob role (latest wins).
    std::atomic<float> pendingCcValue [ParamID::numRoles] {};
    std::atomic<int>   pendingCcPad   [ParamID::numRoles] {};
    std::atomic<bool>  pendingCcDirty [ParamID::numRoles] {};

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

    struct DecodedSample
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;
    };
    std::optional<DecodedSample> decodeSample (const juce::File& file);
    void applyDecodedSample (int padIndex, const juce::File& file, DecodedSample&& decoded);

    std::atomic<int>  loadGeneration[Config::maxPads] {};   // bumped by every load or clear
    std::atomic<bool> sampleMissing [Config::maxPads] {};

    // Declared last: destroyed first, so a running decode finishes before the members it uses go.
    juce::ThreadPool sampleLoader { 1 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (MosaicAudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MosaicAudioProcessor)
};
