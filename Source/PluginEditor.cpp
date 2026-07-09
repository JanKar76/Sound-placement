#include "PluginEditor.h"

//==============================================================================
SoundPlacementLookAndFeel::SoundPlacementLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Theme::windowBg);

    setColour (juce::Slider::textBoxTextColourId,       Theme::text);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  Theme::accent.withAlpha (0.4f));

    setColour (juce::Label::textColourId, Theme::textDim);

    setColour (juce::TextButton::buttonColourId,   Theme::cardBg);
    setColour (juce::TextButton::buttonOnColourId, Theme::accent);
    setColour (juce::TextButton::textColourOffId,  Theme::textDim);
    setColour (juce::TextButton::textColourOnId,   Theme::windowBg);

    setColour (juce::ComboBox::backgroundColourId, Theme::cardBg);
    setColour (juce::ComboBox::textColourId,       Theme::text);
    setColour (juce::ComboBox::outlineColourId,    Theme::outline);
    setColour (juce::ComboBox::arrowColourId,      Theme::textDim);
    setColour (juce::ComboBox::buttonColourId,     Theme::cardBg);

    setColour (juce::PopupMenu::backgroundColourId,            Theme::cardBg);
    setColour (juce::PopupMenu::textColourId,                  Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent);
    setColour (juce::PopupMenu::highlightedTextColourId,       Theme::windowBg);

    setColour (juce::TextEditor::textColourId, Theme::text);
    setColour (juce::TextEditor::highlightColourId, Theme::accent.withAlpha (0.4f));
    setColour (juce::CaretComponent::caretColourId, Theme::accent);
}

void SoundPlacementLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                  float sliderPos, float rotaryStartAngle,
                                                  float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float thickness = juce::jmax (2.5f, radius * 0.14f);
    const float arcRadius = radius - thickness * 0.5f;

    // Background track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (Theme::outline);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc in the slider's accent colour
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Pointer
    const float pointerLength = arcRadius * 0.45f;
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.2f, -arcRadius + thickness, 2.4f, pointerLength, 1.2f);
    g.setColour (Theme::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void SoundPlacementLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                      const juce::Colour& backgroundColour,
                                                      bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = backgroundColour;

    if (down)             colour = colour.brighter (0.15f);
    else if (highlighted) colour = colour.brighter (0.08f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (Theme::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
}

//==============================================================================
RoomPad::RoomPad (juce::AudioProcessorValueTreeState& state)
    : apvts (state),
      panParam  (*state.getParameter ("pan")),
      distParam (*state.getParameter ("distance"))
{
    apvts.addParameterListener ("pan", this);
    apvts.addParameterListener ("distance", this);
}

RoomPad::~RoomPad()
{
    apvts.removeParameterListener ("pan", this);
    apvts.removeParameterListener ("distance", this);
}

void RoomPad::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate(); // repaint on the message thread
}

juce::Point<float> RoomPad::dotPosition() const
{
    const auto bounds = getLocalBounds().toFloat().reduced (14.0f);
    const float pan  = panParam.convertFrom0to1 (panParam.getValue());   // -1..1
    const float dist = distParam.getValue();                             //  0..1 (already normalised)

    return { bounds.getX() + bounds.getWidth()  * (pan + 1.0f) * 0.5f,
             bounds.getBottom() - bounds.getHeight() * dist };           // bottom edge = near
}

void RoomPad::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f);

    // Display panel, like the analyser area in the reference
    g.setColour (Theme::panelBg);
    g.fillRoundedRectangle (bounds, 6.0f);

    // Grid
    g.setColour (Theme::gridLine);
    for (int i = 1; i < 8; ++i)
    {
        const float x = bounds.getX() + bounds.getWidth() * (float) i / 8.0f;
        g.drawVerticalLine ((int) x, bounds.getY() + 4.0f, bounds.getBottom() - 4.0f);
    }
    for (int i = 1; i < 6; ++i)
    {
        const float y = bounds.getY() + bounds.getHeight() * (float) i / 6.0f;
        g.drawHorizontalLine ((int) y, bounds.getX() + 4.0f, bounds.getRight() - 4.0f);
    }

    g.setColour (Theme::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    // Legend, echoing the MID/SIDE legend in the reference
    g.setFont (11.0f);
    g.setColour (Theme::accent);
    g.fillEllipse (bounds.getX() + 12.0f, bounds.getY() + 12.0f, 6.0f, 6.0f);
    g.setColour (Theme::textDim);
    g.drawText ("SOURCE", (int) bounds.getX() + 24, (int) bounds.getY() + 6, 60, 18,
                juce::Justification::centredLeft, false);

    // The listener at the bottom edge
    const float lx = bounds.getCentreX();
    const float ly = bounds.getBottom() - 14.0f;
    g.setColour (Theme::accentSide);
    g.fillEllipse (lx - 5.0f, ly - 5.0f, 10.0f, 10.0f);
    g.setColour (Theme::textDim);
    g.setFont (10.0f);
    g.drawText ("LISTENER",
                juce::Rectangle<int> ((int) lx + 10, (int) ly - 8, 70, 16),
                juce::Justification::centredLeft, false);

    // The sound source
    const auto dot = dotPosition();
    g.setColour (Theme::accent.withAlpha (0.20f));
    g.fillEllipse (dot.x - 16.0f, dot.y - 16.0f, 32.0f, 32.0f);
    g.setColour (Theme::accent);
    g.fillEllipse (dot.x - 7.0f, dot.y - 7.0f, 14.0f, 14.0f);

    // Pan / distance readout while hovering or dragging
    if (mouseIsOver || isMouseButtonDown())
        drawReadout (g, bounds, dot);
}

void RoomPad::drawReadout (juce::Graphics& g, juce::Rectangle<float> padBounds,
                           juce::Point<float> dot) const
{
    const float pan = panParam.convertFrom0to1 (panParam.getValue());  // -1..1

    // Straight-line distance from the listener (bottom centre) to the source
    const float dx     = pan * roomWidthMetres * 0.5f;
    const float dy     = distParam.getValue() * roomDepthMetres;
    const float metres = std::sqrt (dx * dx + dy * dy);

    juce::String panText;
    if      (pan < -0.005f) panText = "L " + juce::String ((int) std::round (-pan * 100.0f)) + "%";
    else if (pan >  0.005f) panText = "R " + juce::String ((int) std::round ( pan * 100.0f)) + "%";
    else                    panText = "C";

    const auto text = panText + "   " + juce::String (metres, 1) + " m";

    const float boxW = 118.0f, boxH = 22.0f, gap = 14.0f;

    // Above the dot, flipped below it if there is no room, clamped inside the pad
    float bx = juce::jlimit (padBounds.getX() + 4.0f,
                             padBounds.getRight() - boxW - 4.0f,
                             dot.x - boxW * 0.5f);
    float by = dot.y - gap - boxH;
    if (by < padBounds.getY() + 4.0f)
        by = dot.y + gap;

    const juce::Rectangle<float> box (bx, by, boxW, boxH);

    g.setColour (Theme::cardBg.withAlpha (0.95f));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (Theme::outline);
    g.drawRoundedRectangle (box, 4.0f, 1.0f);

    g.setColour (Theme::text);
    g.setFont (12.0f);
    g.drawText (text, box.toNearestInt(), juce::Justification::centred, false);
}

void RoomPad::updateFromMouse (const juce::MouseEvent& e)
{
    const auto bounds = getLocalBounds().toFloat().reduced (14.0f);

    const float normX = juce::jlimit (0.0f, 1.0f, (e.position.x - bounds.getX()) / bounds.getWidth());
    const float normY = juce::jlimit (0.0f, 1.0f, (bounds.getBottom() - e.position.y) / bounds.getHeight());

    panParam.setValueNotifyingHost (normX);   // normalised 0..1
    distParam.setValueNotifyingHost (normY);
    repaint();
}

void RoomPad::mouseDown (const juce::MouseEvent& e)
{
    panParam.beginChangeGesture();
    distParam.beginChangeGesture();
    updateFromMouse (e);
}

void RoomPad::mouseDrag (const juce::MouseEvent& e)
{
    updateFromMouse (e);
}

void RoomPad::mouseUp (const juce::MouseEvent&)
{
    panParam.endChangeGesture();
    distParam.endChangeGesture();
}

void RoomPad::mouseDoubleClick (const juce::MouseEvent&)
{
    // Reset to centre front
    panParam.beginChangeGesture();
    panParam.setValueNotifyingHost (panParam.getDefaultValue());
    panParam.endChangeGesture();

    distParam.beginChangeGesture();
    distParam.setValueNotifyingHost (distParam.getDefaultValue());
    distParam.endChangeGesture();

    repaint();
}

void RoomPad::mouseEnter (const juce::MouseEvent&)
{
    mouseIsOver = true;
    repaint();
}

void RoomPad::mouseExit (const juce::MouseEvent&)
{
    mouseIsOver = false;
    repaint();
}

//==============================================================================
LevelMeter::LevelMeter (std::atomic<float>& sourceToUse, const juce::String& labelText)
    : source (sourceToUse), label (labelText)
{
    startTimerHz (30);
}

void LevelMeter::timerCallback()
{
    const float peak   = source.exchange (0.0f);
    const float peakDb = juce::Decibels::gainToDecibels (peak, -60.0f);

    // Instant attack, smooth release
    displayDb = peakDb > displayDb ? peakDb
                                   : juce::jmax (-60.0f, displayDb - 1.5f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto labelArea = area.removeFromBottom (14.0f);
    area = area.reduced (3.0f, 2.0f);

    g.setColour (Theme::panelBg);
    g.fillRoundedRectangle (area, 3.0f);

    const float norm = juce::jlimit (0.0f, 1.0f, (displayDb + 60.0f) / 60.0f);

    if (norm > 0.001f)
    {
        auto bar = area.reduced (2.0f);
        bar = bar.removeFromBottom (bar.getHeight() * norm);
        g.setColour (displayDb > -3.0f ? Theme::warn : Theme::accent);
        g.fillRoundedRectangle (bar, 2.0f);
    }

    g.setColour (Theme::outline);
    g.drawRoundedRectangle (area, 3.0f, 1.0f);

    g.setColour (Theme::textDim);
    g.setFont (9.0f);
    g.drawText (label, labelArea.toNearestInt(), juce::Justification::centred, false);
}

//==============================================================================
KnobCard::KnobCard (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
                    const juce::String& titleText, juce::Colour accentColour)
{
    title.setText (titleText, juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (11.0f)));
    title.setColour (juce::Label::textColourId, Theme::textDim);
    title.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (title);

    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 18);
    knob.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    addAndMakeVisible (knob);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, knob);
}

void KnobCard::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Theme::cardBg);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (Theme::outline);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
}

void KnobCard::resized()
{
    auto area = getLocalBounds().reduced (6);
    title.setBounds (area.removeFromTop (16));
    knob.setBounds (area);
}

//==============================================================================
SoundPlacementEditor::SoundPlacementEditor (SoundPlacementProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      pad (p.apvts),
      inMeter  (p.inPeak,  "IN"),
      outMeter (p.outPeak, "OUT")
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (pad);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    // Knob cards: reverb row + mix row
    cards.add (new KnobCard (p.apvts, "roomSize",  "ROOM SIZE",  Theme::accent));
    cards.add (new KnobCard (p.apvts, "erLevel",   "EARLY REFL", Theme::accent));
    cards.add (new KnobCard (p.apvts, "revLowCut", "LOW CUT",    Theme::accent));
    cards.add (new KnobCard (p.apvts, "revTone",   "REV TONE",   Theme::accent));
    cards.add (new KnobCard (p.apvts, "duck",      "DUCK",       Theme::accent));
    cards.add (new KnobCard (p.apvts, "depth",     "DEPTH",      Theme::accent));
    cards.add (new KnobCard (p.apvts, "midGain",   "MID",        Theme::accent));
    cards.add (new KnobCard (p.apvts, "sideGain",  "SIDE",       Theme::accentSide));
    cards.add (new KnobCard (p.apvts, "outGain",   "OUTPUT",     Theme::accent));
    cards.add (new KnobCard (p.apvts, "mix",       "MIX",        Theme::accent));

    for (auto* c : cards)
        addAndMakeVisible (c);

    // Preset box
    presetBox.setTextWhenNothingSelected ("Presets");
    int id = 1;
    for (const auto& preset : SoundPlacementProcessor::getFactoryPresets())
        presetBox.addItem (preset.name, id++);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedId() - 1;
        if (index >= 0)
            processor.applyPreset (index);
    };
    addAndMakeVisible (presetBox);

    // A/B compare
    abAButton.onClick = [this] { processor.switchSlot (0); updateAbButtons(); };
    abBButton.onClick = [this] { processor.switchSlot (1); updateAbButtons(); };
    abCopyButton.onClick = [this] { processor.copyActiveSlotToOther(); };
    abCopyButton.setTooltip ("Copy the current settings to the other slot");
    addAndMakeVisible (abAButton);
    addAndMakeVisible (abBButton);
    addAndMakeVisible (abCopyButton);
    updateAbButtons();

    // Monitor buttons
    wetSoloButton.setClickingTogglesState (true);
    addAndMakeVisible (wetSoloButton);
    wetSoloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "wetSolo", wetSoloButton);

    monoButton.setClickingTogglesState (true);
    addAndMakeVisible (monoButton);
    monoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "mono", monoButton);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setColour (juce::TextButton::buttonOnColourId, Theme::warn);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    setSize (720, 780);
    setResizable (true, true);
    setResizeLimits (620, 640, 1100, 1200);
}

SoundPlacementEditor::~SoundPlacementEditor()
{
    setLookAndFeel (nullptr);
}

void SoundPlacementEditor::updateAbButtons()
{
    const bool aActive = processor.getActiveSlot() == 0;
    abAButton.setToggleState (aActive,   juce::dontSendNotification);
    abBButton.setToggleState (! aActive, juce::dontSendNotification);
}

void SoundPlacementEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::windowBg);

    // Header: diamond logo + title + subtitle, like the reference
    const auto header = getLocalBounds().removeFromTop (56).reduced (16, 0);

    const float dSize = 13.0f;
    const float dx = (float) header.getX() + dSize;
    const float dy = (float) header.getCentreY();
    juce::Path diamond;
    diamond.addQuadrilateral (dx, dy - dSize, dx + dSize, dy, dx, dy + dSize, dx - dSize, dy);
    g.setColour (Theme::accent);
    g.fillPath (diamond);

    const int textX = header.getX() + (int) (dSize * 2.0f) + 10;
    g.setColour (Theme::text);
    g.setFont (juce::Font (juce::FontOptions (19.0f, juce::Font::bold)));
    g.drawText ("SOUND PLACEMENT", textX, header.getY() + 9, 250, 22,
                juce::Justification::centredLeft, false);
    g.setColour (Theme::textDim);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("PAN / DEPTH ROOM PLACEMENT", textX, header.getY() + 31, 250, 13,
                juce::Justification::centredLeft, false);
}

void SoundPlacementEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Header row: title (painted) ... presets | A B COPY | WET MONO BYPASS
    auto header = area.removeFromTop (44);
    header.removeFromLeft (240); // space for the painted title

    auto controls = header.withSizeKeepingCentre (header.getWidth(), 30);
    bypassButton.setBounds (controls.removeFromRight (70));
    controls.removeFromRight (6);
    monoButton.setBounds (controls.removeFromRight (54));
    controls.removeFromRight (4);
    wetSoloButton.setBounds (controls.removeFromRight (48));
    controls.removeFromRight (12);
    abCopyButton.setBounds (controls.removeFromRight (52));
    controls.removeFromRight (4);
    abBButton.setBounds (controls.removeFromRight (28));
    controls.removeFromRight (4);
    abAButton.setBounds (controls.removeFromRight (28));
    controls.removeFromRight (12);
    presetBox.setBounds (controls.removeFromRight (juce::jmin (150, controls.getWidth())));

    area.removeFromTop (8);

    // Two rows of knob cards along the bottom
    const int gap = 8;
    const int cardH = 118;
    auto rowBottom = area.removeFromBottom (cardH);
    area.removeFromBottom (gap);
    auto rowTop = area.removeFromBottom (cardH);
    area.removeFromBottom (10);

    const int perRow = 5;
    const int cardW = (rowTop.getWidth() - gap * (perRow - 1)) / perRow;

    for (int i = 0; i < cards.size(); ++i)
    {
        const auto& row = i < perRow ? rowTop : rowBottom;
        const int col = i % perRow;
        cards[i]->setBounds (row.getX() + col * (cardW + gap), row.getY(), cardW, cardH);
    }

    // Pad + meters
    auto meterCol = area.removeFromRight (58);
    meterCol.removeFromTop (2);
    inMeter.setBounds (meterCol.removeFromLeft (28));
    outMeter.setBounds (meterCol);

    area.removeFromRight (6);
    pad.setBounds (area);
}
