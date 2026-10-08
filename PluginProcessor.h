// Chroma-Pulse - Him'z DSP
// Minimalist set-and-forget bus finisher: oversampled saturation, organic micro-drift, optional noise floor.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>

namespace ParamIDs
{
    inline constexpr const char* drive        = "drive";
    inline constexpr const char* character    = "character";
    inline constexpr const char* drift        = "drift";
    inline constexpr const char* driftRate    = "driftRate";
    inline constexpr const char* noise        = "noise";
    inline constexpr const char* output       = "output";
    inline constexpr const char* oversampling = "oversampling";
}

namespace chroma
{
    /** Slow, bounded random walk (mean-reverting, Ornstein-Uhlenbeck style).
        Output is roughly unit-variance noise low-passed at `rateHz`, scaled and clamped to [-1, 1]. */
    struct RandomWalk
    {
        explicit RandomWalk (juce::int64 seed = 1) : rng (seed) {}

        void setRate (float rateHz, double sampleRate) noexcept
        {
            theta = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * rateHz / (float) sampleRate);
            sigma = std::sqrt (juce::jmax (0.0f, 6.0f * theta - 3.0f * theta * theta));
        }

        void reset() noexcept { x = 0.0f; }

        float next() noexcept
        {
            x = (1.0f - theta) * x + sigma * (rng.nextFloat() * 2.0f - 1.0f);
            return juce::jlimit (-1.0f, 1.0f, x * 0.4f);
        }

    private:
        juce::Random rng;
        float x = 0.0f, theta = 1.0e-4f, sigma = 0.0f;
    };

    /** Paul Kellet's economy pink-noise filter. */
    struct PinkNoise
    {
        explicit PinkNoise (juce::int64 seed = 1) : rng (seed) {}

        void reset() noexcept { b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f; }

        float next() noexcept
        {
            const float white = rng.nextFloat() * 2.0f - 1.0f;
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            b3 = 0.86650f * b3 + white * 0.3104856f;
            b4 = 0.55000f * b4 + white * 0.5329522f;
            b5 = -0.7616f * b5 - white * 0.0168980f;
            const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
            b6 = white * 0.115926f;
            return pink * 0.11f;
        }

    private:
        juce::Random rng;
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    };

    /** One-pole DC blocker. */
    struct DcBlocker
    {
        void reset() noexcept { x1 = y1 = 0.0f; }

        float process (float x, float R) noexcept
        {
            const float y = x - x1 + R * y1;
            x1 = x;
            y1 = y;
            return y;
        }

    private:
        float x1 = 0.0f, y1 = 0.0f;
    };
}

class ChromaPulseAudioProcessor : public juce::AudioProcessor
{
public:
    ChromaPulseAudioProcessor();
    ~ChromaPulseAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Chroma-Pulse"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    void configureOversampling (int index);

    // Raw parameter pointers (lock-free reads on the audio thread)
    std::atomic<float>* pDrive        = nullptr;
    std::atomic<float>* pCharacter    = nullptr;
    std::atomic<float>* pDrift        = nullptr;
    std::atomic<float>* pDriftRate    = nullptr;
    std::atomic<float>* pNoise        = nullptr;
    std::atomic<float>* pOutput       = nullptr;
    std::atomic<float>* pOversampling = nullptr;

    // One pre-built oversampler per setting: 2x / 4x / 8x
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    int activeOversampling = -1;

    double currentSampleRate = 44100.0;

    // Smoothers running at the oversampled rate
    juce::SmoothedValue<float> driveSmoother, characterSmoother;
    // Smoothers running at the host rate
    juce::SmoothedValue<float> driftSmoother, noiseSmoother, outputSmoother;

    // Micro-drift
    chroma::RandomWalk gainWalk { 0x5EED01 }, panWalk { 0x5EED02 };

    // Noise floor (independent per channel)
    std::array<chroma::PinkNoise, 2> pink { chroma::PinkNoise { 11 }, chroma::PinkNoise { 23 } };

    // DC blocking for the asymmetric (even-harmonic) waveshaper, at the oversampled rate
    std::array<chroma::DcBlocker, 2> dcBlockers;
    float dcCoeff = 0.999f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaPulseAudioProcessor)
};
