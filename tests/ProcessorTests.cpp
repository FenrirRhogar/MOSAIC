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
