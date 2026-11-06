#include <fstream>
#include "Presets.h"
#include "ScaleEngine.h"
#include "Sequencer.h"
#include <cmath>
#include <iostream>
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>
#include <set>
using namespace juce;
AudioProcessor *JUCE_CALLTYPE createPluginFilter();
bool norynTelemetryActive(AudioProcessor *);
var norynControl(AudioProcessor *, const var &);
var norynPerformanceTelemetry(AudioProcessor *);
void norynRefreshEditor(AudioProcessorEditor *);
bool norynScopeMatches(AudioProcessor *, const AudioBuffer<float> &);
static void require(bool condition, const String &message)
{
    if (!condition)
        throw std::runtime_error(message.toStdString());
}
#include "FeatureTests.h"
struct Harness
{
    std::unique_ptr<AudioProcessor> p{createPluginFilter()};
    std::map<String, RangedAudioParameter *> parameters;
    Harness()
    {
        for (auto *item : p->getParameters())
            if (auto *param = dynamic_cast<RangedAudioParameter *>(item))
                parameters[param->paramID] = param;
    }
    void value(const String &id, float value)
    {
        auto *parameter = parameters.at(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
    void prepare(double sr = 48000, int size = 256)
    {
        p->releaseResources();
        p->setPlayConfigDetails(0, 2, sr, size);
        p->prepareToPlay(sr, size);
    }
    AudioBuffer<float> render(double seconds, float velocity = .8f, std::vector<int> notes = {48},
                              double sr = 48000, bool releaseNotes = true, int modWheel = 0)
    {
        prepare(sr);
        int total = int(seconds * sr);
        AudioBuffer<float> result(2, total), block(2, 256);
        MidiBuffer midi;
        for (int offset = 0; offset < total; offset += 256)
        {
            int count = jmin(256, total - offset);
            block.setSize(2, count, false, false, true);
            midi.clear();
            if (offset == 0)
                for (int note : notes)
                    midi.addEvent(MidiMessage::noteOn(1, note, velocity), 0);
            if (offset == 0 && modWheel > 0)
                midi.addEvent(MidiMessage::controllerEvent(1, 1, modWheel), 0);
            int off = int(seconds * sr * .58);
            if (releaseNotes && off >= offset && off < offset + count)
                for (int note : notes)
                    midi.addEvent(MidiMessage::noteOff(1, note), off - offset);
            p->processBlock(block, midi);
            for (int c = 0; c < 2; ++c)
                result.copyFrom(c, offset, block, c, 0, count);
        }
        return result;
    }
    void baseline(const String &target)
    {
        p->setCurrentProgram(0);
        value("attack", .004f);
        value("decay", .22f);
        value("sustain", .65f);
        value("release", .2f);
        value("cutoff", 2400);
        value("balance", .5f);
        value("position", .65f);
        value("shape", .65f);
        value("unison", .35f);
        value("detune", 8);
        value("envamount", .6f);
        value("fattack", .015f);
        value("fdecay", .3f);
        value("fsustain", .3f);
        value("frelease", .2f);
        value("rate", 2.3f);
        value("depth", .7f);
        value("lfo1dest", 1);
        value("lfo1amount", .25f);
        if (target == "pulsewidth" || target == "pwmdepth")
        {
            value("balance", 1);
            value("shape", 1);
        }
        if (target == "scan" || target == "family" || target == "fm")
        {
            value("family", 1);
            value("balance", 0);
        }
        if (target == "detune" || target == "shape" || target == "octaveb")
            value("balance", 1);
        if (target == "lfoshape" || target == "rate" || target == "depth")
            value("depth", 1);
        if (target.startsWith("f") && target != "fm" && target != "feedback" && target != "family" &&
            target != "filtermode")
        {
            value("envamount", .8f);
            value("cutoff", 650);
        }
        if (target == "keytrack" || target == "velfilter")
        {
            value("cutoff", 650);
            value("envamount", 0);
            value("depth", 0);
        }
        if (target == "chorusrate" || target == "width")
            value("chorus", .85f);
        if (target == "delaytime" || target == "feedback")
        {
            value("delay", .8f);
            value("delaytime", .12f);
            value("feedback", .5f);
        }
        if (target == "fmratio")
        {
            value("engine", 1);
            value("fm", .6f);
            value("balance", 0);
        }
        if (target.startsWith("lfo") && target.length() > 3 && target[3] >= '1' && target[3] <= '4')
        {
            String number = target.substring(3, 4);
            value("lfo" + number + "dest", 1);
            value("lfo" + number + "amount", .7f);
            value(number == "1" ? "rate" : "lfo" + number + "rate", 3);
            value("cutoff", 700);
            value("envamount", 0);
        }
        if (target == "envcurve")
        {
            value("attack", .2f);
            value("decay", .4f);
            value("sustain", .5f);
        }
        if (target == "subshape")
        {
            value("sub", 1);
            value("octavea", -1);
        }
        if (target == "choruscharacter")
            value("chorus", .9f);
        if (target == "oscbsemi")
            value("balance", 1);
        if (target == "filtercharacter")
        {
            value("cutoff", 900);
            value("resonance", .65f);
        }
        if (target == "analogdrift")
            value("engine", 4);
        if (target == "fmprofile")
        {
            value("engine", 1);
            value("fm", .5f);
            value("balance", 0);
        }
        if (target == "osclink")
        {
            value("engine", 4);
            value("detune", 19);
            value("balance", .6f);
            value("shape", 1);
        }
        if (target == "size")
            value("reverb", .85f);
    }
};
static double energy(const AudioBuffer<float> &buffer)
{
    double sum = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float x = buffer.getSample(c, i);
            require(std::isfinite(x) && std::abs(x) < 1, "Non-finite or unbounded audio");
            sum += x * x;
        }
    return sum;
}
static void wav(const File &path, const AudioBuffer<float> &buffer)
{
    path.deleteFile();
    auto stream = path.createOutputStream();
    WavAudioFormat format;
    std::unique_ptr<AudioFormatWriter> writer(format.createWriterFor(stream.release(), 48000, 2, 24, {}, 0));
    require(writer && writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()),
            "WAV write failed");
}
int main(int argc, char **argv)
{
    // Tests must never read or overwrite the player's controller map.
    const auto testMap = File::getSpecialLocation(File::tempDirectory).getChildFile("noryn-test-midi-map.xml");
    testMap.deleteFile();
    setenv("NORYN_MIDI_MAP_FILE", testMap.getFullPathName().toRawUTF8(), 1);
    ScopedJuceInitialiser_GUI initialise;
    try
    {
        File directory = argc > 1 ? File::getCurrentWorkingDirectory().getChildFile(argv[1])
                                  : File::getCurrentWorkingDirectory();
        directory.createDirectory();
        testScaleEngine();
        testSequencer();
        testSequencerSafety();
        testEnvelopeReleaseReference();
        if (argc > 2 && String(argv[2]) == "--sequencer")
        {
            std::cout << "PASS: per-note recording, exact gates, stop/clear, five-minute loop, ADSR release "
                         "regression.\n";
            return 0;
        }

        Harness h;
        require(h.p->getNumPrograms() == 291, "Expected 290 factory presets plus Init");

        // Verify the shipped chord-study MIDI and native presets, not just their filenames.
        const auto exampleRoot = File::getCurrentWorkingDirectory();
        auto midiPath = exampleRoot.getChildFile("examples/Solitude Chord Study.mid");
        if (midiPath.existsAsFile())
        {
            FileInputStream midiStream(midiPath);
            MidiFile midiFile;
            require(midiFile.readFrom(midiStream), "Chord study MIDI is invalid");
            int ons = 0, offs = 0;
            for (int track = 0; track < midiFile.getNumTracks(); ++track)
                for (int event = 0; event < midiFile.getTrack(track)->getNumEvents(); ++event)
                {
                    const auto &msg = midiFile.getTrack(track)->getEventPointer(event)->message;
                    if (msg.isNoteOn())
                        ++ons;
                    else if (msg.isNoteOff())
                        ++offs;
                }
            require(ons == 45 && offs == 45, "Chord study MIDI has unbalanced notes");
            for (const auto &name :
                 StringArray{"M83 Solitude - Swell Chord Test", "M83 Solitude - Electric Piano Test"})
            {
                MemoryBlock patch;
                require(exampleRoot.getChildFile("presets/" + name + ".noryn").loadFileAsData(patch),
                        "Missing public test preset");
                h.p->setStateInformation(patch.getData(), int(patch.getSize()));
                const auto state = norynControl(h.p.get(), JSON::parse("{\"op\":\"state\"}"));
                require(std::abs(float(state["parameters"]["bpm"]["value"]) - 61) < .001f &&
                            state["steps"][0]["notes"].size() == 5 &&
                            std::abs(float(state["steps"][0]["noteDetails"][1]["length"]) - 7.8f) < .01f,
                        "Public chord-test preset did not recall its per-note pattern");
            }
        }
        auto burialPath = exampleRoot.getChildFile("presets/Burial Stolen Dog - Hazy Keys.noryn");
        if (burialPath.existsAsFile())
        {
            MemoryBlock patch;
            require(burialPath.loadFileAsData(patch), "Missing Burial study preset");
            h.p->setStateInformation(patch.getData(), int(patch.getSize()));
            auto state = norynControl(h.p.get(), JSON::parse(R"({"op":"state"})"));
            require(std::abs(float(state["parameters"]["bpm"]["value"]) - 115.5f) < .001f &&
                        int(state["parameters"]["fmprofile"]["value"]) == 12 &&
                        float(state["steps"][0]["noteDetails"][0]["length"]) == 15,
                    "Burial study lost its sound or independent bass gate");
            h.prepare();
            norynControl(h.p.get(), JSON::parse(R"({"op":"transport","args":{"playing":true}})"));
            const int loopSamples = int(16 * 60.0 / 115.5 * 48000);
            const int total = loopSamples + 48000 * 3;
            AudioBuffer<float> rendered(2, total), block(2, 256);
            MidiBuffer midi;
            for (int offset = 0; offset < total; offset += 256)
            {
                if (offset >= loopSamples && offset - 256 < loopSamples)
                    norynControl(h.p.get(), JSON::parse(R"({"op":"transport","args":{"playing":false}})"));
                int count = jmin(256, total - offset);
                block.setSize(2, count, false, false, true);
                midi.clear();
                h.p->processBlock(block, midi);
                for (int c = 0; c < 2; ++c)
                    rendered.copyFrom(c, offset, block, c, 0, count);
            }
            require(energy(rendered) > .01, "Burial study produced no sound");
            require(norynPerformanceTelemetry(h.p.get()).size() == 0, "Burial study held voices after Stop");
            wav(directory.getChildFile("stolen-dog-study.wav"), rendered);
            std::cout << "PASS: Burial study recalls its sound/gates, renders bounded audio and releases after Stop.\n";
        }
        {
            // Blocks larger than the prepared size must be split, not overrun the oversampler/chorus/click buffers.
            h.p->setCurrentProgram(1);
            norynControl(h.p.get(), JSON::parse(R"({"op":"sequence","args":{"replace":true,"steps":[{"index":0,"notes":[48,55,60]},{"index":4,"notes":[50,57,62]}]}})"));
            norynControl(h.p.get(), JSON::parse(R"({"op":"transport","args":{"playing":true}})"));
            h.prepare(48000, 64);
            double energyLarge = 0;
            for (int size : {4096, 37, 1500, 8192, 64})
            {
                AudioBuffer<float> big(2, size);
                MidiBuffer bigMidi;
                bigMidi.addEvent(MidiMessage::noteOn(1, 67, .8f), size - 1);
                h.p->processBlock(big, bigMidi);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < size; ++i)
                    {
                        require(std::isfinite(big.getSample(c, i)) && std::abs(big.getSample(c, i)) < 1,
                                "Oversized block produced invalid audio");
                        energyLarge += big.getSample(c, i) * big.getSample(c, i);
                    }
            }
            norynControl(h.p.get(), JSON::parse(R"({"op":"transport","args":{"playing":false}})"));
            const auto recovered = norynControl(h.p.get(), JSON::parse(R"({"op":"state"})"));
            require(energyLarge > .01 && int(recovered["audioRecoveries"]) == 0,
                    "Oversized blocks were silent or needed recovery");
            std::cout << "PASS: blocks up to 128x the prepared size render safely.\n";
        }
        {
            // MIDI learn: the next controller is assigned; later values drive the control or transport.
            auto send = [&](int cc, int value)
            {
                AudioBuffer<float> learnBlock(2, 256);
                MidiBuffer learnMidi;
                learnMidi.addEvent(MidiMessage::controllerEvent(3, cc, value), 10);
                h.p->processBlock(learnBlock, learnMidi);
            };
            h.prepare();
            norynControl(h.p.get(), JSON::parse(R"({"op":"midiMap","args":{"clearAll":true,"learn":"cutoff"}})"));
            send(21, 5);
            send(21, 127);
            require(h.parameters.at("cutoff")->getValue() > .999f, "Learned CC did not drive its parameter");
            send(21, 0);
            require(h.parameters.at("cutoff")->getValue() < .001f, "Learned CC did not reach the minimum");
            norynControl(h.p.get(), JSON::parse(R"({"op":"midiMap","args":{"learn":"transport:play"}})"));
            send(22, 0);
            send(22, 127);
            auto mapState = norynControl(h.p.get(), JSON::parse(R"({"op":"state"})"));
            require(bool(mapState["playing"]), "Learned button did not start the sequencer");
            send(22, 0);
            send(22, 127);
            mapState = norynControl(h.p.get(), JSON::parse(R"({"op":"state"})"));
            require(!bool(mapState["playing"]), "Learned button did not stop the sequencer");
            auto map = norynControl(h.p.get(), JSON::parse(R"({"op":"midiMap","args":{}})"));
            require(map["mappings"].size() == 2 && int(map["mappings"][0]["channel"]) == 3, "MIDI map not listed");
            require(testMap.existsAsFile() && testMap.loadFileAsString().contains("cutoff"),
                    "MIDI map was not saved");
            norynControl(h.p.get(), JSON::parse(R"({"op":"midiMap","args":{"clearAll":true}})"));
            h.value("cutoff", 1200);
            std::cout << "PASS: MIDI learn maps controllers to parameters and transport, and saves the map.\n";
        }
        if (argc > 2 && String(argv[2]) == "--fuzz")
        {
            // Simulated heavy live use. Every recovery would previously have been a stuck-noise event.
            const int rounds = argc > 3 ? String(argv[3]).getIntValue() : 200;
            Random rng(argc > 4 ? String(argv[4]).getLargeIntValue() : 1234);
            auto users = exampleRoot.getChildFile("presets").findChildFiles(File::findFiles, false, "*.noryn");
            std::vector<RangedAudioParameter *> all;
            for (auto &entry : h.parameters)
                all.push_back(entry.second);
            int nonFinite = 0, stuck = 0;
            for (int round = 0; round < rounds; ++round)
            {
                const double rates[] = {44100, 48000, 96000};
                const int sizes[] = {32, 64, 256, 512, 1024};
                const double sr = rates[rng.nextInt(3)];
                const int size = sizes[rng.nextInt(5)];
                if (rng.nextBool() && !users.isEmpty())
                {
                    MemoryBlock patch;
                    users[rng.nextInt(users.size())].loadFileAsData(patch);
                    h.p->setStateInformation(patch.getData(), int(patch.getSize()));
                }
                else
                    h.p->setCurrentProgram(rng.nextInt(h.p->getNumPrograms()));
                h.p->releaseResources();
                h.p->setPlayConfigDetails(0, 2, sr, size);
                h.p->prepareToPlay(sr, size);
                norynControl(h.p.get(), JSON::parse(R"({"op":"transport","args":{"playing":true}})"));
                AudioBuffer<float> block(2, size);
                MidiBuffer midi;
                const int blocks = int(sr * 3 / size);
                for (int b = 0; b < blocks; ++b)
                {
                    midi.clear();
                    if (rng.nextInt(6) == 0)
                    {
                        auto *param = all[size_t(rng.nextInt(int(all.size())))];
                        const auto id = param->paramID;
                        if (id != "master" && id != "seqlength")
                            param->setValueNotifyingHost(rng.nextInt(4) == 0 ? float(rng.nextInt(2))
                                                                              : rng.nextFloat());
                    }
                    const int at = rng.nextInt(size);
                    switch (rng.nextInt(10))
                    {
                    case 0:
                        midi.addEvent(MidiMessage::noteOn(1 + rng.nextInt(2), 24 + rng.nextInt(84),
                                                          float(rng.nextInt(127) + 1) / 127.f), at);
                        break;
                    case 1:
                        midi.addEvent(MidiMessage::noteOff(1 + rng.nextInt(2), 24 + rng.nextInt(84)), at);
                        break;
                    case 2:
                        midi.addEvent(MidiMessage::controllerEvent(1, 1, rng.nextInt(128)), at);
                        break;
                    case 3:
                        midi.addEvent(MidiMessage::pitchWheel(1, rng.nextInt(16384)), at);
                        break;
                    case 4:
                        midi.addEvent(MidiMessage::controllerEvent(1, 64, rng.nextBool() ? 127 : 0), at);
                        break;
                    default:
                        break;
                    }
                    h.p->processBlock(block, midi);
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < size; ++i)
                            if (!std::isfinite(block.getSample(c, i)) || std::abs(block.getSample(c, i)) >= 1)
                                ++nonFinite;
                }
                norynControl(h.p.get(), JSON::parse(R"({"op":"panic"})"));
                double tail = 0;
                const int tailBlocks = int(sr * 1.5 / size);
                for (int b = 0; b < tailBlocks; ++b)
                {
                    midi.clear();
                    h.p->processBlock(block, midi);
                    if (b >= tailBlocks - int(sr * .25 / size))
                        for (int c = 0; c < 2; ++c)
                            for (int i = 0; i < size; ++i)
                                tail += block.getSample(c, i) * block.getSample(c, i);
                }
                if (tail > .5)
                {
                    ++stuck;
                    std::cout << "stuck audio after panic, round " << round << " program "
                              << h.p->getCurrentProgram() << " energy " << tail << "\n";
                }
            }
            const auto finalState = norynControl(h.p.get(), JSON::parse(R"({"op":"state"})"));
            std::cout << "fuzz rounds " << rounds << " invalid samples " << nonFinite << " stuck " << stuck
                      << " effect recoveries " << int(finalState["audioRecoveries"]) << " voice faults "
                      << int(finalState["voiceFaults"]) << "\n";
            require(nonFinite == 0 && stuck == 0, "Fuzzed live use produced invalid or stuck audio");
            return 0;
        }
        if (argc > 2 && String(argv[2]) == "--user-presets")
        {
            // Play every shipped user preset's whole demo loop, then Stop: audio must stay finite and
            // bounded, sound during the loop, and fall silent with no held voices afterwards.
            auto files = exampleRoot.getChildFile("presets").findChildFiles(File::findFiles, false, "*.noryn");
            files.sort();
            std::ofstream levels(directory.getChildFile("user-preset-levels.csv").getFullPathName().toStdString());
            levels << "preset,steps,seconds,loop_rms,loop_peak,tail_rms,cpu_mean,cpu_worst_block,worst_at_seconds\n";
            int failures = 0;
            for (const auto &file : files)
            {
                MemoryBlock patch;
                require(file.loadFileAsData(patch), "Unreadable user preset");
                h.p->setStateInformation(patch.getData(), int(patch.getSize()));
                const auto state = norynControl(h.p.get(), JSON::parse(R"({"op":"state"})"));
                const double bpm = float(state["parameters"]["bpm"]["value"]);
                const int steps = int(state["parameters"]["seqlength"]["value"]);
                const int division = int(state["parameters"]["seqdivision"]["value"]);
                const double loopSeconds = jmin(48.0, steps * 60.0 / (bpm * (1 << division)));
                const int loopSamples = int(loopSeconds * 48000), total = loopSamples + 48000 * 4;
                h.prepare();
                norynControl(h.p.get(), JSON::parse(R"({"op":"transport","args":{"playing":true}})"));
                AudioBuffer<float> block(2, 256);
                MidiBuffer midi;
                double loopEnergy = 0, tailEnergy = 0, peak = 0, busy = 0, worst = 0, worstAt = 0;
                bool finite = true;
                for (int offset = 0; offset < total; offset += 256)
                {
                    if (offset >= loopSamples && offset - 256 < loopSamples)
                        norynControl(h.p.get(), JSON::parse(R"({"op":"transport","args":{"playing":false}})"));
                    const int count = jmin(256, total - offset);
                    block.setSize(2, count, false, false, true);
                    midi.clear();
                    const double started = Time::getMillisecondCounterHiRes();
                    h.p->processBlock(block, midi);
                    // Fraction of the block's real-time budget spent rendering it.
                    const double load = (Time::getMillisecondCounterHiRes() - started) / (count / 48.0);
                    busy += load * count;
                    if (load > worst)
                    {
                        worst = load;
                        worstAt = offset / 48000.0;
                    }
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < count; ++i)
                        {
                            const float x = block.getSample(c, i);
                            finite = finite && std::isfinite(x) && std::abs(x) < 1;
                            peak = jmax(peak, double(std::abs(x)));
                            if (offset < loopSamples)
                                loopEnergy += x * x;
                            else if (offset >= total - 24000)
                                tailEnergy += x * x;
                        }
                }
                const double loopRms = std::sqrt(loopEnergy / (2.0 * loopSamples)),
                             tailRms = std::sqrt(tailEnergy / (2.0 * 24000));
                const bool held = norynPerformanceTelemetry(h.p.get()).size() != 0;
                levels << '"' << file.getFileNameWithoutExtension() << "\"," << steps << ',' << loopSeconds << ','
                       << loopRms << ',' << peak << ',' << tailRms << ',' << busy / total << ',' << worst << ',' << worstAt << '\n';
                String problem = !finite ? "non-finite or unbounded audio"
                                 : loopRms < .003 ? "silent demo"
                                 : tailRms > .003 ? "sound continued after Stop"
                                 : held ? "voices held after Stop"
                                        : "";
                std::cout << (problem.isEmpty() ? "ok   " : "FAIL ") << file.getFileNameWithoutExtension() << " rms "
                          << loopRms << " peak " << peak << " tail " << tailRms << " cpu " << busy / total << " worst "
                          << worst << ' ' << problem << '\n';
                failures += problem.isNotEmpty();
            }
            require(failures == 0, "User preset demo check failed");
            std::cout << "PASS: " << files.size() << " user preset demos render bounded and stop cleanly.\n";
            return 0;
        }
        if (argc > 2 && String(argv[2]) == "--verify-solitude-exports")
        {
            std::cout << "PASS: shipped Solitude MIDI and both native test presets.\n";
            return 0;
        }
        h.p->setCurrentProgram(1);
        MemoryBlock legacyMemory;
        h.p->getStateInformation(legacyMemory);
        auto legacyXML =
            AudioProcessor::getXmlFromBinary(legacyMemory.getData(), int(legacyMemory.getSize()));
        auto legacyTree = ValueTree::fromXml(*legacyXML);
        const StringArray addedParameters{"filtercharacter", "voicemode",   "choruscharacter", "envcurve",
                                          "phasefree",       "analogdrift", "subshape",        "oscbsemi",
                                          "glidetime",       "bendrange",   "pitchfine",       "fmprofile",
                                          "osclink",         "seqrelease",  "seqfollowrelease"};
        for (int i = legacyTree.getNumChildren() - 1; i >= 0; --i)
            if (addedParameters.contains(legacyTree.getChild(i).getProperty("id").toString()))
                legacyTree.removeChild(i, nullptr);
        MemoryBlock legacyPatch;
        auto stripped = legacyTree.createXml();
        AudioProcessor::copyXmlToBinary(*stripped, legacyPatch);
        h.p->setCurrentProgram(290);
        h.value("seqrelease", 1.5f);
        h.value("seqfollowrelease", 1);
        h.p->setStateInformation(legacyPatch.getData(), int(legacyPatch.getSize()));
        for (const auto &id : addedParameters)
            require(std::abs(h.parameters.at(id)->getValue() - h.parameters.at(id)->getDefaultValue()) <
                        .00001f,
                    "Older patch inherited a new model instead of its legacy default: " + id);
        // Low sustain must not stretch release into seconds; changing sustain during
        // release must not restart or replace its calibrated rate.
        for (float sustain : {0.f, .001f, .02f, .3f, .8f, 1.f})
        {
            h.p->setCurrentProgram(0);
            h.value("attack", .001f);
            h.value("decay", 1);
            h.value("sustain", sustain);
            h.value("release", .08f);
            h.value("frelease", .08f);
            auto releaseCheck = h.render(.6, .8f, {60});
            energy(releaseCheck);
            require(norynPerformanceTelemetry(h.p.get()).size() == 0, "Low sustain caused a runaway release");
        }

        h.p->setCurrentProgram(0);
        h.value("attack", .001f);
        h.value("decay", 1);
        h.value("sustain", .5f);
        h.value("release", .08f);
        h.value("frelease", .08f);
        h.prepare();
        AudioBuffer<float> tailBlock(2, 256);
        MidiBuffer tailMidi;
        tailMidi.addEvent(MidiMessage::noteOn(1, 60, .8f), 0);
        h.p->processBlock(tailBlock, tailMidi);
        tailMidi.clear();
        tailMidi.addEvent(MidiMessage::noteOff(1, 60), 0);
        h.p->processBlock(tailBlock, tailMidi);
        tailMidi.clear();
        for (int i = 0; i < 50; ++i)
        {
            h.value("sustain", i % 2 == 0 ? 0.f : .001f);
            h.p->processBlock(tailBlock, tailMidi);
        }
        require(norynPerformanceTelemetry(h.p.get()).size() == 0,
                "Changing sustain during release created an endless tail");
        // A fast sequence with a long-release, near-zero-sustain patch must not accumulate voices.
        h.p->setCurrentProgram(0);
        h.value("attack", .001f);
        h.value("decay", 1);
        h.value("sustain", .001f);
        h.value("release", 4);
        h.value("seqrelease", .06f);
        h.value("seqfollowrelease", 0);
        h.value("bpm", 120);
        h.value("seqdivision", 2);
        h.value("seqgate", .25f);
        h.value("seqlength", 4);
        h.value("chorus", 0);
        h.value("delay", 0);
        h.value("reverb", 0);
        norynControl(
            h.p.get(),
            JSON::parse(
                "{\"op\":\"sequence\",\"args\":{\"replace\":true,\"steps\":[{\"index\":0,\"notes\":[60]},{"
                "\"index\":1,\"notes\":[62]},{\"index\":2,\"notes\":[64]},{\"index\":3,\"notes\":[67]}]}}"));
        norynControl(h.p.get(), JSON::parse("{\"op\":\"transport\",\"args\":{\"playing\":true}}"));
        auto loopAudio = h.render(3, .8f, {}, 48000, false);
        energy(loopAudio);
        require(norynPerformanceTelemetry(h.p.get()).size() == 0,
                "Sequence release tails accumulated between short gates");
        norynControl(h.p.get(), JSON::parse("{\"op\":\"transport\",\"args\":{\"playing\":false}}"));

        for (const auto &pair : h.parameters)
            if (pair.first.startsWith("seq") || pair.first == "bpm")
                pair.second->setValueNotifyingHost(pair.second->getDefaultValue());
        norynControl(h.p.get(),
                     JSON::parse("{\"op\":\"sequence\",\"args\":{\"replace\":true,\"steps\":[]}}"));
        // Verify actual mono/legato pitch behavior, not a single-note preset render.
        h.p->setCurrentProgram(0);
        h.value("voicemode", 2);
        h.value("glidetime", .2f);
        h.value("release", .03f);
        h.prepare();
        AudioBuffer<float> performanceAudio(2, 256);
        MidiBuffer performanceMidi;
        performanceMidi.addEvent(MidiMessage::noteOn(1, 60, .8f), 0);
        h.p->processBlock(performanceAudio, performanceMidi);
        performanceMidi.clear();
        performanceMidi.addEvent(MidiMessage::noteOn(1, 72, .8f), 0);
        h.p->processBlock(performanceAudio, performanceMidi);
        auto voices = norynPerformanceTelemetry(h.p.get());
        require(voices.size() == 1, "Mono legato allocated more than one voice");
        require(float(voices[0]["frequency"]) > 261 && float(voices[0]["frequency"]) < 300 &&
                    std::abs(float(voices[0]["target"]) - 523.251f) < .1f,
                "Portamento did not interpolate pitch");
        performanceMidi.clear();
        performanceMidi.addEvent(MidiMessage::noteOff(1, 72), 0);
        h.p->processBlock(performanceAudio, performanceMidi);
        voices = norynPerformanceTelemetry(h.p.get());
        require(voices.size() == 1 && std::abs(float(voices[0]["target"]) - 261.626f) < .1f,
                "Mono last-note return failed");
        performanceMidi.clear();
        performanceMidi.addEvent(MidiMessage::noteOff(1, 60), 0);
        h.p->processBlock(performanceAudio, performanceMidi);
        performanceMidi.clear();
        for (int i = 0; i < 30; ++i)
            h.p->processBlock(performanceAudio, performanceMidi);
        require(norynPerformanceTelemetry(h.p.get()).size() == 0, "Mono note-off left a stuck voice");
        h.p->setCurrentProgram(0);
        h.value("bendrange", 12);
        h.prepare();
        performanceMidi.clear();
        performanceMidi.addEvent(MidiMessage::noteOn(1, 60, .8f), 0);
        performanceMidi.addEvent(MidiMessage::pitchWheel(1, 16383), 1);
        h.p->processBlock(performanceAudio, performanceMidi);
        voices = norynPerformanceTelemetry(h.p.get());
        require(voices.size() == 1 && std::abs(float(voices[0]["bendRatio"]) - 2.f) < .001f,
                "Bend range was not applied");
        h.value("bendrange", 2);
        performanceMidi.clear();
        h.p->processBlock(performanceAudio, performanceMidi);
        voices = norynPerformanceTelemetry(h.p.get());
        require(std::abs(float(voices[0]["bendRatio"]) - std::exp2(2.f / 12)) < .001f,
                "Changing bend range ignored a held wheel");
        performanceMidi.clear();
        performanceMidi.addEvent(MidiMessage::pitchWheel(1, 8192), 0);
        h.p->processBlock(performanceAudio, performanceMidi);
        h.p->setCurrentProgram(0);
        if (argc > 2 && String(argv[2]) == "--solitude")
        {
            auto renderPerformance =
                [&](int program, int seconds, const std::vector<std::pair<double, MidiMessage>> &events)
            {
                h.p->setCurrentProgram(program);
                h.prepare();
                const int length = 48000 * seconds;
                AudioBuffer<float> result(2, length), block(2, 256);
                MidiBuffer midi;
                for (int offset = 0; offset < length; offset += 256)
                {
                    const int count = jmin(256, length - offset);
                    block.setSize(2, count, false, false, true);
                    midi.clear();
                    for (const auto &event : events)
                    {
                        int at = roundToInt(event.first * 48000);
                        if (at >= offset && at < offset + count)
                            midi.addEvent(event.second, at - offset);
                    }
                    h.p->processBlock(block, midi);
                    for (int c = 0; c < 2; ++c)
                        result.copyFrom(c, offset, block, c, 0, count);
                }
                require(energy(result) > .01, "Silent Solitude candidate");
                return result;
            };
            std::vector<std::pair<double, MidiMessage>> lead;
            int previous = -1;
            int notes[] = {67, 72, 70, 68, 65, 67};
            for (int i = 0; i < 6; ++i)
            {
                lead.push_back({i * 1.35, MidiMessage::noteOn(1, notes[i], .8f)});
                if (previous >= 0)
                    lead.push_back({i * 1.35 + .08, MidiMessage::noteOff(1, previous)});
                previous = notes[i];
            }
            lead.push_back({8.2, MidiMessage::noteOff(1, previous)});
            auto solo = renderPerformance(289, 12, lead);
            wav(directory.getChildFile("solitude-original-candidate.wav"), solo);
            std::vector<std::pair<double, MidiMessage>> chords;
            const int voicings[][4] = {{48, 55, 60, 63}, {44, 51, 56, 60}, {46, 53, 58, 62}};
            for (int i = 0; i < 3; ++i)
                for (int note : voicings[i])
                {
                    chords.push_back({i * 4.2, MidiMessage::noteOn(1, note, .8f)});
                    chords.push_back({i * 4.2 + 3.2, MidiMessage::noteOff(1, note)});
                }
            auto swell = renderPerformance(290, 17, chords);
            wav(directory.getChildFile("solitude-reinterpretation-candidate.wav"), swell);
            std::cout << "PASS: mono/legato/glide/note-off checks; rendered both Solitude candidates.\n";
            return 0;
        }

        auto started = Time::getMillisecondCounterHiRes();
        String csv = "parameter,normalized_low,normalized_high,relative_audio_difference\n";
        float minimumDifference = 100;
        String weakest;
        int audibleControls = 0;
        for (const auto &pair : h.parameters)
        {
            if (pair.first == "voicemode" || pair.first == "glidetime" || pair.first == "bendrange" ||
                pair.first.startsWith("seq") || pair.first == "bpm" || pair.first == "clicklevel" ||
                pair.first == "metronome" || pair.first == "scale" || pair.first == "root" ||
                pair.first == "customscale" || pair.first.startsWith("quantize"))
                continue;
            ++audibleControls;
            h.baseline(pair.first);
            pair.second->setValueNotifyingHost(.2f);
            auto a = h.render(1.25);
            h.baseline(pair.first);
            pair.second->setValueNotifyingHost(.8f);
            auto b = h.render(1.25);
            double difference = 0, denominator = energy(a) + energy(b);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < a.getNumSamples(); ++i)
                {
                    double d = a.getSample(c, i) - b.getSample(c, i);
                    difference += d * d;
                }
            float relative = float(std::sqrt(difference / jmax(1.e-12, denominator)));
            csv += pair.first + ",0.2,0.8," + String(relative, 6) + "\n";
            if (relative < minimumDifference)
            {
                minimumDifference = relative;
                weakest = pair.first;
            }
            std::cout << "Control " << pair.first << ": " << relative << std::endl;
            // Leave a report even on failure, to identify ineffective controls.
            directory.getChildFile("control-sensitivity.csv").replaceWithText(csv);
            require(relative > .035f, "Ineffective control: " + pair.first + " (" + String(relative) + ")");
        }
        for (const auto &pair : h.parameters)
        {
            auto *parameter = pair.second;
            for (float point : {.2f, .5f, .8f})
            {
                float canonical = parameter->convertTo0to1(parameter->convertFrom0to1(point));
                float roundTrip = parameter->getValueForText(parameter->getText(canonical, 64));
                require(std::abs(canonical - roundTrip) < .02f,
                        "Value/unit round trip failed: " + pair.first);
            }
        }
        std::set<int> testedEngines;
        std::set<String> names;
        double quietest = 1.e10, loudest = 0;
        std::set<String> signatureStates;
        String levels = "index,name,category,rms,peak\n";
        String catalog = "# NORYN factory bank\n\n290 factory sounds plus Init. All patches are "
                         "editable.\n\n192 original signature patches reference songs, artists and classic "
                         "synthesis palettes. They are interpretations, not extracted artist patches, "
                         "samples, exact recreations or endorsements. The original programs 0-96 retain "
                         "their numbers. Search the browser by name, song, artist or playing tip.\n\n\n\n";
        catalog += "## Signature demo guide\n\n`signature-demo.wav` auditions one patch from each new family "
                   "in three-second segments, using original notes/chords.\n\n";
        for (int group = 0; group < 16; ++group)
            catalog += "- " + String(group * 3) + "–" + String(group * 3 + 3) +
                       " s: " + h.p->getProgramName(97 + group * 12) + "\n";
        catalog += "\n## All programs\n\n";
        for (int i = 0; i < h.p->getNumPrograms(); ++i)
        {
            auto name = h.p->getProgramName(i);
            require(names.insert(name).second, "Duplicate preset name");
            h.p->setCurrentProgram(i);
            testedEngines.insert(
                int(h.parameters.at("engine")->convertFrom0to1(h.parameters.at("engine")->getValue())));
            const auto &preset = noryn::factoryPresets()[size_t(i)];
            for (const auto &edit : preset.values)
            {
                auto it = h.parameters.find(edit.first);
                require(it != h.parameters.end(), "Unknown preset parameter: " + String(edit.first));
                const auto range = it->second->getNormalisableRange();
                require(std::isfinite(edit.second) && edit.second >= range.start && edit.second <= range.end,
                        "Out-of-range preset parameter: " + name + " / " + edit.first);
            }
            if (i >= 97)
            {
                String signature;
                for (const auto &pair : h.parameters)
                    signature += pair.first + ":" + String(pair.second->getValue(), 7) + ";";
                require(signatureStates.insert(signature).second, "Duplicate signature sound: " + name);
                require(i >= 289 || preset.description.startsWith("Inspired by "),
                        "Missing signature reference");
            }
            auto sound =
                h.render(i >= 97 ? 2.5 : .9, .8f,
                         preset.category == "Basses" ? std::vector<int>{48} : std::vector<int>{48, 60});
            double rms = std::sqrt(energy(sound) / (sound.getNumSamples() * 2));
            require(rms > .00015, "Silent preset: " + name);
            quietest = jmin(quietest, rms);
            loudest = jmax(loudest, rms);
            double peak = sound.getMagnitude(0, sound.getNumSamples());
            if (i >= 97)
                require(rms < .35 && peak < .98, "Signature preset has excessive level: " + name);
            levels += String(i) + ",\"" + name + "\",\"" + preset.category + "\"," + String(rms, 6) + "," +
                      String(peak, 6) + "\n";
            catalog += "### " + String(i) + ": " + name + "\n\n**" + preset.category + "** — " +
                       preset.description + "\n\n";
            if (i % 12 == 0)
                std::cout << "Presets checked: " << i << std::endl;
        }
        directory.getChildFile("preset-levels.csv").replaceWithText(levels);
        require(testedEngines.size() == 5, "Factory sounds do not cover all five voice engines");
        directory.getChildFile("PRESETS.md").replaceWithText(catalog);
        const auto listed = norynControl(h.p.get(), JSON::parse("{\"op\":\"presets\"}"));
        require(listed["factory"].size() == 291, "MCP omitted new presets");
        require(listed["factory"][288]["description"].toString().contains("ODESZA"),
                "MCP omitted signature reference/description");
        require(h.p->getProgramName(1) == "Cinematic Horizon" &&
                    h.p->getProgramName(13) == "Rubber Circuit" &&
                    h.p->getProgramName(96) == "Orchestral Dream",
                "Legacy program numbering changed");
        h.p->setCurrentProgram(288);
        MemoryBlock signatureState;
        h.p->getStateInformation(signatureState);
        float signatureCutoff = h.parameters.at("cutoff")->getValue();
        h.p->setCurrentProgram(0);
        h.p->setStateInformation(signatureState.getData(), int(signatureState.getSize()));
        require(h.p->getCurrentProgram() == 288 &&
                    std::abs(h.parameters.at("cutoff")->getValue() - signatureCutoff) < .00001f,
                "New signature preset state recall failed");
        h.p->setCurrentProgram(43);
        MemoryBlock state;
        h.p->getStateInformation(state);
        std::vector<float> saved;
        for (auto *param : h.p->getParameters())
            saved.push_back(param->getValue());
        h.p->setCurrentProgram(2);
        h.p->setStateInformation(state.getData(), int(state.getSize()));
        require(h.p->getCurrentProgram() == 43, "Preset identity recall failed");
        for (size_t i = 0; i < saved.size(); ++i)
            require(std::abs(saved[i] - h.p->getParameters()[int(i)]->getValue()) < .0001f,
                    "Parameter recall failed");
        for (double sr : {44100., 48000., 96000.})
            for (bool high : {false, true})
            {
                h.prepare(sr, 512);
                for (auto *param : h.p->getParameters())
                    param->setValueNotifyingHost(high ? 1.f : 0.f);
                for (int size : {16, 64, 127, 512})
                {
                    AudioBuffer<float> block(2, size);
                    MidiBuffer midi;
                    for (int n = 45; n < 65; ++n)
                        midi.addEvent(MidiMessage::noteOn(1, n, .9f), 0);
                    midi.addEvent(MidiMessage::pitchWheel(1, 16000), size / 2);
                    midi.addEvent(MidiMessage::controllerEvent(1, 1, 120), size / 2);
                    h.p->processBlock(block, midi);
                    energy(block);
                }
            }
        norynControl(h.p.get(), JSON::parse("{\"op\":\"sequence\",\"args\":{\"steps\":[{\"index\":63,"
                                            "\"notes\":[60,64,67],\"length\":16}],\"replace\":true}}"));
        auto sequenceState = norynControl(h.p.get(), JSON::parse("{\"op\":\"state\"}"));
        require(sequenceState["steps"][63]["notes"].getArray()->size() == 3,
                "Native MCP sequence control failed");
        MemoryBlock patternSaved;
        h.p->getStateInformation(patternSaved);
        norynControl(h.p.get(),
                     JSON::parse("{\"op\":\"sequence\",\"args\":{\"steps\":[],\"replace\":true}}"));
        h.p->setStateInformation(patternSaved.getData(), int(patternSaved.getSize()));
        sequenceState = norynControl(h.p.get(), JSON::parse("{\"op\":\"state\"}"));
        require(sequenceState["steps"][63]["notes"].getArray()->size() == 3,
                "Full synth sequence recall failed");
        // With routing Off, changing the LFO rate must not change any sound at all.
        h.p->setCurrentProgram(0);
        h.value("attack", .004f);
        h.value("release", .15f);
        h.value("scan", .8f);
        h.value("pwmdepth", .8f);
        h.value("depth", 1);
        h.value("rate", .3f);
        for (int lfo = 2; lfo <= 4; ++lfo)
            h.value("lfo" + String(lfo) + "rate", .3f);
        auto offSlow = h.render(.6, .8f, {48}, 48000, true, 127);
        h.value("rate", 8);
        for (int lfo = 2; lfo <= 4; ++lfo)
            h.value("lfo" + String(lfo) + "rate", 8);
        auto offFast = h.render(.6, .8f, {48}, 48000, true, 127);
        double offDifference = 0;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < offSlow.getNumSamples(); ++i)
            {
                double d = offSlow.getSample(c, i) - offFast.getSample(c, i);
                offDifference += d * d;
            }
        require(offDifference < 1.e-8, "LFO Off still contains a hidden audio route");
        // Check actual GUI bindings rather than only parameter setters.
        h.p->setCurrentProgram(1);
        h.prepare();
        std::unique_ptr<AudioProcessorEditor> editor(h.p->createEditor());
        std::set<String> boundParameters;
        for (auto *child : editor->getChildren())
        {
            if (auto *slider = dynamic_cast<Slider *>(child))
            {
                boundParameters.insert(slider->getName());
                require(!slider->isDoubleClickReturnEnabled(), "Knob still has accidental reset enabled");
                require(slider->getRotaryParameters().stopAtEnd, "Knob wraps past minimum/maximum");
                require(slider->getMouseDragSensitivity() == 250, "Weak drag sensitivity");
                auto *param = h.parameters.at(slider->getName());
                slider->setValue(param->getNormalisableRange().start, sendNotificationSync);
                for (int check = 0; check < 3; ++check)
                    norynRefreshEditor(editor.get());
                require(std::abs(param->getValue()) < .001f &&
                            std::abs(slider->getValue() - param->getNormalisableRange().start) < .001,
                        "Knob minimum snapped back: " + slider->getName());
                float normalized = .67f;
                slider->setValue(param->convertFrom0to1(normalized), sendNotificationSync);
                float expected = param->convertTo0to1(float(slider->getValue()));
                require(std::abs(param->getValue() - expected) < .001f, "Unbound knob " + slider->getName());
            }
            if (auto *combo = dynamic_cast<ComboBox *>(child))
            {
                if (h.parameters.count(combo->getName()))
                {
                    boundParameters.insert(combo->getName());
                    int selected = jmin(2, combo->getNumItems() - 1);
                    combo->setSelectedItemIndex(selected, sendNotificationSync);
                    require(std::abs(h.parameters.at(combo->getName())
                                         ->convertFrom0to1(h.parameters.at(combo->getName())->getValue()) -
                                     selected) < .01f,
                            "Unbound selector");
                }
                if (combo->getName() == "categories")
                {
                    combo->setSelectedItemIndex(3, sendNotificationSync);
                    require(h.p->getCurrentProgram() == 13, "Category did not load its first bass patch");
                    combo->setSelectedItemIndex(0, sendNotificationSync);
                }
                if (combo->getName() == "presets")
                {
                    combo->setSelectedId(26, sendNotificationSync);
                    require(h.p->getCurrentProgram() == 25, "Preset browser binding failed");
                }
            }
        }
        for (auto *child : editor->getChildren())
            if (auto *toggle = dynamic_cast<ToggleButton *>(child))
                if (h.parameters.count(toggle->getName()))
                {
                    boundParameters.insert(toggle->getName());
                    toggle->setToggleState(true, sendNotificationSync);
                    require(h.parameters.at(toggle->getName())->getValue() > .5f, "Unbound toggle");
                    toggle->setToggleState(false, sendNotificationSync);
                }
        for (auto *child : editor->getChildren())
            if (auto *note = dynamic_cast<TextButton *>(child))
                if (note->getName() == "custom-note0")
                {
                    auto before = h.parameters.at("customscale")->getValue();
                    note->onClick();
                    require(h.parameters.at("customscale")->getValue() != before,
                            "Custom pitch-class control unbound");
                    boundParameters.insert("customscale");
                    break;
                }
        require(boundParameters.size() == h.parameters.size(), "A parameter has no visible control");
        auto button = [&](const String &name) -> TextButton *
        {
            for (auto *child : editor->getChildren())
                if (auto *candidate = dynamic_cast<TextButton *>(child))
                    if (candidate->getButtonText() == name)
                        return candidate;
            throw std::runtime_error("Missing button: " + name.toStdString());
        };
        button("INIT")->onClick();
        require(h.p->getCurrentProgram() == 0, "Init button failed");
        button(">")->onClick();
        require(h.p->getCurrentProgram() == 1, "Next button failed");
        button("<")->onClick();
        require(h.p->getCurrentProgram() == 0, "Previous button failed");
        auto *play = button("PLAY PATCH");
        play->setToggleState(true, dontSendNotification);
        play->onClick();
        AudioBuffer<float> previewAudio(2, 256);
        MidiBuffer previewMidi;
        double previewEnergy = 0;
        for (int i = 0; i < 32; ++i)
        {
            previewMidi.clear();
            h.p->processBlock(previewAudio, previewMidi);
            previewEnergy += energy(previewAudio);
        }
        require(previewEnergy > .001, "Audition button produced no audio");
        button("STOP")->onClick();
        double afterStop = 0;
        for (int i = 0; i < 4; ++i)
        {
            previewMidi.clear();
            h.p->processBlock(previewAudio, previewMidi);
            afterStop = energy(previewAudio);
        }
        require(afterStop < 1.e-10, "Stop button left sounding notes or tails");
        h.p->setCurrentProgram(0);
        h.value("cutoff", 700);
        h.value("lfo2dest", 1);
        h.value("lfo2rate", 3);
        h.value("lfo2amount", .7f);
        h.prepare();
        Slider *cutoffControl = nullptr;
        for (auto *child : editor->getChildren())
            if (auto *candidate = dynamic_cast<Slider *>(child))
                if (candidate->getName() == "cutoff")
                    cutoffControl = candidate;
        require(cutoffControl != nullptr, "Missing cutoff control");
        MidiBuffer modMidi;
        modMidi.addEvent(MidiMessage::noteOn(1, 60, .8f), 0);
        AudioBuffer<float> modAudio(2, 256);
        h.p->processBlock(modAudio, modMidi);
        norynRefreshEditor(editor.get());
        float indicatorBefore = float(cutoffControl->getProperties()["modulationValue"]);
        for (int block = 0; block < 20; ++block)
        {
            modMidi.clear();
            h.p->processBlock(modAudio, modMidi);
        }
        norynRefreshEditor(editor.get());
        float indicatorAfter = float(cutoffControl->getProperties()["modulationValue"]);
        require(bool(cutoffControl->getProperties()["showModulation"]) &&
                    std::abs(indicatorAfter - indicatorBefore) > 50,
                "Mapped LFO indicator did not move");
        require(std::abs(h.parameters.at("cutoff")->convertFrom0to1(h.parameters.at("cutoff")->getValue()) -
                         700) < .01,
                "LFO overwrote the manual knob value");
        // Exactly one selected destination must move; changing it must clear the previous destination.
        h.p->setCurrentProgram(0);
        h.value("depth", 1);
        h.value("scan", .8f);
        h.value("pwmdepth", .8f);
        h.value("lfo1amount", .8f);
        h.prepare();
        MidiBuffer routeMidi;
        routeMidi.addEvent(MidiMessage::noteOn(1, 60, .8f), 0);
        AudioBuffer<float> routeAudio(2, 256);
        h.p->processBlock(routeAudio, routeMidi);
        for (int destination : {1, 3, 6, 15, 0})
        {
            h.value("lfo1dest", float(destination));
            routeMidi.clear();
            h.p->processBlock(routeAudio, routeMidi);
            norynRefreshEditor(editor.get());
            for (auto *child : editor->getChildren())
                if (auto *knob = dynamic_cast<Slider *>(child))
                {
                    String id = knob->getName();
                    if (id == "cutoff" || id == "position" || id == "pulsewidth" || id=="pitchfine")
                    {
                        bool expected = destination == 1   ? id == "cutoff"
                                        : destination == 3 ? id == "position"
                                        : destination == 6 ? id == "pulsewidth"
                                        : destination == 15 ? id == "pitchfine"
                                                           : false;
                        require(bool(knob->getProperties()["showModulation"]) == expected,
                                "LFO routed/indicated the wrong destination: " + id);
                    }
                }
        }
        h.p->setCurrentProgram(1);
        h.prepare();
        norynRefreshEditor(editor.get());
        h.value("seqfollowrelease", 0);
        h.value("seqrelease", .08f);
        h.value("seqnotelength", 1);
        auto beforePlayback = editor->createComponentSnapshot(editor->getLocalBounds());
        auto live = h.render(.9, .8f, {48, 55, 60, 64});
        require(norynTelemetryActive(h.p.get()), "Missing audio telemetry");
        AudioBuffer<float> trace(2, 256);
        MidiBuffer traceMidi;
        h.p->processBlock(trace, traceMidi);
        require(norynScopeMatches(h.p.get(), trace), "LCD data does not match audio");
        norynRefreshEditor(editor.get());
        auto changedPixels = [](const Image &a, const Image &b, Rectangle<int> area)
        {
            int count = 0;
            for (int y = area.getY(); y < area.getBottom(); ++y)
                for (int x = area.getX(); x < area.getRight(); ++x)
                    if (a.getPixelAt(x, y) != b.getPixelAt(x, y))
                        ++count;
            return count;
        };
        auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        for (auto area : {Rectangle<int>(28, 153, 441, 112), Rectangle<int>(503, 153, 441, 112),
                          Rectangle<int>(978, 153, 434, 112), Rectangle<int>(28, 450, 326, 80),
                          Rectangle<int>(388, 450, 326, 80), Rectangle<int>(748, 450, 266, 80)})
            require(changedPixels(beforePlayback, image, area) == 0,
                    "A sound-design graph animated during playback");
        require(changedPixels(beforePlayback, image, {798, 704, 544, 97}) > 50,
                "Output scope did not update during playback");
        h.value("detune", -35);
        h.value("shape", .9f);
        h.value("pulsewidth", .2f);
        h.value("octaveb", 1);
        norynRefreshEditor(editor.get());
        auto changedB = editor->createComponentSnapshot(editor->getLocalBounds());
        require(changedPixels(image, changedB, {28, 153, 441, 112}) == 0, "OSC B changed the OSC A display");
        h.value("rate", 2);
        h.value("depth", .8f);
        norynRefreshEditor(editor.get());
        auto changedLfo = editor->createComponentSnapshot(editor->getLocalBounds());
        require(changedPixels(changedB, changedLfo, {748, 450, 266, 80}) > 50,
                "LFO graph ignored rate/depth knobs");
        h.p->setCurrentProgram(1);
        norynRefreshEditor(editor.get());

        TextEditor *search = nullptr;
        ComboBox *presetBox = nullptr;
        for (auto *child : editor->getChildren())
        {
            if (child->getName() == "presetSearch")
                search = dynamic_cast<TextEditor *>(child);
            if (child->getName() == "presets")
                presetBox = dynamic_cast<ComboBox *>(child);
        }
        require(search && presetBox, "Missing searchable preset browser");
        search->setText("Daft Punk");
        search->onTextChange();
        require(presetBox->getNumItems() >= 6, "Artist search omitted signature patches");
        search->setText("DAFT voyager");
        search->onTextChange();
        require(presetBox->getNumItems() == 1, "Case-insensitive multiword search failed");
        search->setText("unlikely-no-match-92847");
        search->onTextChange();
        require(presetBox->getNumItems() == 0, "Empty search results failed");
        search->clear();
        search->onTextChange();
        require(presetBox->getNumItems() >= 291, "Clearing search did not restore bank");
        h.p->setCurrentProgram(193); // House & Garage signature bank.
        norynRefreshEditor(editor.get());
        search->setText("Fingers");
        search->onTextChange();
        auto bankImage = editor->createComponentSnapshot(editor->getLocalBounds());
        auto bankFile = directory.getChildFile("signature-bank.png");
        bankFile.deleteFile();
        auto bankStream = bankFile.createOutputStream();
        PNGImageFormat bankPng;
        require(bankStream && bankPng.writeImageToStream(bankImage, *bankStream), "Bank screenshot failed");
        bankStream.reset();
        search->clear();
        search->onTextChange();
        h.p->setCurrentProgram(1);
        norynRefreshEditor(editor.get());
        auto imageFile = directory.getChildFile("preview.png");
        imageFile.deleteFile();
        auto stream = imageFile.createOutputStream();
        PNGImageFormat png;
        require(stream && png.writeImageToStream(image, *stream), "Preview write failed");
        stream.reset();
        for (const auto &page : std::vector<std::pair<String, String>>{{"256 STEP SEQUENCER", "sequencer.png"},
                                                                       {"4 LFO ROUTING", "lfos.png"},
                                                                       {"SCALE ENGINE", "scales.png"},
                                                                       {"VOICE DESIGN", "character.png"}})
        {
            if (page.first == "VOICE DESIGN")
                h.p->setCurrentProgram(205);
            button(page.first)->onClick();
            norynRefreshEditor(editor.get());
            auto pageImage = editor->createComponentSnapshot(editor->getLocalBounds());
            auto file = directory.getChildFile(page.second);
            file.deleteFile();
            auto out = file.createOutputStream();
            require(out && png.writeImageToStream(pageImage, *out), "Page preview write failed");
        }
        // The step grid follows the playhead onto later pages; choosing a page while playing pauses follow.
        {
            auto stepVisible = [&](int step)
            {
                for (auto *child : editor->getChildren())
                    if (child->getName() == "step" + String(step) && child->isVisible())
                        return true;
                return false;
            };
            auto follow = [&]() -> ToggleButton *
            {
                for (auto *child : editor->getChildren())
                    if (auto *candidate = dynamic_cast<ToggleButton *>(child))
                        if (candidate->getButtonText() == "FOLLOW PLAYHEAD")
                            return candidate;
                throw std::runtime_error("Missing follow toggle");
            };
            button("256 STEP SEQUENCER")->onClick();
            h.value("seqlength", 256);
            h.value("bpm", 300);
            h.value("seqdivision", 3); // 1/32 at 300 BPM: 1200 samples per step at 48 kHz
            h.prepare();
            auto *transport = button("PLAY");
            transport->setToggleState(true, dontSendNotification);
            transport->onClick();
            AudioBuffer<float> run(2, 256);
            MidiBuffer runMidi;
            for (int at = 0; at < 1200 * 200 + 600; at += 256)
            {
                runMidi.clear();
                h.p->processBlock(run, runMidi);
            }
            norynRefreshEditor(editor.get());
            require(follow()->getToggleState() && stepVisible(201) && !stepVisible(1),
                    "Step grid did not follow the playhead to steps 193-256");
            button("1-64")->onClick();
            norynRefreshEditor(editor.get());
            require(!follow()->getToggleState() && stepVisible(1),
                    "Choosing a page during playback did not pause follow");
            follow()->setToggleState(true, dontSendNotification);
            norynRefreshEditor(editor.get());
            require(stepVisible(201), "Re-enabled follow did not return to the playing page");
            button("STOP SEQ")->onClick();
            runMidi.clear();
            h.p->processBlock(run, runMidi);
            norynRefreshEditor(editor.get());
            button("1-64")->onClick();
            require(follow()->getToggleState() && stepVisible(1), "Stopped grid did not return to page one");
            h.value("seqlength", 64);
            h.value("bpm", 120);
            h.value("seqdivision", 2);
        }
        button("SYNTH")->onClick();
        editor->setSize(1008, 651);
        norynRefreshEditor(editor.get());
        auto small = editor->createComponentSnapshot(editor->getLocalBounds());
        auto smallFile = directory.getChildFile("preview-small.png");
        smallFile.deleteFile();
        auto smallStream = smallFile.createOutputStream();
        require(smallStream && png.writeImageToStream(small, *smallStream), "Small preview failed");
        smallStream.reset();
        editor.reset();
        h.p->setCurrentProgram(30);
        std::vector<int> dense;
        for (int n = 48; n < 64; ++n)
            dense.push_back(n);
        double benchStart = Time::getMillisecondCounterHiRes();
        auto denseChord = h.render(2, .7f, dense, 48000, false);
        energy(denseChord);
        double realtimeRatio = (Time::getMillisecondCounterHiRes() - benchStart) / 2000;
        require(realtimeRatio < .85, "Insufficient 16-voice real-time headroom");
        std::cout << "16-voice real-time ratio: " << realtimeRatio << std::endl;
        AudioBuffer<float> demo(2, 48000 * 24);
        demo.clear();
        int offset = 0;
        for (int program : {1, 13, 25, 37, 49, 61, 73, 85})
        {
            h.p->setCurrentProgram(program);
            auto sound =
                h.render(3, .8f, program == 13 ? std::vector<int>{48} : std::vector<int>{48, 55, 60});
            for (int c = 0; c < 2; ++c)
                demo.copyFrom(c, offset, sound, c, 0, sound.getNumSamples());
            offset += sound.getNumSamples();
        }
        wav(directory.getChildFile("demo.wav"), demo);
        AudioBuffer<float> signatureDemo(2, 48000 * 48);
        signatureDemo.clear();
        offset = 0;
        for (int group = 0; group < 16; ++group)
        {
            int program = 97 + group * 12;
            h.p->setCurrentProgram(program);
            auto sound = h.render(3, .72f,
                                  group < 4 ? std::vector<int>{48}
                                  : group == 4 || group == 6 || group == 7 || group == 15
                                      ? std::vector<int>{60}
                                      : std::vector<int>{48, 55, 62, 65});
            for (int c = 0; c < 2; ++c)
                signatureDemo.copyFrom(c, offset, sound, c, 0, sound.getNumSamples());
            offset += sound.getNumSamples();
        }
        wav(directory.getChildFile("signature-demo.wav"), signatureDemo);
        double elapsed = (Time::getMillisecondCounterHiRes() - started) / 1000;
        String report = "{\n  \"version\": \"0.7.0\",\n  \"factory_presets\": 290,\n  \"signature_presets\": "
                        "192,\n  \"reference_studies\": 2,\n  \"reference_likeness_verified\": false,\n  "
                        "\"unique_reference_states\": " +
                        String(int(signatureStates.size())) +
                        ",\n  \"tested_controls\": " + String(audibleControls) +
                        ",\n  \"weakest_control\": \"" + weakest +
                        "\",\n  \"minimum_relative_audio_difference\": " + String(minimumDifference, 6) +
                        ",\n  \"quietest_preset_rms\": " + String(quietest, 6) +
                        ",\n  \"loudest_preset_rms\": " + String(loudest, 6) +
                        ",\n  \"state_recall\": true,\n  \"gui_bindings\": true,\n  \"audio_telemetry\": "
                        "true,\n  \"low_sustain_release\": true,\n  \"per_note_sequence_gates\": true,\n  "
                        "\"five_minute_note_balance\": true,\n  \"stress_sample_rates\": [44100, 48000, "
                        "96000],\n  \"stress_block_sizes\": "
                        "[16, 64, 127, 512],\n  \"elapsed_seconds\": " +
                        String(elapsed, 2) +
                        ",\n  \"dense_16_voice_realtime_ratio\": " + String(realtimeRatio, 4) + "\n}\n";
        directory.getChildFile("validation.json").replaceWithText(report);
        std::cout << "PASS: " << audibleControls
                  << " audible synthesis controls, 290 presets, GUI bindings, live telemetry, state recall, "
                     "stress checks ("
                  << elapsed << " s)\n";
        h.p->releaseResources();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
}
