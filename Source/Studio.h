#pragma once
#include "VocalChain.h"
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
namespace noryn
{
// Multitrack recorder for the standalone app. Takes are written to disk on a background thread and
// streamed back through read-ahead buffers, so tracks can be minutes long without filling memory.
// Each track records one source and can run the vocal chain on playback and live monitoring.
constexpr int studioTracks = 8;
enum class TrackSource
{
    InputLeft,
    InputRight,
    InputStereo,
    Synth,
    Drums,
    SynthAndDrums
};
inline const juce::StringArray &trackSourceNames()
{
    static const juce::StringArray names{"Input 1 (mic)", "Input 2", "Input 1+2", "NORYN synth", "Drums",
                                         "Synth + drums"};
    return names;
}
inline bool isInputSource(int source)
{
    return source <= int(TrackSource::InputStereo);
}
struct StudioTrack
{
    juce::String name;
    std::atomic<int> source{0};
    std::atomic<bool> armed{false}, monitor{false}, mute{false}, solo{false}, fx{false};
    std::atomic<float> gain{.8f}, pan{0}, meter{0};
    VocalChain chain;
    // Take shown and played. Message thread writes these only while holding `sourceLock`.
    juce::File take, previousTake;
    juce::int64 takeStart = 0, previousStart = 0, takeLength = 0;
    std::unique_ptr<juce::AudioFormatReaderSource> reader;
    std::unique_ptr<juce::BufferingAudioSource> stream;
    juce::SpinLock sourceLock;
    juce::int64 streamPosition = 0; // audio thread: next sample the stream will deliver
    // Recording
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::SpinLock writerLock;
    juce::File recordingFile;
    juce::int64 recordStart = 0;
    int recordChannels = 1;
};

class Studio
{
  public:
    std::array<StudioTrack, studioTracks> tracks;
    std::atomic<bool> playing{false}, recording{false}, bouncing{false}, runSequencer{true}, stopRequested{false};
    std::atomic<juce::int64> position{0};
    std::atomic<int> roundTripLatency{0};
    std::atomic<float> masterMeter{0};
    std::atomic<int> generation{0}; // bumps when takes change, so the UI can refresh waveforms
    juce::File sessionDirectory;
    juce::String lastMessage;

    Studio()
    {
        for (int i = 0; i < studioTracks; ++i)
        {
            tracks[size_t(i)].name = "Track " + juce::String(i + 1);
            tracks[size_t(i)].source.store(i == 0 ? int(TrackSource::InputLeft)
                                           : i == 1 ? int(TrackSource::Synth)
                                           : i == 2 ? int(TrackSource::Drums)
                                                    : int(TrackSource::InputLeft));
        }
        tracks[0].name = "Vocal";
        tracks[0].fx.store(true);
        tracks[1].name = "Synth";
        tracks[2].name = "Drums";
        formats.registerBasicFormats();
        writerThread.startThread();
        readThread.startThread();
    }
    ~Studio()
    {
        stop();
        for (auto &track : tracks)
        {
            const juce::SpinLock::ScopedLockType lock(track.sourceLock);
            track.stream.reset();
            track.reader.reset();
        }
        bounceWriter.reset();
        writerThread.stopThread(4000);
        readThread.stopThread(4000);
    }
    void prepare(double sr, int block)
    {
        sampleRate = sr;
        blockSize = juce::jmax(1, block);
        mono.setSize(1, blockSize);
        carrier.setSize(1, blockSize);
        trackAudio.setSize(2, blockSize);
        chainOut.setSize(2, blockSize);
        for (auto &track : tracks)
        {
            track.chain.prepare(sr, blockSize);
            const juce::SpinLock::ScopedLockType lock(track.sourceLock);
            if (track.stream)
                track.stream->prepareToPlay(blockSize, sr);
        }
        relocateStreams();
    }
    double getSampleRate() const
    {
        return sampleRate;
    }
    // ---------- Message thread ----------
    static juce::File defaultRoot()
    {
        const auto overridePath = juce::SystemStats::getEnvironmentVariable("NORYN_STUDIO_DIR", {});
        if (overridePath.isNotEmpty())
            return juce::File(overridePath);
        return juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("NORYN Sessions");
    }
    juce::File ensureSession()
    {
        if (sessionDirectory == juce::File())
            newSession(defaultRoot().getChildFile("Session " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S")));
        return sessionDirectory;
    }
    void newSession(const juce::File &directory)
    {
        stop();
        for (int i = 0; i < studioTracks; ++i)
            clearTrack(i, false);
        sessionDirectory = directory;
        sessionDirectory.getChildFile("Audio").createDirectory();
        position.store(0);
        saveSession();
        lastMessage = "New session: " + sessionDirectory.getFullPathName();
    }
    bool openSession(const juce::File &directory)
    {
        auto xml = juce::XmlDocument::parse(directory.getChildFile("session.xml"));
        if (!xml || !xml->hasTagName("NorynSession"))
            return false;
        stop();
        sessionDirectory = directory;
        auto tree = juce::ValueTree::fromXml(*xml);
        sessionBpm = float(tree.getProperty("bpm", 120));
        for (const auto node : tree)
        {
            const int i = int(node.getProperty("index", -1));
            if (i < 0 || i >= studioTracks)
                continue;
            auto &track = tracks[size_t(i)];
            track.name = node.getProperty("name", track.name).toString();
            track.source.store(juce::jlimit(0, 5, int(node.getProperty("source", 0))));
            track.gain.store(float(node.getProperty("gain", .8f)));
            track.pan.store(float(node.getProperty("pan", 0)));
            track.mute.store(bool(node.getProperty("mute", false)));
            track.solo.store(bool(node.getProperty("solo", false)));
            track.fx.store(bool(node.getProperty("fx", false)));
            track.armed.store(false);
            track.monitor.store(false);
            track.chain.settings.restore(node.getChildWithName("Vocal"));
            const juce::String take = node.getProperty("take", "").toString();
            const juce::String previous = node.getProperty("previousTake", "").toString();
            track.previousTake = previous.isNotEmpty() ? directory.getChildFile(previous) : juce::File();
            track.previousStart = juce::int64(node.getProperty("previousStart", 0));
            setTake(i, take.isNotEmpty() ? directory.getChildFile(take) : juce::File(),
                    juce::int64(node.getProperty("takeStart", 0)));
        }
        position.store(0);
        relocateStreams();
        lastMessage = "Opened " + directory.getFileName();
        return true;
    }
    void saveSession()
    {
        if (sessionDirectory == juce::File())
            return;
        juce::ValueTree tree("NorynSession");
        tree.setProperty("version", "1.0", nullptr);
        tree.setProperty("sampleRate", sampleRate, nullptr);
        tree.setProperty("bpm", sessionBpm, nullptr);
        for (int i = 0; i < studioTracks; ++i)
        {
            const auto &track = tracks[size_t(i)];
            juce::ValueTree node("Track");
            node.setProperty("index", i, nullptr);
            node.setProperty("name", track.name, nullptr);
            node.setProperty("source", track.source.load(), nullptr);
            node.setProperty("gain", track.gain.load(), nullptr);
            node.setProperty("pan", track.pan.load(), nullptr);
            node.setProperty("mute", track.mute.load(), nullptr);
            node.setProperty("solo", track.solo.load(), nullptr);
            node.setProperty("fx", track.fx.load(), nullptr);
            if (track.take.existsAsFile())
            {
                node.setProperty("take", track.take.getRelativePathFrom(sessionDirectory), nullptr);
                node.setProperty("takeStart", track.takeStart, nullptr);
            }
            if (track.previousTake.existsAsFile())
            {
                node.setProperty("previousTake", track.previousTake.getRelativePathFrom(sessionDirectory), nullptr);
                node.setProperty("previousStart", track.previousStart, nullptr);
            }
            node.addChild(track.chain.settings.save(), -1, nullptr);
            tree.addChild(node, -1, nullptr);
        }
        if (auto xml = tree.createXml())
            xml->writeTo(sessionDirectory.getChildFile("session.xml"));
    }
    void setBpm(float bpm)
    {
        sessionBpm = bpm;
    }
    void play()
    {
        relocateStreams();
        primeStreams();
        stopRequested.store(false);
        playing.store(true);
    }
    bool record()
    {
        bool any = false;
        for (auto &track : tracks)
            any = any || track.armed.load();
        if (!any)
        {
            lastMessage = "Arm a track (R) before recording.";
            return false;
        }
        auto audioDir = ensureSession().getChildFile("Audio");
        audioDir.createDirectory();
        const auto start = position.load();
        for (int i = 0; i < studioTracks; ++i)
        {
            auto &track = tracks[size_t(i)];
            if (!track.armed.load())
                continue;
            const int source = track.source.load();
            track.recordChannels = source == int(TrackSource::InputLeft) || source == int(TrackSource::InputRight) ? 1 : 2;
            track.recordingFile = audioDir.getNonexistentChildFile(
                juce::File::createLegalFileName(track.name) + " take", ".wav", false);
            track.recordStart = start;
            juce::WavAudioFormat wav;
            auto stream = track.recordingFile.createOutputStream();
            if (!stream)
                continue;
            std::unique_ptr<juce::AudioFormatWriter> raw(
                wav.createWriterFor(stream.get(), sampleRate, juce::uint32(track.recordChannels), 24, {}, 0));
            if (!raw)
                continue;
            stream.release();
            auto threaded = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(raw.release(), writerThread, 32768);
            const juce::SpinLock::ScopedLockType lock(track.writerLock);
            track.writer = std::move(threaded);
        }
        relocateStreams();
        primeStreams();
        stopRequested.store(false);
        recording.store(true);
        playing.store(true);
        lastMessage = "Recording";
        return true;
    }
    // Stops transport; finishes any takes and makes them the tracks' new takes.
    void stop()
    {
        const bool wasRecording = recording.exchange(false);
        playing.store(false);
        if (bouncing.exchange(false))
        {
            const juce::SpinLock::ScopedLockType lock(bounceLock);
            bounceWriter.reset();
            lastMessage = "Bounced to " + bounceFile.getFullPathName();
        }
        if (!wasRecording)
            return;
        for (int i = 0; i < studioTracks; ++i)
        {
            auto &track = tracks[size_t(i)];
            std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> finished;
            {
                const juce::SpinLock::ScopedLockType lock(track.writerLock);
                finished = std::move(track.writer);
            }
            if (!finished)
                continue;
            finished.reset(); // flushes the FIFO and closes the file
            // Live inputs arrive late by the device's round trip; place them where they were played.
            const auto start = track.recordStart - (isInputSource(track.source.load()) ? roundTripLatency.load() : 0);
            track.previousTake = track.take;
            track.previousStart = track.takeStart;
            setTake(i, track.recordingFile, start);
        }
        saveSession();
        lastMessage = "Take saved";
    }
    void locate(juce::int64 samples)
    {
        position.store(std::max<juce::int64>(0, samples));
        relocateStreams();
    }
    void undoTake(int index)
    {
        auto &track = tracks[size_t(index)];
        if (!track.previousTake.existsAsFile())
            return;
        const auto file = track.previousTake;
        const auto start = track.previousStart;
        track.previousTake = track.take;
        track.previousStart = track.takeStart;
        setTake(index, file, start);
        saveSession();
    }
    void clearTrack(int index, bool save = true)
    {
        auto &track = tracks[size_t(index)];
        if (track.take.existsAsFile())
        {
            track.previousTake = track.take;
            track.previousStart = track.takeStart;
        }
        setTake(index, juce::File(), 0);
        if (save)
            saveSession();
    }
    juce::int64 sessionLength() const
    {
        juce::int64 end = 0;
        for (const auto &track : tracks)
            if (track.takeLength > 0)
                end = juce::jmax(end, track.takeStart + track.takeLength);
        return end;
    }
    // Real-time bounce of the master output from the start to the end of the longest take.
    bool bounce(const juce::File &file)
    {
        const auto end = sessionLength();
        if (end <= 0)
        {
            lastMessage = "Nothing recorded to bounce yet.";
            return false;
        }
        stop();
        juce::WavAudioFormat wav;
        file.deleteFile();
        auto stream = file.createOutputStream();
        if (!stream)
            return false;
        std::unique_ptr<juce::AudioFormatWriter> raw(wav.createWriterFor(stream.get(), sampleRate, 2, 24, {}, 0));
        if (!raw)
            return false;
        stream.release();
        {
            const juce::SpinLock::ScopedLockType lock(bounceLock);
            bounceWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(raw.release(), writerThread, 65536);
        }
        bounceFile = file;
        bounceEnd = end + juce::int64(sampleRate * 2);
        locate(0);
        bouncing.store(true);
        playing.store(true);
        lastMessage = "Bouncing...";
        return true;
    }
    // ---------- Audio thread ----------
    struct Context
    {
        const float *inputLeft = nullptr, *inputRight = nullptr;
        const juce::AudioBuffer<float> *synth = nullptr, *drums = nullptr;
        juce::AudioBuffer<float> *output = nullptr; // tracks are added into this
        int count = 0;
        float bpm = 120;
        uint16_t scaleMask = 4095;
        int root = 0;
    };
    void process(const Context &c)
    {
        const int n = juce::jmin(c.count, blockSize);
        const bool run = playing.load(), rec = recording.load();
        const auto start = position.load();
        bool anySolo = false;
        for (auto &track : tracks)
            anySolo = anySolo || track.solo.load();
        // Synth mono sum feeds the vocoder's synth carrier.
        auto *carrierData = carrier.getWritePointer(0);
        for (int i = 0; i < n; ++i)
            carrierData[i] = c.synth ? (c.synth->getSample(0, i) + c.synth->getSample(1, i)) * .5f : 0.f;
        for (int t = 0; t < studioTracks; ++t)
        {
            auto &track = tracks[size_t(t)];
            const int source = track.source.load();
            const bool armed = track.armed.load();
            // 1. Record the raw source; the vocal chain stays non-destructive.
            if (rec && armed)
            {
                const juce::SpinLock::ScopedTryLockType lock(track.writerLock);
                if (lock.isLocked() && track.writer)
                {
                    sourceSignal(c, source, n);
                    const float *channels[] = {trackAudio.getReadPointer(0), trackAudio.getReadPointer(1)};
                    track.writer->write(channels, n);
                }
            }
            // 2. What the track plays: live input when monitoring an input, otherwise its take.
            trackAudio.clear(0, n);
            bool sounding = false;
            const bool liveInput = isInputSource(source) && (track.monitor.load() || (armed && rec));
            if (liveInput)
            {
                sourceSignal(c, source, n);
                sounding = true;
            }
            else if (run && !(rec && armed))
                sounding = readTake(track, start, n);
            if (!sounding)
            {
                track.meter.store(track.meter.load() * .8f);
                continue;
            }
            // 3. Vocal chain
            if (track.fx.load())
            {
                auto *m = mono.getWritePointer(0);
                for (int i = 0; i < n; ++i)
                    m[i] = (trackAudio.getSample(0, i) + trackAudio.getSample(1, i)) * .5f;
                track.chain.process(m, carrierData, trackAudio.getWritePointer(0), trackAudio.getWritePointer(1), n,
                                    c.bpm, c.scaleMask, c.root);
            }
            // 4. Fader, pan, mute/solo
            const bool audible = !track.mute.load() && (!anySolo || track.solo.load());
            const float gain = track.gain.load() * (audible ? 1.f : 0.f), pan = track.pan.load();
            const float gl = gain * std::sqrt((1 - pan) * .5f) * 1.4142f, gr = gain * std::sqrt((1 + pan) * .5f) * 1.4142f;
            float peak = 0;
            for (int i = 0; i < n; ++i)
            {
                const float l = trackAudio.getSample(0, i) * gl, r = trackAudio.getSample(1, i) * gr;
                if (c.output)
                {
                    c.output->addSample(0, i, l);
                    c.output->addSample(1, i, r);
                }
                peak = juce::jmax(peak, std::abs(l), std::abs(r));
            }
            track.meter.store(juce::jmax(peak, track.meter.load() * .8f));
        }
        if (run)
        {
            position.store(start + n);
            if (bouncing.load() && start + n >= bounceEnd)
                stopRequested.store(true);
        }
    }
    // Called with the final master output so a bounce captures exactly what was heard.
    void captureMaster(const juce::AudioBuffer<float> &master, int count)
    {
        float peak = 0;
        for (int c = 0; c < juce::jmin(2, master.getNumChannels()); ++c)
            peak = juce::jmax(peak, master.getMagnitude(c, 0, count));
        masterMeter.store(juce::jmax(peak, masterMeter.load() * .85f));
        if (!bouncing.load())
            return;
        const juce::SpinLock::ScopedTryLockType lock(bounceLock);
        if (lock.isLocked() && bounceWriter)
        {
            const float *channels[] = {master.getReadPointer(0), master.getReadPointer(juce::jmin(1, master.getNumChannels() - 1))};
            bounceWriter->write(channels, count);
        }
    }

  private:
    juce::AudioFormatManager formats;
    juce::TimeSliceThread writerThread{"NORYN recorder"}, readThread{"NORYN playback"};
    double sampleRate = 44100;
    int blockSize = 512;
    float sessionBpm = 120;
    juce::AudioBuffer<float> mono, carrier, trackAudio, chainOut;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> bounceWriter;
    juce::SpinLock bounceLock;
    juce::File bounceFile;
    juce::int64 bounceEnd = 0;

    void setTake(int index, const juce::File &file, juce::int64 start)
    {
        auto &track = tracks[size_t(index)];
        std::unique_ptr<juce::AudioFormatReaderSource> reader;
        std::unique_ptr<juce::BufferingAudioSource> stream;
        juce::int64 length = 0;
        if (file.existsAsFile())
            if (auto *formatReader = formats.createReaderFor(file))
            {
                length = formatReader->lengthInSamples;
                reader = std::make_unique<juce::AudioFormatReaderSource>(formatReader, true);
                stream = std::make_unique<juce::BufferingAudioSource>(reader.get(), readThread, false,
                                                                      int(sampleRate * 3), 2);
                stream->prepareToPlay(blockSize, sampleRate);
            }
        std::unique_ptr<juce::AudioFormatReaderSource> oldReader;
        std::unique_ptr<juce::BufferingAudioSource> oldStream;
        {
            const juce::SpinLock::ScopedLockType lock(track.sourceLock);
            oldStream = std::move(track.stream);
            oldReader = std::move(track.reader);
            track.reader = std::move(reader);
            track.stream = std::move(stream);
            track.take = file.existsAsFile() ? file : juce::File();
            track.takeStart = start;
            track.takeLength = length;
            track.streamPosition = -1;
        }
        relocateStream(track);
        generation.fetch_add(1);
    }
    // Wait (briefly) until each take has audio buffered at the play position, so the first beat plays.
    void primeStreams()
    {
        juce::AudioBuffer<float> probe(2, blockSize);
        for (auto &track : tracks)
        {
            const juce::SpinLock::ScopedLockType lock(track.sourceLock);
            if (track.stream)
                track.stream->waitForNextAudioBlockReady(juce::AudioSourceChannelInfo(&probe, 0, blockSize), 500);
        }
    }
    void relocateStreams()
    {
        for (auto &track : tracks)
            relocateStream(track);
    }
    void relocateStream(StudioTrack &track)
    {
        const juce::SpinLock::ScopedLockType lock(track.sourceLock);
        if (!track.stream)
            return;
        const auto offset = std::max<juce::int64>(0, position.load() - track.takeStart + latencyFor(track));
        track.stream->setNextReadPosition(offset);
        track.streamPosition = offset;
    }
    static int latencyFor(const StudioTrack &track)
    {
        return track.fx.load() ? track.chain.latency() : 0;
    }
    bool readTake(StudioTrack &track, juce::int64 timeline, int n)
    {
        const juce::SpinLock::ScopedTryLockType lock(track.sourceLock);
        if (!lock.isLocked() || !track.stream || track.takeLength <= 0)
            return false;
        const auto offset = timeline - track.takeStart + latencyFor(track);
        if (offset + n <= 0 || offset >= track.takeLength)
            return false;
        const int skip = int(std::clamp<juce::int64>(-offset, 0, n));
        const auto from = std::max<juce::int64>(0, offset);
        if (track.streamPosition != from)
        {
            // Out of step (a locate, a latency change): jump; the read-ahead refills in the background.
            track.stream->setNextReadPosition(from);
        }
        juce::AudioSourceChannelInfo info(&trackAudio, skip, n - skip);
        track.stream->getNextAudioBlock(info);
        track.streamPosition = from + (n - skip);
        if (track.reader && track.reader->getAudioFormatReader()->numChannels == 1)
            trackAudio.copyFrom(1, skip, trackAudio, 0, skip, n - skip);
        return true;
    }
    void sourceSignal(const Context &c, int source, int n)
    {
        trackAudio.clear(0, n);
        for (int i = 0; i < n; ++i)
        {
            float l = 0, r = 0;
            switch (TrackSource(source))
            {
            case TrackSource::InputLeft:
                l = r = c.inputLeft ? c.inputLeft[i] : 0.f;
                break;
            case TrackSource::InputRight:
                l = r = c.inputRight ? c.inputRight[i] : (c.inputLeft ? c.inputLeft[i] : 0.f);
                break;
            case TrackSource::InputStereo:
                l = c.inputLeft ? c.inputLeft[i] : 0.f;
                r = c.inputRight ? c.inputRight[i] : l;
                break;
            case TrackSource::Synth:
                l = c.synth ? c.synth->getSample(0, i) : 0.f;
                r = c.synth ? c.synth->getSample(1, i) : 0.f;
                break;
            case TrackSource::Drums:
                l = c.drums ? c.drums->getSample(0, i) : 0.f;
                r = c.drums ? c.drums->getSample(1, i) : 0.f;
                break;
            case TrackSource::SynthAndDrums:
                l = (c.synth ? c.synth->getSample(0, i) : 0.f) + (c.drums ? c.drums->getSample(0, i) : 0.f);
                r = (c.synth ? c.synth->getSample(1, i) : 0.f) + (c.drums ? c.drums->getSample(1, i) : 0.f);
                break;
            }
            trackAudio.setSample(0, i, l);
            trackAudio.setSample(1, i, r);
        }
    }
};
} // namespace noryn
