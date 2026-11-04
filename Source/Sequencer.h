#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
namespace noryn
{
struct SequenceStep
{
    std::atomic<uint64_t> low{0}, high{0};
    std::atomic<float> velocity{.8f}, length{1};
    bool has(int note) const
    {
        return ((note < 64 ? low.load() : high.load()) >> (note % 64)) & 1;
    }
    bool empty() const
    {
        return low.load() == 0 && high.load() == 0;
    }
    void add(int note, float vel)
    {
        (note < 64 ? low : high).fetch_or(uint64_t(1) << (note % 64));
        velocity.store(vel);
    }
    void clear()
    {
        low.store(0);
        high.store(0);
        length.store(1);
    }
    void toggle(int note, float vel)
    {
        (note < 64 ? low : high).fetch_xor(uint64_t(1) << (note % 64));
        velocity.store(vel);
    }
    int first() const
    {
        for (int n = 0; n < 128; ++n)
            if (has(n))
                return n;
        return -1;
    }
    int count() const
    {
        int count = 0;
        for (int n = 0; n < 128; ++n)
            if (has(n))
                ++count;
        return count;
    }
};
struct Sequencer
{
    std::array<SequenceStep, 64> steps;
    std::atomic<bool> playing{false}, recording{false}, stepRecord{false};
    std::atomic<int> playhead{-1}, recordCursor{0};
    std::atomic<float> effectiveBpm{120};
    std::array<double, 128> noteOffs{};
    std::array<bool, 128> held{};
    std::array<double, 128> recordedAt{};
    std::array<int, 128> recordedStep{};
    double sampleClock = 0;
    bool wasPlaying = false, wasRecording = false;
    uint64_t ticks = 0;
    int next = 0, current = 0;
    double untilNext = 0;
    float clickEnv = 0, clickPhase = 0, clickFrequency = 1600;
    void reset()
    {
        noteOffs.fill(-1);
        recordedAt.fill(-1);
        recordedStep.fill(-1);
        sampleClock = 0;
        held.fill(false);
        wasPlaying = wasRecording = false;
        ticks = 0;
        untilNext = 0;
        next = current = 0;
        playhead.store(-1);
        clickEnv = 0;
    }
    void clear()
    {
        for (auto &step : steps)
            step.clear();
        recordCursor.store(0);
    }
    void fill(int pattern)
    {
        clear();
        if (pattern == 0)
            return;
        const int acid[] = {36, 36, 43, 36, 48, 36, 46, 43, 36, 39, 43, 48, 46, 43, 39, 34};
        const int dream[] = {48, 55, 60, 64, 55, 60, 67, 64, 50, 57, 62, 65, 57, 62, 69, 65};
        for (int s = 0; s < 64; ++s)
        {
            int at = s % 16;
            if (pattern == 1)
            {
                if (at % 4 == 0 || at == 7 || at == 14)
                    steps[s].add(48, at % 4 == 0 ? .9f : .6f);
            }
            else if (pattern == 2)
            {
                if (at != 3 && at != 11)
                    steps[s].add(acid[at], at % 4 == 0 ? .95f : .65f);
            }
            else if (pattern == 3)
                steps[s].add(dream[at], .6f + (at % 4) * .08f);
            else if (pattern == 4 && at % 4 == 0)
            {
                int root = 48 + (s / 16) % 4 * 2;
                for (int interval : {0, 4, 7})
                    steps[s].add(root + interval, .75f);
            }
        }
    }
    void process(const juce::MidiBuffer &input, juce::MidiBuffer &output, juce::AudioBuffer<float> &clicks,
                 int count, double sr, float bpm, int length, int division, float gate, bool click,
                 float clickLevel)
    {
        using namespace juce;
        bpm = jlimit(30.f, 300.f, bpm);
        length = jlimit(1, 64, length);
        int perBeat = 1 << jlimit(0, 3, division);
        double stepSamples = sr * 60 / (bpm * perBeat);
        effectiveBpm.store(bpm);
        output.clear();
        output.addEvents(input, 0, count, 0);
        clicks.clear();
        bool run = playing.load(), record = recording.load(), stepMode = stepRecord.load();
        if (run && !wasPlaying)
        {
            untilNext = 0;
            ticks = 0;
            next = 0;
            current = 0;
        }
        if (!run && wasPlaying)
        {
            for (int note = 0; note < 128; ++note)
                if (noteOffs[note] >= 0)
                {
                    output.addEvent(MidiMessage::noteOff(16, note), 0);
                    noteOffs[note] = -1;
                }
            playhead.store(-1);
            clickEnv = 0;
        }
        if (!record && wasRecording)
            held.fill(false);
        auto event = input.cbegin(), end = input.cend();
        for (int sample = 0; sample < count; ++sample)
        {
            for (int note = 0; note < 128; ++note)
                if (noteOffs[note] >= 0)
                {
                    if (noteOffs[note] <= 0)
                    {
                        output.addEvent(MidiMessage::noteOff(16, note), sample);
                        noteOffs[note] = -1;
                    }
                    else
                        noteOffs[note] -= 1;
                }
            if (run && untilNext <= 0)
            {
                current = next % length;
                next = (current + 1) % length;
                playhead.store(current);
                auto &step = steps[size_t(current)];
                for (int note = 0; note < 128; ++note)
                    if (step.has(note))
                    {
                        if (noteOffs[note] >= 0)
                            output.addEvent(MidiMessage::noteOff(16, note), sample);
                        output.addEvent(MidiMessage::noteOn(16, note, step.velocity.load()), sample);
                        noteOffs[note] = jmax(1.0, stepSamples * gate * step.length.load());
                    }
                untilNext += stepSamples;
                if (click && ticks % perBeat == 0)
                {
                    clickEnv = 1;
                    clickPhase = 0;
                    clickFrequency = ticks % (perBeat * 4) == 0 ? 2200.f : 1500.f;
                }
                ++ticks;
            }
            while (event != end && (*event).samplePosition <= sample)
            {
                auto message = (*event).getMessage();
                if (record && message.isNoteOn())
                {
                    int target = stepMode ? recordCursor.load() % length
                                          : (current + (untilNext < stepSamples * .5 ? 1 : 0)) % length;
                    steps[size_t(target)].add(message.getNoteNumber(), message.getFloatVelocity());
                    recordedAt[size_t(message.getNoteNumber())] = sampleClock;
                    recordedStep[size_t(message.getNoteNumber())] = target;
                    held[size_t(message.getNoteNumber())] = true;
                }
                else if (record && message.isNoteOff())
                {
                    int note = message.getNoteNumber();
                    if (!stepMode && recordedAt[size_t(note)] >= 0 && recordedStep[size_t(note)] >= 0)
                    {
                        steps[size_t(recordedStep[size_t(note)])].length.store(juce::jlimit(
                            .05f, 64.f,
                            float((sampleClock - recordedAt[size_t(note)]) / (stepSamples * gate))));
                        recordedAt[size_t(note)] = -1;
                    }
                    held[size_t(message.getNoteNumber())] = false;
                    if (stepMode && std::none_of(held.begin(), held.end(), [](bool down) { return down; }))
                        recordCursor.store((recordCursor.load() + 1) % length);
                }
                ++event;
            }
            if (run)
                untilNext -= 1;
            sampleClock += 1;
            if (click && run && clickEnv > 1.e-4f)
            {
                float value =
                    std::sin(clickPhase * juce::MathConstants<float>::twoPi) * clickEnv * clickLevel * .5f;
                clicks.setSample(0, sample, value);
                clickPhase += clickFrequency / float(sr);
                clickPhase -= std::floor(clickPhase);
                clickEnv *= std::exp(-1.f / (float(sr) * .008f));
            }
        }
        wasPlaying = run;
        wasRecording = record;
    }
    juce::ValueTree save() const
    {
        juce::ValueTree tree("Sequence64");
        for (int i = 0; i < 64; ++i)
        {
            juce::ValueTree step("Step");
            step.setProperty("index", i, nullptr);
            step.setProperty("low", juce::String::toHexString(juce::int64(steps[i].low.load())), nullptr);
            step.setProperty("high", juce::String::toHexString(juce::int64(steps[i].high.load())), nullptr);
            step.setProperty("velocity", steps[i].velocity.load(), nullptr);
            step.setProperty("length", steps[i].length.load(), nullptr);
            tree.addChild(step, -1, nullptr);
        }
        return tree;
    }
    void restore(const juce::ValueTree &tree)
    {
        if (!tree.isValid())
            return;
        clear();
        for (const auto step : tree)
        {
            int i = int(step.getProperty("index", -1));
            if (i < 0 || i >= 64)
                continue;
            steps[i].low.store(uint64_t(step.getProperty("low").toString().getHexValue64()));
            steps[i].high.store(uint64_t(step.getProperty("high").toString().getHexValue64()));
            steps[i].velocity.store(juce::jlimit(.01f, 1.f, float(step.getProperty("velocity", .8f))));
            steps[i].length.store(juce::jlimit(.05f, 64.f, float(step.getProperty("length", 1.f))));
        }
    }
};
} // namespace noryn
