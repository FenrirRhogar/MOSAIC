#pragma once

#include <JuceHeader.h>

//==============================================================================
/*  Global configuration constants.

    The plugin exposes a *fixed* pool of parameters (one set per pad up to
    MAX_PADS) because juce::AudioProcessorValueTreeState must know every
    parameter at construction time. The Grid Rows / Grid Columns parameters only
    decide how many of those pads are visible and active at any moment.
*/
namespace Config
{
    static constexpr int maxRows          = 8;
    static constexpr int maxCols          = 8;
    static constexpr int maxPads          = maxRows * maxCols;   // 64
    static constexpr int numVoices        = 32;                  // polyphony
    static constexpr int numOutputBuses   = 8;                   // 8 stereo pairs

    static constexpr int defaultRows      = 2;
    static constexpr int defaultCols      = 4;

    // Per-pad trigger behaviour.
    enum PlayMode { modeMomentary = 0, modeLatching = 1, modeLooping = 2 };

    // Order must match the PlayMode enum above.
    inline juce::StringArray playModeChoices() { return { "Momentary", "Latching", "Looping" }; }

    // Per-pad pitch/time algorithm.
    enum PitchMode { pitchRepitch = 0, pitchGranular = 1, pitchStudio = 2 };
    inline juce::StringArray pitchModeChoices() { return { "Repitch", "Granular", "Studio" }; }

    // Global LFOs (modulation).
    static constexpr int numLfos = 3;
    enum LfoShape  { lfoSine = 0, lfoTri, lfoSaw, lfoSquare, lfoSnH };
    enum LfoTarget { tgtOff = 0, tgtVolume, tgtPan, tgtPitch, tgtSpeed };
    enum LfoScope  { scopeAll = 0, scopeSelected };
    inline juce::StringArray lfoShapeChoices()  { return { "Sine", "Triangle", "Saw", "Square", "S&H" }; }
    inline juce::StringArray lfoTargetChoices() { return { "Off", "Volume", "Pan", "Pitch", "Speed" }; }
    inline juce::StringArray lfoScopeChoices()  { return { "All pads", "Selected" }; }
    inline juce::StringArray lfoSyncChoices()   { return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T" }; }

    inline double syncRateToHz (int idx, double bpm)
    {
        const double beatsPerCycle[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0 };
        const double bpc = beatsPerCycle[juce::jlimit (0, 6, idx)];
        return (bpm / 60.0) / juce::jmax (1.0e-6, bpc);
    }

    // A pleasant default colour spread across the pad pool.
    inline juce::Colour defaultPadColour (int padIndex)
    {
        return juce::Colour::fromHSV (std::fmod (0.11f * (float) padIndex, 1.0f),
                                      0.55f, 0.80f, 1.0f);
    }
}

//==============================================================================
/*  Central registry of parameter IDs. Per-pad IDs are suffixed with "_<pad>"
    so a single helper keeps the processor, engine and editor in perfect sync.
*/
namespace ParamID
{
    // Global ---------------------------------------------------------------
    static const juce::String gridRows { "gridRows" };
    static const juce::String gridCols { "gridCols" };

    // Per-LFO (global) -----------------------------------------------------
    inline juce::String lfoEnable  (int i) { return "lfoEn_"    + juce::String (i); }
    inline juce::String lfoShape   (int i) { return "lfoShape_" + juce::String (i); }
    inline juce::String lfoRate    (int i) { return "lfoRate_"  + juce::String (i); }
    inline juce::String lfoSync    (int i) { return "lfoSync_"  + juce::String (i); }
    inline juce::String lfoSyncRate(int i) { return "lfoSyncR_" + juce::String (i); }
    inline juce::String lfoDepth   (int i) { return "lfoDepth_" + juce::String (i); }
    inline juce::String lfoTarget  (int i) { return "lfoTgt_"   + juce::String (i); }
    inline juce::String lfoScope   (int i) { return "lfoScope_" + juce::String (i); }

    // Per-pad --------------------------------------------------------------
    inline juce::String gain     (int p) { return "gain_"     + juce::String (p); }
    inline juce::String pan      (int p) { return "pan_"      + juce::String (p); }
    inline juce::String pitch    (int p) { return "pitch_"    + juce::String (p); }
    inline juce::String attack   (int p) { return "attack_"   + juce::String (p); }
    inline juce::String release  (int p) { return "release_"  + juce::String (p); }
    inline juce::String speed    (int p) { return "speed_"    + juce::String (p); }
    inline juce::String start    (int p) { return "start_"    + juce::String (p); }
    inline juce::String end      (int p) { return "end_"      + juce::String (p); }
    inline juce::String playMode (int p) { return "mode_"     + juce::String (p); }
    inline juce::String pitchMode(int p) { return "pmode_"    + juce::String (p); }
    inline juce::String grainSize(int p) { return "grain_"    + juce::String (p); }

    // Inspector-knob "roles". A MIDI-CC mapping is stored per role (not per pad),
    // so one learned knob drives whichever pad is currently selected.
    enum KnobRole { roleGain = 0, rolePan, rolePitch, roleAttack,
                    roleRelease, roleSpeed, roleStart, roleEnd, numRoles };

    inline juce::String forRole (int role, int p)
    {
        switch (role)
        {
            case roleGain:    return gain (p);
            case rolePan:     return pan (p);
            case rolePitch:   return pitch (p);
            case roleAttack:  return attack (p);
            case roleRelease: return release (p);
            case roleSpeed:   return speed (p);
            case roleStart:   return start (p);
            case roleEnd:     return end (p);
            default:          return {};
        }
    }
}

//==============================================================================
/*  Builds the full APVTS parameter layout: two global parameters plus a fixed
    block of DSP parameters for every pad in the pool.
*/
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { ParamID::gridRows, 1 }, "Grid Rows",
        1, Config::maxRows, Config::defaultRows));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { ParamID::gridCols, 1 }, "Grid Columns",
        1, Config::maxCols, Config::defaultCols));

    for (int p = 0; p < Config::maxPads; ++p)
    {
        const String tag = " (Pad " + String (p + 1) + ")";

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::gain (p), 1 }, "Volume" + tag,
            NormalisableRange<float> (0.0f, 2.0f, 0.0001f, 0.5f), 1.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::pan (p), 1 }, "Pan" + tag,
            NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::pitch (p), 1 }, "Pitch" + tag,
            NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::attack (p), 1 }, "Attack" + tag,
            NormalisableRange<float> (0.0f, 2000.0f, 0.1f, 0.35f), 1.0f));   // ms

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::release (p), 1 }, "Release" + tag,
            NormalisableRange<float> (1.0f, 5000.0f, 0.1f, 0.35f), 40.0f));  // ms

        // Playback speed / time-stretch (independent of pitch). 1.0 = original.
        NormalisableRange<float> speedRange (0.25f, 4.0f, 0.0001f);
        speedRange.setSkewForCentre (1.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::speed (p), 1 }, "Speed" + tag, speedRange, 1.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::start (p), 1 }, "Sample Start" + tag,
            NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::end (p), 1 }, "Sample End" + tag,
            NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 1.0f));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamID::playMode (p), 1 }, "Play Mode" + tag,
            Config::playModeChoices(), Config::modeLatching));   // tap-to-play by default

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamID::pitchMode (p), 1 }, "Pitch Mode" + tag,
            Config::pitchModeChoices(), Config::pitchGranular));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::grainSize (p), 1 }, "Grain Size" + tag,
            NormalisableRange<float> (5.0f, 120.0f, 0.1f), 40.0f));   // ms (Granular mode)
    }

    for (int i = 0; i < Config::numLfos; ++i)
    {
        const String tag = " (LFO " + String (i + 1) + ")";

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ParamID::lfoEnable (i), 1 }, "Enable" + tag, false));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamID::lfoShape (i), 1 }, "Shape" + tag, Config::lfoShapeChoices(), 0));

        NormalisableRange<float> rate (0.01f, 30.0f, 0.0001f);
        rate.setSkewForCentre (2.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::lfoRate (i), 1 }, "Rate" + tag, rate, 1.0f));   // Hz

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { ParamID::lfoSync (i), 1 }, "Sync" + tag, false));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamID::lfoSyncRate (i), 1 }, "Sync Rate" + tag, Config::lfoSyncChoices(), 2));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { ParamID::lfoDepth (i), 1 }, "Depth" + tag,
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamID::lfoTarget (i), 1 }, "Target" + tag, Config::lfoTargetChoices(), 0));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { ParamID::lfoScope (i), 1 }, "Scope" + tag, Config::lfoScopeChoices(), 0));
    }

    return layout;
}
