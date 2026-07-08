#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** Simple multi-tap early reflections. Tap times scale with room size,
    output level scales with distance so it acts as a depth cue. */
class EarlyReflections
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    /** level 0..1, sizeScale 0..1 (room size). Adds reflections onto the buffer. */
    void process (juce::AudioBuffer<float>& buffer, float level, float sizeScale);

private:
    static constexpr int numTaps = 5;
    // Slightly different tap times per channel for decorrelation (ms)
    static constexpr float tapTimesMs[2][numTaps] = { { 13.0f, 29.0f, 47.0f, 71.0f, 97.0f },
                                                      { 17.0f, 31.0f, 53.0f, 79.0f, 103.0f } };
    static constexpr float tapGains[numTaps] = { 0.80f, 0.62f, 0.48f, 0.36f, 0.27f };

    double sr = 44100.0;
    std::vector<std::vector<float>> delayBuffers;
    std::vector<int> writePos;
    int bufferLength = 0;
};

//==============================================================================
class SoundPlacementProcessor : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    SoundPlacementProcessor();
    ~SoundPlacementProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                       { return true; }

    const juce::String getName() const override           { return "Sound Placement"; }
    bool acceptsMidi() const override                     { return false; }
    bool producesMidi() const override                    { return false; }
    double getTailLengthSeconds() const override          { return 3.0; }

    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** Builds a synthetic stereo impulse response (exponentially decaying,
        progressively lowpassed noise) and hands it to the convolution engine.
        Called from the message thread; Convolution swaps it in on a
        background thread without glitching the audio. */
    void updateImpulseResponse();
    void timerCallback() override;

    // DSP chain
    juce::dsp::Gain<float>                    distanceGain;
    juce::dsp::StateVariableTPTFilter<float>  lowpass;
    juce::dsp::Panner<float>                  panner;
    EarlyReflections                          earlyReflections;
    juce::dsp::Convolution                    convolution;
    float loadedRoomSize = -1.0f;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> predelay { 8192 };
    juce::dsp::Gain<float>                    outputGain;

    juce::AudioBuffer<float> wetBuffer, dryInputBuffer;

    juce::SmoothedValue<float> smoothedCutoff, smoothedWidth,
                               smoothedMidGain, smoothedSideGain,
                               smoothedPredelay, smoothedMix,
                               smoothedRevDry, smoothedRevWet;

    std::atomic<float>* panParam      = nullptr;  // -1..1
    std::atomic<float>* distanceParam = nullptr;  //  0..1
    std::atomic<float>* roomSizeParam = nullptr;  //  0..1
    std::atomic<float>* erLevelParam  = nullptr;  //  0..1
    std::atomic<float>* midGainParam  = nullptr;  //  dB
    std::atomic<float>* sideGainParam = nullptr;  //  dB
    std::atomic<float>* outGainParam  = nullptr;  //  dB
    std::atomic<float>* mixParam      = nullptr;  //  0..1
    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundPlacementProcessor)
};
