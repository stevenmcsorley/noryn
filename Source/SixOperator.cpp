/*
 * The FM routing topology table is derived from Google's Music Synthesizer for Android.
 * Copyright 2012 Google Inc. Licensed under Apache-2.0.
 * http://www.apache.org/licenses/LICENSE-2.0
 * See licenses/MSFA-APACHE-2.0.txt and Source/ThirdParty/msfa/NORYN-NOTICE.md.
 * Original NORYN envelope/voice wrapper and voice designs: AGPL-3.0-only.
 */
#include "SixOperator.h"
#include "ThirdParty/msfa/fm_core.h"
#include "ThirdParty/msfa/sin.h"
#undef N
#undef LG_N
#include <JuceHeader.h>
#include <mutex>
namespace noryn
{
namespace
{
struct FMDesign
{
    int algorithm, feedback;
    std::array<float, 6> ratios, levels, decays;
    float sustain, attack, release;
};
// Original voice designs. Operator order is 6..1, following MSFA routing.
const FMDesign designs[12] = {
    {4,
     0,
     {1, 1, 14, 1, 1, 1},
     {.22f, .52f, .08f, .13f, .15f, .25f},
     {.6f, 2.8f, .09f, 1.2f, .8f, 3.2f},
     .08f,
     .001f,
     .8f},
    {4,
     1,
     {1, 1, 7, 1, 3, 1},
     {.32f, .48f, .14f, .18f, .2f, .24f},
     {.45f, 2.2f, .07f, .7f, .4f, 2.6f},
     .04f,
     .001f,
     .65f},
    {4,
     0,
     {3.5f, 1, 2.75f, 2, 7, 4},
     {.27f, .48f, .19f, .22f, .11f, .13f},
     {1.8f, 3.5f, .8f, 2.6f, .35f, 1.7f},
     0,
     .001f,
     1.8f},
    {4,
     0,
     {1, 1, .5f, 1, 1, 2},
     {.12f, .58f, .08f, .18f, .06f, .1f},
     {.8f, 4, .5f, 3, .6f, 2.8f},
     .12f,
     .006f,
     1.2f},
    {4,
     0,
     {2, 1, 3, 2, 4, 3},
     {.17f, .55f, .12f, .19f, .07f, .1f},
     {.16f, .7f, .12f, .5f, .08f, .35f},
     0,
     .001f,
     .3f},
    {2,
     2,
     {7.13f, 3.5f, 1, 5.71f, 2.75f, 2},
     {.13f, .18f, .5f, .14f, .16f, .24f},
     {1.4f, 1.8f, 3.2f, .6f, 1.2f, 2.7f},
     0,
     .001f,
     1.6f},
    {4,
     2,
     {1, 1, 2, 1, 3, 1},
     {.19f, .49f, .13f, .2f, .08f, .22f},
     {.5f, 2.7f, .19f, 1.3f, .25f, 2.1f},
     .1f,
     .002f,
     .7f},
    {4,
     3,
     {1, 1, 2, 1, 3, 1},
     {.35f, .7f, .17f, .1f, .08f, .08f},
     {.1f, .8f, .06f, .5f, .04f, .3f},
     .15f,
     .001f,
     .15f},
    {0,
     2,
     {1, 2, 1, 1, 3, 1},
     {.18f, .15f, .2f, .45f, .13f, .3f},
     {.08f, .12f, .15f, .5f, .09f, .45f},
     0,
     .001f,
     .15f},
    {2,
     5,
     {5.71f, 2.37f, 1, 8.13f, 3.5f, 1.5f},
     {.21f, .16f, .42f, .14f, .18f, .24f},
     {.15f, .25f, .7f, .1f, .2f, .5f},
     0,
     .001f,
     .35f},
    {4, 0, {1, 1, 2, 2, 3, 4}, {.035f, .5f, .025f, .25f, .015f, .15f}, {3, 3, 3, 3, 3, 3}, .85f, .004f, .15f},
    {4,
     0,
     {1, 1, 2, 1, 4, 2},
     {.13f, .47f, .09f, .22f, .06f, .14f},
     {1.5f, 3.5f, .7f, 2.7f, .5f, 2.4f},
     .22f,
     .012f,
     1.4f}};
// Routing flags are read from the upstream 32-algorithm table below. This helper
// is used for original UI previews and carrier-dependent brightness scaling.
constexpr unsigned char topology[32][6] = {
    {0xc1, 0x11, 0x11, 0x14, 0x01, 0x14}, {0x01, 0x11, 0x11, 0x14, 0xc1, 0x14},
    {0xc1, 0x11, 0x14, 0x01, 0x11, 0x14}, {0x41, 0x11, 0x94, 0x01, 0x11, 0x14},
    {0xc1, 0x14, 0x01, 0x14, 0x01, 0x14}, {0x41, 0x94, 0x01, 0x14, 0x01, 0x14},
    {0xc1, 0x11, 0x05, 0x14, 0x01, 0x14}, {0x01, 0x11, 0xc5, 0x14, 0x01, 0x14},
    {0x01, 0x11, 0x05, 0x14, 0xc1, 0x14}, {0x01, 0x05, 0x14, 0xc1, 0x11, 0x14},
    {0xc1, 0x05, 0x14, 0x01, 0x11, 0x14}, {0x01, 0x05, 0x05, 0x14, 0xc1, 0x14},
    {0xc1, 0x05, 0x05, 0x14, 0x01, 0x14}, {0xc1, 0x05, 0x11, 0x14, 0x01, 0x14},
    {0x01, 0x05, 0x11, 0x14, 0xc1, 0x14}, {0xc1, 0x11, 0x02, 0x25, 0x05, 0x14},
    {0x01, 0x11, 0x02, 0x25, 0xc5, 0x14}, {0x01, 0x11, 0x11, 0xc5, 0x05, 0x14},
    {0xc1, 0x14, 0x14, 0x01, 0x11, 0x14}, {0x01, 0x05, 0x14, 0xc1, 0x14, 0x14},
    {0x01, 0x14, 0x14, 0xc1, 0x14, 0x14}, {0xc1, 0x14, 0x14, 0x14, 0x01, 0x14},
    {0xc1, 0x14, 0x14, 0x01, 0x14, 0x04}, {0xc1, 0x14, 0x14, 0x14, 0x04, 0x04},
    {0xc1, 0x14, 0x14, 0x04, 0x04, 0x04}, {0xc1, 0x05, 0x14, 0x01, 0x14, 0x04},
    {0x01, 0x05, 0x14, 0xc1, 0x14, 0x04}, {0x04, 0xc1, 0x11, 0x14, 0x01, 0x14},
    {0xc1, 0x14, 0x01, 0x14, 0x04, 0x04}, {0x04, 0xc1, 0x11, 0x14, 0x04, 0x04},
    {0xc1, 0x14, 0x04, 0x04, 0x04, 0x04}, {0xc4, 0x04, 0x04, 0x04, 0x04, 0x04}};
} // namespace
struct SixOperator::Impl
{
    FmCore core;
    FmOpParams operators[6]{};
    std::array<juce::ADSR, 6> envelopes;
    int32_t feedback[2]{}, output[64]{};
    int at = 64, profile = 0;
    double sr = 96000;
    float velocity = .8f;
    void fill(float hz, float index, float ratio)
    {
        const auto &d = designs[profile];
        std::fill(std::begin(output), std::end(output), 0);
        for (int op = 0; op < 6; ++op)
        {
            const bool carrier = (topology[d.algorithm][op] & 7) == 4;
            const float frequency =
                juce::jmin(float(sr * .44), hz * d.ratios[size_t(op)] * (carrier ? 1.f : ratio * .5f));
            operators[op].freq = int32_t(frequency / sr * 16777216.0);
            operators[op].gain[0] = operators[op].gain[1];
            float envelope = 0;
            for (int i = 0; i < 64; ++i)
                envelope = envelopes[size_t(op)].getNextSample();
            const float level = d.levels[size_t(op)] * envelope *
                                (carrier ? 1.f : (.15f + index * 2.4f) * (.3f + .7f * velocity));
            operators[op].gain[1] = int32_t(level * 16777216);
        }
        core.compute(output, operators, d.algorithm, feedback, d.feedback ? 8 - d.feedback : 16);
        at = 0;
    }
};
SixOperator::SixOperator() : impl(std::make_unique<Impl>())
{
    static std::once_flag initialized;
    std::call_once(initialized, [] { Sin::init(); });
    prepare(96000);
}
SixOperator::~SixOperator() = default;
void SixOperator::prepare(double sr)
{
    impl->sr = sr;
    for (auto &envelope : impl->envelopes)
        envelope.setSampleRate(sr);
}
void SixOperator::start(int profile, float velocity, float startPhase)
{
    auto &v = *impl;
    v.profile = juce::jlimit(0, 11, profile - 1);
    v.velocity = velocity;
    v.at = 64;
    std::fill(std::begin(v.feedback), std::end(v.feedback), 0);
    std::fill(std::begin(v.operators), std::end(v.operators), FmOpParams{});
    const auto &d = designs[v.profile];
    for (int op = 0; op < 6; ++op)
    {
        const float initialPhase = startPhase * (1 + op * .137f);
        v.operators[op].phase = int32_t((initialPhase - std::floor(initialPhase)) * 16777216);
        auto &env = v.envelopes[size_t(op)];
        env.reset();
        const bool carrier = (topology[d.algorithm][op] & 7) == 4;
        env.setParameters({d.attack, d.decays[size_t(op)], carrier ? d.sustain : d.sustain * .3f,
                           d.release * (carrier ? 1.f : .5f)});
        env.noteOn();
    }
}
void SixOperator::release()
{
    for (auto &env : impl->envelopes)
        env.noteOff();
}
float SixOperator::next(float hz, float index, float ratio)
{
    if (impl->at == 64)
        impl->fill(hz, index, ratio);
    return float(impl->output[impl->at++]) / 16777216.f;
}
float SixOperator::preview(int profile, float phase, float index, float ratio)
{
    const auto &d = designs[juce::jlimit(0, 11, profile - 1)];
    float buses[3]{};
    for (int op = 0; op < 6; ++op)
    {
        const int flags = topology[d.algorithm][op], in = (flags >> 4) & 3, out = flags & 3;
        const bool carrier = (flags & 7) == 4;
        const float r = d.ratios[size_t(op)] * (carrier ? 1.f : ratio * .5f);
        const float level = d.levels[size_t(op)] * (carrier ? 1.f : (.15f + index * 2.4f));
        const float value =
            std::sin(juce::MathConstants<float>::twoPi * (phase * r + (in ? buses[in] : 0))) * level;
        if (flags & 4)
            buses[out] += value;
        else
            buses[out] = value;
    }
    return buses[0];
}
} // namespace noryn
