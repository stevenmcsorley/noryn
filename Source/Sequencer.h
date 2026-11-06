#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
namespace noryn
{
constexpr int maxSequenceSteps = 256;
constexpr float maxStepLength = float(maxSequenceSteps);
struct SequenceStep
{
    std::atomic<uint64_t> low{0}, high{0};
    std::atomic<float> velocity{.8f}, length{1};
    std::array<std::atomic<float>, 128> noteLengths{}, noteVelocities{};
    float lengthFor(int note) const
    {
        float own = noteLengths[size_t(note)].load();
        return own > 0 ? own : length.load();
    }
    float velocityFor(int note) const
    {
        float own = noteVelocities[size_t(note)].load();
        return own > 0 ? own : velocity.load();
    }
    bool has(int note) const
    {
        return ((note < 64 ? low.load() : high.load()) >> (note % 64)) & 1;
    }
    bool empty() const
    {
        return low.load() == 0 && high.load() == 0;
    }
    void add(int note, float vel, float duration = 0)
    {
        if (note < 0 || note > 127 || (!has(note) && count() >= 16))
            return;
        noteVelocities[size_t(note)].store(juce::jlimit(.01f, 1.f, vel));
        noteLengths[size_t(note)].store(duration > 0 ? juce::jlimit(.05f, maxStepLength, duration) : 0);
        (note < 64 ? low : high).fetch_or(uint64_t(1) << (note % 64));
        velocity.store(vel);
    }
    void clear()
    {
        low.store(0);
        high.store(0);
        length.store(1);
        for (auto &n : noteLengths)
            n.store(0);
        for (auto &n : noteVelocities)
            n.store(0);
    }
    void toggle(int note, float vel)
    {
        if (note < 0 || note > 127 || (!has(note) && count() >= 16))
            return;
        noteLengths[size_t(note)].store(0);
        noteVelocities[size_t(note)].store(vel);
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
    std::array<SequenceStep, maxSequenceSteps> steps;
    std::atomic<bool> playing{false}, recording{false}, stepRecord{false};
    std::atomic<int> playhead{-1}, recordCursor{0};
    std::atomic<float> effectiveBpm{120};
    std::array<double, 128> noteOffs{};
    std::array<bool, 128> held{};
    std::array<double, 128> recordedAt{};
    std::array<int, 128> recordedStep{};
    std::array<int, 128> activeStep{};
    std::atomic<bool> flushRequested{true}, restartRequested{false};
    std::atomic<int> startStep{0}; // where the next start begins (the studio timeline sets it)
    double sampleClock = 0, nextTick = 0, lastStepSamples = 0;
    bool wasPlaying = false, wasRecording = false;
    uint64_t ticks = 0;
    int next = 0, current = 0;
    double untilNext = 0;
    float clickEnv = 0, clickPhase = 0, clickFrequency = 1600;
    void reset()
    {
        noteOffs.fill(-1);
        activeStep.fill(-1);
        flushRequested.store(true);
        restartRequested.store(false);
        nextTick = lastStepSamples = 0;
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
    void stop()
    {
        playing.store(false);
        recording.store(false);
        flushRequested.store(true);
    }
    void clear()
    {
        flushRequested.store(true);
        restartRequested.store(true);
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
        for (int s = 0; s < maxSequenceSteps; ++s)
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
                 float clickLevel, float entryLength = 1)
    {
        using namespace juce;
        bpm = jlimit(30.f, 300.f, bpm);
        length = jlimit(1, maxSequenceSteps, length);
        gate = jlimit(.05f, 1.f, gate);
        const int perBeat = 1 << jlimit(0, 3, division);
        const double stepSamples = sr * 60 / (bpm * perBeat), blockStart = sampleClock;
        effectiveBpm.store(bpm);
        output.clear();
        output.addEvents(input, 0, count, 0);
        clicks.clear();
        const bool run = playing.load(), record = recording.load(), stepMode = stepRecord.load();
        auto releaseAll = [&]
        {
            output.addEvent(MidiMessage::controllerEvent(16, 64, 0), 0);
            output.addEvent(MidiMessage::controllerEvent(16, 66, 0), 0);
            output.addEvent(MidiMessage::allNotesOff(16), 0);
            noteOffs.fill(-1);
            activeStep.fill(-1);
            clickEnv = 0;
        };
        if (flushRequested.exchange(false) || (!run && wasPlaying))
            releaseAll();
        if (lastStepSamples > 0 && std::abs(lastStepSamples - stepSamples) > .00001)
        {
            const double ratio = stepSamples / lastStepSamples;
            nextTick = blockStart + jmax(0.0, nextTick - blockStart) * ratio;
            for (auto &deadline : noteOffs)
                if (deadline >= 0)
                    deadline = blockStart + jmax(0.0, deadline - blockStart) * ratio;
        }
        lastStepSamples = stepSamples;
        const bool restart = restartRequested.exchange(false);
        if ((run && !wasPlaying) || restart)
        {
            nextTick = blockStart;
            ticks = 0;
            next = current = jmax(0, startStep.exchange(0)) % length;
        }
        if (!run)
        {
            playhead.store(-1);
            clickEnv = 0;
        }
        // Removing a sounding pad/note releases it in the next block, even if its old gate was long.
        for (int note = 0; note < 128; ++note)
            if (noteOffs[size_t(note)] >= 0 && activeStep[size_t(note)] >= 0 &&
                !steps[size_t(activeStep[size_t(note)])].has(note))
            {
                output.addEvent(MidiMessage::noteOff(16, note), 0);
                noteOffs[size_t(note)] = -1;
                activeStep[size_t(note)] = -1;
            }
        auto finishRecorded = [&](int note, double now)
        {
            if (recordedAt[size_t(note)] >= 0 && recordedStep[size_t(note)] >= 0)
                steps[size_t(recordedStep[size_t(note)])].noteLengths[size_t(note)].store(
                    jlimit(.05f, maxStepLength, float((now - recordedAt[size_t(note)]) / stepSamples)));
            recordedAt[size_t(note)] = -1;
            recordedStep[size_t(note)] = -1;
            held[size_t(note)] = false;
        };
        if (!record && wasRecording)
            for (int note = 0; note < 128; ++note)
                finishRecorded(note, blockStart);
        if (record && !wasRecording)
        {
            held.fill(false);
            recordedAt.fill(-1);
            recordedStep.fill(-1);
        }
        auto event = input.cbegin(), end = input.cend();
        const float clickDecay = std::exp(-1.f / (float(sr) * .008f));
        int at = 0;
        while (at < count)
        {
            const double now = blockStart + at;
            // Off events precede a retrigger at the same sample.
            for (int note = 0; note < 128; ++note)
                if (noteOffs[size_t(note)] >= 0 && noteOffs[size_t(note)] <= now + .000001)
                {
                    output.addEvent(MidiMessage::noteOff(16, note), at);
                    noteOffs[size_t(note)] = -1;
                    activeStep[size_t(note)] = -1;
                }
            if (run && nextTick <= now + .000001)
            {
                current = next % length;
                next = (current + 1) % length;
                playhead.store(current);
                auto &step = steps[size_t(current)];
                int voices = 0;
                for (int note = 0; note < 128 && voices < 16; ++note)
                    if (step.has(note))
                    {
                        if (record && !stepMode && held[size_t(note)] &&
                            recordedStep[size_t(note)] == current)
                            continue;
                        if (noteOffs[size_t(note)] >= 0)
                            output.addEvent(MidiMessage::noteOff(16, note), at);
                        output.addEvent(MidiMessage::noteOn(16, note, step.velocityFor(note)), at);
                        noteOffs[size_t(note)] =
                            now + jmax(1.0, std::round(stepSamples * gate * step.lengthFor(note)));
                        activeStep[size_t(note)] = current;
                        ++voices;
                    }
                nextTick += stepSamples;
                if (click && ticks % perBeat == 0)
                {
                    clickEnv = 1;
                    clickPhase = 0;
                    clickFrequency = ticks % (perBeat * 4) == 0 ? 2200.f : 1500.f;
                }
                ++ticks;
            }
            while (event != end && (*event).samplePosition <= at)
            {
                const auto msg = (*event).getMessage();
                if (record && msg.isNoteOn())
                {
                    const int target = stepMode || !run
                                           ? recordCursor.load() % length
                                           : (current + (nextTick - now < stepSamples * .5 ? 1 : 0)) % length;
                    const int note = msg.getNoteNumber();
                    steps[size_t(target)].add(note, msg.getFloatVelocity(), stepMode ? entryLength : 0);
                    recordedAt[size_t(note)] = stepMode ? -1 : now;
                    recordedStep[size_t(note)] = stepMode ? -1 : target;
                    held[size_t(note)] = true;
                }
                else if (record && msg.isNoteOff())
                {
                    const int note = msg.getNoteNumber();
                    if (!stepMode)
                        finishRecorded(note, now);
                    else
                        held[size_t(note)] = false;
                    if (stepMode && std::none_of(held.begin(), held.end(), [](bool down) { return down; }))
                        recordCursor.store((recordCursor.load() + 1) % length);
                }
                else if (record && (msg.isAllNotesOff() || msg.isAllSoundOff()))
                    for (int note = 0; note < 128; ++note)
                        finishRecorded(note, now);
                ++event;
            }
            int nextAt = count;
            if (run)
                nextAt = jmin(nextAt, jmax(at + 1, int(std::ceil(nextTick - blockStart - .000001))));
            if (event != end)
                nextAt = jmin(nextAt, jmax(at + 1, (*event).samplePosition));
            for (double deadline : noteOffs)
                if (deadline >= 0)
                    nextAt = jmin(nextAt, jmax(at + 1, int(std::ceil(deadline - blockStart - .000001))));
            if (click && run && clickEnv > 1.e-4f)
                for (int sample = at; sample < nextAt; ++sample)
                {
                    clicks.setSample(0, sample,
                                     std::sin(clickPhase * juce::MathConstants<float>::twoPi) * clickEnv *
                                         clickLevel * .5f);
                    clickPhase += clickFrequency / float(sr);
                    clickPhase -= std::floor(clickPhase);
                    clickEnv *= clickDecay;
                }
            at = nextAt;
        }
        sampleClock += count;
        untilNext = nextTick - sampleClock;
        wasPlaying = run;
        wasRecording = record;
    }
    juce::ValueTree save() const
    {
        // The tag keeps its original name so existing patches and host sessions still load.
        juce::ValueTree tree("Sequence64");
        for (int i = 0; i < maxSequenceSteps; ++i)
        {
            if (steps[i].empty())
                continue; // restore() clears first, so empty steps need no entry
            juce::ValueTree step("Step");
            step.setProperty("index", i, nullptr);
            step.setProperty("low", juce::String::toHexString(juce::int64(steps[i].low.load())), nullptr);
            step.setProperty("high", juce::String::toHexString(juce::int64(steps[i].high.load())), nullptr);
            step.setProperty("velocity", steps[i].velocity.load(), nullptr);
            step.setProperty("length", steps[i].length.load(), nullptr);
            for (int n = 0; n < 128; ++n)
                if (steps[i].has(n))
                {
                    juce::ValueTree detail("Note");
                    detail.setProperty("note", n, nullptr);
                    detail.setProperty("velocity", steps[i].velocityFor(n), nullptr);
                    detail.setProperty("length", steps[i].lengthFor(n), nullptr);
                    step.addChild(detail, -1, nullptr);
                }
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
            if (i < 0 || i >= maxSequenceSteps)
                continue;
            steps[i].low.store(uint64_t(step.getProperty("low").toString().getHexValue64()));
            steps[i].high.store(uint64_t(step.getProperty("high").toString().getHexValue64()));
            steps[i].velocity.store(juce::jlimit(.01f, 1.f, float(step.getProperty("velocity", .8f))));
            steps[i].length.store(juce::jlimit(.05f, maxStepLength, float(step.getProperty("length", 1.f))));
            for (const auto &detail : step)
            {
                const int n = int(detail.getProperty("note", -1));
                if (n < 0 || n > 127 || !steps[i].has(n))
                    continue;
                steps[i].noteLengths[size_t(n)].store(
                    juce::jlimit(.05f, maxStepLength, float(detail.getProperty("length", steps[i].length.load()))));
                steps[i].noteVelocities[size_t(n)].store(
                    juce::jlimit(.01f, 1.f, float(detail.getProperty("velocity", steps[i].velocity.load()))));
            }
        }
    }
};
} // namespace noryn
