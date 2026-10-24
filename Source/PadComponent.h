#pragma once

#include <JuceHeader.h>
#include "Parameters.h"

class MosaicAudioProcessor;

//==============================================================================
/*  A single pad in the grid.

    Features:
      - Accepts .wav / .mp3 / .aif drag-and-drop.
      - Renders the audio waveform (AudioThumbnail) once a sample is loaded.
      - Draggable Sample Start / Sample End handles over the waveform, bound to
        the pad's APVTS parameters.
      - A colour swatch that opens a ColourSelector.
      - A MIDI-learn toggle that maps the next incoming NoteOn to this pad.
      - Clicking the pad selects it (for the inspector) WITHOUT auditioning; a
        dedicated press-and-hold ▶ button auditions it.
*/

//==============================================================================
/*  A press-and-hold trigger button. Unlike a normal button (which fires on
    release), this reports mouse-down and mouse-up separately so Gate/Momentary
    pads sound only while held.
*/
class PadTriggerButton : public juce::Component
{
public:
    std::function<void()> onPress, onRelease;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { held = true;  repaint(); if (onPress)   onPress(); }
    void mouseUp   (const juce::MouseEvent&) override { held = false; repaint(); if (onRelease) onRelease(); }

private:
    bool held = false;
};

//==============================================================================
class PadComponent : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     private juce::ChangeListener
{
public:
    PadComponent (MosaicAudioProcessor& proc,
                  int padIndex,
                  juce::AudioThumbnailCache& thumbCache);
    ~PadComponent() override;

    void setSelected (bool shouldBeSelected, bool isPrimary);
    bool isSelected() const noexcept { return selected; }

    // Driven by the editor's timer from the engine's activity meter and
    // playback position. pos is normalised 0..1 over the whole sample.
    void setPlayback (bool active, float pos);

    // Pull colour / file / learned-note / learn-state from the processor.
    void refreshFromState();

    std::function<void (int, const juce::ModifierKeys&)> onSelected;   // notify editor of selection

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit  (const juce::StringArray&) override;
    void filesDropped  (const juce::StringArray& files, int, int) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }  // thumbnail ready

    juce::Rectangle<int> waveformArea() const;
    float paramValue (const juce::String& id) const;
    void  setParam (const juce::String& id, float value01);
    int   handleHitTest (juce::Point<int> pos) const;   // 0=none, 1=start, 2=end

    void openColourSelector();
    void toggleMidiLearn();

    MosaicAudioProcessor& processor;
    const int pad;

    juce::AudioThumbnail thumbnail;
    juce::String loadedPath;          // guards redundant thumbnail reloads
    juce::Colour padColour;
    int  learnedNote = -1;
    int  playModeCached = Config::modeLatching;
    bool  learning   = false;
    bool  selected   = false;
    bool  primary    = false;
    bool  playing    = false;
    float playPos    = 0.0f;
    bool  fileHover  = false;
    int  draggingHandle = 0;          // 0=none, 1=start, 2=end

    PadTriggerButton triggerButton;
    juce::TextButton learnButton { "L" };
    juce::TextButton colourButton { "" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadComponent)
};
