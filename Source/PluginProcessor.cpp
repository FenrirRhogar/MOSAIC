#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Identifier kMetaType   { "PADS_META" };
    const juce::Identifier kPadType    { "PAD" };
    const juce::Identifier kIndexProp  { "index" };
    const juce::Identifier kFileProp   { "file" };
    const juce::Identifier kColourProp { "colour" };
    const juce::Identifier kNoteProp   { "note" };
    const juce::Identifier kBusProp    { "outbus" };

    const juce::Identifier kCCMapType  { "CCMAP" };
    juce::Identifier ccPropForNumber (int cc) { return juce::Identifier ("cc" + juce::String (cc)); }

    // Approximate Launchpad X / Mini MK3 programmer-mode velocity palette. Each
    // entry pairs a representative RGB with a "bright" and "dim" velocity so an
    // arbitrary pad colour maps to the nearest colour the controller can show.
    // (Tuned for the current Launchpad line; adjust the indices for other kit.)
    struct LpColour { int r, g, b, bright, dim; };
    const LpColour lpPalette[] =
    {
        { 255, 255, 255,  3,  1 },   // white / grey
        { 255,   0,   0,  5,  7 },   // red
        { 255,  85,   0,  9, 11 },   // orange
        { 255, 255,   0, 13, 15 },   // yellow
        {   0, 255,   0, 21, 23 },   // green
        {   0, 255, 128, 33, 35 },   // spring
        {   0, 255, 255, 37, 39 },   // cyan
        {   0,   0, 255, 45, 47 },   // blue
        { 128,   0, 255, 49, 51 },   // purple
        { 255,   0, 255, 53, 55 },   // magenta
        { 255,   0, 128, 57, 59 },   // pink
    };

    int velocityForColour (juce::Colour c, bool bright)
    {
        const int r = c.getRed(), g = c.getGreen(), b = c.getBlue();
        int  bestIdx  = 0;
        long bestDist = 200000;   // > max squared RGB distance (3 * 255^2)

        for (int i = 0; i < (int) juce::numElementsInArray (lpPalette); ++i)
        {
            const long dr = r - lpPalette[i].r, dg = g - lpPalette[i].g, db = b - lpPalette[i].b;
            const long dist = dr * dr + dg * dg + db * db;
            if (dist < bestDist) { bestDist = dist; bestIdx = i; }
        }

        return bright ? lpPalette[bestIdx].bright : lpPalette[bestIdx].dim;
    }
}

//==============================================================================
juce::AudioProcessor::BusesProperties MosaicAudioProcessor::makeBusesProperties()
{
    BusesProperties props;

    for (int b = 0; b < Config::numOutputBuses; ++b)
    {
        const auto name = "Output " + juce::String (b * 2 + 1) + "-" + juce::String (b * 2 + 2);
        props = props.withOutput (name, juce::AudioChannelSet::stereo(), b == 0);   // only main enabled by default
    }

    return props;
}

//==============================================================================
MosaicAudioProcessor::MosaicAudioProcessor()
    : AudioProcessor (makeBusesProperties()),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();   // wav, aiff, mp3 (where supported), etc.

    for (auto& slot : ccToRole)
        slot.store (-1);

    for (int p = 0; p < Config::maxPads; ++p)
    {
        lastLedVel[p]  = -2;   // nothing sent yet
        lastLedNote[p] = -1;
    }

    // Cache parameter pointers for each (role, pad) so CC handling never builds
    // strings or does lookups on the audio thread.
    for (int role = 0; role < ParamID::numRoles; ++role)
        for (int p = 0; p < Config::maxPads; ++p)
            roleParams[role][p] = apvts.getParameter (ParamID::forRole (role, p));

    buildDefaultMetaTree();
    apvts.state.appendChild (metaTree, nullptr);
    metaTree.addListener (this);

    ccMapTree = juce::ValueTree (kCCMapType);
    apvts.state.appendChild (ccMapTree, nullptr);

    engine.cacheParameterPointers (apvts);
    engine.createSoundsAndVoices();

    syncEngineFromMeta();
    markKitClean();
    markControllerClean();
    startTimerHz (100);
}

MosaicAudioProcessor::~MosaicAudioProcessor()
{
    stopTimer();
    metaTree.removeListener (this);
}

//==============================================================================
void MosaicAudioProcessor::buildDefaultMetaTree()
{
    metaTree = juce::ValueTree (kMetaType);

    for (int p = 0; p < Config::maxPads; ++p)
    {
        juce::ValueTree pad (kPadType);
        pad.setProperty (kIndexProp,  p, nullptr);
        pad.setProperty (kFileProp,   juce::String(), nullptr);
        pad.setProperty (kColourProp, (int) Config::defaultPadColour (p).getARGB(), nullptr);
        pad.setProperty (kNoteProp,   -1, nullptr);
        pad.setProperty (kBusProp,    0, nullptr);
        metaTree.appendChild (pad, nullptr);
    }
}

juce::ValueTree MosaicAudioProcessor::padMeta (int padIndex)
{
    return metaTree.getChildWithProperty (kIndexProp, padIndex);
}

juce::ValueTree MosaicAudioProcessor::padMeta (int padIndex) const
{
    return metaTree.getChildWithProperty (kIndexProp, padIndex);
}

//==============================================================================
void MosaicAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

bool MosaicAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // No inputs (pure instrument). Main output must be stereo; every other
    // output bus must be stereo or disabled.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    for (const auto& set : layouts.outputBuses)
        if (set != juce::AudioChannelSet::stereo() && set != juce::AudioChannelSet::disabled())
            return false;

    return true;
}

void MosaicAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                   juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear every output channel; the synth adds into the buffer.
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Map each logical output bus to its absolute channel offset in the buffer.
    int offset = 0;
    const int busCount = getBusCount (false);
    for (int b = 0; b < Config::numOutputBuses; ++b)
    {
        if (b < busCount && getBus (false, b)->isEnabled())
        {
            engine.setBusOffset (b, offset);
            offset += getBus (false, b)->getNumberOfChannels();
        }
        else
        {
            engine.setBusOffset (b, -1);
        }
    }

    // MIDI note-learn: capture the first NoteOn while a pad is listening.
    if (midiLearnTarget.load() >= 0)
    {
        for (const auto meta : midiMessages)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                pendingLearnPad.store (midiLearnTarget.load());
                pendingLearnNote.store (m.getNoteNumber());
                midiLearnTarget.store (-1);
                break;
            }
        }
    }

    // MIDI-CC: learn a role mapping if one is pending, otherwise remember the value for the
    // message thread, which applies it to the selected pad's parameter (servicePendingMidi).
    int learnRole = knobLearnRole.load();
    for (const auto meta : midiMessages)
    {
        const auto m = meta.getMessage();
        if (! m.isController())
            continue;

        const int   cc  = m.getControllerNumber();
        const float val = (float) m.getControllerValue() / 127.0f;

        if (learnRole >= 0)
        {
            ccToRole[cc].store (learnRole);
            pendingLearnCC.store (cc);
            pendingLearnRole.store (learnRole);
            knobLearnRole.store (-1);
            learnRole = -1;
        }
        else if (const int role = ccToRole[cc].load(); role >= 0)
        {
            pendingCcValue[role].store (val);
            pendingCcPad[role].store (activePad.load());
            pendingCcDirty[role].store (true);
        }
    }

    // Advance the LFO bank once per block (tempo from the host when synced).
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm())
                bpm = *hostBpm;
    engine.setTempo (bpm);
    engine.advanceLfos (buffer.getNumSamples());

    engine.process (buffer, midiMessages);

    // Replace the MIDI output with hardware-LED feedback: incoming notes are not
    // echoed back to the controller, only our per-pad colour updates.
    midiMessages.clear();
    sendLedFeedback (midiMessages);
}

void MosaicAudioProcessor::sendLedFeedback (juce::MidiBuffer& out)
{
    // A whole 8x8 grid lighting up would be 128 events in one block, and the host owns this
    // buffer, so growing it could allocate. Send a few per block; the rest follows next block.
    int budget = maxLedEventsPerBlock;
    const auto send = [&] (int note, int velocity)
    {
        out.addEvent (juce::MidiMessage::noteOn (ledChannel, note, (juce::uint8) velocity), 0);
        --budget;
    };

    for (int p = 0; p < Config::maxPads && budget > 0; ++p)
    {
        const int note = engine.getMappedNote (p);

        if (note < 0 || note > 127)
        {
            // Pad no longer addressable — extinguish whatever LED it last lit.
            if (lastLedNote[p] >= 0 && lastLedNote[p] <= 127)
                send (lastLedNote[p], 0);
            lastLedNote[p] = -1;
            lastLedVel[p]  = -2;
            continue;
        }

        int vel;
        if      (! engine.hasSampleLoaded (p)) vel = 0;                      // empty → off
        else if (engine.isPadActive (p))       vel = engine.getLedBright (p); // playing → bright
        else                                   vel = engine.getLedDim (p);    // idle    → dim

        if (note != lastLedNote[p])
        {
            if (lastLedNote[p] >= 0 && lastLedNote[p] <= 127)
                send (lastLedNote[p], 0);
            lastLedNote[p] = note;
            lastLedVel[p]  = -2;   // force a resend on the new note
        }

        if (vel != lastLedVel[p])
        {
            send (note, vel);
            lastLedVel[p] = vel;
        }
    }
}

//==============================================================================
void MosaicAudioProcessor::servicePendingMidi()
{
    const int pad  = pendingLearnPad.exchange (-1);
    const int note = pendingLearnNote.exchange (-1);

    if (pad >= 0 && note >= 0)
    {
        auto m = padMeta (pad);
        if (m.isValid())
            m.setProperty (kNoteProp, note, nullptr);   // → valueTreePropertyChanged → engine + UI
    }

    // Finalise a MIDI-CC learn: enforce one CC per role, then persist + notify.
    const int cc   = pendingLearnCC.exchange (-1);
    const int role = pendingLearnRole.exchange (-1);

    if (cc >= 0 && role >= 0)
    {
        for (int c = 0; c < 128; ++c)
            if (c != cc && ccToRole[c].load() == role)
                ccToRole[c].store (-1);

        writeCCMapToTree();
        sendChangeMessage();
    }

    // CC values the audio thread collected: drive the pad that was selected when each arrived.
    for (int r = 0; r < ParamID::numRoles; ++r)
    {
        if (! pendingCcDirty[r].exchange (false))
            continue;

        const int target = juce::jlimit (0, Config::maxPads - 1, pendingCcPad[r].load());
        if (auto* p = roleParams[r][target])
        {
            // Start stays before End (both are 0-1, so normalised values compare directly).
            float v = pendingCcValue[r].load();
            if (r == ParamID::roleStart)
                v = juce::jmin (v, roleParams[ParamID::roleEnd][target]->getValue() - minTrimGap);
            else if (r == ParamID::roleEnd)
                v = juce::jmax (v, roleParams[ParamID::roleStart][target]->getValue() + minTrimGap);
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
        }
    }
}

//==============================================================================
void MosaicAudioProcessor::valueTreePropertyChanged (juce::ValueTree& tree,
                                                              const juce::Identifier& prop)
{
    if (tree.hasType (kPadType))
    {
        const int pad = (int) tree.getProperty (kIndexProp, -1);
        if (pad >= 0 && pad < Config::maxPads)
        {
            if (prop == kNoteProp)       engine.setMappedNote (pad, (int) tree.getProperty (kNoteProp, -1));
            else if (prop == kBusProp)   engine.setOutputBus (pad, (int) tree.getProperty (kBusProp, 0));
            else if (prop == kColourProp) updatePadLedColour (pad);
        }
    }

    sendChangeMessage();   // let the editor refresh visuals
}

//==============================================================================
std::optional<MosaicAudioProcessor::DecodedSample> MosaicAudioProcessor::decodeSample (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return std::nullopt;

    const int numCh   = (int) reader->numChannels;
    const int lengthN = (int) reader->lengthInSamples;

    DecodedSample decoded { juce::AudioBuffer<float> (juce::jmax (1, numCh), lengthN), reader->sampleRate };
    reader->read (&decoded.buffer, 0, lengthN, 0, true, true);
    return decoded;
}

void MosaicAudioProcessor::applyDecodedSample (int padIndex, const juce::File& file, DecodedSample&& decoded)
{
    engine.loadSample (padIndex, std::move (decoded.buffer), decoded.sampleRate);
    sampleMissing[padIndex].store (false);

    if (auto m = padMeta (padIndex); m.isValid())
        m.setProperty (kFileProp, file.getFullPathName(), nullptr);
}

bool MosaicAudioProcessor::setPadSample (int padIndex, const juce::File& file)
{
    if (padIndex < 0 || padIndex >= Config::maxPads)
        return false;

    ++loadGeneration[padIndex];   // supersedes any background load still running
    auto decoded = decodeSample (file);
    if (! decoded)
        return false;

    applyDecodedSample (padIndex, file, std::move (*decoded));
    return true;
}

void MosaicAudioProcessor::requestPadSample (int padIndex, const juce::File& file)
{
    if (padIndex < 0 || padIndex >= Config::maxPads)
        return;

    const int generation = ++loadGeneration[padIndex];
    juce::WeakReference<MosaicAudioProcessor> self (this);

    sampleLoader.addJob ([this, self, padIndex, file, generation]
    {
        auto decoded = decodeSample (file);   // the slow part, off the message thread
        if (! decoded)
            return;

        juce::MessageManager::callAsync ([self, padIndex, file, generation, decoded = std::move (*decoded)]() mutable
        {
            // Skip results that a newer load or a clear has already superseded.
            if (self == nullptr || self->loadGeneration[padIndex].load() != generation)
                return;
            self->applyDecodedSample (padIndex, file, std::move (decoded));
        });
    });
}

bool MosaicAudioProcessor::isPadSampleMissing (int padIndex) const noexcept
{
    return padIndex >= 0 && padIndex < Config::maxPads && sampleMissing[padIndex].load();
}

int MosaicAudioProcessor::getNumMissingSamples() const noexcept
{
    int count = 0;
    for (const auto& missing : sampleMissing)
        count += missing.load() ? 1 : 0;
    return count;
}

int MosaicAudioProcessor::relinkMissingSamples (const juce::File& folder)
{
    int found = 0;
    for (int p = 0; p < Config::maxPads; ++p)
    {
        if (! sampleMissing[p].load())
            continue;

        const auto name = juce::File (getPadFilePath (p)).getFileName();
        const auto matches = folder.findChildFiles (juce::File::findFiles, true, name);
        if (! matches.isEmpty() && setPadSample (p, matches.getFirst()))
            ++found;
    }

    sendChangeMessage();
    return found;
}

void MosaicAudioProcessor::clearPad (int padIndex)
{
    if (padIndex < 0 || padIndex >= Config::maxPads)
        return;

    ++loadGeneration[padIndex];   // cancels a background load still in flight
    sampleMissing[padIndex].store (false);
    engine.clearSample (padIndex);

    if (auto m = padMeta (padIndex); m.isValid())
        m.setProperty (kFileProp, juce::String(), nullptr);
}

void MosaicAudioProcessor::setPadColour (int padIndex, juce::Colour colour)
{
    if (auto m = padMeta (padIndex); m.isValid())
        m.setProperty (kColourProp, (int) colour.getARGB(), nullptr);
}

void MosaicAudioProcessor::setPadOutputBus (int padIndex, int bus)
{
    if (auto m = padMeta (padIndex); m.isValid())
        m.setProperty (kBusProp, juce::jlimit (0, Config::numOutputBuses - 1, bus), nullptr);
}

juce::Colour MosaicAudioProcessor::getPadColour (int padIndex) const
{
    if (auto m = padMeta (padIndex); m.isValid())
        return juce::Colour ((juce::uint32) (int) m.getProperty (kColourProp,
                                                                  (int) Config::defaultPadColour (padIndex).getARGB()));
    return Config::defaultPadColour (padIndex);
}

juce::String MosaicAudioProcessor::getPadFilePath (int padIndex) const
{
    if (auto m = padMeta (padIndex); m.isValid())
        return m.getProperty (kFileProp, juce::String()).toString();
    return {};
}

//==============================================================================
void MosaicAudioProcessor::syncEngineFromMeta()
{
    for (int p = 0; p < Config::maxPads; ++p)
    {
        auto m = padMeta (p);
        if (! m.isValid())
            continue;

        engine.setMappedNote (p, (int) m.getProperty (kNoteProp, -1));
        engine.setOutputBus  (p, (int) m.getProperty (kBusProp, 0));
        updatePadLedColour (p);

        // Samples load synchronously here: some hosts render offline right after opening a
        // session, and a sample arriving a moment later would be missing from the bounce.
        const auto path = m.getProperty (kFileProp, juce::String()).toString();
        const juce::File f (path);
        const bool found = path.isNotEmpty() && f.existsAsFile() && setPadSample (p, f);
        if (! found)
        {
            ++loadGeneration[p];
            engine.clearSample (p);
        }
        // A referenced file that is gone stays referenced, so it can be relinked later.
        sampleMissing[p].store (path.isNotEmpty() && ! found);
    }

    sendChangeMessage();
}

//==============================================================================
void MosaicAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty ("activePad", activePad.load(), nullptr);   // restore UI selection

    // metaTree is a child of apvts.state, so copyState captures everything.
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MosaicAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        restoreFromXml (*xml);
        markKitClean();
        markControllerClean();
    }
}

void MosaicAudioProcessor::restoreFromXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (apvts.state.getType()))
        return;

    metaTree.removeListener (this);
    apvts.replaceState (juce::ValueTree::fromXml (xml));

    // A switch keeps whatever raw value the host last sent (0.7 still reads as "on"), and
    // replaceState skips parameters whose on/off state is unchanged, so snap them to 0 or 1.
    for (auto* raw : getParameters())
        if (auto* toggle = dynamic_cast<juce::AudioParameterBool*> (raw))
            if (const float exact = toggle->get() ? 1.0f : 0.0f; std::abs (raw->getValue() - exact) > 1.0e-6f)
                toggle->setValueNotifyingHost (exact);

    // Re-acquire (or rebuild) the metadata child after the state swap.
    metaTree = apvts.state.getChildWithName (kMetaType);
    if (! metaTree.isValid())
    {
        buildDefaultMetaTree();
        apvts.state.appendChild (metaTree, nullptr);
    }
    metaTree.addListener (this);

    // Re-acquire (or rebuild) the CC map child, then restore the runtime map.
    ccMapTree = apvts.state.getChildWithName (kCCMapType);
    if (! ccMapTree.isValid())
    {
        ccMapTree = juce::ValueTree (kCCMapType);
        apvts.state.appendChild (ccMapTree, nullptr);
    }
    rebuildCCMapFromTree();

    activePad.store (juce::jlimit (0, Config::maxPads - 1, (int) apvts.state.getProperty ("activePad", 0)));

    syncEngineFromMeta();
}

//==============================================================================
juce::File MosaicAudioProcessor::getPresetsDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("MOSAIC").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

bool MosaicAudioProcessor::savePresetToFile (const juce::File& file)
{
    apvts.state.setProperty ("activePad", activePad.load(), nullptr);
    if (auto xml = apvts.copyState().createXml())
        return xml->writeTo (file);
    return false;
}

bool MosaicAudioProcessor::loadPresetFromFile (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            restoreFromXml (*xml);
            markKitClean();
            markControllerClean();
            return true;
        }
    }
    return false;
}

//==============================================================================
namespace
{
    const juce::Identifier kControllerType { "MOSAIC_CONTROLLER" };
    const juce::Identifier kKitNameProp    { "kitName" };
    const juce::Identifier kCtlNameProp    { "controllerName" };
}

juce::File MosaicAudioProcessor::getKitsDirectory()
{
    auto dir = getPresetsDirectory().getChildFile ("Kits");
    dir.createDirectory();
    return dir;
}

juce::File MosaicAudioProcessor::getControllersDirectory()
{
    auto dir = getPresetsDirectory().getChildFile ("Controllers");
    dir.createDirectory();
    return dir;
}

juce::String MosaicAudioProcessor::getKitName() const        { return apvts.state.getProperty (kKitNameProp).toString(); }
juce::String MosaicAudioProcessor::getControllerName() const { return apvts.state.getProperty (kCtlNameProp).toString(); }

MosaicAudioProcessor::KitSnapshot MosaicAudioProcessor::captureKitSnapshot() const
{
    KitSnapshot snap;
    for (auto* raw : getParameters())
        snap.values.push_back (raw->getValue());
    for (int p = 0; p < Config::maxPads; ++p)
    {
        const auto m = padMeta (p);
        snap.pads.add (m.getProperty (kFileProp).toString() + "|" + m.getProperty (kColourProp).toString() + "|"
                       + m.getProperty (kBusProp).toString());
    }
    return snap;
}

MosaicAudioProcessor::ControllerSnapshot MosaicAudioProcessor::captureControllerSnapshot() const
{
    ControllerSnapshot snap;
    for (int p = 0; p < Config::maxPads; ++p)
        snap.notes[(size_t) p] = (int) padMeta (p).getProperty (kNoteProp, -1);
    for (int c = 0; c < 128; ++c)
        snap.ccs[(size_t) c] = ccToRole[c].load();
    return snap;
}

void MosaicAudioProcessor::markKitClean()
{
    auto snap = captureKitSnapshot();
    const juce::ScopedLock lock (cleanLock);
    cleanKit = std::move (snap);
}

void MosaicAudioProcessor::markControllerClean()
{
    auto snap = captureControllerSnapshot();
    const juce::ScopedLock lock (cleanLock);
    cleanController = std::move (snap);
}

bool MosaicAudioProcessor::isKitModified() const
{
    const auto now = captureKitSnapshot();
    const juce::ScopedLock lock (cleanLock);
    return ! (now == cleanKit);
}

bool MosaicAudioProcessor::isControllerModified() const
{
    const auto now = captureControllerSnapshot();
    const juce::ScopedLock lock (cleanLock);
    return ! (now == cleanController);
}

juce::ValueTree MosaicAudioProcessor::captureController() const
{
    juce::ValueTree controller (kControllerType);
    for (int p = 0; p < Config::maxPads; ++p)
    {
        juce::ValueTree pad (kPadType);
        pad.setProperty (kIndexProp, p, nullptr);
        pad.setProperty (kNoteProp, padMeta (p).getProperty (kNoteProp, -1), nullptr);
        controller.appendChild (pad, nullptr);
    }
    // The live map, not ccMapTree: a CC learnt on the audio thread reaches the tree a moment later.
    juce::ValueTree cc (kCCMapType);
    for (int c = 0; c < 128; ++c)
        if (const int role = ccToRole[c].load(); role >= 0)
            cc.setProperty (ccPropForNumber (c), role, nullptr);
    controller.appendChild (cc, nullptr);
    return controller;
}

void MosaicAudioProcessor::applyController (const juce::ValueTree& controller)
{
    for (int p = 0; p < Config::maxPads; ++p)
    {
        const auto pad = controller.getChildWithProperty (kIndexProp, p);
        const int note = pad.isValid() ? (int) pad.getProperty (kNoteProp, -1) : -1;
        padMeta (p).setProperty (kNoteProp, juce::jlimit (-1, 127, note), nullptr);   // listener updates the engine
    }
    ccMapTree.removeAllProperties (nullptr);
    if (const auto cc = controller.getChildWithName (kCCMapType); cc.isValid())
        ccMapTree.copyPropertiesFrom (cc, nullptr);
    rebuildCCMapFromTree();
    sendChangeMessage();
}

bool MosaicAudioProcessor::saveKitToFile (const juce::File& file)
{
    apvts.state.setProperty ("activePad", activePad.load(), nullptr);
    auto kit = apvts.copyState();
    // A kit carries no controller mapping and no preset names.
    for (auto pad : kit.getChildWithName (kMetaType))
        pad.setProperty (kNoteProp, -1, nullptr);
    if (auto cc = kit.getChildWithName (kCCMapType); cc.isValid())
        cc.removeAllProperties (nullptr);
    kit.removeProperty (kKitNameProp, nullptr);
    kit.removeProperty (kCtlNameProp, nullptr);
    if (auto xml = kit.createXml(); xml != nullptr && xml->writeTo (file))
    {
        apvts.state.setProperty (kKitNameProp, file.getFileNameWithoutExtension(), nullptr);
        markKitClean();
        return true;
    }
    return false;
}

bool MosaicAudioProcessor::loadKitFromFile (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;
    const auto controller = captureController();
    const auto controllerName = getControllerName();
    restoreFromXml (*xml);
    applyController (controller);
    apvts.state.setProperty (kKitNameProp, file.getFileNameWithoutExtension(), nullptr);
    apvts.state.setProperty (kCtlNameProp, controllerName, nullptr);
    markKitClean();
    return true;
}

bool MosaicAudioProcessor::saveControllerToFile (const juce::File& file)
{
    if (auto xml = captureController().createXml(); xml != nullptr && xml->writeTo (file))
    {
        apvts.state.setProperty (kCtlNameProp, file.getFileNameWithoutExtension(), nullptr);
        markControllerClean();
        return true;
    }
    return false;
}

bool MosaicAudioProcessor::loadControllerFromFile (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (kControllerType.toString()))
        return false;
    applyController (juce::ValueTree::fromXml (*xml));
    apvts.state.setProperty (kCtlNameProp, file.getFileNameWithoutExtension(), nullptr);
    markControllerClean();
    return true;
}

void MosaicAudioProcessor::clearControllerMapping()
{
    applyController (juce::ValueTree (kControllerType));
    apvts.state.setProperty (kCtlNameProp, juce::String(), nullptr);
    markControllerClean();
}

//==============================================================================
void MosaicAudioProcessor::startKnobLearn (int role)
{
    knobLearnRole.store (role);
    sendChangeMessage();
}

void MosaicAudioProcessor::cancelKnobLearn()
{
    knobLearnRole.store (-1);
    sendChangeMessage();
}

void MosaicAudioProcessor::clearKnobCC (int role)
{
    bool changed = false;

    for (int c = 0; c < 128; ++c)
    {
        if (ccToRole[c].load() == role)
        {
            ccToRole[c].store (-1);
            changed = true;
        }
    }

    if (changed)
    {
        writeCCMapToTree();
        sendChangeMessage();
    }
}

bool MosaicAudioProcessor::isKnobLearning (int role) const
{
    return knobLearnRole.load() == role;
}

int MosaicAudioProcessor::getCCForKnobRole (int role) const
{
    for (int c = 0; c < 128; ++c)
        if (ccToRole[c].load() == role)
            return c;

    return -1;
}

void MosaicAudioProcessor::updatePadLedColour (int padIndex)
{
    const auto c = getPadColour (padIndex);
    engine.setLedColours (padIndex, velocityForColour (c, true), velocityForColour (c, false));
}

void MosaicAudioProcessor::writeCCMapToTree()
{
    ccMapTree.removeAllProperties (nullptr);

    for (int c = 0; c < 128; ++c)
        if (const int role = ccToRole[c].load(); role >= 0)
            ccMapTree.setProperty (ccPropForNumber (c), role, nullptr);
}

void MosaicAudioProcessor::rebuildCCMapFromTree()
{
    for (auto& slot : ccToRole)
        slot.store (-1);

    for (int c = 0; c < 128; ++c)
    {
        const auto id = ccPropForNumber (c);
        if (const int role = ccMapTree.getProperty (id, -1); juce::isPositiveAndBelow (role, (int) ParamID::numRoles))
            ccToRole[c].store (role);
    }
}

//==============================================================================
juce::AudioProcessorEditor* MosaicAudioProcessor::createEditor()
{
    return new MosaicEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MosaicAudioProcessor();
}
