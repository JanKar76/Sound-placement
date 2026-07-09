#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Distance 0..1 -> attenuation in dB (0 dB near, -15 dB at the back)
    constexpr float maxAttenuationDb = -15.0f;

    // Distance 0..1 -> lowpass cutoff (exponential, 20 kHz near, 1.8 kHz far)
    float distanceToCutoff (float d)
    {
        return 20000.0f * std::pow (1800.0f / 20000.0f, d);
    }

    // Distance 0..1 -> reverb predelay in ms.
    // Near sources: big gap between direct sound and the room. Far: almost none.
    float distanceToPredelayMs (float d)
    {
        return 5.0f + 25.0f * (1.0f - d);
    }

    // Rev Tone 0..1 -> wet lowpass cutoff (dark 2 kHz .. neutral 20 kHz)
    float toneToCutoff (float t)
    {
        return 2000.0f * std::pow (10.0f, t);
    }
}

//==============================================================================
constexpr float EarlyReflections::tapTimesMs[2][EarlyReflections::numTaps];
constexpr float EarlyReflections::tapGains[EarlyReflections::numTaps];

void EarlyReflections::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    bufferLength = (int) std::ceil (sr * 0.25); // max 250 ms

    delayBuffers.assign ((size_t) numChannels, std::vector<float> ((size_t) bufferLength, 0.0f));
    writePos.assign ((size_t) numChannels, 0);
}

void EarlyReflections::reset()
{
    for (auto& b : delayBuffers)
        std::fill (b.begin(), b.end(), 0.0f);
}

void EarlyReflections::process (juce::AudioBuffer<float>& buffer, float level, float sizeScale)
{
    if (bufferLength == 0)
        return;

    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) delayBuffers.size());
    const float timeScale = 0.4f + 1.1f * sizeScale; // small room = tight, big = spread out

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& delay = delayBuffers[(size_t) ch];
        int wp = writePos[(size_t) ch];

        int tapDelays[numTaps];
        for (int t = 0; t < numTaps; ++t)
            tapDelays[t] = juce::jlimit (1, bufferLength - 1,
                                         (int) (tapTimesMs[ch % 2][t] * 0.001f * timeScale * (float) sr));

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            delay[(size_t) wp] = data[i];

            float refl = 0.0f;
            for (int t = 0; t < numTaps; ++t)
            {
                const int rp = (wp - tapDelays[t] + bufferLength) % bufferLength;
                refl += tapGains[t] * delay[(size_t) rp];
            }

            data[i] += level * refl;
            wp = (wp + 1) % bufferLength;
        }

        writePos[(size_t) ch] = wp;
    }
}

//==============================================================================
SoundPlacementProcessor::SoundPlacementProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    panParam       = apvts.getRawParameterValue ("pan");
    distanceParam  = apvts.getRawParameterValue ("distance");
    roomSizeParam  = apvts.getRawParameterValue ("roomSize");
    erLevelParam   = apvts.getRawParameterValue ("erLevel");
    midGainParam   = apvts.getRawParameterValue ("midGain");
    sideGainParam  = apvts.getRawParameterValue ("sideGain");
    outGainParam   = apvts.getRawParameterValue ("outGain");
    mixParam       = apvts.getRawParameterValue ("mix");
    revLowCutParam = apvts.getRawParameterValue ("revLowCut");
    revToneParam   = apvts.getRawParameterValue ("revTone");
    duckParam      = apvts.getRawParameterValue ("duck");
    depthParam     = apvts.getRawParameterValue ("depth");
    bypassParam    = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    wetSoloParam   = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("wetSolo"));
    monoParam      = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("mono"));

    startTimerHz (10); // watches Room Size and rebuilds the IR when it changes
}

SoundPlacementProcessor::~SoundPlacementProcessor()
{
    stopTimer();
}

juce::AudioProcessorValueTreeState::ParameterLayout SoundPlacementProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const juce::NormalisableRange<float> msGainRange (-60.0f, 6.0f, 0.1f, 3.0f);
    juce::NormalisableRange<float> lowCutRange (20.0f, 500.0f, 1.0f);
    lowCutRange.setSkewForCentre (120.0f);

    layout.add (std::make_unique<P> (juce::ParameterID { "pan", 1 },
                                     "Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "distance", 1 },
                                     "Distance", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "roomSize", 1 },
                                     "Room Size", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    layout.add (std::make_unique<P> (juce::ParameterID { "erLevel", 1 },
                                     "Early Reflections", juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f));
    layout.add (std::make_unique<P> (juce::ParameterID { "midGain", 1 },
                                     "Mid Level", msGainRange, 0.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "sideGain", 1 },
                                     "Side Level", msGainRange, 0.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "outGain", 1 },
                                     "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "mix", 1 },
                                     "Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "revLowCut", 1 },
                                     "Reverb Low Cut", lowCutRange, 100.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "revTone", 1 },
                                     "Reverb Tone", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "duck", 1 },
                                     "Reverb Duck", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<P> (juce::ParameterID { "depth", 1 },
                                     "Depth Amount", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bypass", 1 },
                                                            "Bypass", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "wetSolo", 1 },
                                                            "Wet Solo", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "mono", 1 },
                                                            "Mono Check", false));
    return layout;
}

//==============================================================================
// A/B compare + factory presets

void SoundPlacementProcessor::switchSlot (int newSlot)
{
    newSlot = juce::jlimit (0, 1, newSlot);
    if (newSlot == activeSlot)
        return;

    slots[activeSlot] = apvts.copyState();

    if (slots[newSlot].isValid())
        apvts.replaceState (slots[newSlot].createCopy());

    activeSlot = newSlot;
}

void SoundPlacementProcessor::copyActiveSlotToOther()
{
    slots[1 - activeSlot] = apvts.copyState();
}

const std::vector<FactoryPreset>& SoundPlacementProcessor::getFactoryPresets()
{
    // pan, distance, roomSize, erLevel, midGain, sideGain, outGain, mix,
    // revLowCut, revTone, duck, depth
    static const std::vector<FactoryPreset> presets = {
        { "Default",        {  0.0f, 0.00f, 0.50f, 0.40f, 0.0f,  0.0f, 0.0f, 1.0f, 100.0f, 1.00f, 0.0f,  1.0f } },
        { "Subtle Depth",   {  0.0f, 0.15f, 0.40f, 0.30f, 0.0f,  0.0f, 0.0f, 1.0f, 120.0f, 0.90f, 0.2f,  0.7f } },
        { "Vocal Back",     {  0.0f, 0.35f, 0.45f, 0.45f, 0.0f,  0.0f, 0.0f, 1.0f, 150.0f, 0.85f, 0.4f,  1.0f } },
        { "Backing Vocals", {  0.0f, 0.45f, 0.50f, 0.40f, 0.0f, -3.0f, 0.0f, 1.0f, 180.0f, 0.80f, 0.3f,  1.0f } },
        { "Drum Room",      {  0.0f, 0.50f, 0.35f, 0.70f, 0.0f,  0.0f, 0.0f, 1.0f, 120.0f, 0.70f, 0.0f,  1.0f } },
        { "Tight Booth",    {  0.0f, 0.25f, 0.10f, 0.55f, 0.0f,  0.0f, 0.0f, 1.0f, 200.0f, 0.80f, 0.0f,  1.0f } },
        { "Big Hall",       {  0.0f, 0.60f, 1.00f, 0.50f, 0.0f,  0.0f, 0.0f, 1.0f, 100.0f, 0.75f, 0.3f,  1.0f } },
        { "Far Away",       {  0.0f, 0.90f, 0.80f, 0.60f, 0.0f,  0.0f, 0.0f, 1.0f, 100.0f, 0.60f, 0.0f,  1.0f } },
    };
    return presets;
}

void SoundPlacementProcessor::applyPreset (int index)
{
    const auto& presets = getFactoryPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    static const char* ids[12] = { "pan", "distance", "roomSize", "erLevel",
                                   "midGain", "sideGain", "outGain", "mix",
                                   "revLowCut", "revTone", "duck", "depth" };

    for (int i = 0; i < 12; ++i)
    {
        if (auto* p = apvts.getParameter (ids[i]))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (presets[(size_t) index].values[i]));
            p->endChangeGesture();
        }
    }
}

//==============================================================================
void SoundPlacementProcessor::timerCallback()
{
    const float size = roomSizeParam->load();

    if (std::abs (size - loadedRoomSize) > 0.01f && getSampleRate() > 0)
    {
        loadedRoomSize = size;
        updateImpulseResponse();
    }
}

void SoundPlacementProcessor::updateImpulseResponse()
{
    const double sr = getSampleRate();
    if (sr <= 0)
        return;

    const float size         = roomSizeParam->load();
    const float decaySeconds = 0.4f + 2.6f * size;              // RT60: 0.4 s .. 3.0 s
    const int   length       = juce::jmax (64, (int) (sr * decaySeconds));
    const int   fadeIn       = juce::jmax (8,  (int) (sr * 0.002)); // 2 ms fade-in

    juce::AudioBuffer<float> ir (2, length);

    for (int ch = 0; ch < 2; ++ch)
    {
        juce::Random rng (0x5eed + ch); // deterministic, decorrelated per channel
        auto* data = ir.getWritePointer (ch);
        float lp = 0.0f;

        for (int i = 0; i < length; ++i)
        {
            const float progress = (float) i / (float) length;

            // -60 dB at the end of the buffer
            const float envelope = std::exp (-6.91f * progress);

            // Progressively darker tail: one-pole lowpass whose smoothing
            // increases over time (air + wall absorption)
            const float noise = rng.nextFloat() * 2.0f - 1.0f;
            const float coeff = juce::jmap (progress, 0.25f, 0.92f);
            lp += (noise - lp) * (1.0f - coeff);

            float sample = lp * envelope;

            if (i < fadeIn)
                sample *= (float) i / (float) fadeIn;

            data[i] = sample;
        }
    }

    convolution.loadImpulseResponse (std::move (ir), sr,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::no,
                                     juce::dsp::Convolution::Normalise::yes);
}

//==============================================================================
void SoundPlacementProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) samplesPerBlock,
                                  (juce::uint32) getTotalNumOutputChannels() };

    distanceGain.prepare (spec);
    distanceGain.setRampDurationSeconds (0.05);

    lowpass.prepare (spec);
    lowpass.setType (juce::dsp::StateVariableTPTFilter<float>::Type::lowpass);
    lowpass.setCutoffFrequency (20000.0f);

    panner.prepare (spec);
    panner.setRule (juce::dsp::PannerRule::balanced);

    earlyReflections.prepare (sampleRate, (int) spec.numChannels);

    convolution.prepare (spec);
    loadedRoomSize = roomSizeParam->load();
    updateImpulseResponse();

    predelay.setMaximumDelayInSamples ((int) std::ceil (sampleRate * 0.06) + samplesPerBlock);
    predelay.prepare (spec);

    wetLowCut.prepare (spec);
    wetLowCut.setType (juce::dsp::StateVariableTPTFilter<float>::Type::highpass);
    wetLowCut.setCutoffFrequency (100.0f);

    wetTone.prepare (spec);
    wetTone.setType (juce::dsp::StateVariableTPTFilter<float>::Type::lowpass);
    wetTone.setCutoffFrequency (20000.0f);

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.05);

    wetBuffer.setSize ((int) spec.numChannels, samplesPerBlock);
    dryInputBuffer.setSize ((int) spec.numChannels, samplesPerBlock);

    duckEnvelope = 0.0f;
    duckAttack   = std::exp (-1.0f / (0.005f * (float) sampleRate));
    duckRelease  = std::exp (-1.0f / (0.150f * (float) sampleRate));

    auto initSmoothed = [sampleRate] (juce::SmoothedValue<float>& sv, float value, double seconds = 0.05)
    {
        sv.reset (sampleRate, seconds);
        sv.setCurrentAndTargetValue (value);
    };

    initSmoothed (smoothedCutoff,   20000.0f);
    initSmoothed (smoothedWidth,    1.0f);
    initSmoothed (smoothedMidGain,  1.0f);
    initSmoothed (smoothedSideGain, 1.0f);
    initSmoothed (smoothedPredelay, distanceToPredelayMs (0.0f), 0.1);
    initSmoothed (smoothedMix,      1.0f);
    initSmoothed (smoothedRevDry,   1.0f);
    initSmoothed (smoothedRevWet,   0.0f);
    initSmoothed (smoothedLowCut,   100.0f);
    initSmoothed (smoothedTone,     20000.0f);
}

bool SoundPlacementProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Stereo out; mono or stereo in
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void SoundPlacementProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n           = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Mono in -> copy to both channels (also when bypassed, to keep output stereo)
    if (getTotalNumInputChannels() == 1 && numChannels > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    atomicMax (inPeak, buffer.getMagnitude (0, n));

    if (bypassParam != nullptr && bypassParam->get())
    {
        atomicMax (outPeak, buffer.getMagnitude (0, n));
        return;
    }

    const float depth    = depthParam->load();
    const float pan      = panParam->load();
    const float effDist  = distanceParam->load() * depth; // depth macro scales every cue
    const float roomSize = roomSizeParam->load();
    const float erLevel  = erLevelParam->load();
    const float duckAmt  = duckParam->load();
    const bool  wetSolo  = wetSoloParam != nullptr && wetSoloParam->get();

    // Keep the untouched input for the global dry/wet mix
    for (int ch = 0; ch < numChannels; ++ch)
        dryInputBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // 1) Level: attenuate with distance
    distanceGain.setGainDecibels (maxAttenuationDb * effDist);
    distanceGain.process (ctx);

    // 2) Lowpass: distant sources sound duller
    smoothedCutoff.setTargetValue (distanceToCutoff (effDist));
    lowpass.setCutoffFrequency (smoothedCutoff.skip (n));
    lowpass.process (ctx);

    // 3) Mid/side: user gains + width narrowing with distance
    smoothedWidth.setTargetValue (1.0f - 0.6f * effDist);
    smoothedMidGain.setTargetValue (juce::Decibels::decibelsToGain (midGainParam->load(), -60.0f));
    smoothedSideGain.setTargetValue (juce::Decibels::decibelsToGain (sideGainParam->load(), -60.0f));

    if (numChannels >= 2)
    {
        const float width    = smoothedWidth.skip (n);
        const float midGain  = smoothedMidGain.skip (n);
        const float sideGain = smoothedSideGain.skip (n);

        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            const float mid  = 0.5f * (l[i] + r[i]) * midGain;
            const float side = 0.5f * (l[i] - r[i]) * sideGain * width;
            l[i] = mid + side;
            r[i] = mid - side;
        }
    }

    // 4) Panning
    panner.setPan (pan);
    panner.process (ctx);

    // 5) Early reflections: level rises with distance (depth cue)
    earlyReflections.process (buffer, erLevel * (0.25f + 0.75f * effDist), roomSize);

    // 6) Convolution reverb (inherently 100% wet) on a copy, with
    //    distance-linked predelay. Room Size regenerates the IR via the timer.
    for (int ch = 0; ch < numChannels; ++ch)
        wetBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

    juce::dsp::AudioBlock<float> wetBlock (wetBuffer);
    auto wetSub = wetBlock.getSubBlock (0, (size_t) n);
    juce::dsp::ProcessContextReplacing<float> wetCtx (wetSub);

    convolution.process (wetCtx);

    smoothedPredelay.setTargetValue (distanceToPredelayMs (effDist));
    predelay.setDelay (smoothedPredelay.skip (n) * 0.001f * (float) getSampleRate());
    predelay.process (wetCtx);

    // 7) Wet path shaping: low cut (mud control) + tone (tail darkness)
    smoothedLowCut.setTargetValue (revLowCutParam->load());
    wetLowCut.setCutoffFrequency (smoothedLowCut.skip (n));
    wetLowCut.process (wetCtx);

    smoothedTone.setTargetValue (toneToCutoff (revToneParam->load()));
    wetTone.setCutoffFrequency (smoothedTone.skip (n));
    wetTone.process (wetCtx);

    // 8) Ducking: the direct signal pushes the reverb down, it blooms in gaps
    if (duckAmt > 0.001f && numChannels >= 2)
    {
        auto* dl = buffer.getReadPointer (0);
        auto* dr = buffer.getReadPointer (1);
        auto* wl = wetBuffer.getWritePointer (0);
        auto* wr = wetBuffer.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            const float x = juce::jmax (std::abs (dl[i]), std::abs (dr[i]));
            const float coeff = x > duckEnvelope ? duckAttack : duckRelease;
            duckEnvelope = coeff * duckEnvelope + (1.0f - coeff) * x;

            const float g = juce::jmax (0.05f, 1.0f - duckAmt * juce::jmin (1.0f, duckEnvelope * 2.5f));
            wl[i] *= g;
            wr[i] *= g;
        }
    }

    // 9) Blend: dry path + distance-driven amount of the wet room
    smoothedRevDry.setTargetValue (1.0f - 0.35f * effDist);
    smoothedRevWet.setTargetValue (0.75f * effDist);
    const float revDry = smoothedRevDry.skip (n);
    const float revWet = smoothedRevWet.skip (n);

    if (wetSolo)
    {
        // Monitor the reverb path at unity gain, regardless of distance
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, wetBuffer, ch, 0, n);
    }
    else
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.applyGain (ch, 0, n, revDry);
            buffer.addFrom (ch, 0, wetBuffer, ch, 0, n, revWet);
        }
    }

    // 10) Output gain
    outputGain.setGainDecibels (outGainParam->load());
    outputGain.process (ctx);

    // 11) Global dry/wet mix against the untouched input (skipped in wet solo)
    smoothedMix.setTargetValue (mixParam->load());
    const float mix = smoothedMix.skip (n);

    if (! wetSolo && mix < 1.0f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.applyGain (ch, 0, n, mix);
            buffer.addFrom (ch, 0, dryInputBuffer, ch, 0, n, 1.0f - mix);
        }
    }

    // 12) Mono compatibility check: collapse to the mid signal
    if (monoParam != nullptr && monoParam->get() && numChannels >= 2)
    {
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            l[i] = m;
            r[i] = m;
        }
    }

    atomicMax (outPeak, buffer.getMagnitude (0, n));
}

//==============================================================================
void SoundPlacementProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SoundPlacementProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SoundPlacementProcessor::createEditor()
{
    return new SoundPlacementEditor (*this);
}

// Factory function required by JUCE
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoundPlacementProcessor();
}
