#pragma once
namespace noryn
{
inline void appendSolitudePresets(std::vector<Preset> &out)
{
    out.push_back(
        {"Solitude / Original Lead",
         "Leads",
         "M83 - Solitude (original): lead study. Play a single-note line with overlapping notes for glide; "
         "pitch wheel spans 12 semitones. Candidate for listening comparison, not a verified reconstruction.",
         {{"engine", 4},        {"position", .96f},  {"balance", .46f},     {"shape", .12f},
          {"pulsewidth", .47f}, {"detune", 4},       {"unison", .12f},      {"sub", .08f},
          {"subshape", .5f},    {"octavea", 0},      {"octaveb", 0},        {"filtercharacter", 1},
          {"filtermode", 1},    {"cutoff", 2150},    {"resonance", .39f},   {"drive", .24f},
          {"envamount", .42f},  {"fattack", .04f},   {"fdecay", .85f},      {"fsustain", .6f},
          {"frelease", .35f},   {"attack", .01f},    {"decay", .7f},        {"sustain", .84f},
          {"release", .3f},     {"envcurve", .75f},  {"voicemode", 2},      {"glidetime", .085f},
          {"bendrange", 12},    {"phasefree", .7f},  {"analogdrift", .16f}, {"choruscharacter", 1},
          {"chorus", .12f},     {"chorusrate", .3f}, {"delay", .3f},        {"delaytime", .375f},
          {"feedback", .38f},   {"reverb", .32f},    {"size", .7f},         {"width", 1.15f},
          {"velfilter", .22f},  {"lfo1dest", 15},    {"lfo1amount", .09f},  {"rate", 5.1f},
          {"lfoshape", 0},      {"depth", 1},        {"master", .72f}}});
    out.push_back(
        {"Solitude / Reinterpretation Swell",
         "Pads",
         "M83 - Solitude (Felsmann + Tiley): dark brass-swell study. Hold low, widely spaced chords for 3-4 "
         "seconds. Candidate for listening comparison; the full recording also contains other layers.",
         {{"engine", 4},         {"position", .98f},     {"balance", .52f},   {"shape", .09f},
          {"pulsewidth", .44f},  {"detune", 7},          {"unison", .6f},     {"sub", .18f},
          {"subshape", .7f},     {"octavea", 0},         {"octaveb", 0},      {"filtercharacter", 1},
          {"filtermode", 1},     {"cutoff", 180},        {"resonance", .19f}, {"drive", .2f},
          {"envamount", .58f},   {"fattack", 1.85f},     {"fdecay", 3.f},     {"fsustain", .2f},
          {"frelease", 2.3f},    {"attack", .42f},       {"decay", 2.3f},     {"sustain", .72f},
          {"release", 2.8f},     {"envcurve", .65f},     {"voicemode", 0},    {"phasefree", .8f},
          {"analogdrift", .25f}, {"choruscharacter", 1}, {"chorus", .3f},     {"chorusrate", .3f},
          {"delay", .06f},       {"delaytime", .42f},    {"feedback", .3f},   {"reverb", .42f},
          {"size", .9f},         {"width", 1.55f},       {"velfilter", .12f}, {"keytrack", .25f},
          {"lfo1dest", 0},       {"master", .76f}}});
}
} // namespace noryn
