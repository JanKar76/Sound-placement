#pragma once

#include "PluginProcessor.h"

//==============================================================================
namespace Theme
{
    const juce::Colour windowBg   { 0xff141619 };
    const juce::Colour panelBg    { 0xff1a1d21 };
    const juce::Colour cardBg     { 0xff212429 };
    const juce::Colour outline    { 0xff2e3238 };
    const juce::Colour gridLine   { 0xff23262b };
    const juce::Colour accent     { 0xff4dbf9e };  // mint green
    const juce::Colour accentSide { 0xffe09b4d };  // orange (side/S in the reference)
    const juce::Colour text       { 0xffd0d4d9 };
    const juce::Colour textDim    { 0xff8b929b };
}

//==============================================================================
class SoundPlacementLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SoundPlacementLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
};

//==============================================================================
/** 2D pad: the room seen from above. X = pan, Y = distance (up = further away). */
class RoomPad : public juce::Component,
                private juce::AudioProcessorValueTreeState::Listener,
                private juce::AsyncUpdater
{
public:
    explicit RoomPad (juce::AudioProcessorValueTreeState& state);
    ~RoomPad() override;

    void paint (juce::Graphics&) override;
    void mouseDown  (const juce::MouseEvent&) override;
    void mouseDrag  (const juce::MouseEvent&) override;
    void mouseUp    (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit  (const juce::MouseEvent&) override;

    /** Room dimensions, used only for the readout display. The distance shown
        is the straight line from the listener (bottom centre) to the source. */
    static constexpr float roomDepthMetres = 10.0f;
    static constexpr float roomWidthMetres = 10.0f;

private:
    void drawReadout (juce::Graphics&, juce::Rectangle<float> padBounds,
                      juce::Point<float> dot) const;

    bool mouseIsOver = false;
    void parameterChanged (const juce::String&, float) override;
    void handleAsyncUpdate() override { repaint(); }
    void updateFromMouse (const juce::MouseEvent&);
    juce::Point<float> dotPosition() const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::RangedAudioParameter& panParam;
    juce::RangedAudioParameter& distParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomPad)
};

//==============================================================================
/** Rounded card with a title, a rotary knob and a value box - like the
    band strips in the reference design. */
class KnobCard : public juce::Component
{
public:
    KnobCard (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
              const juce::String& titleText, juce::Colour accentColour);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label title;
    juce::Slider knob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobCard)
};

//==============================================================================
class SoundPlacementEditor : public juce::AudioProcessorEditor
{
public:
    explicit SoundPlacementEditor (SoundPlacementProcessor&);
    ~SoundPlacementEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SoundPlacementLookAndFeel lookAndFeel;

    SoundPlacementProcessor& processor;

    RoomPad pad;
    juce::OwnedArray<KnobCard> cards;
    juce::TextButton bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundPlacementEditor)
};
