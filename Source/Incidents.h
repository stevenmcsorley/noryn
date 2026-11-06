#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
namespace noryn
{
// Black-box recorder for audio faults that cannot be reproduced offline. The audio thread keeps the
// last few seconds of output and watches for loud, white-noise-like output; it never mutes anything.
// When it triggers (or a safety guard fires), the ring freezes until the message thread saves it.
struct IncidentRecorder
{
    enum Reason
    {
        none,
        noiseLikeOutput,
        effectRecovery,
        voiceFault
    };
    static constexpr double seconds = 4, windowSeconds = .25, cooldownSeconds = 30;
    std::vector<float> left, right;
    int write = 0, windowLength = 1, windowCount = 0, crossings = 0, noisyWindows = 0;
    double energy = 0, sampleRate = 44100;
    int64_t sinceLast = 0;
    float previous = 0;
    std::atomic<int> pending{none};
    std::atomic<int> saved{0};
    float lastRms = 0, lastZeroCrossingRate = 0;

    void prepare(double sr)
    {
        sampleRate = sr;
        left.assign(size_t(sr * seconds), 0.f);
        right.assign(size_t(sr * seconds), 0.f);
        write = windowCount = crossings = noisyWindows = 0;
        energy = 0;
        windowLength = jmax(1, int(sr * windowSeconds));
        sinceLast = int64_t(sr * cooldownSeconds);
    }
    void trigger(Reason reason)
    {
        if (left.empty() || pending.load() != none || sinceLast < int64_t(sampleRate * cooldownSeconds))
            return;
        pending.store(reason);
        sinceLast = 0;
    }
    // Audio thread, once per output sample.
    void push(float l, float r)
    {
        if (left.empty() || pending.load() != none)
            return;
        ++sinceLast;
        left[size_t(write)] = l;
        right[size_t(write)] = r;
        write = (write + 1) % int(left.size());
        energy += double(l) * l;
        crossings += (l >= 0) != (previous >= 0);
        previous = l;
        if (++windowCount < windowLength)
            return;
        lastRms = float(std::sqrt(energy / windowCount));
        lastZeroCrossingRate = float(crossings) / float(windowCount);
        // Played material stays under this crossing rate even with the filter fully open (randomised
        // sessions measured at most ~0.36 above RMS 0.05); broadband noise sits at 0.5 or more.
        // The noise oscillator alone tops out near RMS 0.11, so this level is already unusual.
        const bool noisy = lastRms > .1f && lastZeroCrossingRate > .4f;
        noisyWindows = noisy ? noisyWindows + 1 : 0;
        windowCount = crossings = 0;
        energy = 0;
        if (noisyWindows >= 2)
        {
            noisyWindows = 0;
            trigger(noiseLikeOutput);
        }
    }
    static String describe(int reason)
    {
        return reason == noiseLikeOutput ? "loud noise-like output"
               : reason == effectRecovery ? "non-finite audio in effects (auto-recovered)"
               : reason == voiceFault     ? "non-finite voice output (auto-recovered)"
                                          : "none";
    }
    static File directory()
    {
        const auto overridePath = SystemStats::getEnvironmentVariable("NORYN_INCIDENT_DIR", {});
        if (overridePath.isNotEmpty())
            return File(overridePath);
        return File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("NORYN/incidents");
    }
    // Message thread. Writes <stamp>.wav (the last seconds of output), <stamp>.noryn (the full state,
    // loadable as a user preset) and <stamp>.txt (summary), then lets the audio thread record again.
    bool flush(const String &summary, const MemoryBlock &stateData)
    {
        const int reason = pending.load();
        if (reason == none)
            return false;
        auto dir = directory();
        dir.createDirectory();
        const auto stamp = Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
        const int length = int(left.size());
        AudioBuffer<float> clip(2, length);
        for (int i = 0; i < length; ++i)
        {
            const int at = (write + i) % length;
            clip.setSample(0, i, left[size_t(at)]);
            clip.setSample(1, i, right[size_t(at)]);
        }
        WavAudioFormat wav;
        auto wavFile = dir.getChildFile(stamp + ".wav");
        if (auto stream = wavFile.createOutputStream())
            if (std::unique_ptr<AudioFormatWriter> writer{
                    wav.createWriterFor(stream.get(), sampleRate, 2, 24, {}, 0)})
            {
                stream.release();
                writer->writeFromAudioSampleBuffer(clip, 0, length);
            }
        dir.getChildFile(stamp + ".noryn").replaceWithData(stateData.getData(), stateData.getSize());
        dir.getChildFile(stamp + ".txt")
            .replaceWithText("NORYN incident " + stamp + "\nReason: " + describe(reason) +
                             "\nLast window RMS " + String(lastRms, 3) + ", zero-crossing rate " +
                             String(lastZeroCrossingRate, 3) + "\n" + summary);
        // Keep the newest 20 incidents.
        auto texts = dir.findChildFiles(File::findFiles, false, "*.txt");
        texts.sort();
        for (int i = 0; i < texts.size() - 20; ++i)
            for (auto ext : {".txt", ".wav", ".noryn"})
                texts[i].withFileExtension(ext).deleteFile();
        saved.fetch_add(1);
        pending.store(none);
        return true;
    }
};
} // namespace noryn
