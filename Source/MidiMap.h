#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
namespace noryn
{
// Controller (channel + CC) to control assignments, learned from incoming MIDI.
// Targets are parameter indices, or one of the transport targets below.
struct MidiMap
{
    static constexpr int none = -1, playTarget = -2, recordTarget = -3;
    static constexpr int slots = 16 * 128;
    std::array<std::atomic<int>, slots> targets;
    std::atomic<int> learning{none}; // target waiting for a controller
    std::atomic<int> learnedSlot{-1};
    std::atomic<bool> dirty{false};
    std::array<int, slots> lastValue{}; // audio thread only, for transport button edges
    MidiMap()
    {
        for (auto &t : targets)
            t.store(none);
    }
    static int slotFor(int channel, int cc)
    {
        return (channel - 1) * 128 + cc;
    }
    static juce::String describe(int slot)
    {
        return "CC " + juce::String(slot % 128) + " / ch " + juce::String(slot / 128 + 1);
    }
    juce::Array<int> slotsFor(int target) const
    {
        juce::Array<int> found;
        for (int s = 0; s < slots; ++s)
            if (targets[size_t(s)].load() == target)
                found.add(s);
        return found;
    }
    void assign(int slot, int target)
    {
        // One controller per target keeps learning predictable: re-learning replaces the old controller.
        for (auto &t : targets)
            if (t.load() == target)
                t.store(none);
        targets[size_t(slot)].store(target);
        dirty.store(true);
    }
    void clearTarget(int target)
    {
        for (auto &t : targets)
            if (t.load() == target)
                t.store(none);
        dirty.store(true);
    }
    void clearAll()
    {
        for (auto &t : targets)
            t.store(none);
        dirty.store(true);
    }
    // Parameters are stored by ID so maps survive parameter reordering between versions.
    juce::ValueTree save(const juce::StringArray &ids) const
    {
        juce::ValueTree tree("MidiMap");
        for (int s = 0; s < slots; ++s)
        {
            const int t = targets[size_t(s)].load();
            if (t == none)
                continue;
            juce::ValueTree entry("Map");
            entry.setProperty("channel", s / 128 + 1, nullptr);
            entry.setProperty("cc", s % 128, nullptr);
            entry.setProperty("target", t == playTarget ? "transport:play"
                                        : t == recordTarget ? "transport:record"
                                                            : ids[t],
                              nullptr);
            tree.addChild(entry, -1, nullptr);
        }
        return tree;
    }
    void restore(const juce::ValueTree &tree, const juce::StringArray &ids)
    {
        for (auto &t : targets)
            t.store(none);
        for (const auto entry : tree)
        {
            const int channel = entry.getProperty("channel", 0), cc = entry.getProperty("cc", -1);
            const juce::String id = entry.getProperty("target").toString();
            const int target = id == "transport:play" ? playTarget
                               : id == "transport:record" ? recordTarget
                                                          : ids.indexOf(id);
            if (channel >= 1 && channel <= 16 && cc >= 0 && cc < 120 && target != none)
                targets[size_t(slotFor(channel, cc))].store(target);
        }
    }
};
} // namespace noryn
