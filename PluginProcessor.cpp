#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Drift depth at 100% (the walk is bounded to +/-1, typical excursions are ~0.4 of this)
    constexpr float kMaxGainDriftDb   = 0.6f;   // +/- dB
    constexpr float kMaxPanDriftAngle = 0.09f;  // radians of equal-power pan rotation (~ +/-0.8 dB L/R)
    constexpr float kDbToNeper        = 0.11512925f;

    constexpr float kDcCutoffHz = 5.0f;

    float noiseGainFromPercent (float percent) noexcept
    {
        if (percent <= 0.5f)
            return 0.0f;                                   // off

        // 0.5%..100% -> roughly -96 dBFS .. -60 dBFS (pink, approximate)
        return juce::Decibels::decibelsToGain (-96.0f + 36.0f * (percent * 0.01f));
    }
}

//==============================================================================
ChromaPulseAudioProcessor::ChromaPulseAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    pDrive        = apvts.getRawParameterValue (ParamIDs::drive);
    pCharacter    = apvts.getRawParameterValue (ParamIDs::character);
    pDrift        = apvts.getRawParameterValue (ParamIDs::drift);
    pDriftRate    = apvts.getRawParameterValue (ParamIDs::driftRate);
    pNoise        = apvts.getRawParameterValue (ParamIDs::noise);
    pOutput       = apvts.getRawParameterValue (ParamIDs::output);
    pOversampling = apvts.getRawParameterValue (ParamIDs::oversampling);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ChromaPulseAudioProcessor::createParameterLayout()
{
    using Attr = juce::AudioParameterFloatAttributes;

    const auto parseNumber = [] (const juce::String& s) { return s.getFloatValue(); };

    const auto percent = Attr()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })
        .withValueFromStringFunction (parseNumber);

    const auto hertz = Attr()
        .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })
        .withValueFromStringFunction (parseNumber);

    const auto decibels = Attr()
        .withStringFromValueFunction ([] (float v, int)
                                      { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; })
        .withValueFromStringFunction (parseNumber);

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::drive, 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, percent));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::character, 1 }, "Character (Odd to Even)",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f, percent));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::drift, 1 }, "Drift",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, percent));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::driftRate, 1 }, "Drift Rate",
        juce::NormalisableRange<float> (0.02f, 2.0f, 0.001f, 0.4f), 0.15f, hertz));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::noise, 1 }, "Noise Floor",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, percent));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::output, 1 }, "Output",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, decibels));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::oversampling, 1 }, "Oversampling",
        juce::StringArray { "2x", "4x", "8x" }, 1));

    return layout;
}

//==============================================================================
bool ChromaPulseAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void ChromaPulseAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Build all three oversamplers up front so switching quality never allocates on the audio thread.
    // Constructor arg 2 is the number of 2x stages: 1 -> 2x, 2 -> 4x, 3 -> 8x.
    for (size_t i = 0; i < oversamplers.size(); ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, i + 1,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true,   // max quality
            true);  // integer latency

        oversamplers[i]->initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
    }

    driftSmoother.reset  (sampleRate, 0.05);
    noiseSmoother.reset  (sampleRate, 0.05);
    outputSmoother.reset (sampleRate, 0.02);

    driftSmoother.setCurrentAndTargetValue  (pDrift->load() * 0.01f);
    noiseSmoother.setCurrentAndTargetValue  (noiseGainFromPercent (pNoise->load()));
    outputSmoother.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    gainWalk.reset();
    panWalk.reset();
    gainWalk.setRate (pDriftRate->load(), sampleRate);
    panWalk.setRate  (pDriftRate->load() * 0.7f, sampleRate);

    for (auto& p : pink) p.reset();

    activeOversampling = -1;
    configureOversampling (juce::jlimit (0, 2, (int) std::lround (pOversampling->load())));
}

void ChromaPulseAudioProcessor::configureOversampling (int index)
{
    activeOversampling = index;

    auto& os = *oversamplers[(size_t) index];
    os.reset();

    const int factor = 1 << (index + 1);
    const double osRate = currentSampleRate * (double) factor;

    driveSmoother.reset (osRate, 0.03);
    characterSmoother.reset (osRate, 0.03);
    driveSmoother.setCurrentAndTargetValue (pDrive->load() * 0.01f);
    characterSmoother.setCurrentAndTargetValue (pCharacter->load() * 0.01f);

    dcCoeff = 1.0f - (juce::MathConstants<float>::twoPi * kDcCutoffHz / (float) osRate);
    for (auto& d : dcBlockers) d.reset();

    setLatencySamples (juce::roundToInt (os.getLatencyInSamples()));
}

//==============================================================================
void ChromaPulseAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh      = juce::jmin (buffer.getNumChannels(), 2);

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0 || numCh == 0 || activeOversampling < 0)
        return;

    // --- Oversampling mode change (latency is updated to match) --------------
    const int requestedOs = juce::jlimit (0, 2, (int) std::lround (pOversampling->load()));
    if (requestedOs != activeOversampling)
        configureOversampling (requestedOs);

    // --- Parameter targets ---------------------------------------------------
    driveSmoother.setTargetValue     (pDrive->load() * 0.01f);
    characterSmoother.setTargetValue (pCharacter->load() * 0.01f);
    driftSmoother.setTargetValue     (pDrift->load() * 0.01f);
    noiseSmoother.setTargetValue     (noiseGainFromPercent (pNoise->load()));
    outputSmoother.setTargetValue    (juce::Decibels::decibelsToGain (pOutput->load()));

    gainWalk.setRate (pDriftRate->load(), currentSampleRate);
    panWalk.setRate  (pDriftRate->load() * 0.7f, currentSampleRate);

    float* chan[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    // --- Stage 1: micro-drift (gain + subtle pan), host rate, pre-saturation --
    for (int i = 0; i < numSamples; ++i)
    {
        const float amount = driftSmoother.getNextValue();
        const float g = gainWalk.next();
        const float p = panWalk.next();

        const float gain = std::exp (kDbToNeper * kMaxGainDriftDb * amount * g);

        if (chan[1] != nullptr)
        {
            const float a = kMaxPanDriftAngle * amount * p;
            const float s = std::sin (a), c = std::cos (a);
            chan[0][i] *= gain * (c - s);
            chan[1][i] *= gain * (c + s);
        }
        else
        {
            chan[0][i] *= gain;
        }
    }

    // --- Stage 2: oversampled saturation -------------------------------------
    juce::dsp::AudioBlock<float> block (buffer);
    auto subBlock = block.getSubsetChannelBlock (0, (size_t) numCh);

    auto& os = *oversamplers[(size_t) activeOversampling];
    auto osBlock = os.processSamplesUp (subBlock);

    float* osChan[2] = { osBlock.getChannelPointer (0),
                         numCh > 1 ? osBlock.getChannelPointer (1) : nullptr };
    const size_t osSamples = osBlock.getNumSamples();

    for (size_t s = 0; s < osSamples; ++s)
    {
        const float drive = driveSmoother.getNextValue();
        const float character = characterSmoother.getNextValue();

        // Asymmetric tanh: bias introduces even harmonics, no bias = pure odd.
        // Normalised so small-signal gain stays at unity regardless of drive/character.
        const float g        = 1.5f + 2.5f * drive;
        const float bias     = 0.9f * character;
        const float tb       = std::tanh (bias);
        const float invNorm  = 1.0f / (g * (1.0f - tb * tb));

        for (int c = 0; c < numCh; ++c)
        {
            const float x   = osChan[c][s];
            const float wet = (std::tanh (g * x + bias) - tb) * invNorm;

            // Only the added harmonic content is DC-blocked, so the dry path stays untouched.
            const float delta = dcBlockers[(size_t) c].process (wet - x, dcCoeff);
            osChan[c][s] = x + drive * delta;
        }
    }

    os.processSamplesDown (subBlock);

    // --- Stage 3: output trim + optional analog noise floor (host rate) ------
    for (int i = 0; i < numSamples; ++i)
    {
        const float outGain   = outputSmoother.getNextValue();
        const float noiseGain = noiseSmoother.getNextValue();

        for (int c = 0; c < numCh; ++c)
        {
            float v = chan[c][i] * outGain;

            if (noiseGain > 0.0f)
                v += pink[(size_t) c].next() * noiseGain;

            chan[c][i] = v;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* ChromaPulseAudioProcessor::createEditor()
{
    return new ChromaPulseAudioProcessorEditor (*this);
}

void ChromaPulseAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ChromaPulseAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChromaPulseAudioProcessor();
}
