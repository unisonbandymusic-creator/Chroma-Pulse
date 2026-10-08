// Chroma-Pulse - Him'z DSP

#pragma once

#include "PluginProcessor.h"

#include <array>
#include <memory>

//==============================================================================
class ChromaPulseLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChromaPulseLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    static juce::Colour background() { return juce::Colour (0xff121418); }
    static juce::Colour panel()      { return juce::Colour (0xff1b1e24); }
    static juce::Colour accent()     { return juce::Colour (0xff2fd6c4); }
    static juce::Colour accentAlt()  { return juce::Colour (0xffe0559c); }
    static juce::Colour textDim()    { return juce::Colour (0xff9aa4b2); }
};

//==============================================================================
class ChromaPulseAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChromaPulseAudioProcessorEditor (ChromaPulseAudioProcessor&);
    ~ChromaPulseAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment; // destroyed before slider
    };

    ChromaPulseAudioProcessor& chromaProc;
    ChromaPulseLookAndFeel lnf;

    static constexpr size_t numKnobs = 6;
    std::array<Knob, numKnobs> knobs;

    juce::Label osLabel;
    juce::ComboBox osBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaPulseAudioProcessorEditor)
};
