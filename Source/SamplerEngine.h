#pragma once

#include <JuceHeader.h>
JUCE_BEGIN_IGNORE_WARNINGS_MSVC (4305 4244 4267 4456 4458)
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wconversion", "-Wfloat-conversion", "-Wshadow")
#include "signalsmith-stretch.h"   // high-quality pitch/time for "Studio" mode
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
JUCE_END_IGNORE_WARNINGS_MSVC
#include "Parameters.h"

//==============================================================================
/*  A sound owned by one pad. Holds the loaded audio and answers note-mapping
    queries by reading an atomic that the message thread updates (MIDI-learn).
    The audio buffer is only ever swapped under the Synthesiser lock, so the
    audio thread reads it without allocating or copying.
*/
class PadSound : public juce::SynthesiserSound
{
public:
    PadSound (int padIndex, const std::atomic<int>& mappedNote)
        : index (padIndex), mappedNoteRef (mappedNote) {}

    bool appliesToNote    (int midiNote) override { const int n = mappedNoteRef.load(); return n >= 0 && n == midiNote; }
    bool appliesToChannel (int)          override { return true; }

    int  getPadIndex() const noexcept { return index; }

    // Called on the message thread while the Synthesiser lock is held.
    void setSample (juce::AudioBuffer<float>&& newData, double sr) noexcept
    {
        data = std::move (newData);
        sourceSampleRate = sr;
    }

    void clearSample() noexcept { data.setSize (0, 0); sourceSampleRate = 44100.0; }

    const juce::AudioBuffer<float>& getData()          const noexcept { return data; }
    double                          getSourceRate()    const noexcept { return sourceSampleRate; }
    bool                            hasSample()        const noexcept { return data.getNumSamples() > 0; }

private:
    int index;
    const std::atomic<int>& mappedNoteRef;
    juce::AudioBuffer<float> data;
    double sourceSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadSound)
};

//==============================================================================
class SamplerEngine;   // fwd

/*  One playing sample voice. All per-block values are read from cached atomic
    parameter pointers held by the engine — no allocation, no locking here.
*/
class PadVoice : public juce::SynthesiserVoice
{
public:
    explicit PadVoice (SamplerEngine& ownerEngine) : engine (ownerEngine) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<PadSound*> (s) != nullptr;
    }

    void startNote (int midiNote, float velocity,
                    juce::SynthesiserSound* sound, int currentPitchWheel) override;
    void stopNote  (float velocity, bool allowTailOff) override;
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    // Allocate the Studio (Signalsmith) engine + buffers (message thread, in prepare).
    void prepareStudio (double sampleRate, int maxBlock);

    // Choke: initiate a fast fade-out, then free the voice.
    void choke() noexcept;

    int  getCurrentPad() const noexcept { return currentPad; }

    // "Sustaining" == a voice whose pad is *currently* in Looping mode and which
    // has not been asked to stop. Read live so mode changes take effect at once.
    bool isSustaining() const noexcept;

private:
    void finish() noexcept;

    // Two playback engines share the same envelope / pan / choke / routing tail:
    //  - renderDirect:   single read head at the native rate. Bit-transparent, used
    //                    when pitch == 0 and speed == 1 (the common drum-hit case).
    //  - renderGranular: overlap-add granular player that shifts pitch and time
    //                    independently. Engaged only when pitch or speed differ.
    void renderDirect   (juce::AudioBuffer<float>&, int startSample, int numSamples);
    void renderGranular (juce::AudioBuffer<float>&, int startSample, int numSamples);
    void renderStudio   (juce::AudioBuffer<float>&, int startSample, int numSamples);
    bool interpolate (double sourcePos, float& l, float& r) const noexcept;
    bool updateLoopState() noexcept;      // live mode read + stop-on-mode-change
    void spawnGrain (double anchor) noexcept;

    static constexpr int maxGrains = 4;
    struct Grain { double anchor = 0.0; int pos = 0; bool active = false; };

    SamplerEngine& engine;
    PadSound* currentSound = nullptr;
    int    currentPad          = -1;
    double sourceSamplePosition = 0.0;    // direct-path read head
    float  noteVelocity         = 1.0f;
    int    lastSeenMode         = Config::modeLatching;   // to detect a live mode change
    int    registeredPad        = -1;      // pad counted in the engine's activity meter
    bool   released             = false;
    bool   chokeActive          = false;

    // Playback path chosen at note start: 0=Direct(repitch/transparent), 1=Granular, 2=Studio.
    int    pitchPath            = 0;
    Grain  grains[maxGrains];
    double grainAnchor          = 0.0;     // time-base position (next grain origin)
    int    hopCounter           = 0;
    int    grainSize            = 1024;
    int    grainHop             = 512;
    bool   stopSpawning         = false;

    juce::ADSR adsr;
    juce::LinearSmoothedValue<float> chokeLevel;
    juce::LinearSmoothedValue<float> gainLSm, gainRSm;   // de-zippered L/R gains

    // Studio (Signalsmith) path state.
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::AudioBuffer<float> studioIn, studioOut;
    double studioReadPos = 0.0;
    bool   studioEnded   = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadVoice)
};

//==============================================================================
/*  Synthesiser subclass adding mode-aware, one-voice-per-loop pad triggering
    and a direct pad-trigger path used by the UI for auditioning pads.
*/
class PadSynthesiser : public juce::Synthesiser
{
public:
    void setEngine (SamplerEngine* e) noexcept { engine = e; }

    // Expose the internal render lock so the message thread can swap sample
    // buffers safely against renderNextBlock.
    juce::CriticalSection& getRenderLock() noexcept { return lock; }

    void noteOn  (int midiChannel, int midiNoteNumber, float velocity) override;
    void noteOff (int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override;

    // Mode-aware pad triggering (used by both MIDI and the UI, message thread).
    void pressPad (int padIndex, float velocity);   // start / retrigger / toggle-off looping
    void stopPad  (int padIndex);                   // force release all voices of a pad
    bool isPadSustaining (int padIndex);

private:
    void pressPadLocked      (int padIndex, float velocity);
    void startPadVoiceLocked (int padIndex, float velocity);
    void stopPadLocked       (int padIndex);
    bool isPadSustainingLocked (int padIndex) const;

    PadSound* findSoundForPad (int padIndex) const;

    SamplerEngine* engine = nullptr;
};

//==============================================================================
/*  Owns the synthesiser, the per-pad sounds, and the cached atomic parameter
    pointers. Provides cheap, allocation-free accessors for the audio thread.
*/
class SamplerEngine
{
public:
    SamplerEngine();

    // Message thread ------------------------------------------------------
    void cacheParameterPointers (juce::AudioProcessorValueTreeState& apvts);
    void createSoundsAndVoices();
    void prepare (double sampleRate, int samplesPerBlock);

    void loadSample (int padIndex, juce::AudioBuffer<float>&& data, double sr);
    void clearSample (int padIndex);

    void setMappedNote (int padIndex, int note) noexcept { mappedNote[padIndex].store (note); }
    void setOutputBus  (int padIndex, int bus)  noexcept { outputBus[padIndex].store (bus); }
    int  getMappedNote (int padIndex) const noexcept     { return mappedNote[padIndex].load(); }
    int  getOutputBus  (int padIndex) const noexcept     { return outputBus[padIndex].load(); }
    const std::atomic<int>& mappedNoteRef (int padIndex) const noexcept { return mappedNote[padIndex]; }

    void pressPad   (int padIndex, float velocity) { synth.pressPad (padIndex, velocity); }
    void releasePad (int padIndex)                 { synth.stopPad (padIndex); }
    bool isPadSustaining (int padIndex)            { return synth.isPadSustaining (padIndex); }

    // Per-pad activity meter. Voices register/unregister as they start/finish so
    // the UI can light up pads that are currently sounding. A pad may hold more
    // than one voice (layered latching hits), hence a counter rather than a flag.
    void incrementActivity (int padIndex) noexcept
    {
        if (padIndex >= 0 && padIndex < Config::maxPads) activity[padIndex].fetch_add (1);
    }
    void decrementActivity (int padIndex) noexcept
    {
        if (padIndex >= 0 && padIndex < Config::maxPads) activity[padIndex].fetch_sub (1);
    }
    bool isPadActive (int padIndex) const noexcept
    {
        return padIndex >= 0 && padIndex < Config::maxPads && activity[padIndex].load() > 0;
    }

    // Records the most recently triggered pad. playCounter bumps on every note
    // start so the UI can detect even a same-pad retrigger.
    void notePlayed (int padIndex) noexcept
    {
        lastPlayedPad.store (padIndex);
        playCounter.fetch_add (1, std::memory_order_relaxed);
    }
    int          getLastPlayedPad() const noexcept { return lastPlayedPad.load(); }
    juce::uint32 getPlayCounter()   const noexcept { return playCounter.load(); }

    // Normalised (0..1 over the whole sample) playback position, for the UI cursor.
    void setPlayPosition (int padIndex, float pos01) noexcept
    {
        if (padIndex >= 0 && padIndex < Config::maxPads) playPosition[padIndex].store (pos01);
    }
    float getPlayPosition (int padIndex) const noexcept
    {
        return (padIndex >= 0 && padIndex < Config::maxPads) ? playPosition[padIndex].load() : 0.0f;
    }

    // Hardware LED feedback: per-pad "has a sample" flag and the controller
    // velocity/colour to send when the pad is lit bright (playing) or dim (idle).
    bool hasSampleLoaded (int padIndex) const noexcept
    {
        return padIndex >= 0 && padIndex < Config::maxPads && sampleLoaded[padIndex].load();
    }
    void setLedColours (int padIndex, int brightVel, int dimVel) noexcept
    {
        if (padIndex >= 0 && padIndex < Config::maxPads)
        {
            ledBright[padIndex].store (brightVel);
            ledDim[padIndex].store (dimVel);
        }
    }
    int getLedBright (int padIndex) const noexcept
    {
        return (padIndex >= 0 && padIndex < Config::maxPads) ? ledBright[padIndex].load() : 0;
    }
    int getLedDim (int padIndex) const noexcept
    {
        return (padIndex >= 0 && padIndex < Config::maxPads) ? ledDim[padIndex].load() : 0;
    }

    // Audio thread --------------------------------------------------------
    void setBusOffset (int bus, int channelOffset) noexcept { busChannelOffset[bus] = channelOffset; }
    void process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    }

    // Real-time-safe parameter reads (used by PadVoice) -------------------
    float gain      (int p) const noexcept { return load (gainP,    p, 1.0f); }
    float pan       (int p) const noexcept { return load (panP,     p, 0.0f); }
    float pitch     (int p) const noexcept { return load (pitchP,   p, 0.0f); }
    float speed     (int p) const noexcept { return load (speedP,   p, 1.0f); }
    float startNorm (int p) const noexcept { return load (startP,   p, 0.0f); }
    float endNorm   (int p) const noexcept { return load (endP,     p, 1.0f); }
    float attackSec (int p) const noexcept { return load (attackP,  p, 1.0f)   * 0.001f; }
    float releaseSec(int p) const noexcept { return load (releaseP, p, 40.0f)  * 0.001f; }
    int   playMode  (int p) const noexcept { return (int) load (modeP, p, (float) Config::modeLatching); }
    bool  isLooping (int p) const noexcept { return playMode (p) == Config::modeLooping; }
    int   pitchModeOf (int p) const noexcept { return (int) load (pitchModeP, p, (float) Config::pitchGranular); }
    float grainSizeMs (int p) const noexcept { return load (grainSizeP, p, 40.0f); }

    // LFO-modulated parameter reads (used by the voices).
    float gainMod  (int p) const noexcept { return juce::jmax (0.0f, gain (p) * (1.0f + modFor (Config::tgtVolume, p))); }
    float panMod   (int p) const noexcept { return juce::jlimit (-1.0f, 1.0f, pan (p) + modFor (Config::tgtPan, p)); }
    float pitchMod (int p) const noexcept { return pitch (p) + modFor (Config::tgtPitch, p) * 12.0f; }
    float speedMod (int p) const noexcept { return juce::jmax (0.01f, speed (p) * (1.0f + modFor (Config::tgtSpeed, p))); }

    // Modulation bank --------------------------------------------------------
    void  setTempo (double bpm) noexcept { tempoBpm = bpm > 0.0 ? bpm : 120.0; }
    void  setSelectedPad (int pad) noexcept { selectedPadForMod.store (pad); }
    void  advanceLfos (int blockSamples) noexcept;     // once per processBlock

    float modFor (int target, int pad) const noexcept
    {
        float s = 0.0f;
        const int sel = selectedPadForMod.load();
        for (int i = 0; i < Config::numLfos; ++i)
            if ((int) load (lfoTargetP, i, 0.0f) == target
                && ((int) load (lfoScopeP, i, 0.0f) == Config::scopeAll || pad == sel))
                s += lfoCurrent[i];
        return s;
    }

    // Resolve a pad's output pair to absolute channels in the buses buffer.
    void getOutputChannels (int padIndex, int& left, int& right) const noexcept
    {
        int bus = outputBus[padIndex].load();
        if (bus < 0 || bus >= Config::numOutputBuses) bus = 0;
        int off = busChannelOffset[bus];
        if (off < 0) off = busChannelOffset[0];     // fall back to main if disabled
        if (off < 0) off = 0;
        left  = off;
        right = off + 1;
    }

    PadSynthesiser& getSynth() noexcept { return synth; }

private:
    static float load (const std::atomic<float>* const* arr, int p, float fallback) noexcept
    {
        auto* ptr = arr[p];
        return ptr != nullptr ? ptr->load() : fallback;
    }

    PadSynthesiser synth;
    juce::ReferenceCountedArray<juce::SynthesiserSound> ownedSounds;   // keep sounds alive
    PadSound* padSounds[Config::maxPads] = {};

    // Cached raw parameter pointers (owned by the APVTS).
    const std::atomic<float>* gainP    [Config::maxPads] = {};
    const std::atomic<float>* panP     [Config::maxPads] = {};
    const std::atomic<float>* pitchP   [Config::maxPads] = {};
    const std::atomic<float>* attackP  [Config::maxPads] = {};
    const std::atomic<float>* releaseP [Config::maxPads] = {};
    const std::atomic<float>* speedP   [Config::maxPads] = {};
    const std::atomic<float>* startP   [Config::maxPads] = {};
    const std::atomic<float>* endP     [Config::maxPads] = {};
    const std::atomic<float>* modeP      [Config::maxPads] = {};
    const std::atomic<float>* pitchModeP [Config::maxPads] = {};
    const std::atomic<float>* grainSizeP [Config::maxPads] = {};

    // LFO cached pointers + runtime state (audio thread owns phase/value).
    const std::atomic<float>* lfoEnableP  [Config::numLfos] = {};
    const std::atomic<float>* lfoShapeP   [Config::numLfos] = {};
    const std::atomic<float>* lfoRateP    [Config::numLfos] = {};
    const std::atomic<float>* lfoSyncP    [Config::numLfos] = {};
    const std::atomic<float>* lfoSyncRtP  [Config::numLfos] = {};
    const std::atomic<float>* lfoDepthP   [Config::numLfos] = {};
    const std::atomic<float>* lfoTargetP  [Config::numLfos] = {};
    const std::atomic<float>* lfoScopeP   [Config::numLfos] = {};
    double lfoPhase   [Config::numLfos] = {};
    float  lfoCurrent [Config::numLfos] = {};
    float  snhValue   [Config::numLfos] = {};
    double currentSampleRate = 44100.0;
    double tempoBpm = 120.0;
    std::atomic<int> selectedPadForMod { 0 };
    juce::Random lfoRandom;

    // Metadata mirrored into atomics for the audio thread.
    std::atomic<int> mappedNote[Config::maxPads];
    std::atomic<int> outputBus [Config::maxPads];

    // Number of live voices per pad (written by audio thread, read by UI).
    std::atomic<int>   activity     [Config::maxPads];
    std::atomic<float> playPosition [Config::maxPads];

    // Last-played tracking (for "follow last played").
    std::atomic<int>          lastPlayedPad { -1 };
    std::atomic<juce::uint32>  playCounter  { 0 };

    // Hardware LED feedback state.
    std::atomic<bool>  sampleLoaded [Config::maxPads];
    std::atomic<int>   ledBright    [Config::maxPads];
    std::atomic<int>   ledDim       [Config::maxPads];

    int busChannelOffset[Config::numOutputBuses] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEngine)
};
