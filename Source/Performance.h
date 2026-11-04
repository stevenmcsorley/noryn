#pragma once
#include "Engine.h"
namespace noryn
{
// Per-channel last-note priority. MIDI channel 16 remains independent for the sequencer.
class PerformanceSynth : public juce::Synthesiser
{
    juce::AudioProcessorValueTreeState &state;
    struct Held
    {
        int note = 0;
        float velocity = 0;
    };
    std::array<std::array<Held, 128>, 16> keys{};
    std::array<int, 16> counts{};
    std::array<float, 16> lastFrequency{};
    float value(const char *id) const
    {
        return state.getRawParameterValue(id)->load();
    }
    Voice *active(int channel)
    {
        Voice *result = nullptr;
        for (int i = 0; i < getNumVoices(); ++i)
            if (auto *v = dynamic_cast<Voice *>(getVoice(i));
                v && v->isVoiceActive() && v->isPlayingChannel(channel))
            {
                if (result)
                    result->stopNote(0, false);
                result = v;
            }
        return result;
    }
    void erase(int channel, int note)
    {
        const int c = channel - 1;
        for (int i = 0; i < counts[size_t(c)];)
            if (keys[size_t(c)][size_t(i)].note == note)
            {
                for (int j = i + 1; j < counts[size_t(c)]; ++j)
                    keys[size_t(c)][size_t(j - 1)] = keys[size_t(c)][size_t(j)];
                --counts[size_t(c)];
            }
            else
                ++i;
    }

  public:
    explicit PerformanceSynth(juce::AudioProcessorValueTreeState &s) : state(s) {}
    void noteOn(int channel, int note, float velocity) override
    {
        const juce::ScopedLock lock(lockObject());
        int mode = int(value("voicemode"));
        if (mode == 0)
        {
            juce::Synthesiser::noteOn(channel, note, velocity);
            return;
        }
        int c = juce::jlimit(0, 15, channel - 1);
        const bool overlapping = counts[size_t(c)] > 0;
        erase(channel, note);
        keys[size_t(c)][size_t(counts[size_t(c)]++)] = {note, velocity};
        Voice *voice = active(channel);
        if (!voice)
        {
            juce::Synthesiser::noteOn(channel, note, velocity);
            voice = active(channel);
            if (voice && mode == 1 && lastFrequency[size_t(c)] > 0 && value("glidetime") > 0)
                voice->hz = lastFrequency[size_t(c)];
        }
        if (voice)
            voice->retarget(note, velocity, mode == 1 || !overlapping,
                            (mode == 1 || overlapping) ? value("glidetime") : 0);
        lastFrequency[size_t(c)] = float(juce::MidiMessage::getMidiNoteInHertz(note));
    }
    void noteOff(int channel, int note, float velocity, bool tail) override
    {
        const juce::ScopedLock lock(lockObject());
        if (int(value("voicemode")) == 0)
        {
            juce::Synthesiser::noteOff(channel, note, velocity, tail);
            return;
        }
        int c = juce::jlimit(0, 15, channel - 1);
        bool wasTop = counts[size_t(c)] > 0 && keys[size_t(c)][size_t(counts[size_t(c)] - 1)].note == note;
        erase(channel, note);
        if (auto *voice = active(channel))
        {
            if (counts[size_t(c)] > 0 && wasTop)
            {
                auto next = keys[size_t(c)][size_t(counts[size_t(c)] - 1)];
                voice->retarget(next.note, next.velocity, int(value("voicemode")) == 1, value("glidetime"));
                lastFrequency[size_t(c)] = float(juce::MidiMessage::getMidiNoteInHertz(next.note));
            }
            else if (counts[size_t(c)] == 0)
            {
                voice->setKeyDown(false);
                if (!tail || (!voice->isSustainPedalDown() && !voice->isSostenutoPedalDown()))
                    voice->stopNote(velocity, tail);
            }
        }
    }
    void allNotesOff(int channel, bool tail) override
    {
        if (channel == 0)
        {
            counts.fill(0);
            lastFrequency.fill(0);
        }
        else
        {
            counts[size_t(channel - 1)] = 0;
            lastFrequency[size_t(channel - 1)] = 0;
        }
        juce::Synthesiser::allNotesOff(channel, tail);
    }

  private:
    const juce::CriticalSection &lockObject()
    {
        return lock;
    }
};
} // namespace noryn
