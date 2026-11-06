#pragma once
#include <JuceHeader.h>
#include <functional>
namespace noryn::ui
{
inline const juce::Colour cyan(0xff5de7ed), amber(0xffffb569), ink(0xff090e12), text(0xffe3edf0),
    muted(0xff8ca5ae), panelFill(0xff0d171e), panelEdge(0xff2c414c), red(0xffff5a5a), green(0xff7dff9a);

inline void panel(juce::Graphics &g, juce::Rectangle<float> r, const juce::String &title, juce::Colour accent,
                  const juce::String &subtitle = {})
{
    g.setColour(panelFill);
    g.fillRoundedRectangle(r, 7);
    g.setColour(panelEdge);
    g.drawRoundedRectangle(r.reduced(.5f), 7, 1);
    g.setColour(accent);
    g.setFont(juce::FontOptions(11.f, juce::Font::bold));
    g.drawText(title, r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 16, juce::Justification::centredLeft);
    if (subtitle.isNotEmpty())
    {
        g.setColour(muted);
        g.setFont(juce::FontOptions(9.f));
        g.drawText(subtitle, r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 16, juce::Justification::centredRight);
    }
}
// Rotary knob with a caption, driving a plain value through callbacks (not a host parameter).
struct Knob : juce::Component
{
    juce::Slider slider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
    juce::Label caption;
    std::function<void(float)> onChange;
    Knob(const juce::String &name, double min, double max, double interval = 0, juce::Colour accent = cyan)
    {
        slider.setRange(min, max, interval);
        slider.setNumDecimalPlacesToDisplay(interval >= 1 ? 0 : max - min > 20 ? 1 : 2);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour(juce::Slider::textBoxTextColourId, text);
        slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.onValueChange = [this]
        {
            if (onChange)
                onChange(float(slider.getValue()));
        };
        caption.setText(name, juce::dontSendNotification);
        caption.setJustificationType(juce::Justification::centred);
        caption.setFont(juce::FontOptions(10.f));
        caption.setColour(juce::Label::textColourId, muted);
        addAndMakeVisible(slider);
        addAndMakeVisible(caption);
    }
    void set(float value)
    {
        slider.setValue(value, juce::dontSendNotification);
    }
    void resized() override
    {
        caption.setBounds(0, 0, getWidth(), 14);
        slider.setBounds(0, 14, getWidth(), getHeight() - 14);
    }
};
// Toggle styled as a small hardware button.
inline void styleToggle(juce::TextButton &button, juce::Colour on)
{
    button.setClickingTogglesState(true);
    button.setColour(juce::TextButton::buttonOnColourId, on.withAlpha(.85f));
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    button.setWantsKeyboardFocus(false);
}
inline juce::String noteName(float hz)
{
    if (hz <= 0)
        return "--";
    const float midi = 69 + 12 * std::log2(hz / 440);
    const int nearest = int(std::round(midi));
    const int cents = int(std::round((midi - nearest) * 100));
    return juce::MidiMessage::getMidiNoteName(nearest, true, true, 3) + (cents >= 0 ? " +" : " ") +
           juce::String(cents) + "c";
}
} // namespace noryn::ui
