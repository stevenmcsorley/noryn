#pragma once
#include <JuceHeader.h>
#include <vector>
namespace noryn
{
// An instrumental arrangement inspired by Bjork's "Hunter" (Homogenic, 1997), written for NORYN's own
// sounds. Harmony and tempo come from the published analyses (G Phrygian dominant main section
// alternating G and A-flat bass; E-flat major bridge) and an analysis of the official preview
// (80 BPM, bass G - A-flat - B-flat). No recorded audio from the song is used, and there is no vocal:
// track 1 is left empty for yours.
struct DemoNote
{
    double beat, length;
    int note, velocity;
};
struct DemoPart
{
    juce::String name, sound; // factory preset name, or a drum kit name when drums is true
    bool drums = false;
    float gain = .8f, pan = 0;
    std::vector<DemoNote> notes;
};
struct HunterDemo
{
    static constexpr float bpm = 80;
    static constexpr int bars = 84;
    enum Section
    {
        Intro,
        Riff,
        Verse,
        Bridge,
        Verse2,
        Climax,
        Outro
    };
    static Section sectionAt(int bar) // 0-based
    {
        if (bar < 4)
            return Intro;
        if (bar < 12)
            return Riff;
        if (bar < 28)
            return Verse;
        if (bar < 36)
            return Bridge;
        if (bar < 52)
            return Verse2;
        if (bar < 60)
            return Bridge;
        if (bar < 76)
            return Climax;
        return Outro;
    }
    // Main riff: four half-bar chords per two bars. Bass (MIDI) and upper voicing.
    struct Chord
    {
        int bass;
        std::vector<int> upper;
    };
    static const std::vector<Chord> &riff()
    {
        static const std::vector<Chord> chords{{31, {55, 59, 62}},  // G
                                               {32, {55, 59, 62}},  // G over A-flat
                                               {34, {51, 55, 58}},  // E-flat over B-flat
                                               {31, {56, 60, 63}}}; // A-flat over G
        return chords;
    }
    // Bridge: Ab9 (2 bars), Ebsus2 (2), Cm7/G (1), Eb/Bb (1), Cm7 (2).
    static const std::vector<std::pair<int, Chord>> &bridge()
    {
        static const std::vector<std::pair<int, Chord>> chords{{2, {44, {60, 63, 67, 70}}},
                                                               {2, {39, {51, 53, 58, 63}}},
                                                               {1, {43, {60, 63, 67, 70}}},
                                                               {1, {34, {63, 67, 70}}},
                                                               {2, {36, {60, 63, 67, 70}}}};
        return chords;
    }
    static const Chord &bridgeChordAt(int barInSection)
    {
        int at = 0;
        for (const auto &[length, chord] : bridge())
        {
            if (barInSection % 8 < at + length)
                return chord;
            at += length;
        }
        return bridge().back().second;
    }
    static std::vector<DemoPart> parts()
    {
        std::vector<DemoPart> list;
        auto velocityRamp = [](int bar, int from, int to, int lowBar, int highBar)
        {
            const float t = juce::jlimit(0.f, 1.f, float(bar - lowBar) / float(juce::jmax(1, highBar - lowBar)));
            return int(from + (to - from) * t);
        };
        // 1. Vocal: empty, for recording.
        list.push_back({"Vocal (record here)", {}, false, .9f, 0, {}});
        // 2. Bolero snare: the military figure that runs through the song.
        {
            DemoPart part{"Bolero Snare", "Linn Electro (Depeche Mode)", true, 0.34f, -.15f, {}};
            const char *figure = "X.xxX.xxxxxxX.xx";
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto section = sectionAt(bar);
                const int base = section == Intro    ? velocityRamp(bar, 50, 80, 0, 3)
                                 : section == Bridge ? 55
                                 : section == Climax ? 105
                                 : section == Outro  ? velocityRamp(bar, 90, 35, 76, 83)
                                                     : 82;
                for (int step = 0; step < 16; ++step)
                    if (figure[step] != '.')
                        part.notes.push_back({bar * 4 + step * .25, .2, 38, juce::jlimit(1, 127, figure[step] == 'X' ? base + 20 : base - 18)});
            }
            list.push_back(part);
        }
        // 3. Beat: a slow, heavy electronic groove from the verse on.
        {
            DemoPart part{"Beat", "Industrial (Nine Inch Nails)", true, 0.44f, 0, {}};
            for (int bar = 0; bar < bars; ++bar)
            {
                const auto section = sectionAt(bar);
                if (section == Intro || section == Riff || (section == Outro && bar >= 80))
                    continue;
                const double b = bar * 4;
                if (section == Bridge)
                {
                    part.notes.push_back({b, .5, 36, 100});
                    part.notes.push_back({b + 2, .3, 37, 70});
                    continue;
                }
                const int accent = section == Climax ? 124 : 108;
                for (double kick : {0.0, 1.5, 2.75})
                    part.notes.push_back({b + kick, .4, 36, kick == 0 ? accent : accent - 18});
                part.notes.push_back({b + 2, .4, 38, accent});
                for (int e = 0; e < 8; ++e)
                    part.notes.push_back({b + e * .5, .2, 42, e % 2 ? 60 : 88});
                if (section == Climax || section == Verse2)
                {
                    part.notes.push_back({b + 1, .3, 39, 96});
                    part.notes.push_back({b + 3, .3, 39, 96});
                }
                if (section == Climax && bar % 4 == 3)
                    for (int s = 12; s < 16; ++s)
                        part.notes.push_back({b + s * .25, .2, 45 + (s - 12) * 2, 110}); // tom fill
                if (bar % 8 == 0 && section != Verse)
                    part.notes.push_back({b, 1, 49, 100}); // crash
            }
            list.push_back(part);
        }
        // 4. Bass: G / A-flat pulse, following the bridge roots.
        {
            DemoPart part{"Bass", "Charlotte Dark Pulse", false, 0.72f, 0, {}};
            for (int bar = 2; bar < 82; ++bar)
            {
                const auto section = sectionAt(bar);
                if (section == Bridge)
                {
                    const auto &chord = bridgeChordAt(bar - (bar < 36 ? 28 : 52));
                    part.notes.push_back({bar * 4.0, 3.8, chord.bass, 96});
                    continue;
                }
                for (int half = 0; half < 2; ++half)
                {
                    const auto &chord = riff()[size_t((bar % 2) * 2 + half)];
                    const double b = bar * 4 + half * 2;
                    const int v = section == Intro ? 80 : section == Climax ? 118 : 100;
                    part.notes.push_back({b, .9, chord.bass, v});
                    part.notes.push_back({b + 1.25, .45, chord.bass, v - 22});
                    part.notes.push_back({b + 1.5, .45, chord.bass + (section == Climax ? 12 : 0), v - 12});
                }
            }
            part.notes.push_back({82 * 4.0, 8, 31, 90});
            list.push_back(part);
        }
        // 5. Strings: the ostinato, with an octave layer at the climax.
        {
            DemoPart part{"Strings", "String Machine", false, 0.64f, .2f, {}};
            for (int bar = 4; bar < bars; ++bar)
            {
                const auto section = sectionAt(bar);
                if (section == Bridge)
                {
                    const auto &chord = bridgeChordAt(bar - (bar < 36 ? 28 : 52));
                    for (int n : chord.upper)
                        part.notes.push_back({bar * 4.0, 1.9, n - 12, 70});
                    continue;
                }
                for (int half = 0; half < 2; ++half)
                {
                    const auto &chord = riff()[size_t((bar % 2) * 2 + half)];
                    const double b = bar * 4 + half * 2;
                    const int v = section == Riff ? velocityRamp(bar, 55, 85, 4, 11) : section == Climax ? 112
                                  : section == Outro ? velocityRamp(bar, 95, 40, 76, 83) : 90;
                    for (int n : chord.upper)
                    {
                        part.notes.push_back({b, 1.35, n, v});
                        part.notes.push_back({b + 1.5, .45, n, v - 15});
                        if (section == Climax)
                            part.notes.push_back({b, 1.8, n + 12, v - 20});
                    }
                }
            }
            part.notes.push_back({83 * 4.0, 4, 55, 60});
            part.notes.push_back({83 * 4.0, 4, 59, 60});
            part.notes.push_back({83 * 4.0, 4, 62, 60});
            list.push_back(part);
        }
        // 6. Bridge chords: the E-flat section, and a shadow of it under the climax.
        {
            DemoPart part{"Bridge Chords", "Bjork Digital Bloom", false, 0.48f, -.25f, {}};
            for (int bar = 28; bar < 60; ++bar)
            {
                if (sectionAt(bar) != Bridge)
                    continue;
                const int inSection = bar - (bar < 36 ? 28 : 52);
                const auto &chord = bridgeChordAt(inSection);
                const auto &previous = bridgeChordAt(juce::jmax(0, inSection - 1));
                if (inSection == 0 || &chord != &previous)
                {
                    int length = 1;
                    while (inSection + length < 8 && &bridgeChordAt(inSection + length) == &chord)
                        ++length;
                    for (int n : chord.upper)
                        part.notes.push_back({bar * 4.0, length * 4 - .1, n, 88});
                }
            }
            for (int bar = 60; bar < 76; bar += 2)
                for (int n : {67, 71, 74})
                    part.notes.push_back({bar * 4.0, 7.8, n, 52});
            list.push_back(part);
        }
        // 7. Stabs: brassy accents on the chord changes in the second verse and climax.
        {
            DemoPart part{"Stabs", "Faithless Insomnia Stab", false, 0.36f, .3f, {}};
            for (int bar = 36; bar < 76; ++bar)
            {
                const auto section = sectionAt(bar);
                if (section != Verse2 && section != Climax)
                    continue;
                for (int half = 0; half < 2; ++half)
                {
                    const auto &chord = riff()[size_t((bar % 2) * 2 + half)];
                    const double b = bar * 4 + half * 2 + (half ? .75 : 0);
                    for (int n : chord.upper)
                        part.notes.push_back({b, .35, n + 12, section == Climax ? 112 : 92});
                }
            }
            list.push_back(part);
        }
        // 8. Glass: high bell texture that sparkles over the riff.
        {
            DemoPart part{"Glass", "Vangelis Glass Bell", false, 0.26f, .4f, {}};
            const int line[] = {74, 79, 83, 79, 75, 79, 82, 79};
            for (int bar = 8; bar < 80; ++bar)
            {
                const auto section = sectionAt(bar);
                if (section == Intro || section == Bridge || (section == Verse && bar < 20))
                    continue;
                for (int e = 0; e < 8; e += (section == Climax ? 1 : 2))
                    part.notes.push_back({bar * 4 + e * .5, .4, line[(bar * 8 + e) % 8] + ((bar / 2) % 2 ? 1 : 0) * 0,
                                          section == Climax ? 80 : 64});
            }
            list.push_back(part);
        }
        return list;
    }
};
} // namespace noryn
