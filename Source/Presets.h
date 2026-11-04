#pragma once
#include <JuceHeader.h>
#include <utility>
#include <vector>
namespace noryn
{
struct Preset
{
    juce::String name, category, description;
    std::vector<std::pair<const char *, float>> values;
};
} // namespace noryn
#include "SignaturePresets.h"
namespace noryn
{
inline const std::vector<Preset> &factoryPresets()
{
    static const std::vector<Preset> bank = []
    {
        std::vector<Preset> out;
        out.push_back({"Init / Open Canvas",
                       "Utility",
                       "A clean starting point. All factory sounds can be edited.",
                       {{"attack", .005f},
                        {"release", .15f},
                        {"chorus", 0},
                        {"delay", 0},
                        {"reverb", 0},
                        {"envamount", 0},
                        {"depth", 1},
                        {"scan", 0},
                        {"unison", 0}}});
        struct Design
        {
            const char *name;
            int family;
            float position, mix, shape, cutoff, env, unison, detune, sub, fm;
        };
        const Design designs[8][12] = {
            {{"Cinematic Horizon", 1, .42f, .28f, .25f, 2200, .35f, .7f, 9, .2f, .03f},
             {"June at Midnight", 0, .72f, .8f, .9f, 1600, .28f, .45f, 5, .18f, 0},
             {"Velvet Solstice", 0, .3f, .48f, .3f, 1100, .35f, .8f, -7, .3f, 0},
             {"Glass Cathedral", 3, .63f, .12f, .2f, 6200, .2f, .65f, 12, .05f, .16f},
             {"Aurora Choir", 2, .32f, .1f, .5f, 4800, .18f, .6f, -11, .08f, .05f},
             {"Distant Satellites", 1, .77f, .18f, .1f, 2900, .45f, .92f, 17, .15f, .09f},
             {"Amber Polysix", 0, .8f, .9f, .2f, 2000, .2f, .35f, -4, .25f, 0},
             {"Underwater Silk", 2, .64f, .3f, .7f, 750, -.2f, .7f, 8, .25f, .03f},
             {"Celestial Strings", 0, .61f, .7f, .25f, 4200, .4f, .88f, 13, .08f, 0},
             {"Winter Tape", 3, .2f, .4f, .6f, 1800, .25f, .38f, -9, .22f, .04f},
             {"Oceanic Memory", 1, .12f, .35f, .8f, 1200, .55f, .78f, 6, .35f, .02f},
             {"Neon Cinema", 2, .82f, .48f, .3f, 3400, .36f, .95f, 19, .15f, .12f}},
            {{"Rubber Circuit", 0, .7f, .85f, .9f, 180, .8f, .05f, 3, .8f, 0},
             {"Solid Foundation", 0, .1f, .4f, .3f, 240, .32f, 0, 0, 1, 0},
             {"Acid Meridian", 0, .8f, 1, .1f, 420, .7f, 0, 0, .2f, 0},
             {"Reese Transmission", 0, .75f, .95f, .25f, 950, .15f, .82f, 24, .5f, 0},
             {"Talking Voltage", 2, .48f, .22f, .7f, 1400, .5f, .12f, 7, .7f, .08f},
             {"Obsidian FM", 3, .72f, .05f, .2f, 1500, .6f, .08f, -12, .7f, .6f},
             {"Round Trip", 0, .35f, .75f, 1, 310, .55f, .18f, 4, .65f, 0},
             {"Basement Funk", 0, .66f, .95f, .6f, 620, .8f, .15f, -5, .5f, 0},
             {"Subterranean", 0, .04f, .08f, .1f, 160, .12f, 0, 0, 1, 0},
             {"Broken Transformer", 3, .48f, .55f, .2f, 2400, .5f, .35f, 18, .6f, .45f},
             {"Warm Octave", 0, .5f, .8f, .8f, 750, .35f, .2f, 7, .65f, 0},
             {"Chrome Runner", 1, .7f, .3f, .6f, 1600, .7f, .28f, -15, .55f, .12f}},
            {{"Solar Flare", 0, .76f, .75f, .2f, 5800, .35f, .8f, 14, .1f, .04f},
             {"Silicon Whistle", 0, .04f, .2f, 1, 2800, .2f, .12f, 4, .05f, 0},
             {"Pulse Racer", 0, .9f, 1, 1, 4200, .4f, .55f, 9, .15f, 0},
             {"Razor Bloom", 3, .6f, .4f, .1f, 8200, .3f, .95f, 26, .1f, .35f},
             {"Singing Wire", 2, .4f, .1f, .5f, 6200, .25f, .5f, 7, .05f, .2f},
             {"Festival Horizon", 0, .7f, .9f, .1f, 12000, .18f, 1, 21, .05f, 0},
             {"Velour Solo", 0, .3f, .72f, .85f, 2200, .42f, .3f, 6, .2f, 0},
             {"Laser Etching", 1, .8f, .2f, .1f, 9000, -.3f, .4f, -12, .05f, .45f},
             {"Copper Reed", 2, .16f, .6f, 1, 3600, .3f, .25f, 3, .1f, .03f},
             {"Retro Arcade", 0, .95f, 1, 1, 14500, 0, 0, 0, 0, 0},
             {"Bending Light", 1, .5f, .4f, .3f, 4800, .6f, .72f, -18, .1f, .15f},
             {"Icebreaker", 3, .82f, .2f, .4f, 10500, .2f, .85f, 23, .08f, .5f}},
            {{"Tine Afterglow", 3, .12f, .08f, .3f, 5200, .3f, .15f, 3, .18f, .2f},
             {"Worn Electric", 0, .2f, .18f, .8f, 3100, .42f, .2f, -4, .25f, .08f},
             {"Prism Bell", 3, .8f, .04f, .2f, 14000, .25f, .4f, 12, 0, .65f},
             {"Ivory Circuit", 1, .14f, .5f, .6f, 4800, .6f, .1f, 2, .15f, .04f},
             {"Sunday Organ", 0, .12f, .32f, 1, 8400, 0, .1f, 0, .5f, 0},
             {"Vapor Clav", 0, .7f, .8f, .8f, 2500, .8f, .1f, 4, .15f, .05f},
             {"Toy Constellation", 1, .9f, .1f, .5f, 7200, .4f, .25f, -12, .05f, .3f},
             {"Soft Mallet", 0, .06f, .12f, .5f, 1800, .35f, .1f, 2, .4f, .12f},
             {"Crystal Rhodes", 3, .32f, .1f, .8f, 6500, .3f, .35f, 7, .15f, .22f},
             {"Radio Harpsichord", 0, .8f, .85f, 1, 8400, .3f, .12f, -3, .05f, .03f},
             {"Glass Marimba", 1, .55f, .04f, .3f, 10000, .4f, .2f, 12, .1f, .48f},
             {"Dream Celesta", 2, .72f, .06f, .4f, 9000, .22f, .4f, -9, .04f, .18f}},
            {{"Plucked Horizon", 0, .7f, .8f, .2f, 950, .85f, .5f, 9, .2f, 0},
             {"Koto Satellite", 1, .6f, .1f, .3f, 5800, .5f, .2f, -12, .08f, .2f},
             {"Muted Neon", 0, .35f, .72f, .85f, 600, .9f, .2f, 5, .25f, 0},
             {"Silver Droplet", 3, .32f, .08f, .2f, 7000, .45f, .4f, 17, 0, .45f},
             {"Analog Raindrop", 0, .85f, 1, .9f, 1700, .8f, .1f, 2, .12f, 0},
             {"Bamboo Logic", 2, .2f, .2f, .5f, 3800, .55f, .12f, 7, .18f, .15f},
             {"Deep House Wire", 0, .58f, .9f, .1f, 460, .95f, .6f, -13, .4f, 0},
             {"Chromatic Spark", 3, .88f, .15f, .2f, 11000, .35f, .7f, 23, .02f, .65f},
             {"Nylon Emulator", 0, .28f, .4f, .8f, 4200, .55f, .15f, 3, .25f, .08f},
             {"Clockwork Harp", 1, .44f, .25f, .4f, 6500, .7f, .35f, -7, .12f, .22f},
             {"Satin Staccato", 2, .52f, .5f, .7f, 1300, .85f, .4f, 9, .2f, .05f},
             {"Resonant Pebble", 0, .75f, .7f, .2f, 2300, .65f, .1f, -3, .15f, .1f}},
            {{"Ghost in Glass", 2, .42f, .05f, .5f, 6200, .2f, .8f, 17, .05f, .4f},
             {"Dust of Europa", 3, .7f, .08f, .3f, 3500, -.4f, .9f, -23, .12f, .6f},
             {"Frozen Transformer", 1, .92f, .2f, .1f, 8500, .3f, .6f, 19, 0, .8f},
             {"Breathing Machine", 2, .14f, .15f, .8f, 1600, .5f, .5f, 11, .25f, .12f},
             {"Ocean of Static", 3, .22f, .3f, .2f, 4200, .3f, .75f, -15, .2f, .3f},
             {"Alien Choir", 2, .83f, .05f, .6f, 7800, .18f, .88f, 23, .04f, .55f},
             {"Bowed Satellite", 1, .62f, .4f, .3f, 2800, .65f, .6f, -9, .18f, .1f},
             {"Abyssal Current", 3, .4f, .25f, .5f, 850, .4f, 1, 28, .6f, .25f},
             {"Spectral Garden", 1, .3f, .06f, .7f, 11500, .2f, .72f, 14, .08f, .35f},
             {"Violet Interference", 3, .56f, .15f, .9f, 6800, -.2f, .65f, -21, .1f, .75f},
             {"Cathedral Dust", 2, .62f, .12f, .5f, 3600, .45f, .9f, 19, .22f, .28f},
             {"Beyond the Signal", 1, .82f, .05f, .2f, 14500, .1f, .95f, 25, .02f, .9f}},
            {{"Orbital Sequence", 1, .4f, .3f, .5f, 1800, .55f, .5f, 12, .2f, .1f},
             {"Pulse Engine", 0, .8f, 1, 1, 3200, .3f, .45f, 7, .15f, 0},
             {"Vowel Conveyor", 2, .35f, .08f, .5f, 6500, .15f, .5f, -9, .1f, .12f},
             {"Sawtooth Highway", 0, .72f, .9f, .1f, 1200, .65f, .8f, 19, .25f, 0},
             {"Digital Carousel", 3, .48f, .1f, .3f, 8400, .25f, .6f, 14, .05f, .45f},
             {"Liquid Clock", 0, .5f, .75f, .8f, 650, .75f, .2f, -5, .4f, .03f},
             {"Binary Aurora", 1, .86f, .2f, .2f, 4500, .2f, .85f, 23, .1f, .25f},
             {"Square Dance", 0, .95f, 1, 1, 2200, .4f, .3f, 5, .2f, 0},
             {"Resonant Traffic", 3, .68f, .4f, .4f, 1600, .6f, .5f, -13, .3f, .35f},
             {"Planetary Gears", 2, .6f, .2f, .6f, 5400, .35f, .7f, 17, .1f, .2f},
             {"Chopped Starlight", 1, .24f, .4f, .3f, 3400, .5f, .75f, -17, .12f, .15f},
             {"Neon Pendulum", 0, .66f, .85f, .95f, 2600, .4f, .6f, 11, .25f, .06f}},
            {{"Golden Brass", 0, .74f, .85f, .2f, 2200, .75f, .55f, 7, .2f, 0},
             {"String Machine", 0, .68f, .95f, .2f, 6500, .28f, .85f, 12, .1f, 0},
             {"Cinema Horns", 1, .45f, .6f, .1f, 1800, .85f, .5f, -9, .35f, .03f},
             {"Amber Ensemble", 0, .38f, .8f, .85f, 3200, .4f, .75f, 9, .2f, 0},
             {"Synthetic Oboe", 2, .18f, .6f, 1, 4800, .35f, .15f, 3, .1f, .03f},
             {"Wide Horizon Brass", 0, .8f, .95f, .1f, 3800, .65f, .95f, 21, .25f, 0},
             {"Chamber of Light", 1, .28f, .35f, .7f, 5200, .32f, .7f, -11, .15f, .08f},
             {"Vintage Solina", 0, .6f, .92f, .35f, 7800, .2f, .65f, 7, .08f, 0},
             {"Spectral Cello", 2, .62f, .3f, .6f, 2400, .5f, .4f, -7, .4f, .08f},
             {"Royal Fanfare", 0, .85f, .88f, .2f, 6500, .55f, .6f, 13, .15f, 0},
             {"Muted Copper", 0, .4f, .7f, .9f, 1400, .65f, .25f, 4, .25f, 0},
             {"Orchestral Dream", 2, .4f, .6f, .3f, 4200, .5f, .9f, 17, .2f, .05f}}};
        const char *categories[] = {"Pads",   "Basses",   "Leads",  "Keys",
                                    "Plucks", "Textures", "Motion", "Brass & Strings"};
        const char *descriptions[] = {
            "Wide sustained harmonics, slow filter motion and a spacious tail.",
            "Weight, articulation and a focused low end. Try the filter envelope.",
            "Forward solo voices. Open the filter or add unison for a bigger lead.",
            "Velocity-sensitive keyboard voices with shaped harmonic decays.",
            "Fast attacks, decaying filter contours and rhythmic echoes.",
            "Evolving spectra and formants. Position, FM and scan reshape the texture.",
            "Free-running rhythmic modulation. Rate, shape and depth set the movement.",
            "Ensemble detuning with expressive filter contours and warm harmonics."};
        for (int cat = 0; cat < 8; ++cat)
            for (int i = 0; i < 12; ++i)
            {
                const auto &d = designs[cat][i];
                Preset p{d.name, categories[cat], descriptions[cat], {}};
                auto add = [&](const char *id, float value) { p.values.emplace_back(id, value); };
                add("family", float(d.family));
                add("position", d.position);
                add("balance", d.mix);
                add("shape", d.shape);
                add("cutoff", d.cutoff);
                add("envamount", d.env);
                add("unison", d.unison);
                add("detune", d.detune);
                add("sub", d.sub);
                add("fm", d.fm);
                add("pulsewidth", .22f + .055f * (i % 6));
                add("pwmdepth", (cat == 0 || cat == 6 || cat == 7) ? (.15f + .06f * (i % 4)) : .04f);
                add("resonance", cat == 1 && i == 2 ? .88f : cat == 4 ? .48f : .12f + .035f * (i % 6));
                add("drive", cat == 1 ? .25f + .035f * (i % 5) : cat == 2 ? .2f : .08f);
                add("keytrack", cat == 1 ? .2f : .35f);
                add("velfilter", cat == 3 || cat == 4 ? .55f : .2f);
                add("rate", cat == 6   ? 1.f + .5f * (i % 8)
                            : cat == 5 ? .07f + .03f * i
                                       : .12f + .09f * (i % 5));
                add("lfoshape", cat == 6 ? float(i % 4) : 0.f);
                add("depth", cat == 6 ? .35f + .06f * (i % 5) : cat == 5 ? .18f : .015f + .015f * (i % 4));
                add("scan", cat == 5 ? .55f : cat == 6 ? .4f : cat == 0 ? .18f : .025f);
                add("noise", cat == 5 ? .04f + .015f * (i % 4) : cat == 7 ? .012f : 0.f);
                add("chorus", cat == 1 ? .05f : cat == 0 || cat == 7 ? .55f : .2f + .05f * (i % 4));
                add("chorusrate", .15f + .13f * (i % 6));
                add("delay", cat == 1 ? .02f : cat == 4 || cat == 6 ? .36f : cat == 2 ? .25f : .12f);
                add("delaytime", cat == 6 ? .1875f : .22f + .055f * (i % 5));
                add("feedback", cat == 5 ? .6f : cat == 4 ? .48f : .32f);
                add("reverb", cat == 1 ? .03f : cat == 5 ? .65f : cat == 0 ? .45f : cat == 4 ? .25f : .22f);
                add("size", cat == 0 || cat == 5 ? .88f : .4f + .07f * (i % 6));
                add("width", cat == 1 ? 1.f : 1.25f + .07f * (i % 5));
                float attack = cat == 0   ? .3f + .13f * (i % 5)
                               : cat == 5 ? .7f + .2f * (i % 5)
                               : cat == 7 ? .045f + .025f * (i % 4)
                                          : .002f;
                float decay = cat == 3   ? .8f + .2f * (i % 5)
                              : cat == 4 ? .18f + .06f * (i % 5)
                              : cat == 1 ? .15f
                                         : .65f;
                float sustain = cat == 3 ? .3f : cat == 4 ? .02f : cat == 1 ? .55f : cat == 2 ? .8f : .72f;
                float release = cat == 0   ? 2.2f + .4f * (i % 4)
                                : cat == 5 ? 3.5f
                                : cat == 1 ? .12f
                                : cat == 3 ? 1.1f
                                : cat == 4 ? .5f
                                           : .55f;
                add("attack", attack);
                add("decay", decay);
                add("sustain", sustain);
                add("release", release);
                add("fattack", cat == 7 ? .08f : cat == 0 ? .6f : cat == 5 ? 1.2f : .002f);
                add("fdecay", cat == 4 ? .15f + .045f * (i % 6) : cat == 1 ? .16f : .7f);
                add("fsustain", cat == 4 ? .01f : cat == 1 ? .2f : .5f);
                add("frelease", release * .8f);
                add("octavea", cat == 1 ? -1.f : 0.f);
                add("octaveb", cat == 1 ? -1.f : cat == 3 && i == 4 ? 1.f : 0.f);
                add("filtermode", cat == 1 || cat == 4     ? 1.f
                                  : cat == 5 && i % 4 == 1 ? 3.f
                                  : cat == 5 && i % 4 == 2 ? 2.f
                                                           : 0.f);
                add("master", cat == 1 ? .78f : cat == 5 ? .8f : .72f);
                add("depth", 1);
                add("lfo1dest", cat == 5 ? 3.f : cat == 6 && i % 3 == 0 ? 6.f : 1.f);
                add("lfo1amount", cat == 6   ? .4f
                                  : cat == 5 ? .5f
                                  : cat == 0 ? .08f
                                  : cat == 1 ? .015f
                                             : .025f);
                add("pwmdepth", cat == 0 || cat == 1 || cat == 7 ? .9f : .65f);
                int voiceEngine = cat == 3               ? (i == 4 ? 3 : (i == 1 || i == 5 || i == 9 ? 4 : 1))
                                  : cat == 4             ? (i % 3 == 0 ? 4 : 2)
                                  : cat == 0 || cat == 7 ? ((i % 3 == 0) ? 0 : 4)
                                  : cat == 1             ? (i == 5 || i == 9 ? 1 : 4)
                                  : cat == 2             ? (i == 1 || i == 4 || i == 7 ? 1 : 4)
                                                         : 0;
                add("engine", float(voiceEngine));
                add("fmratio", cat == 3 ? (.5f + float(i % 6) * .75f) : cat == 2 ? 2.f : 1.5f);
                if (voiceEngine == 1)
                {
                    add("balance", .05f);
                    add("fm", .25f + .08f * (i % 7));
                    add("fattack", .001f);
                    add("fdecay", .25f + .13f * (i % 5));
                    add("fsustain", .04f);
                    add("cutoff", 10000);
                }
                if (voiceEngine == 2)
                {
                    add("balance", 0);
                    add("attack", .001f);
                    add("decay", .45f + .3f * (i % 6));
                    add("sustain", .7f);
                    add("cutoff", 8000);
                    add("envamount", .1f);
                    add("unison", 0);
                }
                if (voiceEngine == 3)
                {
                    add("balance", .05f);
                    add("attack", .004f);
                    add("sustain", 1);
                    add("decay", .1f);
                    add("release", .06f);
                    add("sub", .45f);
                    add("chorus", .4f);
                }
                if (cat == 5 && i % 4 == 2)
                    add("cutoff", d.cutoff * .12f);
                if (cat == 5 && i % 4 == 1)
                    add("cutoff", d.cutoff * .35f);
                if (cat == 3 && i == 4)
                {
                    add("attack", .005f);
                    add("decay", .1f);
                    add("sustain", 1);
                    add("release", .08f);
                }
                if (cat == 3 && i == 5)
                {
                    add("sustain", 0);
                    add("decay", .14f);
                    add("fdecay", .09f);
                }
                out.push_back(std::move(p));
            }
        appendSignaturePresets(out);
        return out;
    }();
    return bank;
}
} // namespace noryn
