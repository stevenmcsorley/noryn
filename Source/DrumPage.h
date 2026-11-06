#pragma once
#include "Drums.h"
#include "UiParts.h"
#include <JuceHeader.h>
namespace noryn
{
// DRUMS page: kit and pattern, a 16-lane step grid, the selected pad's sound, and playable pads.
class DrumPage : public juce::Component, private juce::Timer
{
  public:
    std::function<void(int)> onPattern;          // applies a pattern preset (sets swing too)
    std::function<void(int, float)> onHit;       // auditions a pad through the audio thread
    std::function<void(bool)> onTransport;       // play / stop the shared transport
    std::function<bool()> isPlaying;

    explicit DrumPage(DrumMachine &machine) : drums(machine)
    {
        for (const auto &kit : drumKits())
            kits.addItem(kit.name, kits.getNumItems() + 1);
        kits.setTextWhenNothingSelected("Kit");
        kits.onChange = [this]
        {
            if (kits.getSelectedId() > 0)
            {
                drums.applyKit(kits.getSelectedId() - 1);
                selectPad(selected);
                repaint();
            }
        };
        for (const auto &pattern : drumPatterns())
            patterns.addItem(pattern.name, patterns.getNumItems() + 1);
        patterns.setTextWhenNothingSelected("Load pattern...");
        patterns.onChange = [this]
        {
            if (patterns.getSelectedId() > 0 && onPattern)
                onPattern(patterns.getSelectedId() - 1);
            syncLength();
            repaint();
        };
        for (int bars = 1; bars <= 16; bars *= 2)
            lengths.addItem(juce::String(bars) + (bars == 1 ? " bar" : " bars") + " (" + juce::String(bars * 16) + ")", bars * 16);
        lengths.addItem("3 bars (48)", 48);
        lengths.addItem("12 bars (192)", 192);
        lengths.onChange = [this]
        {
            drums.pattern.length.store(lengths.getSelectedId());
            if (view * 16 >= drums.pattern.length.load())
                showView(0);
            repaint();
        };
        ui::styleToggle(play, ui::green);
        play.onClick = [this]
        {
            if (onTransport)
                onTransport(play.getToggleState());
        };
        ui::styleToggle(record, ui::red);
        record.onClick = [this] { drums.recording.store(record.getToggleState()); };
        record.setTooltip("Hits you play while the transport runs are written into the pattern, quantized to 1/16.");
        ui::styleToggle(keys, ui::amber);
        keys.onClick = [this] { drums.keysPlayDrums.store(keys.getToggleState()); };
        keys.setTooltip("Your MIDI keyboard plays the pads (C3 = pad 1, C#3 = pad 2 ...). Channel 10 always plays drums.");
        clear.onClick = [this]
        {
            drums.pattern.clear();
            repaint();
        };
        for (int i = 0; i < maxDrumSteps / 16; ++i)
        {
            auto &b = viewButtons[size_t(i)];
            b.setButtonText(juce::String(i + 1));
            b.setTooltip("Bar " + juce::String(i + 1) + " (steps " + juce::String(i * 16 + 1) + "-" + juce::String(i * 16 + 16) + ")");
            b.setRadioGroupId(4471);
            b.setClickingTogglesState(true);
            b.onClick = [this, i] { showView(i); };
            b.setWantsKeyboardFocus(false);
            addAndMakeVisible(b);
        }
        for (auto *c : std::initializer_list<juce::Component *>{&kits, &patterns, &lengths, &play, &record, &keys, &clear,
                                                               &padType, &padChoke, &loadSample})
            addAndMakeVisible(c);
        padType.addItemList(drumTypeNames(), 1);
        padType.onChange = [this]
        {
            if (padType.getSelectedId() > 0)
                drums.pads[size_t(selected)].type.store(padType.getSelectedId() - 1);
        };
        padChoke.addItemList({"No choke", "Choke group 1", "Choke group 2"}, 1);
        padChoke.onChange = [this] { drums.pads[size_t(selected)].choke.store(padChoke.getSelectedId() - 1); };
        loadSample.onClick = [this] { chooseSample(); };
        loadSample.setTooltip("Load a WAV/AIFF/FLAC one-shot onto this pad.");
        auto addKnob = [this](std::unique_ptr<ui::Knob> &k, const char *name, double min, double max,
                              std::atomic<float> DrumPad::*field, juce::Colour colour)
        {
            k = std::make_unique<ui::Knob>(name, min, max, 0, colour);
            k->onChange = [this, field](float value) { (drums.pads[size_t(selected)].*field).store(value); };
            addAndMakeVisible(*k);
        };
        addKnob(tune, "Tune", -24, 24, &DrumPad::tune, ui::cyan);
        addKnob(decay, "Decay", 0, 1, &DrumPad::decay, ui::cyan);
        addKnob(tone, "Tone", 0, 1, &DrumPad::tone, ui::amber);
        addKnob(snap, "Snap / drive", 0, 1, &DrumPad::snap, ui::amber);
        addKnob(level, "Level", 0, 1.5, &DrumPad::level, ui::cyan);
        addKnob(pan, "Pan", -1, 1, &DrumPad::pan, ui::cyan);
        auto busKnob = [this](std::unique_ptr<ui::Knob> &k, const char *name, std::atomic<float> &field)
        {
            k = std::make_unique<ui::Knob>(name, 0, 1, 0, ui::amber);
            k->onChange = [&field](float value) { field.store(value); };
            addAndMakeVisible(*k);
        };
        busKnob(busDrive, "Bus drive", drums.busDrive);
        busKnob(busComp, "Bus comp", drums.busCompression);
        busKnob(room, "Room", drums.room);
        showView(0);
        selectPad(0);
        startTimerHz(24);
    }
    void refresh()
    {
        kits.setSelectedId(drums.kitIndex.load() + 1, juce::dontSendNotification);
        syncLength();
        keys.setToggleState(drums.keysPlayDrums.load(), juce::dontSendNotification);
        record.setToggleState(drums.recording.load(), juce::dontSendNotification);
        selectPad(selected);
        repaint();
    }
    void resized() override
    {
        // The right of the top row holds the host-automatable level and swing knobs (placed by the editor).
        kits.setBounds(12, 8, 210, 30);
        patterns.setBounds(230, 8, 240, 30);
        lengths.setBounds(478, 8, 100, 30);
        play.setBounds(586, 8, 70, 30);
        record.setBounds(662, 8, 100, 30);
        keys.setBounds(768, 8, 130, 30);
        clear.setBounds(904, 8, 60, 30);
        for (int i = 0; i < maxDrumSteps / 16; ++i)
            viewButtons[size_t(i)].setBounds(gridX + i * 50, 46, 46, 22);
        const int px = 1000;
        padType.setBounds(px + 12, 132, 180, 24);
        padChoke.setBounds(px + 200, 132, 190, 24);
        const int ky = 160;
        tune->setBounds(px + 8, ky, 64, 70);
        decay->setBounds(px + 72, ky, 64, 70);
        tone->setBounds(px + 136, ky, 64, 70);
        snap->setBounds(px + 200, ky, 64, 70);
        level->setBounds(px + 264, ky, 64, 70);
        pan->setBounds(px + 328, ky, 64, 70);
        loadSample.setBounds(px + 12, 252, 180, 26);
        busDrive->setBounds(px + 200, 232, 64, 70);
        busComp->setBounds(px + 264, 232, 64, 70);
        room->setBounds(px + 328, 232, 64, 70);
    }
    void paint(juce::Graphics &g) override
    {
        const int length = drums.pattern.length.load(), head = drums.playhead.load();
        ui::panel(g, {0, 74, float(gridRight() + 12), float(gridY + drumPads * laneHeight + 14 - 74)}, "PATTERN",
                  ui::cyan, "CLICK: hit   SHIFT: accent   ALT / RIGHT-CLICK: ghost");
        for (int lane = 0; lane < drumPads; ++lane)
        {
            const int y = gridY + lane * laneHeight;
            g.setColour(lane == selected ? ui::amber : ui::text);
            g.setFont(juce::FontOptions(11.f));
            g.drawText(juce::String(lane + 1) + "  " + drums.pads[size_t(lane)].name, 10, y, gridX - 16, laneHeight,
                       juce::Justification::centredLeft);
            for (int c = 0; c < 16; ++c)
            {
                const int step = view * 16 + c;
                auto cell = juce::Rectangle<float>(float(gridX + c * cellWidth), float(y), float(cellWidth), float(laneHeight)).reduced(2);
                const bool inPattern = step < length;
                const int v = drums.pattern.velocity[size_t(lane)][size_t(step)].load();
                g.setColour(!inPattern ? juce::Colour(0xff0a1015) : c % 4 == 0 ? juce::Colour(0xff1b2a33) : juce::Colour(0xff142028));
                g.fillRoundedRectangle(cell, 3);
                if (v > 0 && inPattern)
                {
                    const auto colour = v >= 112 ? ui::amber : v >= 72 ? ui::cyan : ui::cyan.withAlpha(.4f);
                    g.setColour(colour);
                    g.fillRoundedRectangle(cell.reduced(3), 2);
                }
                if (step == head)
                {
                    g.setColour(juce::Colours::white.withAlpha(.25f));
                    g.fillRoundedRectangle(cell, 3);
                }
            }
        }
        // Pad editor and playable pads
        ui::panel(g, {988, 108, 420, 198}, "PAD " + juce::String(selected + 1) + ": " + drums.pads[size_t(selected)].name,
                  ui::amber, "SOUND");
        ui::panel(g, {988, 312, 420, 400}, "PADS", ui::cyan, "CLICK TO PLAY / SELECT");
        for (int p = 0; p < drumPads; ++p)
        {
            auto r = padBounds(p);
            const float flash = drums.padFlash[size_t(p)].load();
            g.setColour(juce::Colour(0xff17262f).interpolatedWith(ui::cyan, juce::jlimit(0.f, .8f, flash)));
            g.fillRoundedRectangle(r, 6);
            g.setColour(p == selected ? ui::amber : ui::panelEdge);
            g.drawRoundedRectangle(r, 6, p == selected ? 2.f : 1.f);
            g.setColour(ui::text);
            g.setFont(juce::FontOptions(10.f));
            g.drawFittedText(drums.pads[size_t(p)].name, r.toNearestInt().reduced(6), juce::Justification::centred, 2);
            g.setColour(ui::muted);
            g.drawText(juce::String(p + 1), r.toNearestInt().reduced(6), juce::Justification::topLeft);
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        const auto at = e.getPosition();
        for (int p = 0; p < drumPads; ++p)
            if (padBounds(p).contains(at.toFloat()))
            {
                selectPad(p);
                if (onHit)
                    onHit(p, juce::jlimit(.3f, 1.f, 1 - (at.y - padBounds(p).getY()) / padBounds(p).getHeight() * .6f));
                return;
            }
        if (at.x >= 10 && at.x < gridX && at.y >= gridY && at.y < gridY + drumPads * laneHeight)
        {
            const int lane = (at.y - gridY) / laneHeight;
            selectPad(lane);
            if (onHit)
                onHit(lane, .9f);
            return;
        }
        if (at.x >= gridX && at.x < gridRight() && at.y >= gridY && at.y < gridY + drumPads * laneHeight)
        {
            const int lane = (at.y - gridY) / laneHeight, step = view * 16 + (at.x - gridX) / cellWidth;
            if (step >= drums.pattern.length.load())
                return;
            auto &cell = drums.pattern.velocity[size_t(lane)][size_t(step)];
            const uint8_t want = e.mods.isShiftDown() ? 127 : (e.mods.isAltDown() || e.mods.isPopupMenu()) ? 48 : 96;
            paintValue = cell.load() == want ? 0 : want;
            cell.store(paintValue);
            painting = true;
            lastPainted = {lane, step};
            repaint();
        }
    }
    void mouseDrag(const juce::MouseEvent &e) override
    {
        if (!painting)
            return;
        const auto at = e.getPosition();
        if (at.x < gridX || at.x >= gridRight() || at.y < gridY || at.y >= gridY + drumPads * laneHeight)
            return;
        const int lane = (at.y - gridY) / laneHeight, step = view * 16 + (at.x - gridX) / cellWidth;
        if (juce::Point<int>(lane, step) == lastPainted || step >= drums.pattern.length.load())
            return;
        lastPainted = {lane, step};
        drums.pattern.velocity[size_t(lane)][size_t(step)].store(paintValue);
        repaint();
    }
    void mouseUp(const juce::MouseEvent &) override
    {
        painting = false;
    }

  private:
    DrumMachine &drums;
    juce::ComboBox kits, patterns, lengths, padType, padChoke;
    juce::TextButton play{"PLAY"}, record{"REC DRUMS"}, keys{"KEYS PLAY DRUMS"}, clear{"CLEAR"},
        loadSample{"LOAD SAMPLE..."};
    std::array<juce::TextButton, maxDrumSteps / 16> viewButtons;
    std::unique_ptr<ui::Knob> tune, decay, tone, snap, level, pan, busDrive, busComp, room;
    std::unique_ptr<juce::FileChooser> chooser;
    int selected = 0, view = 0;
    bool painting = false;
    uint8_t paintValue = 0;
    juce::Point<int> lastPainted;
    static constexpr int gridX = 170, gridY = 104, cellWidth = 50, laneHeight = 37;
    int gridRight() const
    {
        return gridX + 16 * cellWidth;
    }
    juce::Rectangle<float> padBounds(int p) const
    {
        const int col = p % 4, row = 3 - p / 4; // pad 1 bottom-left, like hardware
        return {1000.f + col * 101, 336.f + row * 94, 94, 88};
    }
    void showView(int index)
    {
        view = index;
        for (int i = 0; i < maxDrumSteps / 16; ++i)
        {
            viewButtons[size_t(i)].setToggleState(i == index, juce::dontSendNotification);
            viewButtons[size_t(i)].setEnabled(i * 16 < drums.pattern.length.load());
        }
        repaint();
    }
    void syncLength()
    {
        const int length = drums.pattern.length.load();
        if (lengths.indexOfItemId(length) < 0)
            lengths.addItem(juce::String(length) + " steps", length);
        lengths.setSelectedId(length, juce::dontSendNotification);
        showView(view);
    }
    void selectPad(int p)
    {
        selected = juce::jlimit(0, drumPads - 1, p);
        const auto &pad = drums.pads[size_t(selected)];
        padType.setSelectedId(pad.type.load() + 1, juce::dontSendNotification);
        padChoke.setSelectedId(pad.choke.load() + 1, juce::dontSendNotification);
        tune->set(pad.tune.load());
        decay->set(pad.decay.load());
        tone->set(pad.tone.load());
        snap->set(pad.snap.load());
        level->set(pad.level.load());
        pan->set(pad.pan.load());
        busDrive->set(drums.busDrive.load());
        busComp->set(drums.busCompression.load());
        room->set(drums.room.load());
        repaint();
    }
    void chooseSample()
    {
        chooser = std::make_unique<juce::FileChooser>("Load a drum sample", juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                      "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser &fc)
                             {
                                 if (fc.getResult().existsAsFile() && drums.loadSample(selected, fc.getResult()))
                                     selectPad(selected);
                             });
    }
    void timerCallback() override
    {
        const bool running = isPlaying && isPlaying();
        if (play.getToggleState() != running)
            play.setToggleState(running, juce::dontSendNotification);
        // Long patterns: the grid follows the bar that is playing.
        if (running && !painting && drums.playhead.load() >= 0 && drums.playhead.load() / 16 != view)
            showView(drums.playhead.load() / 16);
        if (isVisible())
            repaint(0, 74, getWidth(), getHeight() - 74);
    }
};
} // namespace noryn
