#pragma once
#include "Sequencer.h"
namespace noryn
{
struct SequencePad : juce::Button
{
    Sequencer &sequence;
    int step;
    std::function<int()> entryNote;
    std::function<float()> entryVelocity, entryLength;
    std::function<void(int, float, float)> select;
    SequencePad(Sequencer &s, int i) : Button("step" + juce::String(i + 1)), sequence(s), step(i)
    {
        onClick = [this]
        {
            sequence.recordCursor.store(step);
            sequence.steps[size_t(step)].toggle(entryNote(), entryVelocity());
            sequence.steps[size_t(step)].length.store(entryLength());
        };
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        if (e.mods.isRightButtonDown())
        {
            sequence.recordCursor.store(step);
            auto &cell = sequence.steps[size_t(step)];
            int n = cell.first();
            if (n >= 0)
                select(n, cell.velocityFor(n), cell.lengthFor(n));
            repaint();
            return;
        }
        if (e.mods.isShiftDown())
        {
            sequence.steps[size_t(step)].clear();
            repaint();
            return;
        }
        Button::mouseDown(e);
    }
    void paintButton(juce::Graphics &g, bool hover, bool down) override
    {
        using namespace juce;
        auto &cell = sequence.steps[size_t(step)];
        bool active = !cell.empty(), head = sequence.playhead.load() == step;
        auto r = getLocalBounds().toFloat().reduced(2);
        Colour accent = head ? Colour(0xffffc47b) : Colour(0xff66dce4);
        g.setColour(head ? Colour(0xff354a4c) : active ? Colour(0xff203643) : Colour(0xff0b151e));
        g.fillRoundedRectangle(r, 5);
        g.setColour(head     ? accent
                    : hover  ? Colour(0xffbac2db)
                    : active ? Colour(0xff63748c)
                             : Colour(0xff344653));
        g.drawRoundedRectangle(r, 5, head ? 2.f : 1.f);
        if (sequence.recording.load() && sequence.stepRecord.load() && sequence.recordCursor.load() == step)
        {
            g.setColour(Colour(0xfff08297));
            g.drawRoundedRectangle(r.reduced(3), 3, 1);
        }
        g.setFont(FontOptions(11.f));
        g.setColour(Colour(0xffbaa789));
        g.drawText(String(step + 1), 8, 5, getWidth() - 16, 18, Justification::centredLeft);
        int first = cell.first();
        String help = "Step " + String(step + 1);
        for (int note = 0; note < 128; ++note)
            if (cell.has(note))
                help += "\n" + MidiMessage::getMidiNoteName(note, true, true, 3) + " / " +
                        String(cell.lengthFor(note), 2) + " steps / " +
                        String(roundToInt(cell.velocityFor(note) * 100)) + "%";
        setTooltip(help);

        String label = first < 0 ? "-" : MidiMessage::getMidiNoteName(first, true, true, 3);
        if (cell.count() > 1)
            label += " +" + String(cell.count() - 1);
        g.setFont(FontOptions(17.f));
        g.setColour(active ? Colour(0xffb6e9ec) : Colour(0xff405667));
        g.drawText(label, 4, 27, getWidth() - 8, 24, Justification::centred);
        g.setColour(Colour(0xff8ca5ae));
        g.setFont(FontOptions(9.f));
        if (active)
            g.drawText("LEN " + String(cell.lengthFor(first), 2), 4, 54, getWidth() - 8, 16,
                       Justification::centred);
        g.setColour(Colour(0xffa88ece));
        if (active)
            g.fillRoundedRectangle(9, float(getHeight() - 15), float(getWidth() - 18) * cell.velocity.load(),
                                   3, 1);
        ignoreUnused(down);
    }
};
} // namespace noryn
