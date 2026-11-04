#pragma once
#include "ScaleEngine.h"
#include "Sequencer.h"
inline void testScaleEngine()
{
    using namespace noryn;
    using namespace juce;
    for (int scale = 0; scale < int(ScaleEngine::scales().size()); ++scale)
        for (int root = 0; root < 12; ++root)
            for (int direction = 0; direction < 3; ++direction)
                for (int note = 0; note < 128; ++note)
                {
                    int q = ScaleEngine::quantize(note, scale, root, direction, 0xA95);
                    auto bits = scale == int(ScaleEngine::scales().size()) - 1
                                    ? 0xA95
                                    : ScaleEngine::scales()[size_t(scale)].mask;
                    require(q >= 0 && q < 128 && ((bits >> ((q - root + 120) % 12)) & 1),
                            "Scale quantizer produced an out-of-scale note");
                    require(std::abs(q - note) <= 11, "Scale boundary wrapped to another octave range");
                }
    ScaleEngine scale;
    MidiBuffer input, output;
    input.addEvent(MidiMessage::noteOn(1, 61, .8f), 0);
    input.addEvent(MidiMessage::noteOn(1, 60, .7f), 1);
    scale.process(input, output, 1, 0, 0, 4095, true);
    require(output.getNumEvents() == 1, "Colliding quantized notes were retriggered");
    input.clear();
    input.addEvent(MidiMessage::noteOff(1, 61), 0);
    scale.process(input, output, 1, 4, 0, 4095, true);
    require(output.isEmpty(), "First colliding key released the other key");
    input.clear();
    input.addEvent(MidiMessage::noteOff(1, 60), 0);
    scale.process(input, output, 1, 4, 0, 4095, true);
    require(output.getNumEvents() == 1 && (*output.cbegin()).getMessage().getNoteNumber() == 60,
            "Scale change left a stuck note");
}
inline void testSequencer()
{
    using namespace noryn;
    using namespace juce;
    for (int bpm : {120, 240})
    {
        Sequencer seq;
        seq.reset();
        for (int s = 0; s < 64; ++s)
            seq.steps[size_t(s)].add(48 + s % 12, .8f);
        seq.playing.store(true);
        int expectedStep = 48000 * 60 / (bpm * 4), total = expectedStep * 64;
        std::vector<int> times;
        double clickEnergy = 0;
        for (int at = 0; at < total; at += 256)
        {
            int n = jmin(256, total - at);
            AudioBuffer<float> click(1, n);
            MidiBuffer in, out;
            seq.process(in, out, click, n, 48000, float(bpm), 64, 2, .7f, true, .4f);
            for (const auto event : out)
                if (event.getMessage().isNoteOn())
                    times.push_back(at + event.samplePosition);
            for (int i = 0; i < n; ++i)
                clickEnergy += click.getSample(0, i) * click.getSample(0, i);
        }
        require(times.size() == 64, "Sequencer did not play all 64 steps");
        for (int i = 0; i < 64; ++i)
            require(std::abs(times[size_t(i)] - i * expectedStep) <= 1, "Sequencer tempo drift");
        require(clickEnergy > .01, "Metronome silent");
        seq.playing.store(false);
        AudioBuffer<float> click(1, 256);
        MidiBuffer in, out;
        seq.process(in, out, click, 256, 48000, float(bpm), 64, 2, .7f, true, .4f);
        require(seq.playhead.load() == -1, "Sequence stop left the playhead running");
    }
    Sequencer live;
    live.reset();
    live.playing.store(true);
    live.recording.store(true);
    AudioBuffer<float> click(1, 6000);
    MidiBuffer input, output;
    input.addEvent(MidiMessage::noteOn(1, 60, .8f), 1000);
    input.addEvent(MidiMessage::noteOff(1, 60), 2500);
    input.addEvent(MidiMessage::noteOn(1, 64, .7f), 5000);
    input.addEvent(MidiMessage::noteOff(1, 64), 5900);
    live.process(input, output, click, 6000, 48000, 120, 64, 2, .7f, false, .4f);
    require(live.steps[0].has(60) && live.steps[1].has(64),
            "Live recording was not quantized to nearest step");
    require(live.steps[0].lengthFor(60) > .05f && live.steps[0].lengthFor(60) < 1,
            "Live note length was not recorded");
    Sequencer step;
    step.reset();
    step.recording.store(true);
    step.stepRecord.store(true);
    input.clear();
    input.addEvent(MidiMessage::noteOn(1, 60, .8f), 0);
    input.addEvent(MidiMessage::noteOn(1, 64, .8f), 0);
    input.addEvent(MidiMessage::noteOff(1, 60), 100);
    input.addEvent(MidiMessage::noteOff(1, 64), 200);
    input.addEvent(MidiMessage::noteOn(1, 67, .8f), 300);
    input.addEvent(MidiMessage::noteOff(1, 67), 400);
    step.process(input, output, click, 6000, 48000, 120, 64, 2, .7f, false, .4f);
    require(step.steps[0].count() == 2 && step.steps[1].has(67) && step.recordCursor.load() == 2,
            "Step-record chord entry failed");
    step.steps[63].add(127, .6f);
    step.steps[63].length.store(16);
    auto saved = step.save();
    Sequencer restored;
    restored.restore(saved);
    require(restored.steps[63].has(127) && restored.steps[0].count() == 2 &&
                restored.steps[63].length.load() == 16,
            "64-step pattern recall failed");
}
inline void testEnvelopeReleaseReference()
{
    // Reproduce the former integration error with the exact JUCE ADSR behavior.
    juce::ADSR correct, former;
    const juce::ADSR::Parameters parameters{.001f, 1, .02f, .08f};
    for (auto *envelope : {&correct, &former})
    {
        envelope->setSampleRate(48000);
        envelope->setParameters(parameters);
        envelope->noteOn();
    }
    for (int i = 0; i < 960; ++i)
    {
        correct.getNextSample();
        former.getNextSample();
    }
    correct.noteOff();
    former.noteOff();
    for (int i = 0; i < 9600; ++i)
    {
        if (i % 256 == 0)
            former.setParameters(parameters);
        correct.getNextSample();
        former.getNextSample();
    }
    require(!correct.isActive() && former.isActive(), "ADSR release regression was not reproduced");
}
inline void testSequencerSafety()
{
    using namespace noryn;
    using namespace juce;
    Sequencer record;
    record.reset();
    record.playing.store(true);
    record.recording.store(true);
    AudioBuffer<float> click(1, 16000);
    MidiBuffer in, out;
    in.addEvent(MidiMessage::noteOn(1, 60, .9f), 100);
    in.addEvent(MidiMessage::noteOn(1, 64, .5f), 100);
    in.addEvent(MidiMessage::noteOff(1, 60), 700);
    in.addEvent(MidiMessage::noteOff(1, 64), 3100);
    record.process(in, out, click, 6000, 48000, 120, 64, 2, 1, false, .4f);
    require(std::abs(record.steps[0].lengthFor(60) - .1f) < .001f &&
                std::abs(record.steps[0].lengthFor(64) - .5f) < .001f,
            "Recording one chord note overwrote another note's duration");
    require(std::abs(record.steps[0].velocityFor(60) - .9f) < .01f &&
                std::abs(record.steps[0].velocityFor(64) - .5f) < .01f,
            "Chord velocities were flattened");
    auto state = record.save();
    Sequencer recalled;
    recalled.restore(state);
    require(std::abs(recalled.steps[0].lengthFor(60) - .1f) < .001f &&
                std::abs(recalled.steps[0].lengthFor(64) - .5f) < .001f,
            "Per-note durations were not recalled");
    Sequencer timing;
    timing.reset();
    timing.steps[0].add(60, .8f, 2);
    timing.steps[0].add(64, .7f, .25f);
    timing.playing.store(true);
    in.clear();
    timing.process(in, out, click, 13000, 48000, 120, 64, 2, 1, false, .4f);
    int off60 = -1, off64 = -1;
    for (auto event : out)
        if (event.getMessage().isNoteOff())
        {
            if (event.getMessage().getNoteNumber() == 60)
                off60 = event.samplePosition;
            if (event.getMessage().getNoteNumber() == 64)
                off64 = event.samplePosition;
        }
    require(off60 == 12000 && off64 == 1500, "Note-offs did not match independent gate durations");
    Sequencer loop;
    loop.reset();
    loop.steps[0].add(60, .8f, 16);
    loop.playing.store(true);
    loop.process(in, out, click, 256, 48000, 120, 1, 2, 1, false, .4f);
    loop.clear();
    loop.process(in, out, click, 256, 48000, 120, 1, 2, 1, false, .4f);
    bool released = false;
    for (auto event : out)
        released = released || event.getMessage().isAllNotesOff();
    require(released &&
                std::all_of(loop.noteOffs.begin(), loop.noteOffs.end(), [](double n) { return n < 0; }),
            "Clear left a long note sounding");
    loop.steps[0].add(60, .8f, 64);
    loop.process(in, out, click, 256, 48000, 120, 1, 2, 1, false, .4f);
    loop.playing.store(false);
    loop.process(in, out, click, 256, 48000, 120, 1, 2, 1, false, .4f);
    released = false;
    for (auto event : out)
        released = released || event.getMessage().isAllNotesOff();
    require(released, "Stop did not release the sequencer channel");
    // A five-minute 64-step run must keep note events balanced across loop boundaries.
    Sequencer longRun;
    longRun.reset();
    longRun.fill(3);
    longRun.playing.store(true);
    std::array<bool, 128> sounding{};
    int onCount = 0, offCount = 0, maximum = 0;
    for (int offset = 0; offset < 48000 * 300; offset += 256)
    {
        longRun.process(in, out, click, 256, 48000, 173, 64, 2, .55f, false, .4f);
        for (auto event : out)
        {
            auto msg = event.getMessage();
            if (msg.isNoteOn())
            {
                require(!sounding[size_t(msg.getNoteNumber())], "Retrigger omitted previous note-off");
                sounding[size_t(msg.getNoteNumber())] = true;
                ++onCount;
            }
            else if (msg.isNoteOff())
            {
                sounding[size_t(msg.getNoteNumber())] = false;
                ++offCount;
            }
            else if (msg.isAllNotesOff())
                sounding.fill(false);
        }
        maximum = jmax(maximum, int(std::count(sounding.begin(), sounding.end(), true)));
    }
    longRun.playing.store(false);
    longRun.process(in, out, click, 256, 48000, 173, 64, 2, .55f, false, .4f);
    for (auto event : out)
        if (event.getMessage().isAllNotesOff())
            sounding.fill(false);
    require(maximum <= 1 && onCount > 3000 && std::abs(onCount - offCount) <= 1 &&
                std::none_of(sounding.begin(), sounding.end(), [](bool b) { return b; }),
            "Long-run sequencer note accounting accumulated held notes");
}
