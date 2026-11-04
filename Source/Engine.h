#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

namespace noryn
{
using namespace juce;
constexpr float tau = MathConstants<float>::twoPi;
inline float wrap(float x)
{
    return x - std::floor(x);
}
inline float blep(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return 2 * t - t * t - 1;
    }
    if (t > 1 - dt)
    {
        t = (t - 1) / dt;
        return t * t + 2 * t + 1;
    }
    return 0;
}
struct Tables
{
    static constexpr int size = 2048, frames = 16, bands = 7, families = 4;
    std::array<float, size> sine{};
    std::vector<float> data;
    Tables() : data(size * frames * bands * families)
    {
        std::vector<float> basis(64 * size);
        for (int h = 1; h <= 64; ++h)
            for (int i = 0; i < size; ++i)
                basis[(h - 1) * size + i] = std::sin(tau * h * i / size);
        std::copy_n(basis.data(), size, sine.data());
        for (int family = 0; family < families; ++family)
            for (int b = 0; b < bands; ++b)
                for (int f = 0; f < frames; ++f)
                {
                    float t = float(f) / (frames - 1);
                    auto *out = data.data() + ((family * bands + b) * frames + f) * size;
                    for (int h = 1; h <= (1 << b); ++h)
                    {
                        float gain = 0;
                        if (family == 0)
                        { // Sine -> triangle -> saw -> narrow pulse.
                            float tri = h % 2 ? ((h % 4 == 1 ? 1.f : -1.f) / (h * h)) : 0.f;
                            float saw = (h % 2 ? 1.f : -1.f) / h;
                            float pulse = std::sin(MathConstants<float>::pi * h * (.5f - .37f * t)) / h;
                            gain = t < .33f   ? (1 - t / .33f) * (h == 1 ? 1.f : 0.f) + (t / .33f) * tri
                                   : t < .67f ? tri * (1 - (t - .33f) / .34f) + saw * ((t - .33f) / .34f)
                                              : saw * (1 - (t - .67f) / .33f) + pulse * ((t - .67f) / .33f);
                        }
                        else if (family == 1)
                        { // Moving harmonic clusters.
                            gain = (h == 1 ? .35f : 0.f) +
                                   std::exp(-std::pow((h - (2 + t * 17)) / (1.2f + t * 3), 2)) /
                                       std::sqrt(float(h));
                            gain += (h % 2 ? .3f : -.18f) / h;
                        }
                        else if (family == 2)
                        { // Two moving formants on a harmonic carrier.
                            gain = (h == 1 ? .3f : 0.f) +
                                   (std::exp(-std::pow((h - (3 + t * 7)) / 1.7f, 2)) +
                                    .65f * std::exp(-std::pow((h - (11 + t * 17)) / 3.f, 2))) /
                                       std::sqrt(float(h));
                        }
                        else
                        { // Hollow, bright and metallic spectra.
                            gain = (h == 1 ? .45f : 0.f) + std::sin(h * (.4f + t * 2.4f)) *
                                                               std::cos(h * h * .09f + t * 4) /
                                                               std::pow(float(h), .65f + t * .3f);
                        }
                        for (int i = 0; i < size; ++i)
                            out[i] += gain * basis[(h - 1) * size + i];
                    }
                    double energy = 0;
                    float peak = 0;
                    for (int i = 0; i < size; ++i)
                    {
                        energy += out[i] * out[i];
                        peak = jmax(peak, std::abs(out[i]));
                    }
                    float gain =
                        jmin(.46f / float(std::sqrt(energy / size) + 1.e-9), 1.3f / jmax(.001f, peak));
                    for (int i = 0; i < size; ++i)
                        out[i] *= gain;
                }
    }
    float sin(float phase) const
    {
        float x = wrap(phase) * size;
        int i = int(x);
        return sine[i] + (x - i) * (sine[(i + 1) % size] - sine[i]);
    }
    static int bandFor(float hz, float sr)
    {
        return jlimit(0, bands - 1, int(std::log2(jmax(1.f, sr * .45f / hz))));
    }
    float read(float phase, float position, int family, int band) const
    {
        float x = wrap(phase) * size, f = jlimit(0.f, 1.f, position) * (frames - 1);
        int i = int(x), j = (i + 1) % size, fi = int(f);
        const float *a = data.data() + ((family * bands + band) * frames + fi) * size;
        const float *b = data.data() + ((family * bands + band) * frames + jmin(fi + 1, frames - 1)) * size;
        float av = a[i] + (x - i) * (a[j] - a[i]), bv = b[i] + (x - i) * (b[j] - b[i]);
        return av + (f - fi) * (bv - av);
    }
};
inline const Tables &sharedTables()
{
    static const Tables tables;
    return tables;
}
inline float lfoValue(float phase, int shape, const Tables &tables)
{
    phase = wrap(phase);
    if (shape == 1)
        return 1 - 4 * std::abs(phase - .5f);
    if (shape == 2)
        return 2 * phase - 1;
    if (shape == 3)
        return phase < .5f ? 1.f : -1.f;
    return tables.sin(phase);
}
// Atomic cells avoid data races while the UI reads a rolling audio history.
struct Scope
{
    static constexpr unsigned size = 4096;
    std::array<std::atomic<float>, size> samples{};
    std::atomic<unsigned> published{0};
    unsigned write = 0;
    void push(float value)
    {
        samples[write++ % size].store(value, std::memory_order_relaxed);
    }
    void publish()
    {
        published.store(write, std::memory_order_release);
    }
    void snapshot(float *out, int count) const
    {
        unsigned end = published.load(std::memory_order_acquire);
        for (int i = 0; i < count; ++i)
        {
            unsigned back = unsigned(count - i);
            out[i] = end >= back ? samples[(end - back) % size].load(std::memory_order_relaxed) : 0.f;
        }
    }
};
struct Visuals
{
    Scope left, right;
    std::atomic<int> voices{0}, selectedVoice{-1};
    std::array<std::atomic<float>, 20> effective{};
    std::atomic<uint64_t> modMask{0};
    std::atomic<float> ampMod{1};
    std::atomic<float> peakL{0}, peakR{0}, sampleRate{48000};
    std::array<std::atomic<float>, 16> wheel{};
};
struct Sound : SynthesiserSound
{
    bool appliesToNote(int) override
    {
        return true;
    }
    bool appliesToChannel(int) override
    {
        return true;
    }
};
struct Filter
{
    float ic1 = 0, ic2 = 0;
    void reset()
    {
        ic1 = ic2 = 0;
    }
    float tick(float input, float g, float k, int mode)
    {
        float a1 = 1 / (1 + g * (g + k)), a2 = g * a1, a3 = g * a2;
        float v3 = input - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        return mode == 2 ? input - k * v1 - v2 : mode == 3 ? k * v1 : v2;
    }
};
struct Voice : SynthesiserVoice
{
    enum P
    {
        Position,
        Scan,
        Balance,
        Detune,
        Shape,
        Sub,
        Cutoff,
        Resonance,
        Drive,
        Rate,
        Depth,
        Unison,
        Noise,
        PulseWidth,
        PwmDepth,
        FM,
        EnvAmount,
        KeyTrack,
        VelocityFilter,
        Pan,
        Count
    };
    static constexpr const char *ids[Count] = {"position", "scan",      "balance",   "detune",     "shape",
                                               "sub",      "cutoff",    "resonance", "drive",      "rate",
                                               "depth",    "unison",    "noise",     "pulsewidth", "pwmdepth",
                                               "fm",       "envamount", "keytrack",  "velfilter",  "pan"};
    AudioProcessorValueTreeState &state;
    const Tables &tables;
    Visuals &visual;
    int index;
    std::array<std::atomic<float> *, Count> raw{};
    std::array<float, Count> smooth{};
    std::array<float, 3> pa{}, pb{}, pc{};
    std::array<float, 12000> stringBuffer{};
    int stringWrite = 0;
    float stringPrevious = 0;
    std::array<float, 3> extraPhases{};
    float ps = 0, pl = 0, hz = 440, velocity = 0, bend = 1, gNow = .1f, kNow = 1;
    float bRatio = 1, driveGain = 1, driveComp = 1;
    ADSR amp, filterEnv;
    Filter filters[2][2];
    uint32_t random = 1;
    int coefficientCountdown = 0;
    Voice(AudioProcessorValueTreeState &s, const Tables &t, Visuals &vis, int voiceIndex)
        : state(s), tables(t), visual(vis), index(voiceIndex)
    {
        for (int p = 0; p < Count; ++p)
            raw[p] = state.getRawParameterValue(ids[p]);
    }
    float v(const char *id) const
    {
        return state.getRawParameterValue(id)->load();
    }
    bool canPlaySound(SynthesiserSound *s) override
    {
        return dynamic_cast<Sound *>(s) != nullptr;
    }
    void startNote(int note, float vel, SynthesiserSound *, int wheel) override
    {
        hz = float(MidiMessage::getMidiNoteInHertz(note));
        velocity = vel;
        pitchWheelMoved(wheel);
        for (int p = 0; p < Count; ++p)
            smooth[p] = raw[p]->load();
        for (int u = 0; u < 3; ++u)
        {
            pa[u] = wrap(.173f * u + note * .007f);
            pb[u] = wrap(.371f * u + note * .011f);
            pc[u] = 0;
        }
        ps = pl = 0;
        extraPhases.fill(0);
        random = uint32_t(note * 7919 + 11);
        stringWrite = 0;
        stringPrevious = 0;
        for (auto &sample : stringBuffer)
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            sample = (float(random & 0xffff) / 32768.f - 1) * .65f;
        }
        visual.selectedVoice.store(index);
        coefficientCountdown = 0;
        for (auto &channel : filters)
            for (auto &f : channel)
                f.reset();
        amp.setSampleRate(getSampleRate());
        filterEnv.setSampleRate(getSampleRate());
        amp.reset();
        filterEnv.reset();
        bRatio = std::exp2(smooth[Detune] / 1200);
        driveGain = std::exp2(smooth[Drive] * 4.5f);
        driveComp = 1 / std::sqrt(driveGain);
        setEnvelopes();
        amp.noteOn();
        filterEnv.noteOn();
    }
    void setEnvelopes()
    {
        amp.setParameters({v("attack"), v("decay"), v("sustain"), v("release")});
        filterEnv.setParameters({v("fattack"), v("fdecay"), v("fsustain"), v("frelease")});
    }
    void stopNote(float, bool tail) override
    {
        amp.noteOff();
        filterEnv.noteOff();
        if (!tail)
        {
            amp.reset();
            filterEnv.reset();
            clearCurrentNote();
        }
    }
    void pitchWheelMoved(int p) override
    {
        bend = std::pow(2.f, (p - 8192) / 8192.f * 2 / 12);
    }
    void controllerMoved(int, int) override {}
    void renderNextBlock(AudioBuffer<float> &out, int start, int count) override
    {
        setEnvelopes();
        float sr = float(getSampleRate()), alpha = 1 - std::exp(-1 / (sr * .008f));
        std::array<float, Count> target{};
        for (int p = 0; p < Count; ++p)
            target[p] = raw[p]->load();
        int family = int(v("family")), mode = int(v("filtermode")), lfoShape = int(v("lfoshape")),
            engine = int(v("engine"));
        float fmRatio = v("fmratio"),
              stringDamping = std::exp(-6.9078f / (jmax(.03f, v("decay")) * jmax(20.f, hz * bend)));
        bool trace = visual.selectedVoice.load() == index;
        float ha = jmin(hz * bend * std::pow(2.f, v("octavea")), sr * .42f),
              hb = jmin(hz * bend * std::pow(2.f, v("octaveb")), sr * .42f);
        int tableBand = Tables::bandFor(ha * 1.025f, sr);
        float wheel = 0;
        for (int c = 1; c <= 16; ++c)
            if (isPlayingChannel(c))
                wheel = visual.wheel[c - 1].load();
        const char *rateIds[] = {"rate", "lfo2rate", "lfo3rate", "lfo4rate"};
        const char *shapeIds[] = {"lfoshape", "lfo2shape", "lfo3shape", "lfo4shape"};
        const char *destIds[] = {"lfo1dest", "lfo2dest", "lfo3dest", "lfo4dest"};
        const char *amountIds[] = {"lfo1amount", "lfo2amount", "lfo3amount", "lfo4amount"};
        float modRates[4], modAmounts[4];
        int modShapes[4], modDestinations[4];
        for (int m = 0; m < 4; ++m)
        {
            modRates[m] = v(rateIds[m]);
            modShapes[m] = int(v(shapeIds[m]));
            modAmounts[m] = v(amountIds[m]);
            if (m == 0)
                modAmounts[m] *= target[Depth];
            modDestinations[m] = int(v(destIds[m]));
        }
        uint64_t modMask = 0;
        const int destinationParameter[] = {-1,    Cutoff,     Resonance, Position, Balance,
                                            Shape, PulseWidth, Detune,    FM,       Unison,
                                            Drive, Noise,      Sub,       -1,       Pan};
        for (int m = 0; m < 4; ++m)
            if (modDestinations[m] > 0 && std::abs(modAmounts[m]) > .001f &&
                destinationParameter[modDestinations[m]] >= 0)
                modMask |= uint64_t(1) << destinationParameter[modDestinations[m]];
        if (trace)
        {
            for (int m = 0; m < 4; ++m)
                if (modDestinations[m] == 13 && std::abs(modAmounts[m]) > .001f)
                    modMask |= uint64_t(1) << 63;
            visual.modMask.store(modMask);
        }
        float detuneRatios[3], panL[3], panR[3];
        const float uTarget = target[Unison];
        const float targetB = std::exp2(target[Detune] / 1200), targetDrive = std::exp2(target[Drive] * 4.5f);
        const float targetComp = 1 / std::sqrt(targetDrive);
        for (int u = 0; u < 3; ++u)
        {
            float side = u == 0 ? 0.f : u == 1 ? -1.f : 1.f;
            detuneRatios[u] = std::pow(2.f, side * (3 + uTarget * 27) / 1200);
            float pan = side * .8f;
            panL[u] = std::sqrt((1 - pan) * .5f);
            panR[u] = std::sqrt((1 + pan) * .5f);
        }
        for (int i = 0; i < count; ++i)
        {
            for (int p = 0; p < Count; ++p)
                smooth[p] += alpha * (target[p] - smooth[p]);
            auto s = smooth;
            float env = amp.getNextSample(), fe = filterEnv.getNextSample();
            float lfo = lfoValue(pl, lfoShape, tables);
            pl = wrap(pl + s[Rate] / sr);
            float cutoffMod = 0, panMod = 0, ampMod = 0, detuneMod = 0, driveMod = 0;
            for (int m = 0; m < 4; ++m)
            {
                float wave = m == 0 ? lfo : lfoValue(extraPhases[size_t(m - 1)], modShapes[m], tables);
                if (m > 0)
                    extraPhases[size_t(m - 1)] = wrap(extraPhases[size_t(m - 1)] + modRates[m] / sr);
                float amount = wave * modAmounts[m];
                switch (modDestinations[m])
                {
                case 1:
                    cutoffMod += amount * 7;
                    break;
                case 2:
                    s[Resonance] += amount * .8f;
                    break;
                case 3:
                    s[Position] += amount * .8f;
                    break;
                case 4:
                    s[Balance] += amount * .8f;
                    break;
                case 5:
                    s[Shape] += amount * .8f;
                    break;
                case 6:
                    s[PulseWidth] += amount * .4f;
                    break;
                case 7:
                    detuneMod += amount * 50;
                    break;
                case 8:
                    s[FM] += amount * .8f;
                    break;
                case 9:
                    s[Unison] += amount * .8f;
                    break;
                case 10:
                    driveMod += amount * 3;
                    break;
                case 11:
                    s[Noise] += amount * .7f;
                    break;
                case 12:
                    s[Sub] += amount * .8f;
                    break;
                case 13:
                    ampMod += amount * .95f;
                    break;
                case 14:
                    panMod += amount;
                    break;
                default:
                    break;
                }
            }
            for (int parameter : {Resonance, Position, Balance, Shape, FM, Unison, Noise, Sub})
                s[size_t(parameter)] = jlimit(0.f, 1.f, s[size_t(parameter)]);

            float pw = jlimit(.05f, .95f, s[PulseWidth]);
            float stringValue = 0;
            if (engine == 2)
            {
                float delay = jlimit(2.f, 11998.f, sr / jmax(20.f, ha) - .5f);
                float read = float(stringWrite) - delay;
                if (read < 0)
                    read += 12000;
                int at = int(read);
                float signal =
                    stringBuffer[size_t(at)] +
                    (read - at) * (stringBuffer[size_t((at + 1) % 12000)] - stringBuffer[size_t(at)]);
                stringValue = signal;
                stringBuffer[size_t(stringWrite)] = (signal + stringPrevious) * .5f * stringDamping;
                stringPrevious = signal;
                stringWrite = (stringWrite + 1) % 12000;
            }
            float stereo[2] = {0, 0};
            bRatio += alpha * (targetB - bRatio);
            driveGain += alpha * (targetDrive - driveGain);
            driveComp += alpha * (targetComp - driveComp);
            float bDetune = bRatio * (detuneMod != 0 ? std::exp2(detuneMod / 1200) : 1),
                  normal = 1 / (1 + s[Unison] * 1.3f);
            for (int u = 0; u < 3; ++u)
            {
                if (u > 0 && s[Unison] < .0001f)
                    continue;
                float ratio = detuneRatios[u], da = ha * ratio / sr, db = hb * ratio * bDetune / sr;
                float a = tables.read(pa[u] + (s[FM] > .0001f ? tables.sin(pb[u]) * s[FM] * 1.6f : 0.f),
                                      jlimit(0.f, 1.f,
                                             s[Position] + (u == 1   ? -1.f
                                                            : u == 2 ? 1.f
                                                                     : 0.f) *
                                                               s[Scan] * .35f),
                                      family, tableBand);
                if (engine == 1)
                {
                    float modulation = tables.sin(pc[u]) * (.15f + s[FM] * 10) * (.15f + fe * .85f);
                    a = tables.sin(pa[u] + modulation * .15915494f);
                    pc[u] = wrap(pc[u] + da * fmRatio);
                }
                else if (engine == 2)
                    a = stringValue;
                else if (engine == 3)
                {
                    float phase = pa[u] + tables.sin(pb[u]) * s[FM] * .3f;
                    float color = s[Position];
                    a = tables.sin(phase) * .65f + tables.sin(phase * 2) * (.15f + color * .25f) +
                        tables.sin(phase * 3) * color * .3f + tables.sin(phase * 4) * color * .2f;
                }
                else if (engine == 4)
                {
                    float sawA = 2 * pa[u] - 1 - blep(pa[u], da), triangle = 2 * std::abs(2 * pa[u] - 1) - 1;
                    a = triangle * (1 - s[Position]) + sawA * s[Position];
                }
                float saw = 2 * pb[u] - 1 - blep(pb[u], db);
                float pulse = (pb[u] < pw ? 1.f : -1.f) + blep(pb[u], db) - blep(wrap(pb[u] + 1 - pw), db);
                pulse -= (2 * pw - 1);
                float softPhase = pb[u] < pw ? pb[u] / pw * .5f : .5f + (pb[u] - pw) / (1 - pw) * .5f;
                float soft = tables.sin(softPhase) - (2 * pw - 1) * .63661977f;
                pulse = (soft * (1 - s[PwmDepth]) + pulse * s[PwmDepth]) * .7f;
                float b = saw * (1 - s[Shape]) + pulse * s[Shape];
                float weight = u == 0 ? 1.f : s[Unison] * .9f,
                      signal = (a * (1 - s[Balance]) + b * s[Balance]) * weight * normal;
                stereo[0] += signal * panL[u];
                stereo[1] += signal * panR[u];
                pa[u] = wrap(pa[u] + da);
                pb[u] = wrap(pb[u] + db);
            }
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            float noise = (float(random & 0xffff) / 32768.f - 1) * s[Noise] * .6f;
            float sub = tables.sin(ps) * s[Sub] * .7f;
            ps = wrap(ps + hz * bend * .5f / sr);
            if (coefficientCountdown-- <= 0)
            {
                float octaves = cutoffMod + fe * s[EnvAmount] * 7 + wheel * 3 +
                                s[KeyTrack] * std::log2(hz / 261.6256f) +
                                (velocity - .5f) * s[VelocityFilter] * 5;
                float cut = jlimit(20.f, jmin(20000.f, sr * .35f), s[Cutoff] * std::exp2(octaves));
                gNow = std::tan(MathConstants<float>::pi * cut / sr);
                kNow = 2 - 1.94f * s[Resonance];
                coefficientCountdown = 7;
                if (trace)
                {
                    for (int parameter = 0; parameter < Count; ++parameter)
                        visual.effective[size_t(parameter)].store(s[size_t(parameter)],
                                                                  std::memory_order_relaxed);
                    visual.effective[Cutoff].store(cut, std::memory_order_relaxed);
                    visual.effective[Position].store(s[Position], std::memory_order_relaxed);
                    visual.effective[PulseWidth].store(pw, std::memory_order_relaxed);
                    visual.effective[Detune].store(smooth[Detune] + detuneMod, std::memory_order_relaxed);
                    visual.effective[Drive].store(jlimit(0.f, 1.f, smooth[Drive] + driveMod / 4.5f),
                                                  std::memory_order_relaxed);
                    visual.effective[Pan].store(jlimit(-1.f, 1.f, s[Pan] + panMod),
                                                std::memory_order_relaxed);
                    visual.ampMod.store(jlimit(0.f, 2.f, 1 + ampMod), std::memory_order_relaxed);
                }
            }
            float drive = driveGain * (driveMod != 0 ? std::exp2(driveMod) : 1), comp = driveComp;
            for (int c = 0; c < 2; ++c)
            {
                float x = std::tanh((stereo[c] + sub + noise) * drive) * comp;
                float filtered = filters[c][0].tick(x, gNow, kNow, mode);
                if (mode == 1)
                    filtered = filters[c][1].tick(filtered, gNow, kNow, 0);
                out.addSample(c, start + i,
                              filtered * env * velocity * .24f * jlimit(0.f, 2.f, 1 + ampMod) *
                                  std::sqrt(jlimit(0.f, 2.f, 1 + (c == 0 ? -1.f : 1.f) * (panMod + s[Pan]))));
            }
            if (!amp.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }
};
} // namespace noryn
