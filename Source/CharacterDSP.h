#pragma once
#include <JuceHeader.h>
namespace noryn
{
inline const std::array<float, 513> attackContour = []
{
    std::array<float, 513> table{};
    for (int i = 0; i <= 512; ++i)
        table[size_t(i)] = (1 - std::exp(-3 * i / 512.f)) / (1 - std::exp(-3.f));
    return table;
}();
// Legacy envelopes remain unchanged at amount=0. The same function drives the LCD.
inline float contour(float level, bool attacking, float amount)
{
    level = juce::jlimit(0.f, 1.f, level);
    if (amount <= 0)
        return level;
    const float at = level * 512;
    const int i = juce::jmin(511, int(at));
    const float shaped = attacking ? attackContour[size_t(i)] +
                                         (at - i) * (attackContour[size_t(i + 1)] - attackContour[size_t(i)])
                                   : level * level * level;
    return level + amount * (shaped - level);
}
struct CharacterLadder : juce::dsp::LadderFilter<float>
{
    float tick(float sample, int channel)
    {
        return processSample(sample, size_t(channel));
    }
    void advance()
    {
        updateSmoothers();
    }
};
// Two oppositely modulated delay lines with input/output bandwidth limiting.
// This is an original chorus design, not a claim of component-level JUNO emulation.
class EnsembleChorus
{
    juce::AudioBuffer<float> memory;
    int write = 0;
    double rate = 48000;
    float phase = 0, inputLP = 0, outputLP[2]{};
    juce::SmoothedValue<float> wetMix;

  public:
    void prepare(double sr)
    {
        rate = sr;
        memory.setSize(1, int(sr * .06) + 4);
        wetMix.reset(sr, .02);
        reset();
    }
    void reset()
    {
        memory.clear();
        write = 0;
        phase = .173f;
        inputLP = 0;
        outputLP[0] = outputLP[1] = 0;
        wetMix.setCurrentAndTargetValue(0);
    }
    void process(juce::AudioBuffer<float> &audio, float mix, int mode, float speed)
    {
        if (memory.getNumSamples() < 4)
            return;
        const int length = memory.getNumSamples();
        wetMix.setTargetValue(mix);
        const float coefficient = 1 - std::exp(-juce::MathConstants<float>::twoPi * 6500 / float(rate));
        const float hz = (mode == 1 ? .42f : .78f) * (.6f + speed);
        for (int i = 0; i < audio.getNumSamples(); ++i)
        {
            const float mixNow = wetMix.getNextValue();
            const float l = audio.getSample(0, i), r = audio.getSample(1, i);
            inputLP += coefficient * ((l + r) * .5f - inputLP);
            memory.setSample(0, write, inputLP);
            const float triangle = 1 - 4 * std::abs(phase - .5f);
            for (int channel = 0; channel < 2; ++channel)
            {
                float tap =
                    float(write) - float(rate) * (.007f + .0026f * triangle * (channel == 0 ? 1.f : -1.f));
                if (tap < 0)
                    tap += float(length);
                const int a = int(tap), b = (a + 1) % length;
                const float wet =
                    memory.getSample(0, a) + (tap - a) * (memory.getSample(0, b) - memory.getSample(0, a));
                outputLP[channel] += coefficient * (wet - outputLP[channel]);
                const float dry = channel == 0 ? l : r;
                audio.setSample(channel, i, dry * (1 - mixNow * .32f) + outputLP[channel] * mixNow * .72f);
            }
            phase += hz / float(rate);
            phase -= std::floor(phase);
            write = (write + 1) % length;
        }
    }
};
} // namespace noryn
