#include "SamplerEngine.h"

//==============================================================================
// PadVoice
//==============================================================================
void PadVoice::startNote (int /*midiNote*/, float velocity,
                          juce::SynthesiserSound* sound, int /*pitchWheel*/)
{
    // Clear any stale activity registration before (re)starting.
    if (registeredPad >= 0)
    {
        engine.decrementActivity (registeredPad);
        registeredPad = -1;
    }

    currentSound = dynamic_cast<PadSound*> (sound);

    if (currentSound == nullptr || ! currentSound->hasSample())
    {
        finish();
        return;
    }

    currentPad    = currentSound->getPadIndex();
    noteVelocity  = velocity;
    lastSeenMode  = engine.playMode (currentPad);
    released      = false;
    chokeActive   = false;

    engine.incrementActivity (currentPad);
    engine.notePlayed (currentPad);
    registeredPad = currentPad;

    const auto numSamples = (double) currentSound->getData().getNumSamples();
    const double startPos = juce::jlimit (0.0, numSamples - 1.0,
                                          (double) engine.startNorm (currentPad) * numSamples);
    sourceSamplePosition = startPos;

    // Choose the playback path for this note. A transparent direct read is used
    // whenever no pitch/time change is requested (pristine for drum hits),
    // otherwise the pad's Pitch Mode decides: Repitch=direct, Granular, Studio.
    const bool noShift = (std::abs (engine.pitch (currentPad)) <= 0.01f)
                      && (std::abs (engine.speed (currentPad) - 1.0f) <= 0.001f);
    const int  pm      = engine.pitchModeOf (currentPad);
    pitchPath = (noShift || pm == Config::pitchRepitch) ? 0
              : (pm == Config::pitchStudio)             ? 2
                                                        : 1;

    grainSize = juce::jlimit (64, 1 << 15,
                              (int) (getSampleRate() * engine.grainSizeMs (currentPad) * 0.001));
    grainHop  = grainSize / 2;                                       // 50% overlap (Hann COLA)

    if (pitchPath == 2)
    {
        // Prime the Studio engine with the audio leading up to the start point so
        // its first output is aligned to the trim start (near-zero onset latency).
        stretch.reset();
        stretch.setTransposeSemitones (engine.pitch (currentPad));
        studioReadPos = startPos;
        studioEnded   = false;

        const double r0 = currentSound->getSourceRate() / getSampleRate();
        const int    N  = juce::jmin (studioIn.getNumSamples(), stretch.seekLength());
        auto* pL = studioIn.getWritePointer (0);
        auto* pR = studioIn.getWritePointer (1);
        for (int i = 0; i < N; ++i)
        {
            float l, r;
            interpolate (startPos + (double) (i - N) * r0, l, r);   // pre-roll ending at start
            pL[i] = l; pR[i] = r;
        }
        const float* inPtrs[2] = { pL, pR };
        stretch.seek (inPtrs, N, juce::jmax (0.01, (double) engine.speed (currentPad)));
    }
    for (auto& g : grains) g.active = false;
    grainAnchor  = startPos;
    hopCounter   = 0;            // spawn the first grain on the first sample
    stopSpawning = false;

    adsr.setSampleRate (getSampleRate());
    juce::ADSR::Parameters p;
    p.attack  = engine.attackSec (currentPad);
    p.decay   = 0.0f;
    p.sustain = 1.0f;
    p.release = engine.releaseSec (currentPad);
    adsr.setParameters (p);
    adsr.noteOn();

    chokeLevel.reset (getSampleRate(), 0.006);   // ~6 ms fade for chokes
    chokeLevel.setCurrentAndTargetValue (1.0f);

    // Start the gain smoothers AT their target so the ADSR owns the fade-in.
    const float pan0   = juce::jlimit (-1.0f, 1.0f, engine.pan (currentPad));
    const float theta0 = (pan0 * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
    const float g0     = engine.gain (currentPad) * noteVelocity;
    gainLSm.reset (getSampleRate(), 0.008);
    gainRSm.reset (getSampleRate(), 0.008);
    gainLSm.setCurrentAndTargetValue (g0 * std::cos (theta0));
    gainRSm.setCurrentAndTargetValue (g0 * std::sin (theta0));
}

void PadVoice::stopNote (float /*velocity*/, bool allowTailOff)
{
    if (! allowTailOff)
    {
        finish();               // hard stop (voice steal)
        return;
    }

    // Enter the release stage. For a looping voice this also breaks the loop so
    // it plays out to its trim end while the release envelope fades.
    released = true;
    adsr.noteOff();
}

void PadVoice::choke() noexcept
{
    chokeActive = true;
    released    = true;
    chokeLevel.setTargetValue (0.0f);
}

bool PadVoice::isSustaining() const noexcept
{
    return isVoiceActive() && currentPad >= 0
           && engine.playMode (currentPad) == Config::modeLooping && ! released;
}

// Reads the pad's current mode; if it just left Looping, release the voice so a
// loop stops immediately (not stuck), and reports whether it should loop now.
bool PadVoice::updateLoopState() noexcept
{
    const int mode = engine.playMode (currentPad);

    if (lastSeenMode == Config::modeLooping && mode != Config::modeLooping && ! released)
    {
        released = true;
        adsr.noteOff();
    }

    lastSeenMode = mode;
    return (mode == Config::modeLooping) && ! released;
}

void PadVoice::finish() noexcept
{
    if (registeredPad >= 0)
    {
        engine.decrementActivity (registeredPad);
        registeredPad = -1;
    }

    clearCurrentNote();
    adsr.reset();
    currentSound = nullptr;
    currentPad   = -1;
    released     = false;
    chokeActive  = false;
}

bool PadVoice::interpolate (double sourcePos, float& l, float& r) const noexcept
{
    const auto& data = currentSound->getData();
    const int numSrc = data.getNumSamples();

    if (sourcePos < 0.0 || sourcePos >= (double) numSrc)
    {
        l = r = 0.0f;
        return false;
    }

    const int numCh   = data.getNumChannels();
    const float* inL  = data.getReadPointer (0);
    const float* inR  = numCh > 1 ? data.getReadPointer (1) : inL;

    const int   pos  = (int) sourcePos;
    const float frac = (float) (sourcePos - pos);
    const int   nx   = (pos + 1 < numSrc) ? pos + 1 : pos;

    l = inL[pos] + frac * (inL[nx] - inL[pos]);
    r = inR[pos] + frac * (inR[nx] - inR[pos]);
    return true;
}

void PadVoice::spawnGrain (double anchor) noexcept
{
    for (auto& g : grains)
    {
        if (! g.active)
        {
            g.active = true;
            g.anchor = anchor;
            g.pos    = 0;
            return;
        }
    }
}

void PadVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (currentSound == nullptr)
        return;

    const auto& data = currentSound->getData();
    if (data.getNumSamples() == 0 || data.getNumChannels() == 0)
    {
        finish();
        return;
    }

    if (pitchPath == 0)       renderDirect   (output, startSample, numSamples);
    else if (pitchPath == 2)  renderStudio   (output, startSample, numSamples);
    else                      renderGranular (output, startSample, numSamples);
}

void PadVoice::renderDirect (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    const int    pad   = currentPad;
    const int    numSrc = currentSound->getData().getNumSamples();
    const double r0    = currentSound->getSourceRate() / getSampleRate();
    // Repitch / tape: pitch and speed both scale the read rate (couples duration+pitch).
    const double ratio = r0 * (double) engine.speedMod (pad) * std::pow (2.0, engine.pitchMod (pad) / 12.0);
    const bool   loop  = updateLoopState();

    double sp = juce::jlimit (0.0, (double) numSrc, (double) engine.startNorm (pad) * numSrc);
    double ep = juce::jlimit (0.0, (double) numSrc, (double) engine.endNorm   (pad) * numSrc);
    if (ep <= sp + 1.0) ep = (double) numSrc;

    const float pan   = engine.panMod (pad);
    const float theta = (pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
    const float gBase = engine.gainMod (pad) * noteVelocity;
    gainLSm.setTargetValue (gBase * std::cos (theta));
    gainRSm.setTargetValue (gBase * std::sin (theta));

    int chL, chR;
    engine.getOutputChannels (pad, chL, chR);
    const int outCh = output.getNumChannels();
    float* oL = (chL >= 0 && chL < outCh) ? output.getWritePointer (chL) : nullptr;
    float* oR = (chR >= 0 && chR < outCh) ? output.getWritePointer (chR) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float l, r;
        if (! interpolate (sourceSamplePosition, l, r))
        {
            finish();
            return;
        }

        const float amp = adsr.getNextSample() * chokeLevel.getNextValue();
        const float gl  = gainLSm.getNextValue();
        const float gr  = gainRSm.getNextValue();
        const int   idx = startSample + i;
        if (oL != nullptr) oL[idx] += l * gl * amp;
        if (oR != nullptr) oR[idx] += r * gr * amp;

        sourceSamplePosition += ratio;

        if (sourceSamplePosition >= ep)
        {
            if (loop) sourceSamplePosition -= (ep - sp);
            else { finish(); return; }
        }

        if (chokeActive && ! chokeLevel.isSmoothing()) { finish(); return; }
        if (! adsr.isActive())                          { finish(); return; }
    }

    engine.setPlayPosition (pad, (float) (sourceSamplePosition / (double) juce::jmax (1, numSrc)));
}

void PadVoice::renderGranular (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    const int pad    = currentPad;
    const int numSrc = currentSound->getData().getNumSamples();

    // Independent pitch (grain resample rate) and time (grain-anchor advance).
    const double r0        = currentSound->getSourceRate() / getSampleRate();
    const double pitchStep = std::pow (2.0, engine.pitchMod (pad) / 12.0) * r0;
    const double advance   = (double) grainHop * (double) engine.speedMod (pad) * r0;
    const bool   loop      = updateLoopState();
    const double invGrain  = 1.0 / (double) grainSize;

    double sp = juce::jlimit (0.0, (double) numSrc, (double) engine.startNorm (pad) * numSrc);
    double ep = juce::jlimit (0.0, (double) numSrc, (double) engine.endNorm   (pad) * numSrc);
    if (ep <= sp + 1.0) ep = (double) numSrc;

    const float pan   = engine.panMod (pad);
    const float theta = (pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
    const float gBase = engine.gainMod (pad) * noteVelocity;
    gainLSm.setTargetValue (gBase * std::cos (theta));
    gainRSm.setTargetValue (gBase * std::sin (theta));

    int chL, chR;
    engine.getOutputChannels (pad, chL, chR);
    const int outCh = output.getNumChannels();
    float* oL = (chL >= 0 && chL < outCh) ? output.getWritePointer (chL) : nullptr;
    float* oR = (chR >= 0 && chR < outCh) ? output.getWritePointer (chR) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float accL = 0.0f, accR = 0.0f;

        for (auto& g : grains)
        {
            if (! g.active)
                continue;

            const double phase = (double) g.pos * invGrain;                 // 0 .. 1
            const float  win   = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) phase);

            float sL, sR;
            interpolate (g.anchor + (double) g.pos * pitchStep, sL, sR);
            accL += win * sL;
            accR += win * sR;

            if (++g.pos >= grainSize)
                g.active = false;
        }

        const float amp = adsr.getNextSample() * chokeLevel.getNextValue();
        const float gl  = gainLSm.getNextValue();
        const float gr  = gainRSm.getNextValue();
        const int   idx = startSample + i;
        if (oL != nullptr) oL[idx] += accL * gl * amp;
        if (oR != nullptr) oR[idx] += accR * gr * amp;

        // Grain scheduler: fire a new grain every hop, advancing the time base.
        if (--hopCounter <= 0)
        {
            if (! stopSpawning)
                spawnGrain (grainAnchor);

            hopCounter  = grainHop;
            grainAnchor += advance;

            if (grainAnchor >= ep)
            {
                if (loop && (ep - sp) > 0.0)
                    while (grainAnchor >= ep) grainAnchor -= (ep - sp);   // wrap (handles big hops)
                else if (! loop)
                    stopSpawning = true;
            }
        }

        if (stopSpawning)
        {
            bool anyActive = false;
            for (auto& g : grains) if (g.active) { anyActive = true; break; }
            if (! anyActive) { finish(); return; }
        }

        if (chokeActive && ! chokeLevel.isSmoothing()) { finish(); return; }
        if (! adsr.isActive())                          { finish(); return; }
    }

    engine.setPlayPosition (pad, (float) (grainAnchor / (double) juce::jmax (1, numSrc)));
}

void PadVoice::prepareStudio (double sr, int maxBlock)
{
    stretch.presetDefault (2, (float) sr);
    const int cap = juce::jmax (maxBlock * 4, stretch.seekLength()) + 8;
    studioIn.setSize  (2, cap);
    studioOut.setSize (2, juce::jmax (16, maxBlock));
}

void PadVoice::renderStudio (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    const int    pad    = currentPad;
    const int    numSrc = currentSound->getData().getNumSamples();
    const double r0     = currentSound->getSourceRate() / getSampleRate();
    const bool   loop   = updateLoopState();
    const double speed  = juce::jmax (0.01, (double) engine.speedMod (pad));

    double sp = juce::jlimit (0.0, (double) numSrc, (double) engine.startNorm (pad) * numSrc);
    double ep = juce::jlimit (0.0, (double) numSrc, (double) engine.endNorm   (pad) * numSrc);
    if (ep <= sp + 1.0) ep = (double) numSrc;

    stretch.setTransposeSemitones (engine.pitchMod (pad));   // live pitch (independent of time)

    const int capIn = studioIn.getNumSamples();
    const int nIn   = juce::jlimit (1, capIn, (int) std::llround ((double) numSamples * speed));

    // Fill the input block from the source at the native-rate read head.
    float* inL = studioIn.getWritePointer (0);
    float* inR = studioIn.getWritePointer (1);
    for (int i = 0; i < nIn; ++i)
    {
        double pos = studioReadPos + (double) i * r0;
        if (loop && ep > sp)
            pos = sp + std::fmod (pos - sp, ep - sp);
        else if (! loop && pos >= ep)
            { inL[i] = inR[i] = 0.0f; studioEnded = true; continue; }

        float l, r;
        interpolate (pos, l, r);
        inL[i] = l; inR[i] = r;
    }

    studioReadPos += (double) nIn * r0;
    if (loop && ep > sp)
        studioReadPos = sp + std::fmod (studioReadPos - sp, ep - sp);

    // When a one-shot runs out of source, fade its tail via the release stage.
    if (studioEnded && ! released)
    {
        released = true;
        adsr.noteOff();
    }

    const float* inPtrs[2]  = { inL, inR };
    float*       outPtrs[2] = { studioOut.getWritePointer (0), studioOut.getWritePointer (1) };
    stretch.process (inPtrs, nIn, outPtrs, numSamples);

    const float pan   = engine.panMod (pad);
    const float theta = (pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
    const float gBase = engine.gainMod (pad) * noteVelocity;
    gainLSm.setTargetValue (gBase * std::cos (theta));
    gainRSm.setTargetValue (gBase * std::sin (theta));

    int chL, chR;
    engine.getOutputChannels (pad, chL, chR);
    const int outCh = output.getNumChannels();
    float* oL = (chL >= 0 && chL < outCh) ? output.getWritePointer (chL) : nullptr;
    float* oR = (chR >= 0 && chR < outCh) ? output.getWritePointer (chR) : nullptr;

    const float* sL = studioOut.getReadPointer (0);
    const float* sR = studioOut.getReadPointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float amp = adsr.getNextSample() * chokeLevel.getNextValue();
        const float gl  = gainLSm.getNextValue();
        const float gr  = gainRSm.getNextValue();
        const int   idx = startSample + i;
        if (oL != nullptr) oL[idx] += sL[i] * gl * amp;
        if (oR != nullptr) oR[idx] += sR[i] * gr * amp;

        if (chokeActive && ! chokeLevel.isSmoothing()) { finish(); return; }
        if (! adsr.isActive())                          { finish(); return; }
    }

    engine.setPlayPosition (pad, (float) (studioReadPos / (double) juce::jmax (1, numSrc)));
}

//==============================================================================
// PadSynthesiser
//==============================================================================
PadSound* PadSynthesiser::findSoundForPad (int padIndex) const
{
    for (auto* s : sounds)
        if (auto* ps = dynamic_cast<PadSound*> (s))
            if (ps->getPadIndex() == padIndex)
                return ps;

    return nullptr;
}

bool PadSynthesiser::isPadSustainingLocked (int padIndex) const
{
    for (auto* v : voices)
        if (auto* pv = dynamic_cast<PadVoice*> (v))
            if (pv->getCurrentPad() == padIndex && pv->isSustaining())
                return true;

    return false;
}

void PadSynthesiser::stopPadLocked (int padIndex)
{
    for (auto* v : voices)
        if (auto* pv = dynamic_cast<PadVoice*> (v))
            if (pv->isVoiceActive() && pv->getCurrentPad() == padIndex)
                pv->stopNote (0.0f, true);
}

void PadSynthesiser::startPadVoiceLocked (int padIndex, float velocity)
{
    auto* sound = findSoundForPad (padIndex);
    if (sound == nullptr)
        return;

    // Looping pads are strictly one voice per pad: fast-fade any voice already
    // on this pad so loops can never stack up.
    if (engine != nullptr && engine->playMode (padIndex) == Config::modeLooping)
        for (auto* v : voices)
            if (auto* pv = dynamic_cast<PadVoice*> (v))
                if (pv->isVoiceActive() && pv->getCurrentPad() == padIndex)
                    pv->choke();

    const int note = (engine != nullptr && engine->getMappedNote (padIndex) >= 0)
                        ? engine->getMappedNote (padIndex) : 60;

    if (auto* voice = findFreeVoice (sound, 1, note, isNoteStealingEnabled()))
        startVoice (voice, sound, 1, note, velocity);
}

void PadSynthesiser::pressPadLocked (int padIndex, float velocity)
{
    const int mode = engine != nullptr ? engine->playMode (padIndex) : Config::modeLatching;

    // A tap on an already-looping pad toggles it off.
    if (mode == Config::modeLooping && isPadSustainingLocked (padIndex))
    {
        stopPadLocked (padIndex);
        return;
    }

    startPadVoiceLocked (padIndex, velocity);
}

//==============================================================================
void PadSynthesiser::noteOn (int midiChannel, int midiNoteNumber, float velocity)
{
    const juce::ScopedLock sl (lock);

    for (auto* sound : sounds)
    {
        auto* ps = dynamic_cast<PadSound*> (sound);
        if (ps == nullptr || ! ps->appliesToNote (midiNoteNumber) || ! ps->appliesToChannel (midiChannel))
            continue;

        pressPadLocked (ps->getPadIndex(), velocity);
    }
}

void PadSynthesiser::noteOff (int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff)
{
    const juce::ScopedLock sl (lock);

    // Only momentary (gate) pads respond to note-off; latching and looping pads
    // ignore it and are stopped by a subsequent tap instead.
    for (auto* v : voices)
    {
        if (auto* pv = dynamic_cast<PadVoice*> (v))
        {
            const int pad = pv->getCurrentPad();
            if (pv->isVoiceActive() && pad >= 0
                && pv->getCurrentlyPlayingNote() == midiNoteNumber
                && pv->isPlayingChannel (midiChannel)
                && engine != nullptr && engine->playMode (pad) == Config::modeMomentary)
            {
                pv->stopNote (velocity, allowTailOff);
            }
        }
    }
}

//==============================================================================
void PadSynthesiser::pressPad (int padIndex, float velocity)
{
    const juce::ScopedLock sl (lock);
    pressPadLocked (padIndex, velocity);
}

void PadSynthesiser::stopPad (int padIndex)
{
    const juce::ScopedLock sl (lock);
    stopPadLocked (padIndex);
}

bool PadSynthesiser::isPadSustaining (int padIndex)
{
    const juce::ScopedLock sl (lock);
    return isPadSustainingLocked (padIndex);
}

//==============================================================================
// SamplerEngine
//==============================================================================
SamplerEngine::SamplerEngine()
{
    for (int p = 0; p < Config::maxPads; ++p)
    {
        mappedNote[p].store (-1);
        outputBus[p].store (0);
        activity[p].store (0);
        playPosition[p].store (0.0f);
        sampleLoaded[p].store (false);
        ledBright[p].store (0);
        ledDim[p].store (0);
    }

    for (int b = 0; b < Config::numOutputBuses; ++b)
        busChannelOffset[b] = (b == 0 ? 0 : -1);

    synth.setEngine (this);
    synth.setNoteStealingEnabled (true);
}

void SamplerEngine::cacheParameterPointers (juce::AudioProcessorValueTreeState& apvts)
{
    for (int p = 0; p < Config::maxPads; ++p)
    {
        gainP   [p] = apvts.getRawParameterValue (ParamID::gain (p));
        panP    [p] = apvts.getRawParameterValue (ParamID::pan (p));
        pitchP  [p] = apvts.getRawParameterValue (ParamID::pitch (p));
        attackP [p] = apvts.getRawParameterValue (ParamID::attack (p));
        releaseP[p] = apvts.getRawParameterValue (ParamID::release (p));
        speedP  [p] = apvts.getRawParameterValue (ParamID::speed (p));
        startP  [p] = apvts.getRawParameterValue (ParamID::start (p));
        endP    [p] = apvts.getRawParameterValue (ParamID::end (p));
        modeP   [p] = apvts.getRawParameterValue (ParamID::playMode (p));
        pitchModeP[p] = apvts.getRawParameterValue (ParamID::pitchMode (p));
        grainSizeP[p] = apvts.getRawParameterValue (ParamID::grainSize (p));
    }

    for (int i = 0; i < Config::numLfos; ++i)
    {
        lfoEnableP[i] = apvts.getRawParameterValue (ParamID::lfoEnable (i));
        lfoShapeP [i] = apvts.getRawParameterValue (ParamID::lfoShape (i));
        lfoRateP  [i] = apvts.getRawParameterValue (ParamID::lfoRate (i));
        lfoSyncP  [i] = apvts.getRawParameterValue (ParamID::lfoSync (i));
        lfoSyncRtP[i] = apvts.getRawParameterValue (ParamID::lfoSyncRate (i));
        lfoDepthP [i] = apvts.getRawParameterValue (ParamID::lfoDepth (i));
        lfoTargetP[i] = apvts.getRawParameterValue (ParamID::lfoTarget (i));
        lfoScopeP [i] = apvts.getRawParameterValue (ParamID::lfoScope (i));
    }
}

void SamplerEngine::advanceLfos (int blockSamples) noexcept
{
    for (int i = 0; i < Config::numLfos; ++i)
    {
        if (load (lfoEnableP, i, 0.0f) < 0.5f)
        {
            lfoCurrent[i] = 0.0f;
            continue;
        }

        const bool   sync = load (lfoSyncP, i, 0.0f) > 0.5f;
        const double rate = sync ? Config::syncRateToHz ((int) load (lfoSyncRtP, i, 2.0f), tempoBpm)
                                 : (double) load (lfoRateP, i, 1.0f);

        double phase = lfoPhase[i] + rate * (double) blockSamples / currentSampleRate;
        const bool wrapped = phase >= 1.0;
        phase -= std::floor (phase);
        lfoPhase[i] = phase;

        const int shape = (int) load (lfoShapeP, i, 0.0f);
        float v = 0.0f;
        switch (shape)
        {
            case Config::lfoSine:   v = std::sin (juce::MathConstants<float>::twoPi * (float) phase); break;
            case Config::lfoTri:    v = (phase < 0.5 ? (float) (-1.0 + 4.0 * phase) : (float) (3.0 - 4.0 * phase)); break;
            case Config::lfoSaw:    v = (float) (2.0 * phase - 1.0); break;
            case Config::lfoSquare: v = (phase < 0.5 ? 1.0f : -1.0f); break;
            case Config::lfoSnH:
                if (wrapped) snhValue[i] = lfoRandom.nextFloat() * 2.0f - 1.0f;
                v = snhValue[i];
                break;
            default: break;
        }

        lfoCurrent[i] = v * load (lfoDepthP, i, 0.5f);
    }
}

void SamplerEngine::createSoundsAndVoices()
{
    for (int p = 0; p < Config::maxPads; ++p)
    {
        auto* sound = new PadSound (p, mappedNote[p]);
        padSounds[p] = sound;
        ownedSounds.add (sound);
        synth.addSound (sound);
    }

    for (int v = 0; v < Config::numVoices; ++v)
        synth.addVoice (new PadVoice (*this));
}

void SamplerEngine::prepare (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* pv = dynamic_cast<PadVoice*> (synth.getVoice (i)))
            pv->prepareStudio (sampleRate, juce::jmax (16, samplesPerBlock));
}

void SamplerEngine::loadSample (int padIndex, juce::AudioBuffer<float>&& data, double sr)
{
    if (padIndex < 0 || padIndex >= Config::maxPads || padSounds[padIndex] == nullptr)
        return;

    // Swap the buffer under the render lock: the (rare) allocation of the new
    // buffer and the free of the old one both happen on this message thread.
    {
        const juce::ScopedLock sl (synth.getRenderLock());
        padSounds[padIndex]->setSample (std::move (data), sr);
    }
    sampleLoaded[padIndex].store (true);
}

void SamplerEngine::clearSample (int padIndex)
{
    if (padIndex < 0 || padIndex >= Config::maxPads || padSounds[padIndex] == nullptr)
        return;

    {
        const juce::ScopedLock sl (synth.getRenderLock());
        padSounds[padIndex]->clearSample();
    }
    sampleLoaded[padIndex].store (false);
}
