#pragma once
#include <JuceHeader.h>
#include <array>
namespace noryn
{
struct ScaleEngine
{
    struct Scale
    {
        const char *name;
        uint16_t mask;
    };
    static constexpr uint16_t mask(std::initializer_list<int> notes)
    {
        uint16_t bits = 0;
        for (int n : notes)
            bits |= uint16_t(1 << n);
        return bits;
    }
    static const std::vector<Scale> &scales()
    {
        static const std::vector<Scale> items = {{"Chromatic", 4095},
                                                 {"Major / Ionian", mask({0, 2, 4, 5, 7, 9, 11})},
                                                 {"Natural minor / Aeolian", mask({0, 2, 3, 5, 7, 8, 10})},
                                                 {"Dorian", mask({0, 2, 3, 5, 7, 9, 10})},
                                                 {"Phrygian", mask({0, 1, 3, 5, 7, 8, 10})},
                                                 {"Lydian", mask({0, 2, 4, 6, 7, 9, 11})},
                                                 {"Mixolydian", mask({0, 2, 4, 5, 7, 9, 10})},
                                                 {"Locrian", mask({0, 1, 3, 5, 6, 8, 10})},
                                                 {"Harmonic minor", mask({0, 2, 3, 5, 7, 8, 11})},
                                                 {"Melodic minor", mask({0, 2, 3, 5, 7, 9, 11})},
                                                 {"Major pentatonic", mask({0, 2, 4, 7, 9})},
                                                 {"Minor pentatonic", mask({0, 3, 5, 7, 10})},
                                                 {"Blues", mask({0, 3, 5, 6, 7, 10})},
                                                 {"Major blues", mask({0, 2, 3, 4, 7, 9})},
                                                 {"Whole tone", mask({0, 2, 4, 6, 8, 10})},
                                                 {"Diminished whole / half", mask({0, 2, 3, 5, 6, 8, 9, 11})},
                                                 {"Diminished half / whole", mask({0, 1, 3, 4, 6, 7, 9, 10})},
                                                 {"Harmonic major", mask({0, 2, 4, 5, 7, 8, 11})},
                                                 {"Double harmonic", mask({0, 1, 4, 5, 7, 8, 11})},
                                                 {"Phrygian dominant", mask({0, 1, 4, 5, 7, 8, 10})},
                                                 {"Lydian dominant", mask({0, 2, 4, 6, 7, 9, 10})},
                                                 {"Altered", mask({0, 1, 3, 4, 6, 8, 10})},
                                                 {"Hungarian minor", mask({0, 2, 3, 6, 7, 8, 11})},
                                                 {"Neapolitan minor", mask({0, 1, 3, 5, 7, 8, 11})},
                                                 {"Neapolitan major", mask({0, 1, 3, 5, 7, 9, 11})},
                                                 {"Persian", mask({0, 1, 4, 5, 6, 8, 11})},
                                                 {"Hirajoshi", mask({0, 2, 3, 7, 8})},
                                                 {"In sen", mask({0, 1, 5, 7, 10})},
                                                 {"Iwato", mask({0, 1, 5, 6, 10})},
                                                 {"Kumoi", mask({0, 2, 3, 7, 9})},
                                                 {"Egyptian", mask({0, 2, 5, 7, 10})},
                                                 {"Bebop dominant", mask({0, 2, 4, 5, 7, 9, 10, 11})},
                                                 {"Bebop major", mask({0, 2, 4, 5, 7, 8, 9, 11})},
                                                 {"Bebop minor", mask({0, 2, 3, 4, 5, 7, 9, 10})},
                                                 {"Augmented", mask({0, 3, 4, 7, 8, 11})},
                                                 {"Tritone", mask({0, 1, 4, 6, 7, 10})},
                                                 {"Custom", 4095}};
        return items;
    }
    static juce::StringArray names()
    {
        juce::StringArray names;
        for (const auto &scale : scales())
            names.add(scale.name);
        return names;
    }
    static int quantize(int note, int scale, int root, int direction, int custom = 4095)
    {
        auto bits = scale == int(scales().size()) - 1
                        ? uint16_t(custom)
                        : scales()[size_t(juce::jlimit(0, int(scales().size()) - 1, scale))].mask;
        if (bits == 0)
            bits = 4095;
        auto allowed = [&](int n) { return n >= 0 && n <= 127 && ((bits >> ((n - root + 120) % 12)) & 1); };
        if (allowed(note))
            return note;
        for (int distance = 1; distance < 128; ++distance)
        {
            if (direction != 1 && allowed(note - distance))
                return note - distance;
            if (direction != 2 && allowed(note + distance))
                return note + distance;
        }
        if (direction == 1)
        {
            for (int n = 127; n >= 0; --n)
                if (allowed(n))
                    return n;
        }
        else
        {
            for (int n = 0; n < 128; ++n)
                if (allowed(n))
                    return n;
        }
        return note;
    }
    std::array<std::array<int, 128>, 16> mapped{};
    std::array<std::array<int, 128>, 16> counts{};
    ScaleEngine()
    {
        reset();
    }
    void reset()
    {
        for (auto &row : mapped)
            row.fill(-1);
        for (auto &row : counts)
            row.fill(0);
    }
    void process(const juce::MidiBuffer &input, juce::MidiBuffer &output, int scale, int root, int direction,
                 int custom, bool live)
    {
        using namespace juce;
        output.clear();
        for (const auto event : input)
        {
            auto msg = event.getMessage();
            int channel = msg.getChannel() - 1;
            if (channel >= 0 && channel < 16 && (msg.isNoteOn() || msg.isNoteOff()))
            {
                int original = msg.getNoteNumber();
                int &map = mapped[size_t(channel)][size_t(original)];
                if (msg.isNoteOn())
                {
                    int target = (channel == 15 || live) ? quantize(original, scale, root, direction, custom)
                                                         : original;
                    if (map >= 0)
                    {
                        int &oldCount = counts[size_t(channel)][size_t(map)];
                        if (--oldCount <= 0)
                            output.addEvent(MidiMessage::noteOff(channel + 1, map), event.samplePosition);
                    }
                    map = target;
                    int &count = counts[size_t(channel)][size_t(target)];
                    if (count++ == 0)
                        output.addEvent(MidiMessage::noteOn(channel + 1, target, msg.getFloatVelocity()),
                                        event.samplePosition);
                }
                else if (map >= 0)
                {
                    int &count = counts[size_t(channel)][size_t(map)];
                    if (--count <= 0)
                        output.addEvent(MidiMessage::noteOff(channel + 1, map, msg.getFloatVelocity()),
                                        event.samplePosition);
                    map = -1;
                }
                else
                    output.addEvent(msg, event.samplePosition);
            }
            else
            {
                if (channel >= 0 && channel < 16 && (msg.isAllNotesOff() || msg.isAllSoundOff()))
                {
                    mapped[size_t(channel)].fill(-1);
                    counts[size_t(channel)].fill(0);
                }
                output.addEvent(msg, event.samplePosition);
            }
        }
    }
};
} // namespace noryn
