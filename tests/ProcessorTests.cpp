// MOSAIC unit tests: run the MOSAICTests target; it exits non-zero on any failure.

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace
{
    /// Writes a short sine wave as a 16-bit WAV file.
    void writeTestWav (const juce::File& file)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        juce::AudioBuffer<float> sine (1, 4800);
        for (int n = 0; n < sine.getNumSamples(); ++n)
            sine.setSample (0, n, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * (float) n / 48000.0f));

        juce::WavAudioFormat wav;
        auto stream = file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.release(), 48000.0, 1, 16, {}, 0));
        writer->writeFromAudioSampleBuffer (sine, 0, sine.getNumSamples());
    }

    std::unique_ptr<MosaicAudioProcessor> makeProcessor()
    {
        auto* plugin = dynamic_cast<MosaicAudioProcessor*> (createPluginFilter());
        jassert (plugin != nullptr);
        return std::unique_ptr<MosaicAudioProcessor> (plugin);
    }
}

class MissingSampleTests : public juce::UnitTest
{
public:
    MissingSampleTests() : juce::UnitTest ("Missing samples", "MOSAIC") {}

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("MOSAICTests-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        const auto original = root.getChildFile ("kit").getChildFile ("kick.wav");
        writeTestWav (original);

        juce::MemoryBlock session;
        {
            auto plugin = makeProcessor();
            beginTest ("A sample that exists loads and is not flagged");
            expect (plugin->setPadSample (0, original));
            expect (! plugin->isPadSampleMissing (0));
            plugin->getStateInformation (session);
        }

        original.deleteFile();   // the user moved their sample folder

        beginTest ("Reopening a session whose sample is gone flags the pad and keeps the path");
        auto reopened = makeProcessor();
        reopened->setStateInformation (session.getData(), (int) session.getSize());
        expect (reopened->isPadSampleMissing (0));
        expectEquals (reopened->getNumMissingSamples(), 1);
        expectEquals (reopened->getPadFilePath (0), original.getFullPathName());

        beginTest ("Saving again still remembers the missing file");
        juce::MemoryBlock resaved;
        reopened->getStateInformation (resaved);
        auto third = makeProcessor();
        third->setStateInformation (resaved.getData(), (int) resaved.getSize());
        expectEquals (third->getPadFilePath (0), original.getFullPathName());

        beginTest ("Relinking finds the file by name in a subfolder");
        writeTestWav (root.getChildFile ("moved").getChildFile ("drums").getChildFile ("kick.wav"));
        expectEquals (reopened->relinkMissingSamples (root.getChildFile ("moved")), 1);
        expect (! reopened->isPadSampleMissing (0));
        expectEquals (reopened->getNumMissingSamples(), 0);

        beginTest ("Clearing a missing pad clears the flag");
        third->clearPad (0);
        expect (! third->isPadSampleMissing (0));

        root.deleteRecursively();
    }
};

static MissingSampleTests missingSampleTests;

class KnobCCTests : public juce::UnitTest
{
public:
    KnobCCTests() : juce::UnitTest ("Knob MIDI CCs", "MOSAIC") {}

    void runTest() override
    {
        auto plugin = makeProcessor();
        plugin->prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (plugin->getTotalNumOutputChannels(), 256);

        const auto sendCC = [&] (int cc, int value)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, cc, value), 0);
            buffer.clear();
            plugin->processBlock (buffer, midi);
            plugin->servicePendingMidi();   // CC values and learning finish on the message thread
        };
        const auto learn = [&] (int role, int cc)
        {
            plugin->startKnobLearn (role);
            sendCC (cc, 0);
        };
        auto& apvts = plugin->getAPVTS();
        const auto value = [&] (const juce::String& id) { return apvts.getParameter (id)->getValue(); };

        beginTest ("The Grain knob learns a CC and the CC drives it");
        learn (ParamID::roleGrain, 21);
        expectEquals (plugin->getCCForKnobRole (ParamID::roleGrain), 21);
        sendCC (21, 127);
        expectWithinAbsoluteError (value (ParamID::grainSize (0)), 1.0f, 1.0e-4f);

        beginTest ("A CC cannot push Start past End, or End before Start");
        learn (ParamID::roleStart, 20);
        learn (ParamID::roleEnd, 22);
        sendCC (22, 38);   // End at 0.3
        sendCC (20, 127);
        expect (value (ParamID::start (0)) < value (ParamID::end (0)));
        sendCC (20, 64);   // Start at 0.5 is past End too
        sendCC (22, 0);
        expect (value (ParamID::end (0)) > value (ParamID::start (0)));
    }
};

static KnobCCTests knobCCTests;

class PresetSplitTests : public juce::UnitTest
{
public:
    PresetSplitTests() : juce::UnitTest ("Kit and controller presets", "MOSAIC") {}

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("MOSAICPresets-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        root.createDirectory();
        const auto kitFile = root.getChildFile ("Kit A.mosaic");
        const auto controllerFile = root.getChildFile ("Pads A.mosaicctl");

        auto plugin = makeProcessor();
        plugin->prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (plugin->getTotalNumOutputChannels(), 256);
        const auto learnCC = [&] (int role, int cc)
        {
            plugin->startKnobLearn (role);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::controllerEvent (1, cc, 0), 0);
            buffer.clear();
            plugin->processBlock (buffer, midi);
            plugin->servicePendingMidi();   // the message-thread half of MIDI handling, as the plugin's timer does
        };
        auto* gain = plugin->getAPVTS().getParameter (ParamID::gain (0));

        learnCC (ParamID::roleGrain, 21);
        gain->setValueNotifyingHost (0.3f);
        expect (plugin->saveKitToFile (kitFile));
        expect (plugin->saveControllerToFile (controllerFile));

        beginTest ("A kit file carries no controller mapping");
        const auto kitXml = juce::XmlDocument::parse (kitFile);
        expect (kitXml != nullptr);
        if (kitXml != nullptr)
            if (auto* cc = kitXml->getChildByName ("CCMAP"))
                expectEquals (cc->getNumAttributes(), 0);

        beginTest ("A controller preset brings the mapping back without touching the sound");
        plugin->clearControllerMapping();
        expectEquals (plugin->getCCForKnobRole (ParamID::roleGrain), -1);
        expect (plugin->loadControllerFromFile (controllerFile));
        expectEquals (plugin->getCCForKnobRole (ParamID::roleGrain), 21);
        expectWithinAbsoluteError (gain->getValue(), 0.3f, 1.0e-3f);
        expectEquals (plugin->getControllerName(), juce::String ("Pads A"));

        beginTest ("Loading a kit restores the sound and keeps the current mapping");
        gain->setValueNotifyingHost (0.8f);
        learnCC (ParamID::roleGrain, 30);
        expect (plugin->loadKitFromFile (kitFile));
        expectWithinAbsoluteError (plugin->getAPVTS().getParameter (ParamID::gain (0))->getValue(), 0.3f, 1.0e-3f);
        expectEquals (plugin->getCCForKnobRole (ParamID::roleGrain), 30);
        expectEquals (plugin->getKitName(), juce::String ("Kit A"));
        expectEquals (plugin->getControllerName(), juce::String ("Pads A"));

        root.deleteRecursively();
    }
};

static PresetSplitTests presetSplitTests;


class RealtimeSafetyTests : public juce::UnitTest
{
public:
    RealtimeSafetyTests() : juce::UnitTest ("Real-time safety", "MOSAIC") {}

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("MOSAICRealtime-" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()));
        const auto wav = root.getChildFile ("tone.wav");
        writeTestWav (wav);

        auto plugin = makeProcessor();
        plugin->prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buffer (plugin->getTotalNumOutputChannels(), 256);
        const auto block = [&] (juce::MidiBuffer& midi)
        {
            buffer.clear();
            plugin->processBlock (buffer, midi);
            plugin->servicePendingMidi();
        };

        beginTest ("The Studio engine renders a block far larger than the one announced");
        {
            auto small = makeProcessor();
            small->prepareToPlay (48000.0, 64);   // the host promises blocks of at most 64 samples
            expect (small->setPadSample (0, wav));
            auto& smallApvts = small->getAPVTS();
            smallApvts.getParameter (ParamID::pitchMode (0))->setValueNotifyingHost (1.0f);   // Studio
            smallApvts.getParameter (ParamID::pitch (0))->setValueNotifyingHost (0.5625f);    // +3 semitones
            small->padPressed (0, 1.0f);

            juce::AudioBuffer<float> big (small->getTotalNumOutputChannels(), 4096);
            double energy = 0.0;
            bool finite = true;
            for (int i = 0; i < 3; ++i)
            {
                big.clear();
                juce::MidiBuffer midi;
                small->processBlock (big, midi);
                for (int n = 0; n < big.getNumSamples(); ++n)
                {
                    const float v = big.getSample (0, n);
                    finite = finite && std::isfinite (v);
                    energy += (double) v * v;
                }
            }
            expect (finite);
            expect (energy > 0.0);
        }

        beginTest ("A UI click reaches the audio thread through the queue");
        expect (plugin->setPadSample (0, wav));
        plugin->padPressed (0, 1.0f);
        expect (! plugin->isPadActive (0));   // nothing is started until the audio thread runs
        {
            juce::MidiBuffer midi;
            block (midi);
        }
        expect (plugin->isPadActive (0));

        beginTest ("CC values are applied by the message thread, not the audio thread");
        {
            auto& apvts = plugin->getAPVTS();
            const auto sendCC = [&] (int cc, int value, bool service)
            {
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::controllerEvent (1, cc, value), 0);
                buffer.clear();
                plugin->processBlock (buffer, midi);
                if (service)
                    plugin->servicePendingMidi();
            };
            plugin->startKnobLearn (ParamID::roleGrain);
            sendCC (21, 0, true);
            auto* grain = apvts.getParameter (ParamID::grainSize (plugin->getActivePad()));
            const float before = grain->getValue();
            sendCC (21, 127, false);
            expectWithinAbsoluteError (grain->getValue(), before, 1.0e-6f);   // audio thread left it alone
            plugin->servicePendingMidi();
            expectWithinAbsoluteError (grain->getValue(), 1.0f, 1.0e-4f);
        }

        beginTest ("A burst of pad LED updates is spread over several blocks");
        {
            constexpr int pads = 24;
            for (int p = 0; p < pads; ++p)
            {
                expect (plugin->setPadSample (p, wav));
                plugin->startMidiLearn (p);
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, 40 + p, 0.5f), 0);
                block (midi);
                plugin->padReleased (p);
            }

            // Every pad changes colour at once: that is a burst of LED updates for the controller.
            for (int p = 0; p < pads; ++p)
                plugin->setPadColour (p, juce::Colours::white);

            int largest = 0, total = 0, blocksWithEvents = 0;
            for (int i = 0; i < 12; ++i)
            {
                juce::MidiBuffer midi;
                block (midi);
                const int count = midi.getNumEvents();
                largest = juce::jmax (largest, count);
                total += count;
                blocksWithEvents += count > 0 ? 1 : 0;
            }
            expect (largest <= 17);          // 16 events, plus at most one note-off paired with a note-on
            expect (total > 17);             // more than one block's worth was needed...
            expect (blocksWithEvents >= 2);  // ...and it was spread over several blocks
        }

        root.deleteRecursively();
    }
};

static RealtimeSafetyTests realtimeSafetyTests;

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("MOSAIC");

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;
    return failures > 0 ? 1 : 0;
}
