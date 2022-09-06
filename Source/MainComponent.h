#pragma once

#include <JuceHeader.h>

//==============================================================================


struct SineWaveSound : public juce::SynthesiserSound
{
    SineWaveSound() {}

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

//==============================================================================
struct SineWaveVoice : public juce::SynthesiserVoice
{
    SineWaveVoice();

    bool canPlaySound(juce::SynthesiserSound* sound) override;

    void startNote(int midiNoteNumber, float velocity,
        juce::SynthesiserSound*, int /*currentPitchWheelPosition*/) override;

    void stopNote(float /*velocity*/, bool allowTailOff) override;

    void pitchWheelMoved(int) override;
    void controllerMoved(int, int) override;

    void renderNextBlock(juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override;
   

private:
    double currentAngle = 0.0, angleDelta = 0.0, level = 0.0, tailOff = 0.0;
};

//==============================================================================
class SynthAudioSource : public juce::AudioSource
{
public:
    SynthAudioSource(juce::MidiKeyboardState& keyState);

    void setUsingSineWaveSound();

    void prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate) override;

    void releaseResources() override;

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    juce::MidiMessageCollector* getMidiCollector();

private:
    juce::MidiKeyboardState& keyboardState;
    juce::Synthesiser synth;
    juce::MidiMessageCollector midiCollector;
};

class GridButton : public juce::ShapeButton
{
public:
    GridButton(): juce::ShapeButton("button", juce::Colours::lightgrey, juce::Colours::yellow, juce::Colours::orangered)
    {};
};

class MainComponent  : 
    public juce::AudioAppComponent, 
    public juce::MidiKeyboardStateListener, 
    public juce::Button::Listener,
    public juce::KeyListener
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    // Your private member variables go here...
    void setMidiInput(int index);

    // MidiKeyboardStateListener functions
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    
    void buttonClicked(juce::Button* button) override;
    void setJIFrequencies();

    bool changingJIInterval = false;

    // KeyListener functions
    bool keyPressed(const juce::KeyPress& key, Component* originatingComponent) override;

    //==========================================================================
    juce::MidiKeyboardState keyboardState;
    SynthAudioSource synthAudioSource;

    juce::MidiMessageCollector midiCollector;

    juce::ComboBox midiInputList;
    juce::Label midiInputListLabel;
    int lastInputIndex = 0;

    juce::TextEditor midiMessagesBox;
    void logMessage(const juce::String& m);

    GridButton buttonGrid[8][8];

    int bassNum;
    int bassDen;
    int melNum;
    int melDen;
    double rootFreq = juce::MidiMessage::getMidiNoteInHertz(48);

    std::vector<std::vector<int>> JIIntervals;

    juce::TextButton melNumButton;
    juce::TextButton melDenButton;
    juce::TextButton bassNumButton;
    juce::TextButton bassDenButton;

    int* jiNumberTarget;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
