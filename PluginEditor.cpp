#include "PluginEditor.h"

//==============================================================================
ChromaPulseLookAndFeel::ChromaPulseLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       juce::Colours::white.withAlpha (0.85f));
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  accent().withAlpha (0.4f));

    setColour (juce::Label::textColourId,               textDim());

    setColour (juce::ComboBox::backgroundColourId,      panel());
    setColour (juce::ComboBox::textColourId,            juce::Colours::white);
    setColour (juce::ComboBox::outlineColourId,         accent().withAlpha (0.5f));
    setColour (juce::ComboBox::arrowColourId,           accent());

    setColour (juce::PopupMenu::backgroundColourId,            panel());
    setColour (juce::PopupMenu::textColourId,                  juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent().withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
}

void ChromaPulseLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float sliderPos, float startAngle, float endAngle,
                                               juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (8.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float arcWidth = 3.5f;
    const float arcRadius = radius - arcWidth;

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2e36));
    g.strokePath (track, juce::PathStrokeType (arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc
    if (slider.isEnabled())
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setGradientFill (juce::ColourGradient (accent(), centre.x - arcRadius, centre.y,
                                                 accentAlt(), centre.x + arcRadius, centre.y, false));
        g.strokePath (value, juce::PathStrokeType (arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Knob body
    const float bodyRadius = arcRadius - 8.0f;
    g.setColour (panel());
    g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

    // Pointer
    juce::Path pointer;
    const float pointerLength = bodyRadius * 0.6f;
    pointer.addRoundedRectangle (-1.5f, -bodyRadius + 3.0f, 3.0f, pointerLength, 1.5f);
    g.setColour (juce::Colours::white);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

//==============================================================================
namespace
{
    struct KnobSpec
    {
        const char* paramId;
        const char* title;
    };

    constexpr std::array<KnobSpec, 6> knobSpecs {{
        { ParamIDs::drive,     "DRIVE"     },
        { ParamIDs::character, "CHARACTER" },
        { ParamIDs::drift,     "DRIFT"     },
        { ParamIDs::driftRate, "RATE"      },
        { ParamIDs::noise,     "NOISE"     },
        { ParamIDs::output,    "OUTPUT"    }
    }};
}

ChromaPulseAudioProcessorEditor::ChromaPulseAudioProcessorEditor (ChromaPulseAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), chromaProc (p)
{
    setLookAndFeel (&lnf);

    for (size_t i = 0; i < numKnobs; ++i)
    {
        auto& k = knobs[i];

        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 20);
        addAndMakeVisible (k.slider);

        k.label.setText (knobSpecs[i].title, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        addAndMakeVisible (k.label);

        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            chromaProc.apvts, knobSpecs[i].paramId, k.slider);
    }

    osLabel.setText ("OVERSAMPLING", juce::dontSendNotification);
    osLabel.setJustificationType (juce::Justification::centredRight);
    osLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    addAndMakeVisible (osLabel);

    osBox.addItemList (juce::StringArray { "2x", "4x", "8x" }, 1);
    addAndMakeVisible (osBox);
    osAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        chromaProc.apvts, ParamIDs::oversampling, osBox);

    setSize (640, 320);
}

ChromaPulseAudioProcessorEditor::~ChromaPulseAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ChromaPulseAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (ChromaPulseLookAndFeel::background());

    // Subtle top glow
    g.setGradientFill (juce::ColourGradient (ChromaPulseLookAndFeel::accent().withAlpha (0.10f), 0.0f, 0.0f,
                                             juce::Colours::transparentBlack, 0.0f, 120.0f, false));
    g.fillRect (getLocalBounds().removeFromTop (120));

    auto header = getLocalBounds().reduced (24, 16).removeFromTop (48);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("CHROMA-PULSE", header.removeFromTop (30), juce::Justification::centredLeft);

    g.setColour (ChromaPulseLookAndFeel::textDim());
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("Him'z DSP  |  bus finisher", header, juce::Justification::centredLeft);
}

void ChromaPulseAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (24, 16);

    auto header = area.removeFromTop (48);
    osBox.setBounds (header.removeFromRight (70).withSizeKeepingCentre (70, 26));
    header.removeFromRight (8);
    osLabel.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 26));

    area.removeFromTop (14);

    const int colWidth = area.getWidth() / (int) numKnobs;
    for (size_t i = 0; i < numKnobs; ++i)
    {
        auto col = area.removeFromLeft (colWidth);
        knobs[i].label.setBounds (col.removeFromTop (20));
        knobs[i].slider.setBounds (col);
    }
}
