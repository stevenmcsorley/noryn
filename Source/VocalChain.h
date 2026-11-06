#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
namespace noryn
{
// Vocal channel strip for audio tracks: gate > EQ > de-esser > compressor > pitch correction >
// saturation > vocoder > doubler / harmony > tape delay and plate reverb sends.
struct VocalSettings
{
    struct Field
    {
        const char *id, *name;
        float min, max, def;
    };
    // Order matters: it is the index into `values`.
    static const std::vector<Field> &fields()
    {
        static const std::vector<Field> list{
            {"gate", "Gate", 0, 1, 1},
            {"gatethreshold", "Gate threshold dB", -80, -10, -55},
            {"eq", "EQ", 0, 1, 1},
            {"lowcut", "Low cut Hz", 20, 400, 90},
            {"mud", "Mud cut dB", -12, 0, -3},
            {"presence", "Presence dB", -6, 12, 3},
            {"air", "Air dB", -6, 12, 2},
            {"deess", "De-ess", 0, 1, .4f},
            {"comp", "Compressor", 0, 1, 1},
            {"compthreshold", "Comp threshold dB", -40, 0, -18},
            {"compratio", "Comp ratio", 1, 10, 3.5f},
            {"makeup", "Makeup dB", 0, 18, 4},
            {"tune", "Pitch correct", 0, 1, 0},
            {"tuneamount", "Correction amount", 0, 1, 1},
            {"tunespeed", "Retune speed", 0, 1, .35f},
            {"saturation", "Tape saturation", 0, 1, .1f},
            {"vocoder", "Vocoder", 0, 1, 0},
            {"vocodermix", "Vocoder mix", 0, 1, 1},
            {"carrier", "Carrier", 0, 2, 0},
            {"doubler", "Doubler", 0, 1, 0},
            {"harmony", "Harmony semitones", -12, 12, 0},
            {"harmonylevel", "Harmony level", 0, 1, 0},
            {"delay", "Delay send", 0, 1, .1f},
            {"delaytime", "Delay time", 0, 2, 1},
            {"delayfeedback", "Delay feedback", 0, .85f, .35f},
            {"reverb", "Reverb send", 0, 1, .18f},
            {"reverbsize", "Reverb size", 0, 1, .6f},
            {"output", "Output dB", -24, 12, 0}};
        return list;
    }
    static int indexOf(const juce::String &id)
    {
        const auto &list = fields();
        for (size_t i = 0; i < list.size(); ++i)
            if (id == list[i].id)
                return int(i);
        return -1;
    }
    std::array<std::atomic<float>, 32> values{};
    VocalSettings()
    {
        reset();
    }
    void reset()
    {
        const auto &list = fields();
        for (size_t i = 0; i < list.size(); ++i)
            values[i].store(list[i].def);
    }
    float get(const char *id) const
    {
        return values[size_t(indexOf(id))].load();
    }
    void set(const juce::String &id, float value)
    {
        const int i = indexOf(id);
        if (i >= 0)
            values[size_t(i)].store(juce::jlimit(fields()[size_t(i)].min, fields()[size_t(i)].max, value));
    }
    juce::ValueTree save() const
    {
        juce::ValueTree tree("Vocal");
        const auto &list = fields();
        for (size_t i = 0; i < list.size(); ++i)
            tree.setProperty(list[i].id, values[i].load(), nullptr);
        return tree;
    }
    void restore(const juce::ValueTree &tree)
    {
        reset();
        const auto &list = fields();
        for (size_t i = 0; i < list.size(); ++i)
            if (tree.hasProperty(list[i].id))
                set(list[i].id, float(tree.getProperty(list[i].id)));
    }
    struct Preset
    {
        const char *name;
        std::vector<std::pair<const char *, float>> values;
    };
    static const std::vector<Preset> &presets()
    {
        static const std::vector<Preset> list{
            {"Clean vocal", {}},
            {"Radio pop", {{"compthreshold", -24}, {"compratio", 5}, {"makeup", 8}, {"presence", 6}, {"air", 6},
                           {"deess", .6f}, {"reverb", .15f}, {"delay", .08f}}},
            {"Subtle tune", {{"tune", 1}, {"tuneamount", .7f}, {"tunespeed", .55f}, {"reverb", .2f}}},
            {"Hard tune (robot pop)", {{"tune", 1}, {"tuneamount", 1}, {"tunespeed", 0}, {"presence", 5},
                                       {"air", 5}, {"compratio", 5}, {"reverb", .2f}, {"delay", .15f}}},
            {"Vocoder: synth carrier", {{"vocoder", 1}, {"vocodermix", 1}, {"carrier", 0}, {"reverb", .2f},
                                        {"comp", 1}, {"compthreshold", -26}, {"makeup", 8}}},
            {"Vocoder: robot", {{"vocoder", 1}, {"vocodermix", 1}, {"carrier", 1}, {"tune", 1},
                                {"tunespeed", 0}, {"reverb", .15f}, {"makeup", 8}}},
            {"Vocoder: drone", {{"vocoder", 1}, {"vocodermix", .9f}, {"carrier", 2}, {"reverb", .4f},
                                {"reverbsize", .85f}, {"makeup", 8}}},
            {"Double and air", {{"doubler", .7f}, {"air", 8}, {"presence", 4}, {"reverb", .22f}}},
            {"Harmony third", {{"tune", 1}, {"tunespeed", .25f}, {"harmony", 4}, {"harmonylevel", .55f},
                               {"doubler", .3f}, {"reverb", .25f}}},
            {"Lo-fi tape (Portishead)", {{"lowcut", 250}, {"air", -6}, {"presence", 6}, {"saturation", .7f},
                                         {"compthreshold", -26}, {"compratio", 6}, {"delay", .25f},
                                         {"delaytime", 2}, {"reverb", .2f}}},
            {"Industrial distort (NIN)", {{"saturation", 1}, {"presence", 9}, {"lowcut", 180},
                                          {"compthreshold", -30}, {"compratio", 8}, {"doubler", .4f},
                                          {"delay", .12f}, {"output", -6}}},
            {"Ambient wash", {{"reverb", .65f}, {"reverbsize", .95f}, {"delay", .35f}, {"delayfeedback", .6f},
                              {"doubler", .4f}, {"air", 6}}},
            {"Spoken word", {{"lowcut", 120}, {"compthreshold", -22}, {"compratio", 4}, {"makeup", 6},
                             {"deess", .7f}, {"reverb", .05f}, {"delay", 0}}}};
        return list;
    }
    void applyPreset(int index)
    {
        reset();
        const auto &list = presets();
        for (const auto &[id, value] : list[size_t(juce::jlimit(0, int(list.size()) - 1, index))].values)
            set(id, value);
    }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    float process(float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset()
    {
        z1 = z2 = 0;
    }
    void set(double c0, double c1, double c2, double d0, double d1, double d2)
    {
        b0 = float(c0 / d0);
        b1 = float(c1 / d0);
        b2 = float(c2 / d0);
        a1 = float(d1 / d0);
        a2 = float(d2 / d0);
    }
    // RBJ cookbook designs.
    void highpass(double sr, double f, double q = .707)
    {
        const double w = juce::MathConstants<double>::twoPi * juce::jmin(f, sr * .45) / sr, a = std::sin(w) / (2 * q),
                     c = std::cos(w);
        set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
    }
    void bandpass(double sr, double f, double q)
    {
        const double w = juce::MathConstants<double>::twoPi * juce::jmin(f, sr * .45) / sr, a = std::sin(w) / (2 * q),
                     c = std::cos(w);
        set(a, 0, -a, 1 + a, -2 * c, 1 - a);
    }
    void peak(double sr, double f, double q, double db)
    {
        const double A = std::pow(10, db / 40), w = juce::MathConstants<double>::twoPi * juce::jmin(f, sr * .45) / sr,
                     a = std::sin(w) / (2 * q), c = std::cos(w);
        set(1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
    }
    void highShelf(double sr, double f, double db)
    {
        const double A = std::pow(10, db / 40), w = juce::MathConstants<double>::twoPi * juce::jmin(f, sr * .45) / sr,
                     c = std::cos(w), s = std::sin(w), alpha = s / 2 * std::sqrt(2.0), sq = 2 * std::sqrt(A) * alpha;
        set(A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
            (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

// Monophonic pitch detector (YIN) running on a 4x decimated copy of the input.
struct PitchDetector
{
    static constexpr int decimation = 4, window = 384, maxLag = 200, hop = 128, history = 1024;
    std::array<float, history> buffer{};
    std::array<float, maxLag + 1> diff{};
    int write = 0, phase = 0, sinceAnalysis = 0;
    float lp1 = 0, lp2 = 0, rate = 12000, hz = 0, confidence = 0, level = 0;
    void prepare(double sr)
    {
        rate = float(sr / decimation);
        buffer.fill(0);
        hz = confidence = level = 0;
        lp1 = lp2 = 0;
        write = phase = sinceAnalysis = 0;
    }
    void push(float x)
    {
        const float k = .25f;
        lp1 += k * (x - lp1);
        lp2 += k * (lp1 - lp2);
        if (++phase < decimation)
            return;
        phase = 0;
        buffer[size_t(write)] = lp2;
        write = (write + 1) % history;
        if (++sinceAnalysis >= hop)
        {
            sinceAnalysis = 0;
            analyse();
        }
    }
    void analyse()
    {
        auto at = [&](int i) { return buffer[size_t((write - window - maxLag + i + history * 2) % history)]; };
        double energy = 0;
        for (int i = 0; i < window; ++i)
            energy += at(i) * at(i);
        level = float(std::sqrt(energy / window));
        if (level < .003f)
        {
            confidence = 0;
            return;
        }
        const int minLag = int(rate / 1000);
        diff[0] = 1;
        double running = 0;
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            double sum = 0;
            for (int i = 0; i < window; ++i)
            {
                const float d = at(i) - at(i + lag);
                sum += d * d;
            }
            running += sum;
            diff[size_t(lag)] = float(sum * lag / juce::jmax(1e-12, running));
        }
        int best = -1;
        for (int lag = juce::jmax(2, minLag); lag < maxLag; ++lag)
            if (diff[size_t(lag)] < .15f)
            {
                while (lag + 1 < maxLag && diff[size_t(lag + 1)] < diff[size_t(lag)])
                    ++lag;
                best = lag;
                break;
            }
        if (best < 0)
        {
            confidence = 0;
            return;
        }
        const float a = diff[size_t(best - 1)], b = diff[size_t(best)], c = diff[size_t(best + 1)];
        const float shift = (a + c - 2 * b) != 0 ? .5f * (a - c) / (a + c - 2 * b) : 0.f;
        hz = rate / (float(best) + juce::jlimit(-.5f, .5f, shift));
        confidence = 1 - b;
    }
    bool voiced() const
    {
        return confidence > .8f && hz > 60 && hz < 1000;
    }
};

// Delay-line pitch shifter: two crossfaded read heads sweep a 30 ms window.
struct PitchShifter
{
    std::vector<float> buffer;
    int write = 0, length = 0;
    float phase = 0, windowSamples = 1440;
    void prepare(double sr)
    {
        windowSamples = float(sr * .03);
        length = juce::nextPowerOfTwo(int(windowSamples * 2 + 8));
        buffer.assign(size_t(length), 0.f);
        write = 0;
        phase = 0;
    }
    float read(float delay) const
    {
        float pos = float(write) - delay;
        while (pos < 0)
            pos += float(length);
        const int i = int(pos);
        const float frac = pos - float(i);
        const float a = buffer[size_t(i & (length - 1))], b = buffer[size_t((i + 1) & (length - 1))];
        return a + frac * (b - a);
    }
    float process(float x, float ratio)
    {
        buffer[size_t(write)] = x;
        phase += (1 - ratio) / windowSamples;
        phase -= std::floor(phase);
        float second = phase + .5f;
        second -= std::floor(second);
        const float out = read(2 + phase * windowSamples) * (.5f - .5f * std::cos(juce::MathConstants<float>::twoPi * phase)) +
                          read(2 + second * windowSamples) * (.5f - .5f * std::cos(juce::MathConstants<float>::twoPi * second));
        write = (write + 1) & (length - 1);
        return out;
    }
    int latency() const
    {
        return int(2 + windowSamples * .5f);
    }
};

struct Vocoder
{
    static constexpr int bands = 20;
    std::array<Biquad, bands> modulator, carrier;
    std::array<float, bands> envelope{};
    Biquad sibilance;
    float attack = .99f, release = .999f, sr = 44100;
    void prepare(double rate)
    {
        sr = float(rate);
        for (int b = 0; b < bands; ++b)
        {
            const double f = 110 * std::pow(7200.0 / 110, double(b) / (bands - 1));
            modulator[size_t(b)].bandpass(rate, f, 7);
            carrier[size_t(b)].bandpass(rate, f, 7);
            modulator[size_t(b)].reset();
            carrier[size_t(b)].reset();
        }
        envelope.fill(0);
        sibilance.highpass(rate, 5500);
        sibilance.reset();
        attack = std::exp(-1.f / (sr * .002f));
        release = std::exp(-1.f / (sr * .035f));
    }
    float process(float voice, float carrierSample)
    {
        float out = 0;
        for (size_t b = 0; b < bands; ++b)
        {
            const float level = std::abs(modulator[b].process(voice));
            envelope[b] = level > envelope[b] ? attack * envelope[b] + (1 - attack) * level
                                              : release * envelope[b] + (1 - release) * level;
            out += carrier[b].process(carrierSample) * envelope[b];
        }
        return out * 9 + sibilance.process(voice) * .4f;
    }
};

class VocalChain
{
  public:
    VocalSettings settings;
    std::atomic<float> detectedHz{0}, correctedHz{0}, gainReduction{0};

    void prepare(double rate, int block)
    {
        sr = rate;
        detector.prepare(rate);
        shifter.prepare(rate);
        harmonyShifter.prepare(rate);
        vocoder.prepare(rate);
        doubleBuffer.assign(size_t(rate * .05) + 4, 0.f);
        delayBuffer.setSize(2, int(rate * 2.2) + 4);
        delayBuffer.clear();
        reverb.setSampleRate(rate);
        reverb.reset();
        scratchL.assign(size_t(juce::jmax(1, block)), 0.f);
        scratchR.assign(size_t(juce::jmax(1, block)), 0.f);
        for (auto *filter : {&lowCut, &lowCut2, &mud, &presence, &air})
            filter->reset();
        gateEnv = compEnv = gateGain = 0;
        ratioSmooth = 1;
        sinceCoefficients = 0;
        write = delayWrite = 0;
        robotPhase = 0;
        lastVoiced = 220;
    }
    int latency() const
    {
        return settings.get("tune") > .5f ? shifter.latency() : 0;
    }
    // Processes `count` mono samples from `input` into stereo `left`/`right`. `carrier` (may be null)
    // is NORYN's synth output, used by the vocoder's synth carrier. `allowed` is the 12-bit
    // pitch-class mask of the current scale, already rotated to the root.
    void process(const float *input, const float *carrierAudio, float *left, float *right, int count, float bpm,
                 uint16_t allowed, int root)
    {
        const auto &s = settings;
        if (sinceCoefficients-- <= 0)
        {
            sinceCoefficients = 8;
            lowCut.highpass(sr, s.get("lowcut"));
            lowCut2.highpass(sr, s.get("lowcut"));
            mud.peak(sr, 320, .9, s.get("mud"));
            presence.peak(sr, 3600, .8, s.get("presence"));
            air.highShelf(sr, 10000, s.get("air"));
        }
        const bool gateOn = s.get("gate") > .5f, eqOn = s.get("eq") > .5f, compOn = s.get("comp") > .5f,
                   tuneOn = s.get("tune") > .5f, vocoderOn = s.get("vocoder") > .5f;
        const float gateThreshold = juce::Decibels::decibelsToGain(s.get("gatethreshold")),
                    compThreshold = juce::Decibels::decibelsToGain(s.get("compthreshold")),
                    ratio = s.get("compratio"), makeup = juce::Decibels::decibelsToGain(s.get("makeup")),
                    deess = s.get("deess"), saturation = s.get("saturation"),
                    vocoderMix = s.get("vocodermix"), doubler = s.get("doubler"),
                    harmonyLevel = s.get("harmonylevel"),
                    harmonyRatio = std::exp2(std::round(s.get("harmony")) / 12),
                    delaySend = s.get("delay"), feedback = s.get("delayfeedback"),
                    output = juce::Decibels::decibelsToGain(s.get("output")),
                    amount = s.get("tuneamount");
        const int carrierMode = int(std::round(s.get("carrier")));
        const float fast = float(sr);
        const float gateAttack = std::exp(-1.f / (fast * .001f)), gateRelease = std::exp(-1.f / (fast * .08f)),
                    compAttack = std::exp(-1.f / (fast * .005f)), compRelease = std::exp(-1.f / (fast * .09f));
        // Retune speed 0 = instant (hard tune); 1 = 250 ms glide.
        const float retune = std::exp(-1.f / (fast * (.0008f + s.get("tunespeed") * .25f)));
        const float divisions[] = {.5f, .75f, 1};
        const float delaySamples = juce::jlimit(1.f, float(delayBuffer.getNumSamples() - 2),
                                                float(sr * 60 / juce::jlimit(30.f, 300.f, bpm) *
                                                      divisions[juce::jlimit(0, 2, int(std::round(s.get("delaytime"))))]));
        float reduction = 0;
        for (int i = 0; i < count; ++i)
        {
            float x = input[i];
            if (!std::isfinite(x))
                x = 0;
            // Gate
            const float level = std::abs(x);
            gateEnv = level > gateEnv ? gateAttack * gateEnv + (1 - gateAttack) * level
                                      : gateRelease * gateEnv + (1 - gateRelease) * level;
            const float gateTarget = !gateOn || gateEnv > gateThreshold ? 1.f : gateEnv > gateThreshold * .5f ? gateGain : 0.f;
            gateGain += (gateTarget - gateGain) * (gateTarget > gateGain ? .02f : .0015f);
            x *= gateGain;
            // EQ
            if (eqOn)
                x = air.process(presence.process(mud.process(lowCut2.process(lowCut.process(x)))));
            // De-esser: duck the band above ~6 kHz when it dominates.
            if (deess > 0)
            {
                deLow += .45f * (x - deLow);
                const float high = x - deLow;
                deEnvHigh = juce::jmax(std::abs(high), deEnvHigh * .9985f);
                deEnvAll = juce::jmax(std::abs(x), deEnvAll * .9985f);
                const float excess = deEnvAll > 1e-5f ? deEnvHigh / deEnvAll : 0.f;
                const float duck = excess > .45f ? 1 - deess * juce::jmin(1.f, (excess - .45f) * 2.5f) : 1.f;
                x = deLow + high * duck;
            }
            // Compressor
            if (compOn)
            {
                const float in = std::abs(x);
                compEnv = in > compEnv ? compAttack * compEnv + (1 - compAttack) * in
                                       : compRelease * compEnv + (1 - compRelease) * in;
                float gain = 1;
                if (compEnv > compThreshold)
                    gain = std::pow(compThreshold / compEnv, 1 - 1 / ratio);
                reduction = juce::jmax(reduction, 1 - gain);
                x *= gain * makeup;
            }
            // Pitch correction to the nearest note of the scale.
            detector.push(x);
            float target = 1;
            if (detector.voiced())
            {
                lastVoiced = detector.hz;
                const float midi = 69 + 12 * std::log2(detector.hz / 440);
                int bestNote = int(std::round(midi));
                float bestDistance = 99;
                for (int candidate = int(std::floor(midi)) - 6; candidate <= int(std::ceil(midi)) + 6; ++candidate)
                    if (allowed & (1 << (((candidate - root) % 12 + 12) % 12)))
                        if (std::abs(float(candidate) - midi) < bestDistance)
                        {
                            bestDistance = std::abs(float(candidate) - midi);
                            bestNote = candidate;
                        }
                target = std::exp2((float(bestNote) - midi) * amount / 12);
                correctedHz.store(440 * std::exp2((float(bestNote) - 69) / 12));
            }
            detectedHz.store(detector.voiced() ? detector.hz : 0.f);
            ratioSmooth = retune * ratioSmooth + (1 - retune) * (tuneOn ? target : 1.f);
            const float dry = x;
            if (tuneOn)
                x = shifter.process(x, ratioSmooth);
            // Saturation
            if (saturation > 0)
            {
                const float drive = 1 + saturation * 6;
                x = std::tanh(x * drive) / std::tanh(drive) * (1 - saturation * .2f);
            }
            // Vocoder
            if (vocoderOn)
            {
                float c;
                if (carrierMode == 0 && carrierAudio != nullptr)
                    c = carrierAudio[i];
                else
                {
                    const float f = carrierMode == 1 ? lastVoiced * ratioSmooth : 440 * std::exp2((48 + root - 69) / 12.f);
                    robotPhase += f / fast;
                    robotPhase -= std::floor(robotPhase);
                    robotPhase2 += f * 1.5f / fast;
                    robotPhase2 -= std::floor(robotPhase2);
                    c = (2 * robotPhase - 1) * .7f + (carrierMode == 2 ? (2 * robotPhase2 - 1) * .4f : 0.f);
                }
                noiseState ^= noiseState << 13;
                noiseState ^= noiseState >> 17;
                noiseState ^= noiseState << 5;
                c += (float(noiseState & 0xffff) / 32768.f - 1) * .04f;
                x = x * (1 - vocoderMix) + vocoder.process(x, c) * vocoderMix;
            }
            // Harmony voice and doubler (two short modulated delays, panned).
            float l = x, r = x;
            if (harmonyLevel > 0)
            {
                const float h = harmonyShifter.process(dry, harmonyRatio * (tuneOn ? ratioSmooth : 1.f)) * harmonyLevel;
                l += h * .8f;
                r += h * .6f;
            }
            doubleBuffer[size_t(write)] = x;
            if (doubler > 0)
            {
                doublePhase += .37f / fast;
                doublePhase -= std::floor(doublePhase);
                const float wobble = std::sin(juce::MathConstants<float>::twoPi * doublePhase);
                const float dl = float(sr * (.014 + .0012 * wobble)), dr = float(sr * (.023 - .0015 * wobble));
                l += readDouble(dl) * doubler * .8f;
                r += readDouble(dr) * doubler * .8f;
            }
            write = (write + 1) % int(doubleBuffer.size());
            // Tape delay send: ping-pong with darkening repeats.
            float readPos = float(delayWrite) - delaySamples;
            if (readPos < 0)
                readPos += float(delayBuffer.getNumSamples());
            const int a = int(readPos), b = (a + 1) % delayBuffer.getNumSamples();
            const float frac = readPos - float(a);
            const float tapL = delayBuffer.getSample(0, a) + frac * (delayBuffer.getSample(0, b) - delayBuffer.getSample(0, a));
            const float tapR = delayBuffer.getSample(1, a) + frac * (delayBuffer.getSample(1, b) - delayBuffer.getSample(1, a));
            delayToneL += .35f * (tapR - delayToneL);
            delayToneR += .35f * (tapL - delayToneR);
            delayBuffer.setSample(0, delayWrite, std::tanh(x * delaySend + delayToneL * feedback));
            delayBuffer.setSample(1, delayWrite, std::tanh(delayToneR * feedback));
            delayWrite = (delayWrite + 1) % delayBuffer.getNumSamples();
            l += tapL;
            r += tapR;
            scratchL[size_t(i)] = l * output;
            scratchR[size_t(i)] = r * output;
        }
        gainReduction.store(reduction);
        juce::Reverb::Parameters rp;
        rp.roomSize = .5f + s.get("reverbsize") * .48f;
        rp.damping = .45f;
        rp.wetLevel = s.get("reverb") * .6f;
        rp.dryLevel = .5f; // JUCE scales dry by 2
        rp.width = 1;
        reverb.setParameters(rp);
        reverb.processStereo(scratchL.data(), scratchR.data(), count);
        for (int i = 0; i < count; ++i)
        {
            left[i] = std::isfinite(scratchL[size_t(i)]) ? scratchL[size_t(i)] : 0.f;
            right[i] = std::isfinite(scratchR[size_t(i)]) ? scratchR[size_t(i)] : 0.f;
        }
    }

  private:
    double sr = 44100;
    PitchDetector detector;
    PitchShifter shifter, harmonyShifter;
    Vocoder vocoder;
    Biquad lowCut, lowCut2, mud, presence, air;
    std::vector<float> doubleBuffer, scratchL, scratchR;
    juce::AudioBuffer<float> delayBuffer;
    juce::Reverb reverb;
    int write = 0, delayWrite = 0, sinceCoefficients = 0;
    float gateEnv = 0, gateGain = 0, compEnv = 0, ratioSmooth = 1, deLow = 0, deEnvHigh = 0, deEnvAll = 0;
    float doublePhase = 0, robotPhase = 0, robotPhase2 = 0, lastVoiced = 220, delayToneL = 0, delayToneR = 0;
    uint32_t noiseState = 98765;
    float readDouble(float delay) const
    {
        const int size = int(doubleBuffer.size());
        float pos = float(write) - delay;
        while (pos < 0)
            pos += float(size);
        const int i = int(pos) % size;
        const float frac = pos - std::floor(pos);
        return doubleBuffer[size_t(i)] + frac * (doubleBuffer[size_t((i + 1) % size)] - doubleBuffer[size_t(i)]);
    }
};
} // namespace noryn
