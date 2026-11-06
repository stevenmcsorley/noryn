#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
namespace noryn
{
// Synthesized drum machine: 16 pads, each an analog-style model built from oscillators and noise,
// or a loaded one-shot sample. A 16-lane step pattern runs on the shared transport.
constexpr int drumPads = 16, maxDrumSteps = 256; // as long as the 256-step sequencer
enum class DrumType
{
    Kick,
    Snare,
    Clap,
    ClosedHat,
    OpenHat,
    Tom,
    Rim,
    Cowbell,
    Cymbal,
    Shaker,
    Conga,
    Zap,
    NoiseHit,
    Sample,
    Count
};
inline const juce::StringArray &drumTypeNames()
{
    static const juce::StringArray names{"Kick", "Snare",   "Clap",  "Closed hat", "Open hat",
                                         "Tom",  "Rim",     "Cowbell", "Cymbal",   "Shaker",
                                         "Conga", "Zap",    "Noise hit", "Sample"};
    return names;
}
struct DrumSample
{
    juce::AudioBuffer<float> audio;
    double rate = 44100;
    juce::String path;
};
struct DrumPad
{
    std::atomic<int> type{0}, choke{0};
    // tune in semitones; decay/tone/snap 0..1 (0.5 = the model's natural setting); level 0..1.5; pan -1..1
    std::atomic<float> tune{0}, decay{.5f}, tone{.5f}, snap{.5f}, level{.8f}, pan{0};
    std::atomic<const DrumSample *> sample{nullptr};
    juce::String name{"Pad"};
};
struct DrumPattern
{
    std::array<std::array<std::atomic<uint8_t>, maxDrumSteps>, drumPads> velocity{};
    std::atomic<int> length{16};
    void clear()
    {
        for (auto &lane : velocity)
            for (auto &step : lane)
                step.store(0);
    }
    bool empty() const
    {
        for (const auto &lane : velocity)
            for (const auto &step : lane)
                if (step.load())
                    return false;
        return true;
    }
};
struct DrumKitSpec
{
    struct Pad
    {
        const char *name;
        DrumType type;
        float tune, decay, tone, snap, level, pan;
        int choke;
    };
    const char *name;
    std::array<Pad, drumPads> pads;
    float drive, compression, room;
};
struct DrumPatternSpec
{
    const char *name;
    int length;
    float swing;
    std::vector<std::pair<int, const char *>> lanes; // pad index, steps: X accent, x normal, o ghost
};
// Pad roles are shared by every kit so any pattern plays on any kit:
// 1 kick, 2 snare, 3 clap, 4 closed hat, 5 open hat, 6-8 toms, 9 rim, 10 cowbell, 11 cymbal,
// 12 shaker, 13 conga, 14 zap, 15 noise hit, 16 second kick / boom.
inline const std::vector<DrumKitSpec> &drumKits()
{
    using T = DrumType;
    static const std::vector<DrumKitSpec> kits{
        {"808 Classic",
         {{{"808 Kick", T::Kick, -2, .7f, .35f, .35f, 1, 0, 0},
           {"808 Snare", T::Snare, 0, .45f, .55f, .55f, .8f, 0, 0},
           {"808 Clap", T::Clap, 0, .5f, .5f, .5f, .75f, .1f, 0},
           {"808 Closed Hat", T::ClosedHat, 0, .35f, .6f, .5f, .55f, -.2f, 1},
           {"808 Open Hat", T::OpenHat, 0, .5f, .6f, .5f, .5f, -.2f, 1},
           {"Low Tom", T::Tom, -5, .55f, .4f, .5f, .75f, -.35f, 0},
           {"Mid Tom", T::Tom, 0, .5f, .4f, .5f, .7f, 0, 0},
           {"High Tom", T::Tom, 5, .45f, .4f, .5f, .65f, .35f, 0},
           {"Rimshot", T::Rim, 0, .4f, .5f, .5f, .6f, .15f, 0},
           {"Cowbell", T::Cowbell, 0, .45f, .5f, .5f, .5f, .25f, 0},
           {"Cymbal", T::Cymbal, 0, .6f, .55f, .5f, .45f, .3f, 0},
           {"Maracas", T::Shaker, 0, .3f, .65f, .5f, .45f, .4f, 0},
           {"Conga", T::Conga, 0, .5f, .45f, .5f, .6f, -.3f, 0},
           {"Zap", T::Zap, 0, .4f, .4f, .5f, .5f, 0, 0},
           {"Noise Burst", T::NoiseHit, 0, .3f, .5f, .4f, .45f, 0, 0},
           {"808 Boom", T::Kick, -7, 1, .2f, .2f, 1, 0, 0}}},
         .2f, .2f, .1f},
        {"909 House",
         {{{"909 Kick", T::Kick, 0, .45f, .6f, .7f, 1, 0, 0},
           {"909 Snare", T::Snare, 2, .4f, .7f, .7f, .8f, 0, 0},
           {"909 Clap", T::Clap, 1, .45f, .55f, .6f, .8f, .1f, 0},
           {"909 Closed Hat", T::ClosedHat, 2, .3f, .75f, .6f, .55f, -.2f, 1},
           {"909 Open Hat", T::OpenHat, 2, .45f, .75f, .6f, .5f, -.2f, 1},
           {"Low Tom", T::Tom, -3, .5f, .5f, .6f, .7f, -.35f, 0},
           {"Mid Tom", T::Tom, 2, .45f, .5f, .6f, .65f, 0, 0},
           {"High Tom", T::Tom, 7, .4f, .5f, .6f, .6f, .35f, 0},
           {"Rim", T::Rim, 1, .35f, .6f, .5f, .6f, .15f, 0},
           {"Cowbell", T::Cowbell, 1, .4f, .55f, .5f, .45f, .25f, 0},
           {"Ride", T::Cymbal, 4, .75f, .7f, .4f, .4f, .3f, 0},
           {"Shaker", T::Shaker, 0, .3f, .7f, .5f, .45f, .4f, 0},
           {"Conga", T::Conga, 2, .45f, .5f, .5f, .55f, -.3f, 0},
           {"Zap", T::Zap, 2, .35f, .5f, .5f, .45f, 0, 0},
           {"Noise Hit", T::NoiseHit, 0, .3f, .6f, .5f, .4f, 0, 0},
           {"Sub Kick", T::Kick, -5, .75f, .3f, .4f, .9f, 0, 0}}},
         .3f, .35f, .12f},
        {"CR-78 Vintage",
         {{{"CR Kick", T::Kick, 3, .3f, .3f, .2f, .9f, 0, 0},
           {"CR Snare", T::Snare, 3, .3f, .45f, .4f, .7f, 0, 0},
           {"CR Clap", T::Clap, 2, .35f, .4f, .4f, .65f, .1f, 0},
           {"CR Hat", T::ClosedHat, -2, .25f, .45f, .4f, .5f, -.2f, 1},
           {"CR Open Hat", T::OpenHat, -2, .4f, .45f, .4f, .45f, -.2f, 1},
           {"Bongo Low", T::Conga, -2, .35f, .4f, .4f, .6f, -.35f, 0},
           {"Bongo High", T::Conga, 5, .3f, .45f, .4f, .55f, .35f, 0},
           {"Tom", T::Tom, 4, .35f, .4f, .4f, .6f, 0, 0},
           {"Rim", T::Rim, 3, .3f, .45f, .4f, .55f, .15f, 0},
           {"Cowbell", T::Cowbell, -1, .35f, .45f, .4f, .45f, .25f, 0},
           {"Cymbal", T::Cymbal, -2, .5f, .4f, .4f, .4f, .3f, 0},
           {"Guiro", T::Shaker, -3, .4f, .45f, .6f, .45f, .4f, 0},
           {"Conga", T::Conga, 0, .4f, .4f, .4f, .55f, -.3f, 0},
           {"Tamb", T::Shaker, 4, .25f, .7f, .3f, .4f, .2f, 0},
           {"Metal Beat", T::Cowbell, 7, .25f, .6f, .5f, .4f, -.1f, 0},
           {"Soft Kick", T::Kick, 0, .4f, .2f, .1f, .8f, 0, 0}}},
         .1f, .15f, .18f},
        {"Linn Electro (Depeche Mode)",
         {{{"Linn Kick", T::Kick, 2, .35f, .55f, .55f, 1, 0, 0},
           {"Linn Snare", T::Snare, 1, .5f, .65f, .75f, .85f, 0, 0},
           {"Linn Clap", T::Clap, 0, .55f, .6f, .65f, .8f, .1f, 0},
           {"Linn Hat", T::ClosedHat, 1, .28f, .7f, .55f, .5f, -.2f, 1},
           {"Linn Open Hat", T::OpenHat, 1, .5f, .7f, .55f, .45f, -.2f, 1},
           {"Tom 1", T::Tom, -4, .55f, .55f, .6f, .7f, -.35f, 0},
           {"Tom 2", T::Tom, 1, .5f, .55f, .6f, .65f, 0, 0},
           {"Tom 3", T::Tom, 6, .45f, .55f, .6f, .6f, .35f, 0},
           {"Sidestick", T::Rim, 0, .35f, .6f, .5f, .6f, .15f, 0},
           {"Anvil Hit", T::Cowbell, 5, .55f, .75f, .6f, .5f, .25f, 0},
           {"Crash", T::Cymbal, 1, .7f, .65f, .5f, .4f, .3f, 0},
           {"Tambourine", T::Shaker, 2, .35f, .75f, .4f, .45f, .4f, 0},
           {"Metal Clank", T::NoiseHit, 4, .25f, .8f, .7f, .45f, -.3f, 0},
           {"Laser", T::Zap, 0, .3f, .55f, .5f, .45f, 0, 0},
           {"Chain Hit", T::NoiseHit, -2, .35f, .65f, .6f, .45f, .3f, 0},
           {"Gated Kick", T::Kick, -2, .3f, .5f, .6f, .9f, 0, 0}}},
         .25f, .45f, .2f},
        {"Industrial (Nine Inch Nails)",
         {{{"Distorted Kick", T::Kick, -1, .45f, .6f, .95f, 1, 0, 0},
           {"Crushed Snare", T::Snare, -1, .5f, .8f, .9f, .85f, 0, 0},
           {"Metal Clap", T::Clap, -2, .45f, .75f, .8f, .8f, .1f, 0},
           {"Grit Hat", T::ClosedHat, -3, .3f, .8f, .8f, .5f, -.2f, 1},
           {"Grit Open Hat", T::OpenHat, -3, .45f, .8f, .8f, .45f, -.2f, 1},
           {"Pipe Low", T::Tom, -7, .5f, .7f, .85f, .7f, -.35f, 0},
           {"Pipe Mid", T::Tom, -2, .45f, .7f, .85f, .65f, 0, 0},
           {"Pipe High", T::Tom, 3, .4f, .7f, .85f, .6f, .35f, 0},
           {"Spark", T::Rim, -2, .3f, .8f, .8f, .55f, .15f, 0},
           {"Anvil", T::Cowbell, -5, .5f, .8f, .8f, .5f, .25f, 0},
           {"Sheet Metal", T::Cymbal, -5, .65f, .8f, .8f, .45f, .3f, 0},
           {"Static", T::Shaker, -2, .35f, .9f, .7f, .45f, .4f, 0},
           {"Hammer", T::NoiseHit, -6, .35f, .5f, 1, .6f, -.3f, 0},
           {"Servo", T::Zap, -5, .45f, .7f, .8f, .5f, 0, 0},
           {"Machine Hit", T::NoiseHit, 0, .45f, .7f, .9f, .55f, 0, 0},
           {"Stomp", T::Kick, -4, .55f, .4f, 1, 1, 0, 0}}},
         .75f, .6f, .14f},
        {"Trip-Hop (Portishead)",
         {{{"Dusty Kick", T::Kick, -1, .5f, .25f, .45f, 1, 0, 0},
           {"Vinyl Snare", T::Snare, -3, .6f, .35f, .6f, .85f, 0, 0},
           {"Soft Clap", T::Clap, -3, .55f, .3f, .4f, .65f, .1f, 0},
           {"Muted Hat", T::ClosedHat, -4, .3f, .3f, .3f, .45f, -.2f, 1},
           {"Loose Hat", T::OpenHat, -4, .5f, .3f, .3f, .4f, -.2f, 1},
           {"Floor Tom", T::Tom, -6, .6f, .3f, .4f, .7f, -.35f, 0},
           {"Rack Tom", T::Tom, -1, .55f, .3f, .4f, .65f, 0, 0},
           {"High Tom", T::Tom, 4, .5f, .3f, .4f, .6f, .35f, 0},
           {"Brush Rim", T::Rim, -3, .4f, .35f, .3f, .55f, .15f, 0},
           {"Bell", T::Cowbell, -6, .5f, .3f, .4f, .4f, .25f, 0},
           {"Dark Ride", T::Cymbal, -6, .8f, .25f, .3f, .4f, .3f, 0},
           {"Vinyl Crackle", T::Shaker, -6, .6f, .4f, .2f, .3f, .4f, 0},
           {"Hand Drum", T::Conga, -3, .45f, .3f, .4f, .6f, -.3f, 0},
           {"Scratch", T::Zap, -9, .35f, .3f, .3f, .45f, 0, 0},
           {"Tape Hit", T::NoiseHit, -6, .4f, .25f, .5f, .45f, 0, 0},
           {"Boom", T::Kick, -6, .8f, .15f, .3f, .9f, 0, 0}}},
         .4f, .5f, .3f},
        {"Boom Bap Lo-Fi",
         {{{"Thump Kick", T::Kick, 1, .45f, .4f, .65f, 1, 0, 0},
           {"Crack Snare", T::Snare, 0, .5f, .55f, .65f, .85f, 0, 0},
           {"Snap", T::Clap, 3, .3f, .55f, .5f, .7f, .1f, 0},
           {"Tight Hat", T::ClosedHat, 0, .25f, .5f, .45f, .5f, -.2f, 1},
           {"Open Hat", T::OpenHat, 0, .4f, .5f, .45f, .45f, -.2f, 1},
           {"Low Tom", T::Tom, -4, .45f, .4f, .5f, .7f, -.35f, 0},
           {"Mid Tom", T::Tom, 1, .4f, .4f, .5f, .65f, 0, 0},
           {"High Tom", T::Tom, 6, .35f, .4f, .5f, .6f, .35f, 0},
           {"Rim", T::Rim, 0, .35f, .5f, .5f, .6f, .15f, 0},
           {"Cowbell", T::Cowbell, 0, .4f, .45f, .5f, .4f, .25f, 0},
           {"Crash", T::Cymbal, 0, .6f, .45f, .5f, .4f, .3f, 0},
           {"Shaker", T::Shaker, 0, .3f, .55f, .5f, .45f, .4f, 0},
           {"Conga", T::Conga, 0, .4f, .4f, .5f, .55f, -.3f, 0},
           {"Vinyl Zap", T::Zap, -4, .3f, .35f, .4f, .4f, 0, 0},
           {"Dust", T::NoiseHit, -4, .35f, .3f, .4f, .4f, 0, 0},
           {"Sub", T::Kick, -6, .7f, .2f, .3f, .9f, 0, 0}}},
         .45f, .55f, .1f},
        {"Techno Warehouse",
         {{{"Rumble Kick", T::Kick, -1, .6f, .5f, .8f, 1, 0, 0},
           {"Tech Snare", T::Snare, 3, .35f, .75f, .8f, .75f, 0, 0},
           {"Tech Clap", T::Clap, 2, .5f, .65f, .7f, .8f, .1f, 0},
           {"Tick Hat", T::ClosedHat, 4, .2f, .85f, .7f, .5f, -.2f, 1},
           {"Open Hat", T::OpenHat, 4, .4f, .85f, .7f, .5f, -.2f, 1},
           {"Low Tom", T::Tom, -2, .5f, .55f, .7f, .65f, -.35f, 0},
           {"Mid Tom", T::Tom, 3, .45f, .55f, .7f, .6f, 0, 0},
           {"High Tom", T::Tom, 8, .4f, .55f, .7f, .55f, .35f, 0},
           {"Rim", T::Rim, 4, .3f, .7f, .6f, .55f, .15f, 0},
           {"Bell", T::Cowbell, 3, .45f, .7f, .6f, .45f, .25f, 0},
           {"Ride", T::Cymbal, 6, .7f, .85f, .5f, .4f, .3f, 0},
           {"Shaker", T::Shaker, 3, .25f, .85f, .6f, .45f, .4f, 0},
           {"Perc", T::Conga, 5, .35f, .6f, .6f, .5f, -.3f, 0},
           {"Acid Zap", T::Zap, 3, .3f, .7f, .7f, .45f, 0, 0},
           {"Noise Sweep", T::NoiseHit, 3, .55f, .8f, .5f, .4f, 0, 0},
           {"Sub Boom", T::Kick, -8, .9f, .2f, .5f, .85f, 0, 0}}},
         .5f, .5f, .22f},
        {"Synthwave Gated",
         {{{"Big Kick", T::Kick, 0, .4f, .55f, .6f, 1, 0, 0},
           {"Gated Snare", T::Snare, 0, .35f, .7f, .8f, .9f, 0, 0},
           {"Gated Clap", T::Clap, 0, .35f, .65f, .75f, .8f, .1f, 0},
           {"Hat", T::ClosedHat, 0, .3f, .7f, .5f, .5f, -.2f, 1},
           {"Open Hat", T::OpenHat, 0, .45f, .7f, .5f, .45f, -.2f, 1},
           {"Simmons Low", T::Zap, -12, .55f, .35f, .4f, .7f, -.35f, 0},
           {"Simmons Mid", T::Zap, -7, .5f, .35f, .4f, .65f, 0, 0},
           {"Simmons High", T::Zap, -2, .45f, .35f, .4f, .6f, .35f, 0},
           {"Rim", T::Rim, 0, .35f, .6f, .5f, .55f, .15f, 0},
           {"Cowbell", T::Cowbell, 0, .4f, .55f, .5f, .4f, .25f, 0},
           {"Crash", T::Cymbal, 0, .75f, .6f, .5f, .45f, .3f, 0},
           {"Tambourine", T::Shaker, 3, .35f, .75f, .4f, .45f, .4f, 0},
           {"Conga", T::Conga, 0, .4f, .5f, .5f, .5f, -.3f, 0},
           {"Laser", T::Zap, 5, .3f, .6f, .5f, .45f, 0, 0},
           {"Noise", T::NoiseHit, 0, .3f, .6f, .5f, .4f, 0, 0},
           {"Deep Kick", T::Kick, -4, .6f, .4f, .5f, .9f, 0, 0}}},
         .3f, .55f, .45f},
        {"Minimal Clicks",
         {{{"Click Kick", T::Kick, 2, .25f, .7f, .5f, 1, 0, 0},
           {"Tick Snare", T::Rim, 5, .25f, .7f, .6f, .75f, 0, 0},
           {"Micro Clap", T::Clap, 4, .25f, .7f, .5f, .7f, .1f, 0},
           {"Needle Hat", T::ClosedHat, 6, .15f, .9f, .5f, .45f, -.2f, 1},
           {"Air Hat", T::OpenHat, 6, .3f, .9f, .5f, .4f, -.2f, 1},
           {"Blip Low", T::Conga, -5, .25f, .6f, .5f, .6f, -.35f, 0},
           {"Blip Mid", T::Conga, 0, .25f, .6f, .5f, .55f, 0, 0},
           {"Blip High", T::Conga, 7, .2f, .6f, .5f, .5f, .35f, 0},
           {"Click", T::Rim, 9, .15f, .8f, .5f, .5f, .15f, 0},
           {"Glass", T::Cowbell, 12, .3f, .8f, .5f, .35f, .25f, 0},
           {"Shimmer", T::Cymbal, 9, .55f, .9f, .4f, .3f, .3f, 0},
           {"Dust", T::Shaker, 6, .2f, .9f, .4f, .4f, .4f, 0},
           {"Wood", T::Conga, 12, .15f, .7f, .5f, .45f, -.3f, 0},
           {"Bleep", T::Zap, 9, .2f, .8f, .5f, .4f, 0, 0},
           {"Crackle", T::NoiseHit, 6, .2f, .8f, .4f, .35f, 0, 0},
           {"Sub Pulse", T::Kick, -5, .5f, .3f, .3f, .85f, 0, 0}}},
         .15f, .3f, .25f},
        {"Breakbeat Dusty",
         {{{"Break Kick", T::Kick, 1, .4f, .45f, .65f, 1, 0, 0},
           {"Break Snare", T::Snare, 1, .45f, .6f, .75f, .9f, 0, 0},
           {"Break Clap", T::Clap, 0, .4f, .5f, .6f, .7f, .1f, 0},
           {"Break Hat", T::ClosedHat, 0, .3f, .55f, .55f, .55f, -.2f, 1},
           {"Break Open Hat", T::OpenHat, 0, .45f, .55f, .55f, .5f, -.2f, 1},
           {"Low Tom", T::Tom, -4, .5f, .45f, .6f, .7f, -.35f, 0},
           {"Mid Tom", T::Tom, 1, .45f, .45f, .6f, .65f, 0, 0},
           {"High Tom", T::Tom, 6, .4f, .45f, .6f, .6f, .35f, 0},
           {"Ghost Rim", T::Rim, 0, .3f, .5f, .5f, .5f, .15f, 0},
           {"Cowbell", T::Cowbell, 0, .4f, .5f, .5f, .4f, .25f, 0},
           {"Ride", T::Cymbal, 2, .7f, .55f, .45f, .4f, .3f, 0},
           {"Shaker", T::Shaker, 0, .3f, .55f, .5f, .4f, .4f, 0},
           {"Conga", T::Conga, 0, .4f, .45f, .5f, .55f, -.3f, 0},
           {"Zap", T::Zap, 0, .3f, .45f, .5f, .4f, 0, 0},
           {"Crunch", T::NoiseHit, 0, .3f, .5f, .6f, .45f, 0, 0},
           {"Low Kick", T::Kick, -4, .6f, .3f, .5f, .9f, 0, 0}}},
         .55f, .6f, .12f},
        {"Ambient Soft",
         {{{"Pillow Kick", T::Kick, -2, .5f, .15f, .1f, .9f, 0, 0},
           {"Brush Snare", T::Snare, -2, .55f, .3f, .3f, .65f, 0, 0},
           {"Finger Snap", T::Clap, 5, .3f, .4f, .3f, .55f, .2f, 0},
           {"Soft Hat", T::ClosedHat, -2, .3f, .3f, .3f, .4f, -.2f, 1},
           {"Breath Hat", T::OpenHat, -2, .55f, .3f, .3f, .35f, -.2f, 1},
           {"Felt Tom Low", T::Tom, -5, .6f, .2f, .3f, .6f, -.35f, 0},
           {"Felt Tom Mid", T::Tom, 0, .55f, .2f, .3f, .55f, 0, 0},
           {"Felt Tom High", T::Tom, 5, .5f, .2f, .3f, .5f, .35f, 0},
           {"Wood Click", T::Rim, -4, .3f, .3f, .3f, .45f, .15f, 0},
           {"Chime", T::Cowbell, 12, .8f, .4f, .3f, .35f, .25f, 0},
           {"Swell", T::Cymbal, -3, 1, .3f, .3f, .35f, .3f, 0},
           {"Sand", T::Shaker, -2, .45f, .35f, .3f, .35f, .4f, 0},
           {"Hand Drum", T::Conga, -2, .45f, .3f, .3f, .5f, -.3f, 0},
           {"Droplet", T::Zap, 7, .25f, .25f, .3f, .35f, 0, 0},
           {"Wind", T::NoiseHit, -9, .7f, .2f, .2f, .3f, 0, 0},
           {"Heartbeat", T::Kick, -6, .55f, .1f, .2f, .8f, 0, 0}}},
         .05f, .25f, .55f}};
    return kits;
}
inline const std::vector<DrumPatternSpec> &drumPatterns()
{
    static const std::vector<DrumPatternSpec> patterns{
        {"Empty", 16, 0, {}},
        {"Four on the floor", 16, 0,
         {{0, "X...x...X...x..."}, {2, "....X.......X..."}, {3, "x.x.x.x.x.x.x.x."}, {4, "..x...x...x...x."}}},
        {"House shuffle", 16, .3f,
         {{0, "X...x...X...x..."}, {2, "....X.......X..."}, {3, "xoxoxoxoxoxoxoxo"}, {4, "..X...X...X...X."},
          {11, "x.o.x.o.x.o.x.o."}}},
        {"Techno drive", 16, 0,
         {{0, "X...X...X...X..."}, {3, "..x...x...x...x."}, {4, "..X...X...X...X."}, {8, "...x.....x...x.."},
          {10, "x.x.x.x.x.x.x.x."}, {15, "X..............."}}},
        {"Boom bap", 16, .35f,
         {{0, "X.....x...X....."}, {1, "....X.......X..."}, {3, "x.x.x.x.x.x.x.x."}, {8, "......o.......o."}}},
        {"Trip-hop slow (Portishead)", 16, .45f,
         {{0, "X......x..X....."}, {1, "....X.......X..o"}, {3, "x.xox.x.x.xox.x."}, {10, "x...x...x...x..."},
          {11, "o.o.o.o.o.o.o.o."}, {15, "X..........o...."}}},
        {"Industrial stomp (NIN)", 16, 0,
         {{0, "X..x..X.X..x...."}, {1, "....X.......X..."}, {2, "....X.......X..."}, {3, "xxxXxxxXxxxXxxxX"},
          {12, "..x....x..x....x"}, {14, "......X.......x."}, {15, "X.......X......."}}},
        {"Industrial shuffle (Closer)", 16, .15f,
         {{0, "X..x..x.X..x...."}, {1, "....X.......X..."}, {3, "x.xxx.xxx.xxx.xx"}, {4, "..............X."},
          {14, "....x.......x..."}}},
        {"Synth-pop drive (Depeche Mode)", 16, 0,
         {{0, "X...x...X...x..."}, {1, "....X.......X..."}, {2, "....X.......X..."}, {3, "x.x.x.x.x.x.x.x."},
          {9, "..x...x...x..x.."}, {11, "xxxxxxxxxxxxxxxx"}, {12, "...........x..x."}}},
        {"Post-punk (The Cure)", 16, 0,
         {{0, "X.x.....X.x....."}, {1, "....X.......X..."}, {3, "xxxxxxxxxxxxxxxx"}, {5, "..............xx"},
          {6, "............xx.."}}},
        {"Two-step garage (Burial)", 16, .4f,
         {{0, "X.........X....."}, {1, "....X.......X..."}, {8, "..o..o....o..o.."}, {3, "..x.xox...x.xo.x"},
          {11, "o.o.o.o.o.o.o.o."}, {14, "...........o...."}}},
        {"Breakbeat (funky drummer feel)", 16, .25f,
         {{0, "X.x.....x.X....."}, {1, "....X..o.o..X..o"}, {3, "x.x.x.x.x.x.x.x."}, {4, ".............X.."},
          {8, ".o.....o.....o.."}}},
        {"Amen-style break", 32, .1f,
         {{0, "X.x.......xX....X.x.......x....."}, {1, "....X..o.o..X..o....X..o.o....X."},
          {10, "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x."}}},
        {"Drum and bass two-step", 16, 0,
         {{0, "X.........X....."}, {1, "....X.......X..."}, {3, "x.x.x.x.x.x.x.x."}, {8, ".......o.......o"},
          {11, "o.o.o.o.o.o.o.o."}}},
        {"Electro (Planet Rock feel)", 16, 0,
         {{0, "X.....X...X....."}, {2, "....X.......X..."}, {3, "x.x.x.x.x.x.x.x."}, {9, "x..x..x...x..x.."},
          {13, "...........x...."}}},
        {"Trap", 16, 0,
         {{15, "X......X..X....."}, {2, "........X......."}, {3, "x.x.xxx.x.x.xxxx"}, {4, "..............X."},
          {8, "......o........."}}},
        {"Half-time", 16, .2f,
         {{0, "X.....x...x....."}, {1, "........X......."}, {3, "x.x.x.x.x.x.x.x."}, {10, "x...x...x...x..."}}},
        {"Motorik (krautrock)", 16, 0,
         {{0, "X.x.X.x.X.x.X.x."}, {1, "....X.......X..."}, {3, "xxxxxxxxxxxxxxxx"}}},
        {"Rock basic", 16, 0,
         {{0, "X.......X.x....."}, {1, "....X.......X..."}, {3, "x.x.x.x.x.x.x.x."}, {10, "X..............."}}},
        {"Reggae one drop", 16, .3f,
         {{0, "........X......."}, {8, "........X......."}, {3, "x.x.x.x.x.x.x.x."}, {4, "..............x."}}},
        {"Bossa nova", 16, 0,
         {{0, "X..xX..xX..xX..x"}, {8, "x..x..x...x..x.."}, {11, "xxxxxxxxxxxxxxxx"}}},
        {"Dembow", 16, 0,
         {{0, "X...X...X...X..."}, {1, "...x..x....x..x."}, {3, "x.x.x.x.x.x.x.x."}}},
        {"UK garage shuffle", 16, .5f,
         {{0, "X.....x...x....."}, {2, "....X.......X..."}, {3, "..x...x...x...x."}, {11, "xoxoxoxoxoxoxoxo"},
          {8, ".......o.o......"}}},
        {"Afrobeat", 16, .15f,
         {{0, "X.....x...X....."}, {1, "....X..o....X..."}, {4, "x.x.x.x.x.x.x.x."}, {12, "..x..x....x..x.."},
          {9, "x..x..x..x..x..."}}},
        {"Ambient pulse", 16, 0,
         {{15, "X..............."}, {12, "........x......."}, {3, "....o.......o..."}, {10, "x..............."}}},
        {"Gated 80s ballad", 16, 0,
         {{0, "X.......X.x....."}, {1, "....X.......X..."}, {3, "x.x.x.x.x.x.x.x."}, {5, "............xxxx"},
          {10, "X..............."}}}};
    return patterns;
}

// One pad's sound generator. Pads are monophonic; retriggering restarts the hit.
struct DrumVoice
{
    bool active = false;
    double t = 0; // seconds since the hit
    float velocity = 1, phase = 0, phase2 = 0, choking = 1, hold = 0, crushed = 0;
    std::array<float, 6> metal{};
    float lp = 0, hp = 0, bp1 = 0, bp2 = 0, bpb1 = 0, bpb2 = 0;
    double samplePos = 0;
    uint32_t random = 22222;
    float noise()
    {
        random ^= random << 13;
        random ^= random >> 17;
        random ^= random << 5;
        return float(random & 0xffff) / 32768.f - 1;
    }
    void trigger(float vel, int pad)
    {
        active = true;
        t = 0;
        velocity = vel;
        phase = phase2 = 0;
        choking = 1;
        hold = crushed = 0;
        metal.fill(0);
        lp = hp = bp1 = bp2 = bpb1 = bpb2 = 0;
        samplePos = 0;
        random = uint32_t(22222 + pad * 7919);
    }
    // Chamberlin state-variable band-pass, stable for fc < sr/6 (clamped).
    static float bandpass(float x, float fc, float q, float sr, float &low, float &band)
    {
        const float f = 2 * std::sin(juce::MathConstants<float>::pi * juce::jmin(fc, sr / 6.5f) / sr);
        const float high = x - low - band / q;
        band += f * high;
        low += f * band;
        return band;
    }
    float metallic(float base, float sr, const float *ratios)
    {
        float sum = 0;
        for (size_t i = 0; i < 6; ++i)
        {
            metal[i] += base * ratios[i] / sr;
            metal[i] -= std::floor(metal[i]);
            sum += metal[i] < .5f ? 1.f : -1.f;
        }
        return sum / 6;
    }
    float render(const DrumPad &pad, float sr)
    {
        if (!active)
            return 0;
        using std::exp;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        const auto type = DrumType(pad.type.load());
        const float tune = std::exp2(pad.tune.load() / 12), decayScale = std::exp2((pad.decay.load() - .5f) * 3),
                    tone = pad.tone.load(), snap = pad.snap.load();
        const float tt = float(t);
        float out = 0, env = 0;
        switch (type)
        {
        case DrumType::Kick:
        {
            const float f = 52 * tune * (1 + (2.5f + snap * 4) * exp(-tt / (.028f * (1.2f - tone * .6f))));
            phase += f / sr;
            phase -= std::floor(phase);
            env = exp(-tt / (.42f * decayScale));
            const float click = noise() * exp(-tt / .0025f) * (.15f + tone * .7f);
            out = std::tanh((std::sin(twoPi * phase) * env + click) * (1 + snap * 3)) / std::tanh(1 + snap * 3);
            break;
        }
        case DrumType::Snare:
        {
            phase += 185 * tune * (1 + .3f * exp(-tt / .01f)) / sr;
            phase2 += 330 * tune / sr;
            phase -= std::floor(phase);
            phase2 -= std::floor(phase2);
            const float body = (std::sin(twoPi * phase) * .6f + std::sin(twoPi * phase2) * .4f) * exp(-tt / .09f);
            const float n = noise();
            hp += (n - hp) * .35f;
            const float bright = n - hp * (1 - tone);
            const float rattle = bright * exp(-tt / (.16f * decayScale));
            env = juce::jmax(exp(-tt / .09f), exp(-tt / (.16f * decayScale)));
            out = std::tanh((body * (1 - snap * .45f) + rattle * (.35f + snap * .85f)) * (1 + snap));
            break;
        }
        case DrumType::Clap:
        {
            const float filtered =
                bandpass(noise(), 1050 * tune * std::exp2(tone * 1.5f), 1.6f, sr, bp1, bp2) * 2.2f;
            float bursts = 0;
            for (float at : {0.f, .011f, .022f})
                if (tt >= at)
                    bursts = juce::jmax(bursts, exp(-(tt - at) / .0045f));
            const float tail = tt > .028f ? exp(-(tt - .028f) / (.14f * decayScale)) : 0.f;
            env = juce::jmax(bursts, tail);
            out = std::tanh(filtered * env * (1 + snap * 1.5f));
            break;
        }
        case DrumType::ClosedHat:
        case DrumType::OpenHat:
        {
            static const float ratios[6] = {1, 1.4827f, 1.8003f, 2.5456f, 2.6303f, 3.8961f};
            const float m = metallic(205 * tune, sr, ratios) + noise() * (.15f + snap * .4f);
            const float band = bandpass(m, 9000 * std::exp2((tone - .5f)), 1.2f, sr, bp1, bp2);
            hp += (band - hp) * .25f;
            const float bright = band - hp;
            env = exp(-tt / ((type == DrumType::ClosedHat ? .045f : .42f) * decayScale));
            out = bright * env * 2.4f;
            break;
        }
        case DrumType::Tom:
        {
            phase += 120 * tune * (1 + (.45f + snap * .5f) * exp(-tt / .06f)) / sr;
            phase -= std::floor(phase);
            env = exp(-tt / (.34f * decayScale));
            const float n = noise() * exp(-tt / .006f) * tone * .5f;
            out = std::tanh((std::sin(twoPi * phase) + n) * env * (1 + snap * 2));
            break;
        }
        case DrumType::Rim:
        {
            phase += 1700 * tune / sr;
            phase -= std::floor(phase);
            const float click = bandpass(noise(), 3200 * tune, 2, sr, bp1, bp2) * exp(-tt / .003f) * 2;
            env = exp(-tt / (.012f * decayScale));
            out = std::sin(twoPi * phase) * env * (.6f + tone * .4f) + click * (.4f + snap * .6f);
            break;
        }
        case DrumType::Cowbell:
        {
            phase += 540 * tune / sr;
            phase2 += 800 * tune / sr;
            phase -= std::floor(phase);
            phase2 -= std::floor(phase2);
            const float sq = (phase < .5f ? 1.f : -1.f) + (phase2 < .5f ? 1.f : -1.f);
            const float band = bandpass(sq, 2600 * tune * std::exp2(tone - .5f), 2.5f, sr, bp1, bp2);
            env = exp(-tt / .018f) * .6f + exp(-tt / (.28f * decayScale)) * .4f;
            out = std::tanh(band * env * (1.5f + snap * 2));
            break;
        }
        case DrumType::Cymbal:
        {
            static const float ratios[6] = {1, 1.342f, 1.873f, 2.291f, 2.716f, 3.437f};
            const float m = metallic(320 * tune, sr, ratios) * .6f + noise() * (.3f + snap * .4f);
            hp += (m - hp) * (.18f + (1 - tone) * .2f);
            const float bright = m - hp;
            env = exp(-tt / (1.3f * decayScale));
            out = bright * env * 1.6f;
            break;
        }
        case DrumType::Shaker:
        {
            const float n = noise();
            hp += (n - hp) * (.3f + (1 - tone) * .3f);
            const float attack = juce::jmin(1.f, tt / (.004f + snap * .02f));
            env = attack * exp(-tt / (.07f * decayScale));
            out = (n - hp) * env * 1.4f;
            break;
        }
        case DrumType::Conga:
        {
            phase += 210 * tune * (1 + .18f * exp(-tt / .02f)) / sr;
            phase -= std::floor(phase);
            env = exp(-tt / (.22f * decayScale));
            const float slap = bandpass(noise(), 2400, 1.5f, sr, bp1, bp2) * exp(-tt / .004f) * snap * 1.5f;
            out = std::sin(twoPi * phase) * env * (.7f + tone * .3f) + slap;
            break;
        }
        case DrumType::Zap:
        {
            const float f = 60 * tune + 2400 * tune * exp(-tt / (.012f + tone * .05f));
            phase += f / sr;
            phase -= std::floor(phase);
            env = exp(-tt / (.2f * decayScale));
            out = std::tanh(std::sin(twoPi * phase) * env * (1 + snap * 3));
            break;
        }
        case DrumType::NoiseHit:
        {
            // Sample-and-hold crush at a rate set by tone, then driven hard: metal scrapes, hammers.
            hold += (2000 + tone * 14000) * tune / sr;
            if (hold >= 1)
            {
                hold -= std::floor(hold);
                crushed = noise();
            }
            const float band = bandpass(crushed, 1800 * tune * std::exp2(tone), 1.1f, sr, bp1, bp2);
            env = exp(-tt / (.14f * decayScale));
            out = std::tanh(band * env * (2 + snap * 6)) * .8f;
            break;
        }
        case DrumType::Sample:
        {
            const auto *data = pad.sample.load();
            if (data == nullptr || data->audio.getNumSamples() < 2)
            {
                active = false;
                return 0;
            }
            const int length = data->audio.getNumSamples();
            const int at = int(samplePos);
            if (at >= length - 1)
            {
                active = false;
                return 0;
            }
            const float frac = float(samplePos - at);
            float value = 0;
            for (int c = 0; c < data->audio.getNumChannels(); ++c)
            {
                const float *d = data->audio.getReadPointer(c);
                value += d[at] + frac * (d[at + 1] - d[at]);
            }
            value /= float(data->audio.getNumChannels());
            samplePos += data->rate / sr * tune;
            // decay 1 plays the whole sample; lower values fade it out earlier.
            env = pad.decay.load() >= .99f ? 1.f : exp(-tt / (.05f + pad.decay.load() * 2.f));
            lp += (value - lp) * (.05f + tone * .95f);
            out = lp * env * (1 + snap) / (1 + snap * .5f);
            break;
        }
        default:
            break;
        }
        if (choking < 1)
        {
            choking *= .992f;
            if (choking < .001f)
                active = false;
        }
        t += 1.0 / sr;
        if (type != DrumType::Sample && env < .0002f && t > .01)
            active = false;
        return out * velocity * choking;
    }
};

struct DrumMachine
{
    std::array<DrumPad, drumPads> pads;
    DrumPattern pattern;
    std::array<DrumVoice, drumPads> voices;
    std::atomic<float> busDrive{.2f}, busCompression{.3f}, room{.15f};
    std::atomic<bool> recording{false}, keysPlayDrums{false};
    std::atomic<int> playhead{-1}, startStep{0};
    std::array<std::atomic<float>, drumPads> padFlash{};
    std::atomic<int> kitIndex{0};
    juce::String kitName{"808 Classic"};
    juce::Reverb roomReverb;
    double sampleRate = 44100, nextTick = 0, clock = 0;
    int nextStep = 0;
    bool wasRunning = false;
    float compEnv = 0;
    // Samples are never freed while the machine lives, so the audio thread can hold raw pointers.
    std::vector<std::unique_ptr<DrumSample>> sampleStore;

    DrumMachine()
    {
        applyKit(0);
    }
    void prepare(double sr)
    {
        sampleRate = sr;
        roomReverb.setSampleRate(sr);
        roomReverb.reset();
        for (auto &v : voices)
            v.active = false;
        wasRunning = false;
        playhead.store(-1);
    }
    void applyKit(int index)
    {
        const auto &kits = drumKits();
        index = juce::jlimit(0, int(kits.size()) - 1, index);
        const auto &kit = kits[size_t(index)];
        for (int i = 0; i < drumPads; ++i)
        {
            const auto &spec = kit.pads[size_t(i)];
            auto &pad = pads[size_t(i)];
            pad.name = spec.name;
            pad.type.store(int(spec.type));
            pad.tune.store(spec.tune);
            pad.decay.store(spec.decay);
            pad.tone.store(spec.tone);
            pad.snap.store(spec.snap);
            pad.level.store(spec.level);
            pad.pan.store(spec.pan);
            pad.choke.store(spec.choke);
            pad.sample.store(nullptr);
        }
        busDrive.store(kit.drive);
        busCompression.store(kit.compression);
        room.store(kit.room);
        kitIndex.store(index);
        kitName = kit.name;
    }
    void applyPattern(int index)
    {
        const auto &list = drumPatterns();
        index = juce::jlimit(0, int(list.size()) - 1, index);
        const auto &spec = list[size_t(index)];
        pattern.clear();
        pattern.length.store(spec.length);
        for (const auto &[lane, steps] : spec.lanes)
        {
            const juce::String text(steps);
            for (int s = 0; s < juce::jmin(text.length(), maxDrumSteps); ++s)
            {
                const auto c = text[s];
                const uint8_t v = c == 'X' ? 127 : c == 'x' ? 96 : c == 'o' ? 48 : 0;
                pattern.velocity[size_t(lane)][size_t(s)].store(v);
            }
        }
    }
    bool loadSample(int padIndex, const juce::File &file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (!reader || reader->lengthInSamples < 2)
            return false;
        auto sample = std::make_unique<DrumSample>();
        const int length = int(std::min<long long>(reader->lengthInSamples, (long long)(reader->sampleRate * 20)));
        sample->audio.setSize(int(juce::jmin(2u, reader->numChannels)), length);
        reader->read(&sample->audio, 0, length, 0, true, sample->audio.getNumChannels() > 1);
        sample->rate = reader->sampleRate;
        sample->path = file.getFullPathName();
        auto &pad = pads[size_t(padIndex)];
        pad.sample.store(sample.get());
        pad.type.store(int(DrumType::Sample));
        pad.decay.store(1);
        pad.tone.store(1);
        pad.snap.store(0);
        pad.name = file.getFileNameWithoutExtension().substring(0, 18);
        sampleStore.push_back(std::move(sample));
        return true;
    }
    void hit(int padIndex, float velocity)
    {
        padIndex = juce::jlimit(0, drumPads - 1, padIndex);
        const int group = pads[size_t(padIndex)].choke.load();
        if (group > 0)
            for (int other = 0; other < drumPads; ++other)
                if (other != padIndex && pads[size_t(other)].choke.load() == group && voices[size_t(other)].active)
                    voices[size_t(other)].choking = .999f;
        voices[size_t(padIndex)].trigger(velocity, padIndex);
        padFlash[size_t(padIndex)].store(velocity);
    }
    // General MIDI drum notes map to the matching pad roles; 36 upward otherwise maps chromatically.
    static int padForNote(int note)
    {
        switch (note)
        {
        case 35: case 36: return 0;
        case 38: case 40: return 1;
        case 39: return 2;
        case 42: case 44: return 3;
        case 46: return 4;
        case 41: case 43: return 5;
        case 45: case 47: return 6;
        case 48: case 50: return 7;
        case 37: return 8;
        case 56: return 9;
        case 49: case 51: case 52: case 55: case 57: case 59: return 10;
        case 54: case 69: case 70: return 11;
        case 60: case 61: case 62: case 63: case 64: return 12;
        default: break;
        }
        return note >= 36 ? (note - 36) % drumPads : -1;
    }
    // Audio thread. `hits` holds pad triggers as (pad, velocity) for this block, at sample offsets.
    void process(juce::AudioBuffer<float> &out, int count, const juce::MidiBuffer &hits, float bpm, bool running,
                 float swing, float level)
    {
        out.clear(0, count);
        const float sr = float(sampleRate);
        const double stepSamples = sampleRate * 60 / (juce::jlimit(30.f, 300.f, bpm) * 4);
        const int length = juce::jlimit(1, maxDrumSteps, pattern.length.load());
        if (running && !wasRunning)
        {
            nextTick = clock;
            nextStep = juce::jmax(0, startStep.exchange(0)) % length;
        }
        if (!running)
            playhead.store(-1);
        // Stopping the transport stops the drums: ringing cymbals and toms fade out in ~20 ms.
        if (!running && wasRunning)
            for (auto &voice : voices)
                if (voice.active)
                    voice.choking = juce::jmin(voice.choking, .999f);
        auto event = hits.cbegin();
        for (int i = 0; i < count; ++i)
        {
            const double now = clock + i;
            if (running && nextTick <= now + 1.e-6)
            {
                const int step = nextStep % length;
                playhead.store(step);
                for (int p = 0; p < drumPads; ++p)
                    if (const int v = pattern.velocity[size_t(p)][size_t(step)].load())
                        hit(p, v / 127.f);
                nextStep = (step + 1) % length;
                // Swing delays every second sixteenth; the pair keeps its total length.
                nextTick += stepSamples * (step % 2 == 0 ? 1 + swing * .66 : 1 - swing * .66);
            }
            while (event != hits.cend() && (*event).samplePosition <= i)
            {
                const auto msg = (*event).getMessage();
                if (msg.isNoteOn())
                {
                    const int p = msg.getNoteNumber();
                    if (p >= 0 && p < drumPads)
                    {
                        hit(p, msg.getFloatVelocity());
                        if (running && recording.load())
                        {
                            // Quantize to the nearest step on the grid.
                            const double untilNext = nextTick - now;
                            const int target = (untilNext < stepSamples * .5 ? nextStep : nextStep - 1 + length) % length;
                            pattern.velocity[size_t(p)][size_t(target)].store(
                                uint8_t(juce::jlimit(1, 127, int(msg.getVelocity()))));
                        }
                    }
                }
                ++event;
            }
            float l = 0, r = 0;
            for (int p = 0; p < drumPads; ++p)
                if (voices[size_t(p)].active)
                {
                    const auto &pad = pads[size_t(p)];
                    const float s = voices[size_t(p)].render(pad, sr) * pad.level.load();
                    const float pan = juce::jlimit(-1.f, 1.f, pad.pan.load());
                    l += s * std::sqrt((1 - pan) * .5f);
                    r += s * std::sqrt((1 + pan) * .5f);
                }
            out.setSample(0, i, l);
            out.setSample(1, i, r);
        }
        clock += count;
        wasRunning = running;
        // Bus: glue compression, saturation, then a little room.
        const float comp = busCompression.load(), drive = 1 + busDrive.load() * 5;
        const float attack = std::exp(-1.f / (sr * .003f)), release = std::exp(-1.f / (sr * .12f));
        for (int i = 0; i < count; ++i)
        {
            const float l = out.getSample(0, i), r = out.getSample(1, i);
            const float peak = juce::jmax(std::abs(l), std::abs(r));
            compEnv = peak > compEnv ? attack * compEnv + (1 - attack) * peak : release * compEnv + (1 - release) * peak;
            const float threshold = 1 - comp * .8f;
            const float gain = compEnv > threshold ? std::pow(threshold / compEnv, comp * .75f) : 1.f;
            const float makeup = 1 + comp * .6f;
            for (int c = 0; c < 2; ++c)
            {
                const float x = (c == 0 ? l : r) * gain * makeup;
                out.setSample(c, i, std::tanh(x * drive) / std::tanh(drive) * level * .55f); // full hit ~ -5 dBFS
            }
        }
        juce::Reverb::Parameters rp;
        rp.roomSize = .35f;
        rp.damping = .5f;
        rp.wetLevel = room.load() * .22f;
        rp.dryLevel = .5f; // JUCE scales dry by 2
        rp.width = .8f;
        roomReverb.setParameters(rp);
        roomReverb.processStereo(out.getWritePointer(0), out.getWritePointer(1), count);
        for (auto &flash : padFlash)
            flash.store(flash.load() * .9f);
    }
    juce::ValueTree save() const
    {
        juce::ValueTree tree("Drums");
        tree.setProperty("kit", kitName, nullptr);
        tree.setProperty("kitIndex", kitIndex.load(), nullptr);
        tree.setProperty("length", pattern.length.load(), nullptr);
        tree.setProperty("drive", busDrive.load(), nullptr);
        tree.setProperty("compression", busCompression.load(), nullptr);
        tree.setProperty("room", room.load(), nullptr);
        for (int p = 0; p < drumPads; ++p)
        {
            const auto &pad = pads[size_t(p)];
            juce::ValueTree node("Pad");
            node.setProperty("index", p, nullptr);
            node.setProperty("name", pad.name, nullptr);
            node.setProperty("type", pad.type.load(), nullptr);
            node.setProperty("tune", pad.tune.load(), nullptr);
            node.setProperty("decay", pad.decay.load(), nullptr);
            node.setProperty("tone", pad.tone.load(), nullptr);
            node.setProperty("snap", pad.snap.load(), nullptr);
            node.setProperty("level", pad.level.load(), nullptr);
            node.setProperty("pan", pad.pan.load(), nullptr);
            node.setProperty("choke", pad.choke.load(), nullptr);
            if (const auto *sample = pad.sample.load())
                node.setProperty("sample", sample->path, nullptr);
            juce::String steps;
            for (int s = 0; s < pattern.length.load(); ++s)
                steps << juce::String::toHexString(int(pattern.velocity[size_t(p)][size_t(s)].load())).paddedLeft('0', 2);
            node.setProperty("steps", steps, nullptr);
            tree.addChild(node, -1, nullptr);
        }
        return tree;
    }
    void restore(const juce::ValueTree &tree)
    {
        if (!tree.isValid())
        {
            // Patches made before drums existed play without them.
            applyKit(0);
            pattern.clear();
            return;
        }
        applyKit(int(tree.getProperty("kitIndex", 0)));
        kitName = tree.getProperty("kit", kitName).toString();
        pattern.clear();
        pattern.length.store(juce::jlimit(1, maxDrumSteps, int(tree.getProperty("length", 16))));
        busDrive.store(float(tree.getProperty("drive", busDrive.load())));
        busCompression.store(float(tree.getProperty("compression", busCompression.load())));
        room.store(float(tree.getProperty("room", room.load())));
        for (const auto node : tree)
        {
            const int p = int(node.getProperty("index", -1));
            if (p < 0 || p >= drumPads)
                continue;
            auto &pad = pads[size_t(p)];
            pad.name = node.getProperty("name", pad.name).toString();
            pad.type.store(juce::jlimit(0, int(DrumType::Count) - 1, int(node.getProperty("type", pad.type.load()))));
            pad.tune.store(float(node.getProperty("tune", 0)));
            pad.decay.store(float(node.getProperty("decay", .5f)));
            pad.tone.store(float(node.getProperty("tone", .5f)));
            pad.snap.store(float(node.getProperty("snap", .5f)));
            pad.level.store(float(node.getProperty("level", .8f)));
            pad.pan.store(float(node.getProperty("pan", 0)));
            pad.choke.store(int(node.getProperty("choke", 0)));
            const juce::String path = node.getProperty("sample", "").toString();
            if (path.isNotEmpty())
            {
                const auto type = pad.type.load();
                const float tune = pad.tune.load(), decay = pad.decay.load(), tone = pad.tone.load();
                const auto name = pad.name;
                if (juce::File(path).existsAsFile() && loadSample(p, juce::File(path)))
                {
                    pad.type.store(type);
                    pad.tune.store(tune);
                    pad.decay.store(decay);
                    pad.tone.store(tone);
                    pad.name = name;
                }
            }
            const juce::String steps = node.getProperty("steps", "").toString();
            for (int s = 0; s < juce::jmin(maxDrumSteps, steps.length() / 2); ++s)
                pattern.velocity[size_t(p)][size_t(s)].store(uint8_t(steps.substring(s * 2, s * 2 + 2).getHexValue32()));
        }
    }
};
} // namespace noryn
