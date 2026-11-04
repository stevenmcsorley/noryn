#pragma once
namespace noryn
{
// Architectural corrections to the first-pass bank. These are not likeness certifications.
inline void applyReferenceCharacter(std::vector<Preset> &bank)
{
    auto put = [](Preset &p, const char *id, float value)
    {
        for (auto &v : p.values)
            if (juce::String(v.first) == id)
            {
                v.second = value;
                return;
            }
        p.values.emplace_back(id, value);
    };
    struct Character
    {
        int filter, mode, chorus;
        float contour, glide, phase, drift, sub;
    };
    const Character groups[16] = {
        {1, 2, 0, .72f, .045f, .75f, .12f, .45f}, // Funk/disco: envelope and mono articulation.
        {1, 1, 1, .7f, 0, .15f, .06f, .3f},       // Synth-pop: tight retrigger, restrained phase spread.
        {1, 2, 0, .85f, .055f, .1f, .04f, .55f},  // Acid: nonlinear feedback and note-overlap glide.
        {0, 1, 0, .45f, .035f, 1, 0, 0},          // Reese: detuned phases, clean sine fundamental.
        {1, 2, 1, .65f, .07f, .8f, .16f, .15f},   // Solo leads: legato and expressive pitch.
        {0, 0, 1, .55f, 0, .75f, .12f, .2f},      // Brass/pop: polyphonic, shaped attack.
        {0, 0, 0, .35f, 0, 1, 0, 0},              // Festival: digital unison and clean pitch.
        {1, 0, 0, .75f, 0, .35f, .035f, .15f},    // Melodic plucks: curved filter/amp transients.
        {1, 0, 1, .7f, 0, .5f, .06f, .2f},        // House keys: short curved poly contours.
        {0, 0, 0, 0, 0, 0, 0, 0},                 // FM: preserve coherent operator attacks.
        {0, 0, 0, 0, 0, 0, 0, 0},                 // Organ: gate-like contour, stable harmonics.
        {1, 0, 1, .55f, 0, .35f, .12f, .15f},     // Vintage pads: ensemble and restrained variation.
        {1, 0, 1, .5f, 0, .8f, .16f, .1f},        // Ambient: long contours with moving phase relationships.
        {0, 0, 0, .3f, 0, .55f, .04f, 0},         // Spectral/vocal: preserve spectral and modulation roles.
        {1, 0, 0, .5f, 0, .25f, .025f, .1f}, // Motion: explicit rhythmic routes retain their assignments.
        {0, 0, 0, .25f, 0, 0, 0, 0}          // Physical plucks: retain the excitation/decay source.
    };
    for (int i = 97; i < 289 && i < int(bank.size()); ++i)
    {
        auto &p = bank[size_t(i)];
        const auto &c = groups[(i - 97) / 12];
        put(p, "filtercharacter", float(c.filter));
        put(p, "voicemode", float(c.mode));
        put(p, "choruscharacter", float(c.chorus));
        put(p, "envcurve", c.contour);
        put(p, "glidetime", c.glide);
        put(p, "phasefree", c.phase);
        put(p, "analogdrift", c.drift);
        put(p, "subshape", c.sub);
        if (i >= 97 && i < 121) // Avoid an accidental second octave of dominant square sub.
            for (auto &v : p.values)
                if (juce::String(v.first) == "sub")
                    v.second = juce::jmin(v.second, .38f);
        if (i >= 145 && i < 157)
            put(p, "bendrange", 12);
        if (i >= 205 && i < 217)
        {
            const int profiles[12] = {1, 2, 3, 4, 5, 7, 6, 3, 10, 5, 5, 12};
            put(p, "fmprofile", float(profiles[i - 205]));
            put(p, "unison", 0);
            put(p, "balance", 0);
            put(p, "fmratio", 2);
            put(p, "filtercharacter", 0);
        }
        // Record-specific structural corrections, beyond a genre-wide character choice.
        if (p.name == "Juno Midnight Silk")
        {
            put(p, "osclink", 1);
            put(p, "subshape", 1);
            put(p, "detune", 0);
            put(p, "phasefree", .08f);
            put(p, "analogdrift", .025f);
            put(p, "unison", 0);
            put(p, "position", 1);
            put(p, "shape", 1);
            put(p, "chorus", .7f);
        }
        if (p.name == "OB Dream Theater")
        {
            put(p, "unison", .12f);
            put(p, "phasefree", 1);
            put(p, "analogdrift", .22f);
            put(p, "chorus", .08f);
        }
        if (p.name == "Jupiter Wide Horizon" || p.name == "Prophet Polaroid")
            put(p, "unison", .1f);
        if (p.name == "Jump Arena Brass")
        {
            put(p, "chorus", .08f);
            put(p, "unison", .18f);
            put(p, "detune", 5);
            put(p, "fattack", .018f);
        }
        if (p.name == "Blade Neon Rain")
        {
            put(p, "engine", 4);
            put(p, "position", .82f);
            put(p, "balance", .55f);
            put(p, "shape", .7f);
            put(p, "cutoff", 1450);
            put(p, "chorus", .3f);
        }
        if (p.name == "Skream Sub Room")
        {
            put(p, "phasefree", 0);
            put(p, "envcurve", .2f);
            put(p, "glidetime", .08f);
        }
        if (p.name == "Darude Sandstorm Edge")
        {
            put(p, "voicemode", 1);
            put(p, "glidetime", 0);
            put(p, "attack", .001f);
            put(p, "release", .08f);
            put(p, "chorus", .06f);
        }
        if (p.name == "Popcorn Kernel")
        {
            put(p, "voicemode", 1);
            put(p, "glidetime", 0);
            put(p, "envcurve", .9f);
            put(p, "delay", .08f);
        }
        if (p.name == "Robin S Organ House" || p.name == "House Hollow M1" || p.name == "Garage Organ Bite")
        {
            put(p, "envcurve", .4f);
            put(p, "choruscharacter", 0);
            put(p, "chorus", .08f);
        }
    }
}
} // namespace noryn
