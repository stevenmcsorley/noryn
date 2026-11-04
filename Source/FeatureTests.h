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
    require(live.steps[0].length.load() > .05f && live.steps[0].length.load() < 1,
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
