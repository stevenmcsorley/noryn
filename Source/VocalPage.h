#pragma once
#include "Studio.h"
#include "UiParts.h"
#include <JuceHeader.h>
namespace noryn
{
// VOCAL page: the channel strip of the selected studio track.
class VocalPage : public juce::Component, private juce::Timer
{
  public:
    std::function<void()> onNeedInput;
    std::function<juce::String()> scaleDescription;

    explicit VocalPage(Studio &s) : studio(s)
    {
        trackBox.onChange = [this] { selectTrack(trackBox.getSelectedId() - 1); };
        for (const auto &preset : VocalSettings::presets())
            presetBox.addItem(preset.name, presetBox.getNumItems() + 1);
        presetBox.setTextWhenNothingSelected("Chain preset...");
        presetBox.onChange = [this]
        {
            if (presetBox.getSelectedId() > 0)
            {
                settings().applyPreset(presetBox.getSelectedId() - 1);
                track().fx.store(true);
                sync();
                studio.saveSession();
            }
        };
        ui::styleToggle(enabled, ui::cyan);
        enabled.onClick = [this] { track().fx.store(enabled.getToggleState()); };
        ui::styleToggle(arm, ui::red);
        arm.onClick = [this]
        {
            track().armed.store(arm.getToggleState());
            if (arm.getToggleState() && isInputSource(track().source.load()) && onNeedInput)
                onNeedInput();
        };
        ui::styleToggle(monitor, ui::amber);
        monitor.setTooltip("Hear your mic through the chain now (use headphones to avoid feedback).");
        monitor.onClick = [this]
        {
            track().monitor.store(monitor.getToggleState());
            if (monitor.getToggleState() && onNeedInput)
                onNeedInput();
        };
        for (auto *c : std::initializer_list<juce::Component *>{&trackBox, &presetBox, &enabled, &arm, &monitor})
            addAndMakeVisible(c);
        // Sections: title, accent, fields. Switch fields become buttons; carrier and delay time are menus.
        sections = {{"GATE", ui::cyan, {"gate", "gatethreshold"}},
                    {"EQ", ui::cyan, {"eq", "lowcut", "mud", "presence", "air"}},
                    {"DE-ESSER", ui::cyan, {"deess"}},
                    {"COMPRESSOR", ui::amber, {"comp", "compthreshold", "compratio", "makeup"}},
                    {"PITCH CORRECTION", ui::amber, {"tune", "tuneamount", "tunespeed"}},
                    {"TAPE SATURATION", ui::amber, {"saturation"}},
                    {"VOCODER", ui::cyan, {"vocoder", "vocodermix", "carrier"}},
                    {"DOUBLE / HARMONY", ui::cyan, {"doubler", "harmony", "harmonylevel"}},
                    {"SPACE", ui::amber, {"delay", "delaytime", "delayfeedback", "reverb", "reverbsize"}},
                    {"OUTPUT", ui::amber, {"output"}}};
        for (const auto &section : sections)
            for (const auto &id : section.fields)
            {
                const int index = VocalSettings::indexOf(id);
                const auto &field = VocalSettings::fields()[size_t(index)];
                if (isSwitch(id))
                {
                    auto b = std::make_unique<juce::TextButton>("ON");
                    ui::styleToggle(*b, section.accent);
                    b->onClick = [this, id, raw = b.get()]
                    {
                        settings().set(id, raw->getToggleState() ? 1.f : 0.f);
                        raw->setButtonText(raw->getToggleState() ? "ON" : "OFF");
                    };
                    addAndMakeVisible(*b);
                    switches[id] = std::move(b);
                }
                else if (id == juce::String("carrier") || id == juce::String("delaytime"))
                {
                    auto box = std::make_unique<juce::ComboBox>();
                    box->addItemList(id == juce::String("carrier") ? juce::StringArray{"NORYN synth", "Robot (follows voice)", "Drone (root + fifth)"}
                                                                  : juce::StringArray{"1/8", "dotted 1/8", "1/4"},
                                     1);
                    box->onChange = [this, id, raw = box.get()] { settings().set(id, float(raw->getSelectedId() - 1)); };
                    addAndMakeVisible(*box);
                    menus[id] = std::move(box);
                }
                else
                {
                    auto k = std::make_unique<ui::Knob>(field.name, field.min, field.max,
                                                        id == juce::String("harmony") ? 1 : 0, section.accent);
                    k->onChange = [this, id](float value) { settings().set(id, value); };
                    addAndMakeVisible(*k);
                    knobs[id] = std::move(k);
                }
            }
        selectTrack(0);
        startTimerHz(20);
    }
    void selectTrack(int index)
    {
        current = juce::jlimit(0, studioTracks - 1, index);
        trackBox.clear(juce::dontSendNotification);
        for (int t = 0; t < studioTracks; ++t)
            trackBox.addItem(juce::String(t + 1) + ": " + studio.tracks[size_t(t)].name, t + 1);
        trackBox.setSelectedId(current + 1, juce::dontSendNotification);
        sync();
    }
    void resized() override
    {
        trackBox.setBounds(12, 8, 220, 30);
        presetBox.setBounds(240, 8, 250, 30);
        enabled.setBounds(498, 8, 110, 30);
        arm.setBounds(614, 8, 70, 30);
        monitor.setBounds(690, 8, 100, 30);
        // Three rows of panels
        const int rowY[] = {52, 208, 364};
        const int rowSections[3][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {8, 9, -1, -1}};
        for (int row = 0; row < 3; ++row)
        {
            int x = 0;
            for (int column = 0; column < 4; ++column)
            {
                const int s = rowSections[row][column];
                if (s < 0)
                    continue;
                auto &section = sections[size_t(s)];
                int width = 24;
                for (const auto &id : section.fields)
                    width += menus.count(id) ? 128 : 78;
                section.bounds = {float(x), float(rowY[row]), float(width), 146};
                int fx = x + 12;
                for (const auto &id : section.fields)
                {
                    if (auto it = switches.find(id); it != switches.end())
                        it->second->setBounds(fx + 8, rowY[row] + 60, 58, 28);
                    else if (auto m = menus.find(id); m != menus.end())
                        m->second->setBounds(fx, rowY[row] + 60, 122, 28);
                    else
                        knobs[id]->setBounds(fx, rowY[row] + 28, 74, 108);
                    fx += menus.count(id) ? 128 : 78;
                }
                x += width + 10;
            }
        }
    }
    void paint(juce::Graphics &g) override
    {
        for (const auto &section : sections)
            ui::panel(g, section.bounds, section.title, section.accent);
        // Live readouts
        auto r = juce::Rectangle<float>(0, 520, 0, 190);
        r.setRight(float(getWidth()));
        ui::panel(g, r, "LIVE", ui::cyan, scaleDescription ? scaleDescription() : juce::String());
        const auto &chain = track().chain;
        g.setFont(juce::FontOptions(26.f, juce::Font::bold));
        g.setColour(ui::text);
        g.drawText(ui::noteName(chain.detectedHz.load()), int(r.getX()) + 16, int(r.getY()) + 34, 220, 40,
                   juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11.f));
        g.setColour(ui::muted);
        g.drawText("DETECTED PITCH", int(r.getX()) + 16, int(r.getY()) + 74, 220, 16, juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(26.f, juce::Font::bold));
        g.setColour(ui::amber);
        g.drawText(settings().get("tune") > .5f ? ui::noteName(chain.correctedHz.load()) : juce::String("off"),
                   int(r.getX()) + 250, int(r.getY()) + 34, 220, 40, juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11.f));
        g.setColour(ui::muted);
        g.drawText("CORRECTED TO", int(r.getX()) + 250, int(r.getY()) + 74, 220, 16, juce::Justification::centredLeft);
        // Input and gain-reduction meters
        const float input = juce::jlimit(0.f, 1.f, track().meter.load()), reduction = chain.gainReduction.load();
        auto bar = [&](float y, float value, juce::Colour colour, const juce::String &label)
        {
            g.setColour(juce::Colour(0xff050b10));
            g.fillRoundedRectangle(r.getX() + 16, y, r.getWidth() - 120, 12, 3);
            g.setColour(colour);
            g.fillRoundedRectangle(r.getX() + 16, y, (r.getWidth() - 120) * juce::jlimit(0.f, 1.f, value), 12, 3);
            g.setColour(ui::muted);
            g.drawText(label, int(r.getRight()) - 100, int(y) - 2, 90, 16, juce::Justification::centredLeft);
        };
        bar(r.getY() + 110, std::sqrt(input), ui::green, "LEVEL");
        bar(r.getY() + 134, reduction, ui::amber, "COMP GR");
        g.setColour(ui::muted);
        g.setFont(juce::FontOptions(10.f));
        g.drawFittedText(isInputSource(track().source.load())
                             ? "Source: " + trackSourceNames()[track().source.load()] + ". Arm the track on STUDIO and press REC."
                             : "This track records " + trackSourceNames()[track().source.load()] +
                                   "; the chain still processes it.",
                         int(r.getX()) + 16, int(r.getY()) + 156, int(r.getWidth()) - 32, 40, juce::Justification::topLeft, 2);
    }

  private:
    struct Section
    {
        juce::String title;
        juce::Colour accent;
        std::vector<juce::String> fields;
        juce::Rectangle<float> bounds{};
    };
    Studio &studio;
    int current = 0;
    juce::ComboBox trackBox, presetBox;
    juce::TextButton enabled{"CHAIN ON"}, arm{"ARM"}, monitor{"MONITOR"};
    std::vector<Section> sections;
    std::map<juce::String, std::unique_ptr<ui::Knob>> knobs;
    std::map<juce::String, std::unique_ptr<juce::TextButton>> switches;
    std::map<juce::String, std::unique_ptr<juce::ComboBox>> menus;
    static bool isSwitch(const juce::String &id)
    {
        return id == "gate" || id == "eq" || id == "comp" || id == "tune" || id == "vocoder";
    }
    StudioTrack &track()
    {
        return studio.tracks[size_t(current)];
    }
    const StudioTrack &track() const
    {
        return studio.tracks[size_t(current)];
    }
    VocalSettings &settings()
    {
        return track().chain.settings;
    }
    const VocalSettings &settings() const
    {
        return track().chain.settings;
    }
    void sync()
    {
        for (auto &[id, k] : knobs)
            k->set(settings().get(id.toRawUTF8()));
        for (auto &[id, b] : switches)
        {
            b->setToggleState(settings().get(id.toRawUTF8()) > .5f, juce::dontSendNotification);
            b->setButtonText(b->getToggleState() ? "ON" : "OFF");
        }
        for (auto &[id, m] : menus)
            m->setSelectedId(int(std::round(settings().get(id.toRawUTF8()))) + 1, juce::dontSendNotification);
        enabled.setToggleState(track().fx.load(), juce::dontSendNotification);
        arm.setToggleState(track().armed.load(), juce::dontSendNotification);
        monitor.setToggleState(track().monitor.load(), juce::dontSendNotification);
        repaint();
    }
    void timerCallback() override
    {
        if (!isVisible())
            return;
        arm.setToggleState(track().armed.load(), juce::dontSendNotification);
        monitor.setToggleState(track().monitor.load(), juce::dontSendNotification);
        enabled.setToggleState(track().fx.load(), juce::dontSendNotification);
        repaint(0, 520, getWidth(), 192);
    }
};
} // namespace noryn
