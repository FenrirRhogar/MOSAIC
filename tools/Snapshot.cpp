// Renders the MOSAIC editor to PNG while feeding it a synthetic mix, so the meters and
// displays show live content. Used for README screenshots and visual checks.
//   MOSAIC_snapshot out.png [factoryPresetIndex] [--bench] [--scale N] [--demo]
// --bench also prints the average time to paint one full frame of the editor.
// --demo fills the pads with synthetic drum, bass and chord samples (written to a temp folder), so the
// screenshot shows waveforms and a playing pad instead of empty pads.
// --scale renders at N times the editor's size (e.g. 2 for store artwork).

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

#include <cstdio>

namespace
{
    // Kick, bass, hats and a wide pad: enough spectral and stereo content to drive every display.
    struct TestMix
    {
        void render (juce::AudioBuffer<float>& buffer, double sampleRate)
        {
            for (int n = 0; n < buffer.getNumSamples(); ++n)
            {
                const auto t = (double) sample++ / sampleRate;
                const auto beat = std::fmod (t, 0.5);
                const auto kick = std::sin (juce::MathConstants<double>::twoPi * (50.0 + 90.0 * std::exp (-beat * 30.0)) * beat) * std::exp (-beat * 7.0);
                const auto bass = 0.25 * std::sin (juce::MathConstants<double>::twoPi * 55.0 * t) + 0.12 * std::sin (juce::MathConstants<double>::twoPi * 110.0 * t);
                const auto hatEnv = std::exp (-std::fmod (t, 0.125) * 60.0);
                const auto noise = random.nextFloat() * 2.0f - 1.0f;
                hat = 0.6f * (noise - lastNoise) + 0.4f * hat;
                lastNoise = noise;
                const auto padL = 0.08 * std::sin (juce::MathConstants<double>::twoPi * 330.0 * t) + 0.06 * std::sin (juce::MathConstants<double>::twoPi * 495.3 * t);
                const auto padR = 0.08 * std::sin (juce::MathConstants<double>::twoPi * 331.1 * t + 1.0) + 0.06 * std::sin (juce::MathConstants<double>::twoPi * 494.1 * t);
                const auto mono = 0.45 * kick + bass + 0.4 * hatEnv * hat;
                constexpr double level = 0.3; // around -18 dBFS RMS, 0 VU
                buffer.setSample (0, n, (float) (level * (mono + padL)));
                buffer.setSample (1, n, (float) (level * (mono + padR)));
            }
        }

        juce::int64 sample = 0;
        juce::Random random { 42 };
        float hat = 0.0f, lastNoise = 0.0f;
    };
}

namespace
{
    // Eight short synthetic sounds, written as wav files named like real kit samples.
    juce::Array<juce::File> writeDemoSamples()
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mosaic-demo-kit");
        dir.createDirectory();
        constexpr double sr = 44100.0;
        const auto twoPi = juce::MathConstants<double>::twoPi;
        juce::Random random (7);
        juce::Array<juce::File> files;

        const auto write = [&] (const char* name, double seconds, std::function<double (double, int)> voice)
        {
            const int count = (int) (seconds * sr);
            juce::AudioBuffer<float> buffer (1, count);
            for (int n = 0; n < count; ++n)
                buffer.setSample (0, n, (float) juce::jlimit (-1.0, 1.0, voice ((double) n / sr, n)));
            const auto file = dir.getChildFile (juce::String (name) + ".wav");
            file.deleteFile();
            juce::WavAudioFormat wav;
            if (auto stream = file.createOutputStream())
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 1, 16, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (buffer, 0, count);
                }
            files.add (file);
        };
        const auto noise = [&] { return random.nextDouble() * 2.0 - 1.0; };

        write ("Kick", 0.55, [&] (double t, int) { return 0.9 * std::sin (twoPi * (48.0 * t + 90.0 * (1.0 - std::exp (-t * 30.0)) / 30.0)) * std::exp (-t * 7.0); });
        write ("Snare", 0.4, [&] (double t, int) { return (0.5 * noise() + 0.4 * std::sin (twoPi * 190.0 * t)) * std::exp (-t * 14.0); });
        write ("Hat", 0.18, [&] (double t, int) { return 0.5 * noise() * std::exp (-t * 38.0); });
        write ("Clap", 0.45, [&] (double t, int) { return 0.6 * noise() * (std::exp (-std::fmod (t, 0.022) * 160.0) * (t < 0.07 ? 1.0 : 0.0) + std::exp (-t * 12.0) * (t >= 0.07 ? 0.7 : 0.0)); });
        write ("Sub Bass", 1.1, [&] (double t, int) { return 0.8 * std::sin (twoPi * 55.0 * t) * std::min (1.0, t * 80.0) * std::exp (-t * 2.2); });
        write ("Chord Stab", 1.0, [&] (double t, int) { return 0.22 * (std::sin (twoPi * 261.6 * t) + std::sin (twoPi * 329.6 * t) + std::sin (twoPi * 392.0 * t)) * std::min (1.0, t * 40.0) * std::exp (-t * 3.0); });
        write ("Perc", 0.3, [&] (double t, int) { return 0.7 * std::sin (twoPi * (820.0 - 300.0 * t) * t) * std::exp (-t * 16.0); });
        write ("Vox Chop", 0.9, [&] (double t, int) { return 0.4 * (std::sin (twoPi * 210.0 * t) * (0.6 + 0.4 * std::sin (twoPi * 5.0 * t)) + 0.3 * std::sin (twoPi * 840.0 * t)) * std::sin (juce::MathConstants<double>::pi * juce::jmin (1.0, t / 0.9)); });
        return files;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png");

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 400;
    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    processor->setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor->prepareToPlay (sampleRate, blockSize);
    if (argc > 2 && ! juce::String (argv[2]).startsWith ("--"))
        processor->setCurrentProgram (juce::String (argv[2]).getIntValue());

    bool demo = false;
    for (int i = 1; i < argc; ++i)
        demo = demo || juce::String (argv[i]) == "--demo";
    if (demo)
    {
        if (auto* mosaic = dynamic_cast<MosaicAudioProcessor*> (processor.get()))
        {
            const auto files = writeDemoSamples();
            for (int pad = 0; pad < files.size(); ++pad)
                mosaic->setPadSample (pad, files[pad]);
            mosaic->padPressed (0, 1.0f);   // a playing pad: shows the playhead and the lit pad
            mosaic->padPressed (4, 1.0f);
        }
    }

    auto* editor = processor->createEditorAndMakeActive();
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    TestMix mix;

    // ~2 s of audio in real time, two blocks per 60 Hz frame, letting the editor's timers run.
    for (int frame = 0; frame < 120; ++frame)
    {
        for (int b = 0; b < 2; ++b)
        {
            mix.render (buffer, sampleRate);
            processor->processBlock (buffer, midi);
        }
        juce::Timer::callPendingTimersSynchronously();
        juce::Thread::sleep (16);
    }

    bool bench = false;
    float scale = 1.0f;
    for (int i = 3; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (arg == "--bench")
            bench = true;
        else if (arg == "--scale" && i + 1 < argc)
            scale = juce::String (argv[++i]).getFloatValue();
    }

    if (bench)
    {
        constexpr int frames = 60;
        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < frames; ++i)
        {
            mix.render (buffer, sampleRate);
            processor->processBlock (buffer, midi);
            juce::Timer::callPendingTimersSynchronously();
            editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        }
        std::printf ("%s: %.2f ms per frame\n", processor->getName().toRawUTF8(),
                     (juce::Time::getMillisecondCounterHiRes() - start) / frames);
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    output.deleteFile();
    juce::FileOutputStream stream (output);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    delete editor;
    return stream.getStatus().wasOk() ? 0 : 1;
}
