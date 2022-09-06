#include "MainComponent.h"


double melFreq = 0.0;
double bassFreq = 0.0;

SineWaveVoice::SineWaveVoice() {};

bool SineWaveVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SineWaveSound*> (sound) != nullptr;
}


void SineWaveVoice::startNote(int midiNoteNumber, float velocity,
    juce::SynthesiserSound*, int /*currentPitchWheelPosition*/)
{
    currentAngle = 0.0;
    level = velocity;
    tailOff = 0.0;

    // this is where the magic equation happens
    auto cyclesPerSecond = ((midiNoteNumber % 16) * melFreq) + ((7 - (midiNoteNumber / 16)) * bassFreq);
    auto cyclesPerSample = cyclesPerSecond / getSampleRate();

    angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;
}

void SineWaveVoice::stopNote(float /*velocity*/, bool allowTailOff) 
{
    if (allowTailOff)
    {
        if (tailOff == 0.0)
            tailOff = 1.0;
    }
    else
    {
        clearCurrentNote();
        angleDelta = 0.0;
    }
}

void SineWaveVoice::pitchWheelMoved(int) {}
void SineWaveVoice::controllerMoved(int, int) {}

void SineWaveVoice::renderNextBlock(juce::AudioSampleBuffer& outputBuffer, int startSample, int numSamples)
{
    if (angleDelta != 0.0)
    {
        if (tailOff > 0.0)
        {
            while (--numSamples >= 0)
            {
                auto currentSample = (float)(std::sin(currentAngle) * level * tailOff);

                for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    outputBuffer.addSample(i, startSample, currentSample);

                currentAngle += angleDelta;
                ++startSample;

                tailOff *= 0.99;

                if (tailOff <= 0.005)
                {
                    clearCurrentNote();

                    angleDelta = 0.0;
                    break;
                }
            }
        }
        else
        {
            while (--numSamples >= 0) 
            {
                auto currentSample = (float)(std::sin(currentAngle) * level);

                for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    outputBuffer.addSample(i, startSample, currentSample);

                currentAngle += angleDelta;
                ++startSample;
            }
        }
    }
}

//==============================================================================

SynthAudioSource::SynthAudioSource(juce::MidiKeyboardState& keyState)
    : keyboardState(keyState)
{
    for (auto i = 0; i < 4; ++i)
        synth.addVoice(new SineWaveVoice());

    synth.addSound(new SineWaveSound());
}

void SynthAudioSource::setUsingSineWaveSound()
{
    synth.clearSounds();
}

void SynthAudioSource::prepareToPlay(int /*samplesPerBlockExpected*/, double sampleRate)
{
    synth.setCurrentPlaybackSampleRate(sampleRate);
    midiCollector.reset(sampleRate); 
}

void SynthAudioSource::releaseResources() {};

void SynthAudioSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    juce::MidiBuffer incomingMidi;
    midiCollector.removeNextBlockOfMessages(incomingMidi, bufferToFill.numSamples);


    keyboardState.processNextMidiBuffer(incomingMidi, bufferToFill.startSample,
        bufferToFill.numSamples, true);

    synth.renderNextBlock(*bufferToFill.buffer, incomingMidi,
        bufferToFill.startSample, bufferToFill.numSamples);
}

juce::MidiMessageCollector* SynthAudioSource::getMidiCollector()
{
    return &midiCollector;
}


//==============================================================================
MainComponent::MainComponent() : 
        synthAudioSource (keyboardState) 
{
    bassNum = 1;
    bassDen = 1;
    melNum = 3;
    melDen = 2;

    JIIntervals = { {1,1},{9,8},{6,5},{5,4},{4,3},{3,2},{8,5},{5,3} };

    jiNumberTarget = &melNum;
    setJIFrequencies();

    melNumButton.setButtonText(juce::String(melNum));
    melNumButton.addListener(this);
    addAndMakeVisible(melNumButton);

    melDenButton.setButtonText(juce::String(melDen));
    melDenButton.addListener(this);
    addAndMakeVisible(melDenButton);

    bassNumButton.setButtonText(juce::String(bassNum));
    bassNumButton.addListener(this);
    addAndMakeVisible(bassNumButton);

    bassDenButton.setButtonText(juce::String(bassDen));
    bassDenButton.addListener(this);
    addAndMakeVisible(bassDenButton);

    addAndMakeVisible(midiInputListLabel);
    midiInputListLabel.setText("MIDI Input:", juce::dontSendNotification);
    midiInputListLabel.attachToComponent(&midiInputList, true);

    auto midiInputs = juce::MidiInput::getAvailableDevices();
    addAndMakeVisible(midiInputList);
    midiInputList.setTextWhenNoChoicesAvailable("No MIDI Inputs Enabled");

    juce::StringArray midiInputNames;
    for (auto input : midiInputs)
        midiInputNames.add(input.name);

    midiInputList.addItemList(midiInputNames, 1);
    midiInputList.onChange = [this] { setMidiInput(midiInputList.getSelectedItemIndex()); };

    for (auto input : midiInputs)
    {
        if (deviceManager.isMidiInputDeviceEnabled(input.identifier))
        {
            setMidiInput(midiInputs.indexOf(input));
            break;
        }
    }
    setMidiInput(0);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (0, 2);
    }

    addAndMakeVisible(midiMessagesBox);
    midiMessagesBox.setMultiLine(true);
    midiMessagesBox.setReturnKeyStartsNewLine(true);
    midiMessagesBox.setReadOnly(true);
    midiMessagesBox.setScrollbarsShown(true);
    midiMessagesBox.setCaretVisible(false);
    midiMessagesBox.setPopupMenuEnabled(true);
    midiMessagesBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x32ffffff));
    midiMessagesBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0x1c000000));
    midiMessagesBox.setColour(juce::TextEditor::shadowColourId, juce::Colour(0x16000000));
    
    for (int i = 0; i<8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            addAndMakeVisible(buttonGrid[i][j]);
        }
    }
    

    keyboardState.addListener(this);
    addKeyListener(this);

    setSize(800, 600);
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    // This function will be called when the audio device is started, or when
    // its settings (i.e. sample rate, block size, etc) are changed.

    // You can use this function to initialise any resources you might need,
    // but be careful - it will be called on the audio thread, not the GUI thread.

    // For more details, see the help for AudioProcessor::prepareToPlay()
    midiCollector.reset(sampleRate);
    synthAudioSource.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    /*    // manipulate MIDI here
    juce::MidiBuffer incomingMidi;
    midiCollector.removeNextBlockOfMessages(incomingMidi, bufferToFill.numSamples);
    //auto newMidi = incomingMidi.begin();
    //auto newMidiMessage = (*newMidi).getMessage();
    //DBG(newMidiMessage.getDescription());
    //DBG(std::to_string(incomingMidi.isEmpty()));
    for (auto mid = incomingMidi.begin(); mid != incomingMidi.end(); mid++) {
        DBG("In LOOP");
        auto newMessage = (*mid).getMessage();
        // DBG(juce::String(newMessage.getDescription()));
        if (newMessage.getNoteNumber() == 112) {
            if (newMessage.isNoteOn()) changingJIInterval = true;
            else changingJIInterval = false;
            logMessage(juce::String("changingJIInterval = " + std::to_string(changingJIInterval)));
        }
        else if (changingJIInterval) {
            // calculate and change JI values

                ///((midiNoteNumber % 16) * melFreq) 
                ///((7 - (midiNoteNumber / 16)) * bassFreq);
            

            bassNum = JIIntervals[(7 - (newMessage.getNoteNumber()) / 16)][0];
            bassDen = JIIntervals[(7 - (newMessage.getNoteNumber()) / 16)][1];
            melNum = JIIntervals[newMessage.getNoteNumber() % 16][0];
            melDen = JIIntervals[newMessage.getNoteNumber() % 16][1];

            setJIFrequencies();

        }

    }
    */


    synthAudioSource.getNextAudioBlock(bufferToFill);
}

void MainComponent::releaseResources()
{
    // This will be called when the audio device stops, or when it is being
    // restarted due to a setting change.

    // For more details, see the help for AudioProcessor::releaseResources()
    synthAudioSource.releaseResources();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // You can add your drawing code here!
}

void MainComponent::resized()
{
    // This is called when the MainContentComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
    auto area = getLocalBounds();

    midiInputList.setBounds(area.removeFromTop(36).removeFromRight(getWidth() - 150).removeFromLeft(getWidth() - 300).reduced(8));
    midiMessagesBox.setBounds(area.removeFromTop(64).reduced(8));

    auto grid = area.removeFromRight(area.getHeight()).reduced(15);
 
    juce::Path path;  
    juce::Rectangle<int> row;
    juce::Rectangle<int> cell;
    int gridSideLength = grid.getWidth() / 8;
    for (int i = 0; i < 8; i++)
    {
        row = grid.removeFromBottom(gridSideLength);
        for (int j = 0; j < 8; j++)
        {
            cell = row.removeFromLeft(gridSideLength).reduced(2);
            path.clear();
            path.addRectangle(cell);
            buttonGrid[i][j].setBounds(cell);
            buttonGrid[i][j].setShape(path, true, true, false);
        }
    }

    auto JIButtons = area.removeFromTop(area.getWidth()).reduced(30);
    row = JIButtons.removeFromTop(JIButtons.getWidth() / 2);
    melNumButton.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(2));
    melDenButton.setBounds(row.reduced(2));
    bassNumButton.setBounds(JIButtons.removeFromLeft(JIButtons.getWidth() / 2).reduced(2));
    bassDenButton.setBounds(JIButtons.reduced(2));

}

void MainComponent::setMidiInput(int index)
{
    auto list = juce::MidiInput::getAvailableDevices();

    deviceManager.removeMidiInputDeviceCallback(list[lastInputIndex].identifier,
        synthAudioSource.getMidiCollector()); // [12]

    auto newInput = list[index];

    if (!deviceManager.isMidiInputDeviceEnabled(newInput.identifier))
        deviceManager.setMidiInputDeviceEnabled(newInput.identifier, true);

    deviceManager.addMidiInputDeviceCallback(newInput.identifier, synthAudioSource.getMidiCollector()); // [13]
    midiInputList.setSelectedId(index + 1, juce::dontSendNotification);

    lastInputIndex = index;
}

void MainComponent::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) {
    if (midiNoteNumber == 112) changingJIInterval = true;
    else if (changingJIInterval) {
        bassNum = JIIntervals[7 - (midiNoteNumber / 16)][0];
        bassDen = JIIntervals[7 - (midiNoteNumber / 16)][1];
        melNum = JIIntervals[midiNoteNumber % 8][0];
        melDen = JIIntervals[midiNoteNumber % 8][1];
        setJIFrequencies();
    }

    juce::MessageManager::callAsync([=]() {
        int i = 7 - (midiNoteNumber / 16);
        int j = midiNoteNumber % 16;
        buttonGrid[i][j].setColours(juce::Colours::green, juce::Colours::yellowgreen, juce::Colours::red);
        buttonGrid[i][j].repaint();
        logMessage(juce::String("Note On [") + juce::String(i) + juce::String(", ") + juce::String(j) + juce::String("]"));
        logMessage(juce::String(midiNoteNumber));
    });
};

void MainComponent::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) {
    if (midiNoteNumber == 112) changingJIInterval = false;

    juce::MessageManager::callAsync([=]() {
        int i = 7 - (midiNoteNumber / 16); 
        int j = midiNoteNumber % 16;
        buttonGrid[i][j].setColours(juce::Colours::lightgrey, juce::Colours::yellow, juce::Colours::orangered);
        buttonGrid[i][j].repaint();
        logMessage(juce::String("Note Off [") + juce::String(i) + juce::String(", ") + juce::String(j) + juce::String("]"));;
    });
};

bool MainComponent::keyPressed(const juce::KeyPress& key, Component* originatingComponent)
{
    bool jiNumberChanged = false;
    logMessage(juce::String(key.getKeyCode()));

    switch (key.getKeyCode()) {
    case(49):
        jiNumberChanged = true;
        *jiNumberTarget = 1;
        break;
    case(50):
        jiNumberChanged = true;
        *jiNumberTarget = 2;
        break;
    case(51):
        jiNumberChanged = true;
        *jiNumberTarget = 3;
        break;
    case(52):
        jiNumberChanged = true;
        *jiNumberTarget = 4;
        break;
    case(53):
        jiNumberChanged = true;
        *jiNumberTarget = 5;
        break;
    case(54):
        jiNumberChanged = true;
        *jiNumberTarget = 6;
        break;
    case(55):
        jiNumberChanged = true;
        *jiNumberTarget = 7;
        break;
    case(56):
        jiNumberChanged = true;
        *jiNumberTarget = 8;
        break;
    case(57):
        jiNumberChanged = true;
        *jiNumberTarget = 9;
        break;
    case(48):
        jiNumberChanged = true;
        *jiNumberTarget = 10;
        break;

    case(81):
        jiNumberChanged = true;
        *jiNumberTarget = 11;
        break;
    case(87):
        jiNumberChanged = true;
        *jiNumberTarget = 12;
        break;
    case(69):
        jiNumberChanged = true;
        *jiNumberTarget = 13;
        break;
    case(82):
        jiNumberChanged = true;
        *jiNumberTarget = 14;
        break;
    case(84):
        jiNumberChanged = true;
        *jiNumberTarget = 15;
        break;
    case(89):
        jiNumberChanged = true;
        *jiNumberTarget = 16;
        break;
    case(85):
        jiNumberChanged = true;
        *jiNumberTarget = 17;
        break;
    case(73):
        jiNumberChanged = true;
        *jiNumberTarget = 18;
        break;
    case(79):
        jiNumberChanged = true;
        *jiNumberTarget = 19;
        break;
    case(80):
        jiNumberChanged = true;
        *jiNumberTarget = 20;
        break;

    case(70):
        jiNumberTarget = &melNum;
        break;
    case(71):
        jiNumberTarget = &melDen;
        break;
    case(72):
        jiNumberTarget = &bassNum;
        break;
    case(74):
        jiNumberTarget = &bassDen;
        break;
    }

    if (jiNumberChanged) setJIFrequencies();
    return jiNumberChanged;
}

void MainComponent::buttonClicked(juce::Button* button) {
    if (button == &melNumButton) {
        melNumButton.setButtonText(juce::String(melNum));
        jiNumberTarget = &melNum;
    }
    else if (button == &melDenButton) {
        melDenButton.setButtonText(juce::String(melDen));
        jiNumberTarget = &melDen;
    }
    else if (button == &bassNumButton) {
        bassNumButton.setButtonText(juce::String(bassNum));
        jiNumberTarget = &bassNum;
    }
    else if (button == &bassDenButton) {
        bassDenButton.setButtonText(juce::String(bassDen));
        jiNumberTarget = &bassDen;
    }
    setJIFrequencies();
}

void MainComponent::setJIFrequencies() {
    bassFreq = rootFreq * bassNum / bassDen;
    melFreq = bassFreq * melNum / melDen;
    juce::MessageManager::callAsync([=]() {
        melNumButton.setButtonText(juce::String(melNum));

        melDenButton.setButtonText(juce::String(melDen));

        bassNumButton.setButtonText(juce::String(bassNum));

        bassDenButton.setButtonText(juce::String(bassDen));
        });
}

void MainComponent::logMessage(const juce::String& m) {
    midiMessagesBox.moveCaretToEnd();
    midiMessagesBox.insertTextAtCaret(m + juce::newLine);
}
