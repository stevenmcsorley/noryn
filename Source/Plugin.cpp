#include "ControlBridge.h"
#include "Engine.h"
#include "Presets.h"
#include "ScaleEngine.h"
#include "SequencePad.h"
#include "Sequencer.h"
using namespace juce;
using namespace noryn;
namespace
{
const Colour cyan(0xff5de7ed), amber(0xffffb569), ink(0xff090e12), text(0xffe3edf0), muted(0xff8ca5ae);
}
class NorynEditor;
class NorynProcessor : public AudioProcessor
{
  public:
    AudioProcessorValueTreeState state;
    const Tables &tables = sharedTables();
    Visuals visual;
    Synthesiser synth;
    MidiKeyboardState keyboard;
    dsp::Oversampling<float> oversampling{2, 1, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true,
                                          true};
    dsp::Chorus<float> chorus;
    Reverb reverb;
    AudioBuffer<float> delay;
    MidiBuffer scaledMidi, sequenceMidi, quantizedMidi;
    ScaleEngine scaleEngine;
    Sequencer sequencer;
    AudioBuffer<float> clickAudio;
    SmoothedValue<float> delayTime, master, width, delayWet, feedback;
    int delayIndex = 0;
    std::atomic<int> currentPreset{1};
    std::atomic<bool> panicRequested{false};
    std::vector<int> previewNotes;
    std::unique_ptr<ControlBridge> bridge;
    std::atomic<int> userLoaded{-1};
    NorynProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true)),
          state(*this, nullptr, "NORYN", layout())
    {
        for (int i = 0; i < 16; ++i)
            synth.addVoice(new Voice(state, tables, visual, i));
        synth.addSound(new Sound());
        synth.setNoteStealingEnabled(true);
        synth.setMinimumRenderingSubdivisionSize(1, true);
        scaledMidi.ensureSize(8192);
        sequenceMidi.ensureSize(8192);
        quantizedMidi.ensureSize(8192);
        setCurrentProgram(1);
        if (SystemStats::getEnvironmentVariable("NORYN_NO_MCP", {}) != "1")
            bridge = std::make_unique<ControlBridge>([this](const var &request)
                                                     { return handleControl(request); });
    }
    ~NorynProcessor() override
    {
        bridge.reset();
    }
    static AudioProcessorValueTreeState::ParameterLayout layout()
    {
        AudioProcessorValueTreeState::ParameterLayout p;
        auto add =
            [&](const char *id, const char *name, float lo, float hi, float def, bool logarithmic = false)
        {
            NormalisableRange<float> range(lo, hi);
            if (logarithmic)
                range = NormalisableRange<float>(
                    lo, hi, [](float a, float b, float t) { return a * std::pow(b / a, t); },
                    [](float a, float b, float x) { return std::log(x / a) / std::log(b / a); });
            auto attrs =
                AudioParameterFloatAttributes()
                    .withStringFromValueFunction(
                        [key = String(id)](float value, int)
                        {
                            if (key == "fmratio")
                                return String(value, 2) + " x";
                            if (key == "seqnotelength")
                                return String(value, 2) + " steps";
                            if (key == "bpm")
                                return String(value, 1) + " BPM";
                            if (key == "cutoff")
                                return value >= 1000 ? String(value / 1000, 2) + " kHz"
                                                     : String(roundToInt(value)) + " Hz";
                            if (key == "attack" || key == "decay" || key == "release" || key == "fattack" ||
                                key == "fdecay" || key == "frelease" || key == "delaytime")
                                return value < 1 ? String(roundToInt(value * 1000)) + " ms"
                                                 : String(value, 2) + " s";
                            if (key == "rate" || key == "chorusrate" || key.endsWith("rate"))
                                return String(value, 2) + " Hz";
                            if (key == "detune")
                                return String(value, 1) + " ct";
                            return String(value * 100, 1) + " %";
                        })
                    .withValueFromStringFunction(
                        [key = String(id)](const String &str)
                        {
                            float value = str.getFloatValue();
                            if (key == "cutoff")
                                return str.containsIgnoreCase("k") ? value * 1000 : value;
                            bool seconds = key == "attack" || key == "decay" || key == "release" ||
                                           key == "fattack" || key == "fdecay" || key == "frelease" ||
                                           key == "delaytime";
                            if (seconds)
                                return str.containsIgnoreCase("ms") ? value / 1000 : value;
                            if (key == "rate" || key == "chorusrate" || key.endsWith("rate") ||
                                key == "detune" || key == "bpm" || key == "seqnotelength" || key == "fmratio")
                                return value;
                            return str.contains("%") ? value / 100 : value;
                        });
            p.add(std::make_unique<AudioParameterFloat>(ParameterID(id, 1), name, range, def, attrs));
        };
        p.add(std::make_unique<AudioParameterChoice>(
            ParameterID("engine", 1), "Voice Engine",
            StringArray{"Hybrid WT", "FM Operator", "Plucked String", "Drawbar Organ", "Analog Stack"}, 0));
        add("fmratio", "FM Ratio", .5f, 12, 2);
        add("pan", "Voice Pan", -1, 1, 0);
        add("position", "WT Position", 0, 1, .6f);
        add("scan", "WT Spread", 0, 1, .1f);
        add("balance", "A / B Balance", 0, 1, .5f);
        add("detune", "Osc B Detune", -50, 50, 7);
        add("shape", "Saw / Pulse", 0, 1, .4f);
        add("sub", "Sub Level", 0, 1, .2f);
        add("cutoff", "Filter Cutoff", 20, 20000, 3200, true);
        add("resonance", "Resonance", 0, 1, .2f);
        add("drive", "Filter Drive", 0, 1, .1f);
        add("attack", "Amp Attack", .001f, 8, .1f, true);
        add("decay", "Amp Decay", .01f, 8, .8f, true);
        add("sustain", "Amp Sustain", 0, 1, .75f);
        add("release", "Amp Release", .01f, 12, 1.5f, true);
        add("rate", "LFO Rate", .03f, 30, .3f, true);
        add("depth", "LFO 1 Source Gain", 0, 1, 1);
        add("chorus", "Chorus Mix", 0, 1, .3f);
        add("delay", "Delay Mix", 0, 1, .15f);
        add("reverb", "Reverb Mix", 0, 1, .3f);
        add("size", "Reverb Size", 0, 1, .75f);
        add("master", "Output", 0, 1, .72f);
        p.add(std::make_unique<AudioParameterChoice>(ParameterID("family", 1), "Wavetable Family",
                                                     StringArray{"Classic", "Spectral", "Formant", "Digital"},
                                                     0));
        p.add(std::make_unique<AudioParameterInt>(ParameterID("octavea", 1), "Osc A Octave", -2, 2, 0));
        p.add(std::make_unique<AudioParameterInt>(ParameterID("octaveb", 1), "Osc B Octave", -2, 2, 0));
        add("unison", "Stereo Unison", 0, 1, .4f);
        add("noise", "Noise Level", 0, 1, 0);
        add("pulsewidth", "Pulse Width", .05f, .95f, .5f);
        add("pwmdepth", "Pulse Edge", 0, 1, .85f);
        add("fm", "Phase Modulation", 0, 1, 0);
        add("envamount", "Filter Envelope", -1, 1, .3f);
        add("keytrack", "Filter Key Tracking", 0, 1, .3f);
        add("velfilter", "Velocity > Filter", 0, 1, .25f);
        add("fattack", "Filter Attack", .001f, 8, .01f, true);
        add("fdecay", "Filter Decay", .01f, 8, .6f, true);
        add("fsustain", "Filter Sustain", 0, 1, .4f);
        add("frelease", "Filter Release", .01f, 12, 1.2f, true);
        p.add(std::make_unique<AudioParameterChoice>(
            ParameterID("filtermode", 1), "Filter Mode",
            StringArray{"Low pass 12 dB", "Low pass 24 dB", "High pass", "Band pass"}, 0));
        p.add(std::make_unique<AudioParameterChoice>(ParameterID("lfoshape", 1), "LFO Shape",
                                                     StringArray{"Sine", "Triangle", "Saw", "Square"}, 0));
        add("chorusrate", "Chorus Rate", .05f, 4, .3f, true);
        add("delaytime", "Delay Time", .04f, 1.2f, .32f, true);
        add("feedback", "Delay Feedback", 0, .86f, .35f);
        add("width", "Stereo Width", 0, 2, 1.2f);
        const StringArray destinations{
            "Off",         "Filter cutoff", "Resonance", "WT position",      "A / B mix",
            "Saw / pulse", "Pulse width",   "B detune",  "Phase modulation", "Stereo unison",
            "Drive",       "Noise",         "Sub level", "Amplitude",        "Pan"};
        for (int n = 1; n <= 4; ++n)
        {
            String prefix = "lfo" + String(n);
            if (n > 1)
            {
                add((prefix + "rate").toRawUTF8(), ("LFO " + String(n) + " Rate").toRawUTF8(), .03f, 30,
                    n == 2   ? .5f
                    : n == 3 ? .13f
                             : 2.f,
                    true);
                p.add(std::make_unique<AudioParameterChoice>(
                    ParameterID(prefix + "shape", 1), "LFO " + String(n) + " Shape",
                    StringArray{"Sine", "Triangle", "Saw", "Square"}, 0));
            }
            p.add(std::make_unique<AudioParameterChoice>(
                ParameterID(prefix + "dest", 1), "LFO " + String(n) + " Destination", destinations, 0));
            add((prefix + "amount").toRawUTF8(), ("LFO " + String(n) + " Amount").toRawUTF8(), -1, 1, .5f);
        }
        add("bpm", "Sequencer BPM", 30, 300, 120);
        add("seqgate", "Step Gate", .05f, .95f, .7f);
        add("seqvelocity", "Step Velocity", .1f, 1, .8f);
        add("seqnotelength", "Entry Length in Steps", .05f, 64, 1, true);
        add("clicklevel", "Metronome Level", 0, 1, .4f);
        p.add(std::make_unique<AudioParameterInt>(ParameterID("seqlength", 1), "Sequence Length", 1, 64, 64));
        p.add(std::make_unique<AudioParameterInt>(ParameterID("seqnote", 1), "Step Entry Note", 0, 127, 60));
        p.add(std::make_unique<AudioParameterChoice>(ParameterID("seqdivision", 1), "Step Division",
                                                     StringArray{"1/4", "1/8", "1/16", "1/32"}, 2));
        p.add(std::make_unique<AudioParameterBool>(ParameterID("metronome", 1), "Metronome", false));
        p.add(std::make_unique<AudioParameterChoice>(ParameterID("scale", 1), "Scale", ScaleEngine::names(),
                                                     0));
        p.add(std::make_unique<AudioParameterChoice>(
            ParameterID("root", 1), "Scale Root",
            StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
        p.add(std::make_unique<AudioParameterChoice>(ParameterID("quantizedirection", 1),
                                                     "Quantize Direction",
                                                     StringArray{"Nearest", "Up", "Down"}, 0));
        p.add(std::make_unique<AudioParameterBool>(ParameterID("quantizelive", 1), "Quantize Live MIDI",
                                                   false));
        p.add(std::make_unique<AudioParameterInt>(ParameterID("customscale", 1), "Custom Scale Mask", 1, 4095,
                                                  ScaleEngine::mask({0, 2, 4, 5, 7, 9, 11})));
        return p;
    }
    float v(const char *id) const
    {
        return state.getRawParameterValue(id)->load();
    }
    void setValue(const char *id, float value, bool gesture = false)
    {
        if (auto *parameter = state.getParameter(id))
        {
            if (gesture)
                parameter->beginChangeGesture();
            auto range = parameter->getNormalisableRange();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(jlimit(range.start, range.end, value)));
            if (gesture)
                parameter->endChangeGesture();
        }
    }
    void initPatch()
    {
        setCurrentProgram(0);
    }
    void preview(bool play)
    {
        for (int note : previewNotes)
            keyboard.noteOff(1, note, 0);
        previewNotes.clear();
        if (play)
        {
            auto category = factoryPresets()[size_t(currentPreset.load())].category;
            previewNotes = category == "Basses"                          ? std::vector<int>{48}
                           : category == "Leads" || category == "Plucks" ? std::vector<int>{60}
                                                                         : std::vector<int>{48, 55, 60, 64};
            for (int note : previewNotes)
                keyboard.noteOn(1, note, .8f);
        }
    }
    void prepareToPlay(double sr, int block) override
    {
        synth.allNotesOff(0, false);
        sequencer.reset();
        scaleEngine.reset();
        clickAudio.setSize(1, jmax(1, block));
        keyboard.reset();
        for (auto &wheel : visual.wheel)
            wheel.store(0);
        visual.voices.store(0);
        synth.setCurrentPlaybackSampleRate(sr * 2);
        visual.sampleRate.store(float(sr));
        oversampling.initProcessing(size_t(jmax(1, block)));
        oversampling.reset();
        setLatencySamples(int(std::round(oversampling.getLatencyInSamples())));
        chorus.setRate(v("chorusrate"));
        chorus.setMix(v("chorus") * .7f);
        chorus.setDepth(.65f);
        chorus.setCentreDelay(9.f);
        chorus.setFeedback(.18f);
        chorus.prepare({sr, (uint32)jmax(1, block), 2});
        Reverb::Parameters initialReverb;
        initialReverb.roomSize = v("size");
        initialReverb.damping = .4f;
        initialReverb.wetLevel = v("reverb") * .65f;
        initialReverb.dryLevel = 1 - v("reverb") * .3f;
        initialReverb.width = 1;
        reverb.setParameters(initialReverb);
        reverb.setSampleRate(sr);
        reverb.reset();
        delay.setSize(2, int(sr * 1.25) + 2);
        delay.clear();
        delayIndex = 0;
        for (auto *smoother : {&delayTime, &master, &width, &delayWet, &feedback})
            smoother->reset(sr, .025);
        delayTime.setCurrentAndTargetValue(v("delaytime") * float(sr));
        master.setCurrentAndTargetValue(v("master"));
        width.setCurrentAndTargetValue(v("width"));
        delayWet.setCurrentAndTargetValue(v("delay"));
        feedback.setCurrentAndTargetValue(v("feedback"));
    }
    void releaseResources() override
    {
        synth.allNotesOff(0, false);
    }
    bool isBusesLayoutSupported(const BusesLayout &b) const override
    {
        return b.getMainOutputChannelSet() == AudioChannelSet::stereo();
    }
    void processBlock(AudioBuffer<float> &buffer, MidiBuffer &midi) override
    {
        ScopedNoDenormals noDenormals;
        buffer.clear();
        if (buffer.getNumSamples() == 0)
            return;
        keyboard.processNextMidiBuffer(midi, 0, buffer.getNumSamples(), true);
        if (panicRequested.exchange(false))
        {
            synth.allNotesOff(0, false);
            sequencer.playing.store(false);
            sequencer.recording.store(false);
            sequencer.reset();
            scaleEngine.reset();
            keyboard.reset();
            oversampling.reset();
            chorus.reset();
            reverb.reset();
            delay.clear();
        }
        sequencer.process(midi, sequenceMidi, clickAudio, buffer.getNumSamples(), getSampleRate(), v("bpm"),
                          int(v("seqlength")), int(v("seqdivision")), v("seqgate"), v("metronome") > .5f,
                          v("clicklevel"));
        scaleEngine.process(sequenceMidi, quantizedMidi, int(v("scale")), int(v("root")),
                            int(v("quantizedirection")), int(v("customscale")), v("quantizelive") > .5f);
        scaledMidi.clear();
        for (const auto metadata : quantizedMidi)
        {
            const auto msg = metadata.getMessage();
            if (msg.getChannel() == 16)
                keyboard.processNextMidiEvent(msg);
            scaledMidi.addEvent(msg, metadata.samplePosition * 2);
            if (msg.isController() && msg.getControllerNumber() == 1)
                visual.wheel[msg.getChannel() - 1].store(msg.getControllerValue() / 127.f);
        }
        dsp::AudioBlock<float> base(buffer);
        auto up = oversampling.processSamplesUp(base);
        float *channels[] = {up.getChannelPointer(0), up.getChannelPointer(1)};
        AudioBuffer<float> high(channels, 2, int(up.getNumSamples()));
        high.clear();
        synth.renderNextBlock(high, scaledMidi, 0, high.getNumSamples());
        oversampling.processSamplesDown(base);
        chorus.setRate(v("chorusrate"));
        chorus.setMix(v("chorus") * .7f);
        dsp::ProcessContextReplacing<float> context(base);
        chorus.process(context);
        float sr = float(getSampleRate());
        delayTime.setTargetValue(v("delaytime") * sr);
        master.setTargetValue(v("master"));
        width.setTargetValue(v("width"));
        delayWet.setTargetValue(v("delay"));
        feedback.setTargetValue(v("feedback"));
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float read = float(delayIndex) - delayTime.getNextValue();
            if (read < 0)
                read += float(delay.getNumSamples());
            int a = int(read), b = (a + 1) % delay.getNumSamples();
            float frac = read - a;
            float tap[2];
            for (int c = 0; c < 2; ++c)
                tap[c] = delay.getSample(c, a) + frac * (delay.getSample(c, b) - delay.getSample(c, a));
            float fb = feedback.getNextValue(), mix = delayWet.getNextValue();
            for (int c = 0; c < 2; ++c)
            {
                float dry = buffer.getSample(c, i);
                delay.setSample(c, delayIndex, std::tanh(dry + tap[1 - c] * fb));
                buffer.setSample(c, i, dry + tap[c] * mix * .8f);
            }
            delayIndex = (delayIndex + 1) % delay.getNumSamples();
        }
        Reverb::Parameters rp;
        rp.roomSize = v("size");
        rp.damping = .4f;
        rp.wetLevel = v("reverb") * .65f;
        rp.dryLevel = 1 - v("reverb") * .3f;
        rp.width = 1;
        reverb.setParameters(rp);
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
        float peakL = 0, peakR = 0;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float l = buffer.getSample(0, i), r = buffer.getSample(1, i), mid = (l + r) * .5f,
                  side = (l - r) * .5f * width.getNextValue(), gain = master.getNextValue();
            l = .98f * std::tanh(((mid + side + clickAudio.getSample(0, i)) * gain) / .98f);
            r = .98f * std::tanh(((mid - side + clickAudio.getSample(0, i)) * gain) / .98f);
            buffer.setSample(0, i, l);
            buffer.setSample(1, i, r);
            visual.left.push(l);
            visual.right.push(r);
            peakL = jmax(peakL, std::abs(l));
            peakR = jmax(peakR, std::abs(r));
        }
        visual.left.publish();
        visual.right.publish();
        visual.peakL.store(peakL);
        visual.peakR.store(peakR);
        int voices = 0;
        for (int i = 0; i < synth.getNumVoices(); ++i)
            if (synth.getVoice(i)->isVoiceActive())
                ++voices;
        int selected = visual.selectedVoice.load();
        if (voices > 0 && (selected < 0 || !synth.getVoice(selected)->isVoiceActive()))
            for (int voice = synth.getNumVoices() - 1; voice >= 0; --voice)
                if (synth.getVoice(voice)->isVoiceActive())
                {
                    visual.selectedVoice.store(voice);
                    break;
                }
        visual.voices.store(voices);
    }
    File userPresetDirectory() const
    {
        auto dir = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("NORYN/presets");
        dir.createDirectory();
        return dir;
    }
    Array<File> userPresetFiles() const
    {
        auto files = userPresetDirectory().findChildFiles(File::findFiles, false, "*.noryn");
        files.sort();
        return files;
    }
    void saveUserPreset(const String &name)
    {
        String safe = File::createLegalFileName(name).substring(0, 80);
        if (safe.isEmpty())
            throw std::runtime_error("Preset name required");
        MemoryBlock data;
        getStateInformation(data);
        auto file = userPresetDirectory().getChildFile(safe + ".noryn");
        if (!file.replaceWithData(data.getData(), data.getSize()))
            throw std::runtime_error("Could not save preset");
        userLoaded.store(userPresetFiles().indexOf(file));
    }
    void loadUserPreset(int index)
    {
        auto files = userPresetFiles();
        if (index < 0 || index >= files.size())
            throw std::runtime_error("Unknown user preset");
        MemoryBlock data;
        files[index].loadFileAsData(data);
        setStateInformation(data.getData(), int(data.getSize()));
        userLoaded.store(index);
    }
    var controlState()
    {
        auto *result = new DynamicObject();
        result->setProperty("instanceId", bridge ? bridge->id : String());
        result->setProperty("name", "NORYN");
        result->setProperty("version", "0.3.0");
        result->setProperty("factoryPreset", getCurrentProgram());
        result->setProperty("presetName", getProgramName(getCurrentProgram()));
        result->setProperty("playing", sequencer.playing.load());
        result->setProperty("recording", sequencer.recording.load());
        result->setProperty("playhead", sequencer.playhead.load());
        auto *params = new DynamicObject();
        for (auto *item : getParameters())
        {
            auto *param = dynamic_cast<RangedAudioParameter *>(item);
            if (!param)
                continue;
            auto *info = new DynamicObject();
            info->setProperty("name", param->getName(64));
            info->setProperty("value", param->convertFrom0to1(param->getValue()));
            info->setProperty("min", param->getNormalisableRange().start);
            info->setProperty("max", param->getNormalisableRange().end);
            info->setProperty("text", param->getText(param->getValue(), 64));
            if (auto *choice = dynamic_cast<AudioParameterChoice *>(param))
            {
                Array<var> choices;
                for (const auto &label : choice->choices)
                    choices.add(label);
                info->setProperty("choices", var(choices));
            }
            params->setProperty(param->paramID, var(info));
        }
        result->setProperty("parameters", var(params));
        Array<var> steps;
        for (int i = 0; i < 64; ++i)
        {
            auto *cell = new DynamicObject();
            cell->setProperty("index", i);
            Array<var> notes;
            for (int n = 0; n < 128; ++n)
                if (sequencer.steps[i].has(n))
                    notes.add(n);
            cell->setProperty("notes", var(notes));
            cell->setProperty("velocity", sequencer.steps[i].velocity.load());
            cell->setProperty("length", sequencer.steps[i].length.load());
            steps.add(var(cell));
        }
        result->setProperty("steps", var(steps));
        return var(result);
    }
    var handleControl(const var &request)
    {
        String op = request["op"].toString();
        const var args = request["args"];
        auto result = [&] { return controlState(); };
        if (op == "state")
            return result();
        if (op == "presets")
        {
            Array<var> factory, users;
            for (int i = 0; i < getNumPrograms(); ++i)
            {
                auto *item = new DynamicObject();
                item->setProperty("index", i);
                item->setProperty("name", getProgramName(i));
                item->setProperty("category", factoryPresets()[size_t(i)].category);
                factory.add(var(item));
            }
            for (const auto &file : userPresetFiles())
                users.add(file.getFileNameWithoutExtension());
            auto *item = new DynamicObject();
            item->setProperty("factory", var(factory));
            item->setProperty("user", var(users));
            return var(item);
        }
        if (op == "setParameters")
        {
            auto *values = args["values"].getDynamicObject();
            if (!values)
                throw std::runtime_error("values object required");
            for (const auto &pair : values->getProperties())
            {
                auto *param = state.getParameter(pair.name.toString());
                if (!param)
                    throw std::runtime_error("Unknown parameter: " + pair.name.toString().toStdString());
                double value = double(pair.value);
                auto range = param->getNormalisableRange();
                if (!std::isfinite(value) || value < range.start - 1.e-6 * jmax(1.f, std::abs(range.start)) ||
                    value > range.end + 1.e-6 * jmax(1.f, std::abs(range.end)))
                    throw std::runtime_error("Parameter outside range: " +
                                             pair.name.toString().toStdString());
            }
            for (const auto &pair : values->getProperties())
                setValue(pair.name.toString().toRawUTF8(), float(pair.value), true);
            return result();
        }
        if (op == "loadPreset")
        {
            int index = int(args["index"]);
            if (index < 0 || index >= getNumPrograms())
                throw std::runtime_error("Preset index out of range");
            setCurrentProgram(index);
            return result();
        }
        if (op == "savePreset")
        {
            saveUserPreset(args["name"].toString());
            return handleControl(JSON::parse("{\"op\":\"presets\"}"));
        }
        if (op == "loadUserPreset")
        {
            String name = args["name"].toString();
            auto files = userPresetFiles();
            for (int i = 0; i < files.size(); ++i)
                if (files[i].getFileNameWithoutExtension() == name)
                {
                    loadUserPreset(i);
                    return result();
                }
            throw std::runtime_error("User preset not found");
        }
        if (op == "sequence" || op == "progression")
        {
            auto *entries = args["steps"].getArray();
            if (!entries)
                throw std::runtime_error("steps array required");
            for (const auto &entry : *entries)
            {
                int index = int(entry["index"]);
                auto *notes = entry["notes"].getArray();
                if (index < 0 || index > 63 || !notes || notes->size() > 16)
                    throw std::runtime_error("Invalid step");
                for (const auto &note : *notes)
                    if (int(note) < 0 || int(note) > 127)
                        throw std::runtime_error("Invalid MIDI note");
            }
            if (bool(args.getProperty("replace", true)))
                sequencer.clear();
            for (const auto &entry : *entries)
            {
                int index = int(entry["index"]);
                auto &cell = sequencer.steps[size_t(index)];
                cell.clear();
                for (const auto &note : *entry["notes"].getArray())
                    cell.add(int(note), jlimit(.01f, 1.f, float(entry.getProperty("velocity", .8f))));
                cell.length.store(jlimit(.05f, 64.f, float(entry.getProperty("length", 1.f))));
            }
            return result();
        }
        if (op == "transport")
        {
            if (args.hasProperty("bpm"))
                setValue("bpm", jlimit(30.f, 300.f, float(args["bpm"])), true);
            if (args.hasProperty("playing"))
                sequencer.playing.store(bool(args["playing"]));
            if (args.hasProperty("recording"))
                sequencer.recording.store(bool(args["recording"]));
            if (args.hasProperty("stepRecord"))
                sequencer.stepRecord.store(bool(args["stepRecord"]));
            if (args.hasProperty("metronome"))
                setValue("metronome", bool(args["metronome"]) ? 1.f : 0.f, true);
            return result();
        }
        if (op == "scale")
        {
            setValue("scale", jlimit(0.f, float(ScaleEngine::scales().size() - 1), float(args["scale"])),
                     true);
            if (args.hasProperty("root"))
                setValue("root", jlimit(0.f, 11.f, float(args["root"])), true);
            if (args.hasProperty("direction"))
                setValue("quantizedirection", jlimit(0.f, 2.f, float(args["direction"])), true);
            if (args.hasProperty("live"))
                setValue("quantizelive", bool(args["live"]) ? 1.f : 0.f, true);
            if (args.hasProperty("mask"))
                setValue("customscale", jlimit(1.f, 4095.f, float(args["mask"])), true);
            return result();
        }
        if (op == "lfo")
        {
            int lfo = jlimit(1, 4, int(args["lfo"]));
            String prefix = "lfo" + String(lfo);
            if (args.hasProperty("rate"))
                setValue((lfo == 1 ? String("rate") : prefix + "rate").toRawUTF8(),
                         jlimit(.03f, 30.f, float(args["rate"])), true);
            if (args.hasProperty("shape"))
                setValue((lfo == 1 ? String("lfoshape") : prefix + "shape").toRawUTF8(),
                         jlimit(0.f, 3.f, float(args["shape"])), true);
            if (args.hasProperty("destination"))
                setValue((prefix + "dest").toRawUTF8(), jlimit(0.f, 14.f, float(args["destination"])), true);
            if (args.hasProperty("amount"))
                setValue((prefix + "amount").toRawUTF8(), jlimit(-1.f, 1.f, float(args["amount"])), true);
            return result();
        }
        if (op == "panic")
        {
            sequencer.playing.store(false);
            sequencer.recording.store(false);
            preview(false);
            panicRequested.store(true);
            return result();
        }
        throw std::runtime_error("Unknown operation");
    }
    const String getName() const override
    {
        return "NORYN";
    }
    bool acceptsMidi() const override
    {
        return true;
    }
    bool producesMidi() const override
    {
        return false;
    }
    double getTailLengthSeconds() const override
    {
        return 30;
    }
    bool hasEditor() const override
    {
        return true;
    }
    AudioProcessorEditor *createEditor() override;
    int getNumPrograms() override
    {
        return int(factoryPresets().size());
    }
    int getCurrentProgram() override
    {
        return currentPreset.load();
    }
    void setCurrentProgram(int index) override
    {
        index = jlimit(0, getNumPrograms() - 1, index);
        for (auto *parameter : getParameters())
        {
            auto *named = dynamic_cast<AudioProcessorParameterWithID *>(parameter);
            if (named && (named->paramID.startsWith("seq") || named->paramID == "bpm" ||
                          named->paramID == "metronome" || named->paramID == "clicklevel" ||
                          named->paramID == "scale" || named->paramID == "root" ||
                          named->paramID.startsWith("quantize") || named->paramID == "customscale"))
                continue;
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
        }
        for (const auto &pair : factoryPresets()[size_t(index)].values)
            setValue(pair.first, pair.second);
        currentPreset.store(index);
        userLoaded.store(-1);
    }
    const String getProgramName(int index) override
    {
        return factoryPresets()[size_t(jlimit(0, getNumPrograms() - 1, index))].name;
    }
    void changeProgramName(int, const String &) override {}
    void getStateInformation(MemoryBlock &b) override
    {
        auto tree = state.copyState();
        tree.setProperty("factoryPreset", currentPreset.load(), nullptr);
        tree.setProperty("routingVersion", 2, nullptr);
        tree.setProperty("userPresetIndex", userLoaded.load(), nullptr);
        auto old = tree.getChildWithName("Sequence64");
        if (old.isValid())
            tree.removeChild(old, nullptr);
        tree.addChild(sequencer.save(), -1, nullptr);
        auto xml = tree.createXml();
        copyXmlToBinary(*xml, b);
    }
    void setStateInformation(const void *d, int n) override
    {
        auto xml = getXmlFromBinary(d, n);
        if (xml && xml->hasTagName("NORYN"))
        {
            auto tree = ValueTree::fromXml(*xml);
            currentPreset.store(jlimit(0, getNumPrograms() - 1, int(tree.getProperty("factoryPreset", 0))));
            userLoaded.store(int(tree.getProperty("userPresetIndex", -1)));
            sequencer.restore(tree.getChildWithName("Sequence64"));
            bool legacyRouting = int(tree.getProperty("routingVersion", 0)) < 2;
            state.replaceState(tree);
            if (legacyRouting)
                setValue("depth", 1);
        }
    }
};
class HardwareLook : public LookAndFeel_V4
{
  public:
    HardwareLook()
    {
        setColour(Slider::textBoxTextColourId, Colour(0xffbed0d4));
        setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour(Slider::textBoxBackgroundColourId, Colour(0xff0c1114));
    }
    void drawRotarySlider(Graphics &g, int x, int y, int w, int h, float position, float start, float end,
                          Slider &s) override
    {
        auto r = Rectangle<float>(float(x), float(y), float(w), float(h)).reduced(7);
        float d = jmin(r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre(d, d);
        auto c = r.getCentre();
        float radius = d * .5f, angle = start + position * (end - start);
        auto accent = s.findColour(Slider::rotarySliderFillColourId);
        g.setColour(Colours::black.withAlpha(.6f));
        g.fillEllipse(r.translated(1, 3).expanded(3));
        g.setGradientFill(ColourGradient(Colour(0xff465156), c.x, c.y - radius, Colour(0xff101517), c.x,
                                         c.y + radius, false));
        g.fillEllipse(r);
        g.setColour(Colour(0xff647075));
        g.drawEllipse(r, 1);
        g.setColour(Colour(0xff090d0f));
        g.fillEllipse(r.reduced(5));
        Path track;
        track.addCentredArc(c.x, c.y, radius + 3, radius + 3, 0, start, end, true);
        g.setColour(Colour(0xff263438));
        g.strokePath(track, PathStrokeType(2.5f));
        Path arc;
        arc.addCentredArc(c.x, c.y, radius + 3, radius + 3, 0, start, angle, true);
        g.setColour(accent.withAlpha(.15f));
        g.strokePath(arc, PathStrokeType(8));
        g.setColour(accent);
        g.strokePath(arc, PathStrokeType(2.5f));
        if (bool(s.getProperties()["showModulation"]))
        {
            float normalized =
                float(s.valueToProportionOfLength(double(s.getProperties()["modulationValue"])));
            float modAngle = start + jlimit(0.f, 1.f, normalized) * (end - start);
            Path modulationArc;
            modulationArc.addCentredArc(c.x, c.y, radius + 7, radius + 7, 0, jmin(modAngle, angle),
                                        jmax(modAngle, angle), true);
            g.setColour(Colour(0xffbda1f4).withAlpha(.45f));
            g.strokePath(modulationArc, PathStrokeType(2));
            g.setColour(Colour(0xffbda1f4));
            g.fillEllipse(c.x + std::sin(modAngle) * (radius + 7) - 2.5f,
                          c.y - std::cos(modAngle) * (radius + 7) - 2.5f, 5, 5);
        }
        g.setColour(Colour(0xffe5f1f1));
        g.drawLine(c.x + std::sin(angle) * radius * .52f, c.y - std::cos(angle) * radius * .52f,
                   c.x + std::sin(angle) * radius * .82f, c.y - std::cos(angle) * radius * .82f, 2);
    }
};
class ResponsiveKnob : public Slider
{
  public:
    std::function<void()> resetAction;
    bool popupGesture = false;
    void mouseDown(const MouseEvent &e) override
    {
        if (e.mods.isRightButtonDown())
        {
            popupGesture = true;
            PopupMenu menu;
            menu.addItem(1, "Reset to default");
            auto safe = Component::SafePointer<ResponsiveKnob>(this);
            menu.showMenuAsync(PopupMenu::Options().withTargetComponent(this),
                               [safe](int result)
                               {
                                   if (safe && result == 1 && safe->resetAction)
                                       safe->resetAction();
                               });
            return;
        }
        setMouseDragSensitivity(e.mods.isShiftDown() ? 2000 : 250);
        Slider::mouseDown(e);
    }
    void mouseUp(const MouseEvent &e) override
    {
        if (popupGesture)
        {
            popupGesture = false;
            return;
        }
        Slider::mouseUp(e);
        setMouseDragSensitivity(250);
    }
};
class NorynEditor : public AudioProcessorEditor, private Timer
{
    NorynProcessor &p;
    HardwareLook look;
    MidiKeyboardComponent keys;
    TooltipWindow tooltip{this, 600};
    struct Control
    {
        std::unique_ptr<Slider> slider;
        std::unique_ptr<Label> label;
        std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Choice
    {
        std::unique_ptr<ComboBox> box;
        std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    std::vector<Control> controls;
    std::vector<Choice> choices;
    ComboBox categories, presets;
    TextButton previous{"<"}, next{">"}, audition{"PLAY PATCH"}, reset{"INIT"}, panic{"STOP"};
    Label description, status;
    std::vector<int> visiblePrograms;
    dsp::FFT fft{10};
    std::array<float, 2048> fftData{};
    std::array<float, 1024> spectrum{}, leftData{}, rightData{};
    float meterL = 0, meterR = 0;
    double previewStarted = 0;
    int lastProgram = -1;
    int lastUser = -1;
    String touched = "Drag a knob for immediate control. Shift-drag for fine adjustment.";
    int activePage = 0, buildingPage = 0;
    std::map<Component *, int> widgetPages;
    TextButton routeShortcut{"ROUTE LFO"};
    TextButton synthTab{"SYNTH"}, sequenceTab{"64 STEP SEQUENCER"}, modTab{"4 LFO ROUTING"},
        scaleTab{"SCALE ENGINE"};
    TextButton seqPlay{"PLAY"}, seqStop{"STOP SEQ"}, seqRecord{"RECORD"}, seqClear{"CLEAR"};
    ComboBox recordMode, patterns;
    ToggleButton metroToggle{"METRONOME"}, liveQuantize{"QUANTIZE LIVE MIDI"};
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> metroAttachment, liveQuantizeAttachment;
    std::array<std::unique_ptr<SequencePad>, 64> stepPads;
    std::array<std::unique_ptr<TextButton>, 12> customNotes;
    static constexpr int designWidth = 1440, designHeight = 930;
    void knob(const char *id, const char *name, int x, int y, Colour accent, int w = 84)
    {
        Control c;
        c.slider = std::make_unique<ResponsiveKnob>();
        c.label = std::make_unique<Label>();
        auto &s = *c.slider;
        s.setName(id);
        s.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        s.setMouseDragSensitivity(250);
        s.setRotaryParameters(MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle(Slider::TextBoxBelow, false, w - 6, 17);
        s.setColour(Slider::rotarySliderFillColourId, accent);
        s.setDoubleClickReturnValue(false, 0);
        static_cast<ResponsiveKnob &>(s).resetAction = [this, key = String(id)]
        {
            auto *parameter = p.state.getParameter(key);
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->getDefaultValue());
            parameter->endChangeGesture();
        };
        s.setTooltip(p.state.getParameter(id)->getName(60) +
                     ". Full sweep: 250 pixels. Right-click to reset. Type values with units.");
        String help;
        String key(id);
        if (key == "pulsewidth" || key == "shape" || key == "detune" || key == "octaveb")
            help = "Oscillator B: move A/B Mix toward B to hear it clearly.";
        if (key == "position" || key == "scan" || key == "fm" || key == "octavea")
            help = key == "scan" ? "Static wavetable spread between unison layers. Increase unison and mix "
                                   "oscillator A to hear it."
                                 : "Oscillator A: move A/B Mix toward A to hear it clearly.";
        if (key.startsWith("f") && key != "fm" && key != "feedback")
            help = "Filter ADSR: turn ENV > Filter away from zero to hear the contour.";
        if (key == "envamount")
            help = "Bipolar filter envelope: up to seven octaves of cutoff movement.";
        if (key == "keytrack")
            help = "Cutoff follows keyboard pitch. Most apparent on lower notes with a partly closed filter.";
        if (key == "velfilter")
            help = "Strike new notes softly or firmly to change their filter brightness.";
        if (key == "pwmdepth")
            help = "Pulse edge: blends a rounded pulse with a brighter square pulse. Mix in oscillator B.";
        if (help.isNotEmpty())
            s.setTooltip(s.getTooltip() + " " + help);
        s.getProperties().set("baseTooltip", s.getTooltip());
        s.setWantsKeyboardFocus(false);
        s.setBounds(x, y, w, 82);
        c.label->setText(name, dontSendNotification);
        c.label->setFont(FontOptions(11.5f));
        c.label->setJustificationType(Justification::centred);
        c.label->setColour(Label::textColourId, text);
        c.label->setBounds(x, y + 82, w, 18);
        addAndMakeVisible(s);
        addAndMakeVisible(*c.label);
        widgetPages[&s] = buildingPage;
        widgetPages[c.label.get()] = buildingPage;
        c.attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.state, id, s);
        s.setDoubleClickReturnValue(false, 0);
        s.onValueChange = [this, key = String(id), slider = c.slider.get()]
        {
            auto *param = p.state.getParameter(key);
            touched = param->getName(40) + "   " +
                      param->getText(param->convertTo0to1(float(slider->getValue())), 32);
        };
        controls.push_back(std::move(c));
    }
    void choice(const char *id, const StringArray &options, Rectangle<int> r)
    {
        Choice c;
        c.box = std::make_unique<ComboBox>();
        c.box->setName(id);
        c.box->addItemList(options, 1);
        c.box->setBounds(r);
        c.box->setWantsKeyboardFocus(false);
        addAndMakeVisible(*c.box);
        widgetPages[c.box.get()] = buildingPage;
        c.attachment =
            std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, id, *c.box);
        choices.push_back(std::move(c));
    }
    void fillPresets()
    {
        presets.clear(dontSendNotification);
        visiblePrograms.clear();
        String filter = categories.getText();
        int lastCategory = -1;
        String lastName;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            const auto &preset = factoryPresets()[size_t(i)];
            if (filter != "All sounds" && filter != preset.category)
                continue;
            if (filter == "All sounds" && preset.category != lastName)
            {
                presets.addSectionHeading(preset.category);
                lastName = preset.category;
            }
            presets.addItem(preset.name, i + 1);
            visiblePrograms.push_back(i);
            lastCategory = i;
        }
        ignoreUnused(lastCategory);
        if (filter == "All sounds" || filter == "User patches")
        {
            auto files = p.userPresetFiles();
            if (!files.isEmpty())
                presets.addSectionHeading("User patches");
            for (int i = 0; i < files.size(); ++i)
                presets.addItem(files[i].getFileNameWithoutExtension(), 10000 + i);
        }
        presets.setSelectedId(p.userLoaded.load() >= 0 ? 10000 + p.userLoaded.load()
                                                       : p.getCurrentProgram() + 1,
                              dontSendNotification);
    }
    void load(int index)
    {
        p.preview(false);
        p.setCurrentProgram(index);
        if (audition.getToggleState())
        {
            p.preview(true);
            previewStarted = Time::getMillisecondCounterHiRes();
        }
        syncPreset();
        touched = "Loaded " + p.getProgramName(index);
    }
    void step(int direction)
    {
        if (visiblePrograms.empty())
            return;
        auto it = std::find(visiblePrograms.begin(), visiblePrograms.end(), p.getCurrentProgram());
        int i = it == visiblePrograms.end() ? 0 : int(it - visiblePrograms.begin());
        i = (i + direction + int(visiblePrograms.size())) % int(visiblePrograms.size());
        load(visiblePrograms[size_t(i)]);
    }
    void syncPreset()
    {
        int index = p.getCurrentProgram();
        lastProgram = index;
        lastUser = p.userLoaded.load();
        if (std::find(visiblePrograms.begin(), visiblePrograms.end(), index) == visiblePrograms.end())
        {
            categories.setSelectedId(1, dontSendNotification);
            fillPresets();
        }
        presets.setSelectedId(p.userLoaded.load() >= 0 ? 10000 + p.userLoaded.load() : index + 1,
                              dontSendNotification);
        description.setText(factoryPresets()[size_t(index)].description, dontSendNotification);
    }
    void playPreview()
    {
        p.preview(audition.getToggleState());
        previewStarted = Time::getMillisecondCounterHiRes();
    }
    void timerCallback() override
    {
        if (lastUser != p.userLoaded.load())
            fillPresets();
        if (lastProgram != p.getCurrentProgram() || lastUser != p.userLoaded.load())
            syncPreset();
        if (audition.getToggleState() && Time::getMillisecondCounterHiRes() - previewStarted > 3200)
        {
            audition.setToggleState(false, dontSendNotification);
            p.preview(false);
        }
        p.visual.left.snapshot(leftData.data(), 1024);
        p.visual.right.snapshot(rightData.data(), 1024);
        std::fill(fftData.begin(), fftData.end(), 0);
        for (int i = 0; i < 1024; ++i)
            fftData[size_t(i)] = leftData[size_t(i)] * (.5f - .5f * std::cos(tau * i / 1023));
        fft.performFrequencyOnlyForwardTransform(fftData.data());
        for (int i = 0; i < 512; ++i)
            spectrum[size_t(i)] = jmax(fftData[size_t(i)] / 256, spectrum[size_t(i)] * .83f);
        meterL = jmax(p.visual.peakL.load(), meterL * .88f);
        meterR = jmax(p.visual.peakR.load(), meterR * .88f);
        seqPlay.setToggleState(p.sequencer.playing.load(), dontSendNotification);
        seqRecord.setToggleState(p.sequencer.recording.load(), dontSendNotification);
        for (auto &pad : stepPads)
            if (pad && activePage == 1)
                pad->repaint();
        const uint64_t modulationMask = p.visual.modMask.load();
        for (auto &control : controls)
        {
            auto &slider = *control.slider;
            int voiceEngine = int(p.v("engine"));
            String controlId = slider.getName();
            bool relevant = controlId == "fmratio" ? voiceEngine == 1
                            : controlId == "scan"  ? voiceEngine == 0
                            : controlId == "position"
                                ? (voiceEngine == 0 || voiceEngine == 3 || voiceEngine == 4)
                            : controlId == "fm" ? (voiceEngine == 0 || voiceEngine == 1 || voiceEngine == 3)
                                                : true;
            slider.setEnabled(relevant);
            control.label->setAlpha(relevant ? 1.f : .4f);
            if (controlId == "position")
                control.label->setText(voiceEngine == 3   ? "Drawbars"
                                       : voiceEngine == 4 ? "Tri / Saw"
                                                          : "WT Position",
                                       dontSendNotification);
            if (controlId == "fm")
                control.label->setText(voiceEngine == 1 ? "FM Index" : "Phase Mod", dontSendNotification);
            bool mapped = false;
            float effective = 0;
            for (int parameter = 0; parameter < Voice::Count; ++parameter)
                if (slider.getName() == Voice::ids[parameter] && ((modulationMask >> parameter) & 1))
                {
                    mapped = true;
                    effective = p.visual.effective[size_t(parameter)].load();
                    break;
                }
            if (slider.getName() == "master" && (modulationMask & (uint64_t(1) << 63)))
            {
                mapped = true;
                effective = p.v("master") * p.visual.ampMod.load();
            }
            const char *destinationIds[] = {"",      "cutoff",     "resonance", "position", "balance",
                                            "shape", "pulsewidth", "detune",    "fm",       "unison",
                                            "drive", "noise",      "sub",       "master",   "pan"};
            bool explicitlyAssigned = false;
            String modulationSources;
            for (int source = 1; source <= 4; ++source)
            {
                String prefix = "lfo" + String(source);
                int destination = int(p.v((prefix + "dest").toRawUTF8()));
                float amount = p.v((prefix + "amount").toRawUTF8()) * (source == 1 ? p.v("depth") : 1);
                if (destination > 0 && destination < 15 && slider.getName() == destinationIds[destination] &&
                    std::abs(amount) > .001f)
                {
                    explicitlyAssigned = true;
                    if (modulationSources.isNotEmpty())
                        modulationSources += ", ";
                    modulationSources += "LFO " + String(source);
                }
            }
            slider.setTooltip(
                slider.getProperties()["baseTooltip"].toString() +
                (modulationSources.isNotEmpty() ? " Modulated by " + modulationSources + "." : String()));
            slider.getProperties().set("showModulation",
                                       mapped && explicitlyAssigned && p.visual.voices.load() > 0);
            if (mapped)
                slider.getProperties().set("modulationValue", effective);
        }
        for (auto &selector : choices)
            if (selector.box->getName() == "family")
                selector.box->setEnabled(int(p.v("engine")) == 0);
        status.setText(touched, dontSendNotification);
        repaint();
    }
    void panel(Graphics &g, Rectangle<float> r, const String &title, Colour accent,
               const String &subtitle = {})
    {
        g.setColour(Colours::black.withAlpha(.8f));
        g.fillRoundedRectangle(r.expanded(2), 7);
        g.setGradientFill(ColourGradient(Colour(0xff252f34), r.getX(), r.getY(), Colour(0xff11191f),
                                         r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 6);
        g.setColour(Colour(0xff526269));
        g.drawRoundedRectangle(r, 6, .8f);
        g.setColour(accent.withAlpha(.08f));
        g.fillRoundedRectangle(r.withHeight(32), 6);
        g.setColour(accent);
        g.fillEllipse(r.getX() + 13, r.getY() + 13, 5, 5);
        g.setFont(Font(FontOptions(13.f, Font::bold)));
        g.setColour(text);
        g.drawText(title, r.toNearestInt().withTrimmedLeft(28).withHeight(32), Justification::centredLeft);
        g.setColour(muted);
        g.setFont(FontOptions(10.f));
        g.drawText(subtitle, r.toNearestInt().reduced(12, 0).withHeight(32), Justification::centredRight);
    }
    void lcd(Graphics &g, Rectangle<float> r, Colour accent, const String &label, bool live,
             bool animated = true)
    {
        g.setColour(Colour(0xff020a0e));
        g.fillRoundedRectangle(r, 4);
        g.setColour(accent.withAlpha(.23f));
        g.drawRoundedRectangle(r, 4, .8f);
        g.setColour(accent.withAlpha(.07f));
        g.drawHorizontalLine(int(r.getCentreY()), r.getX() + 10, r.getRight() - 10);
        g.setFont(FontOptions(9.f));
        g.setColour(accent.withAlpha(.85f));
        g.drawText(label, r.toNearestInt().reduced(9, 3).withHeight(15), Justification::centredLeft);
        g.setColour(live ? cyan : muted);
        g.drawText(!animated ? "SETTINGS"
                   : live    ? "LIVE"
                             : "IDLE",
                   r.toNearestInt().reduced(9, 3).withHeight(15), Justification::centredRight);
    }
    void glow(Graphics &g, const Path &path, Colour colour, float width = 1.5f)
    {
        g.setColour(colour.withAlpha(.055f));
        g.strokePath(path, PathStrokeType(9));
        g.setColour(colour.withAlpha(.12f));
        g.strokePath(path, PathStrokeType(4));
        g.setColour(colour);
        g.strokePath(path, PathStrokeType(width));
    }
    void oscillator(Graphics &g, Rectangle<float> r, int which)
    {
        Colour accent = which == 0 ? cyan : amber;
        lcd(g, r, accent, which == 0 ? "OSC A / WAVETABLE SHAPE" : "OSC B / ANALOG SHAPE", false, false);
        auto plot = r.reduced(8, 20);
        plot.setBottom(r.getBottom() - 9);
        Path wave;
        for (int i = 0; i < 400; ++i)
        {
            float t = i / 399.f, phase = wrap(t * 2.5f), value = 0;
            if (which == 0)
            {
                int engine = int(p.v("engine"));
                if (engine == 1)
                    value = p.tables.sin(phase + p.tables.sin(phase * p.v("fmratio")) * p.v("fm") * 1.6f);
                else if (engine == 2)
                    value = p.tables.sin(phase) * std::exp(-p.v("decay") * .08f * t);
                else if (engine == 3)
                    value = p.tables.sin(phase) * .65f +
                            p.tables.sin(phase * 2) * (.15f + p.v("position") * .25f) +
                            p.tables.sin(phase * 3) * p.v("position") * .3f +
                            p.tables.sin(phase * 4) * p.v("position") * .2f;
                else if (engine == 4)
                    value = (2 * std::abs(2 * phase - 1) - 1) * (1 - p.v("position")) +
                            (2 * phase - 1) * p.v("position");
                else
                    value = p.tables.read(phase, p.v("position"), int(p.v("family")), 6);
            }
            else
            {
                float pw = p.v("pulsewidth"), saw = 2 * phase - 1;
                float hard = (phase < pw ? 1.f : -1.f) - (2 * pw - 1),
                      softPhase = phase < pw ? phase / pw * .5f : .5f + (phase - pw) / (1 - pw) * .5f;
                float soft = p.tables.sin(softPhase) - (2 * pw - 1) * .63661977f;
                float pulse = (soft * (1 - p.v("pwmdepth")) + hard * p.v("pwmdepth")) * .7f;
                value = saw * (1 - p.v("shape")) + pulse * p.v("shape");
            }
            float x = plot.getX() + t * plot.getWidth(),
                  y = plot.getCentreY() - value * plot.getHeight() * .38f;
            if (i == 0)
                wave.startNewSubPath(x, y);
            else
                wave.lineTo(x, y);
        }
        if (which == 0 && int(p.v("engine")) == 0 && p.v("scan") > .001f)
        {
            for (int side : {-1, 1})
            {
                Path neighbor;
                for (int i = 0; i < 400; ++i)
                {
                    float t = i / 399.f;
                    float value =
                        p.tables.read(t * 2.5f, jlimit(0.f, 1.f, p.v("position") + side * p.v("scan") * .35f),
                                      int(p.v("family")), 6);
                    float x = plot.getX() + t * plot.getWidth(),
                          y = plot.getCentreY() - value * plot.getHeight() * .38f + side * 2;
                    if (i == 0)
                        neighbor.startNewSubPath(x, y);
                    else
                        neighbor.lineTo(x, y);
                }
                g.setColour(Colour(0xffb497e7).withAlpha(.3f));
                g.strokePath(neighbor, PathStrokeType(1));
            }
        }
        glow(g, wave, accent);
    }
    void envelope(Graphics &g, Rectangle<float> r, bool filter)
    {
        float a = p.v(filter ? "fattack" : "attack"), d = p.v(filter ? "fdecay" : "decay"),
              s = p.v(filter ? "fsustain" : "sustain"), release = p.v(filter ? "frelease" : "release");
        lcd(g, r, cyan, filter ? "FILTER CONTOUR" : "AMPLITUDE CONTOUR", false, false);
        auto plot = r.reduced(10, 20);
        plot.setBottom(r.getBottom() - 8);
        float hold = jmax(.15f, (a + d + release) * .2f), total = a + d + hold + release;
        auto point = [&](float time, float level)
        {
            return Point<float>(plot.getX() + time / total * plot.getWidth(),
                                plot.getBottom() - level * plot.getHeight());
        };
        Path wave;
        wave.startNewSubPath(point(0, 0));
        wave.lineTo(point(a, 1));
        wave.lineTo(point(a + d, s));
        wave.lineTo(point(a + d + hold, s));
        wave.lineTo(point(total, 0));
        glow(g, wave, cyan);
    }
    void lfo(Graphics &g, Rectangle<float> r)
    {
        lcd(g, r, cyan, "LFO 1 / 2 s / SOURCE GAIN", false, false);
        auto plot = r.reduced(8, 19);
        plot.setBottom(r.getBottom() - 8);
        Path wave;
        for (int i = 0; i < 300; ++i)
        {
            float t = i / 299.f,
                  value = lfoValue(t * 2 * p.v("rate"), int(p.v("lfoshape")), p.tables) * p.v("depth");
            float x = plot.getX() + t * plot.getWidth(),
                  y = plot.getCentreY() - value * plot.getHeight() * .4f;
            if (i == 0)
                wave.startNewSubPath(x, y);
            else
                wave.lineTo(x, y);
        }
        glow(g, wave, cyan);
    }
    void filter(Graphics &g, Rectangle<float> r)
    {
        float cutoff = p.v("cutoff"), k = 2 - 1.94f * p.v("resonance");
        int mode = int(p.v("filtermode"));
        lcd(g, r, amber, "FILTER / " + String(cutoff, 0) + " Hz", false, false);
        auto plot = r.reduced(8, 20);
        plot.setBottom(r.getBottom() - 8);
        Path curve;
        for (int i = 0; i < 350; ++i)
        {
            float t = i / 349.f, hz = 20 * std::pow(1000.f, t), ratio = hz / cutoff,
                  den = std::sqrt(std::pow(1 - ratio * ratio, 2) + k * k * ratio * ratio);
            float mag = (mode == 2 ? ratio * ratio : mode == 3 ? k * ratio : 1) / jmax(.0001f, den);
            if (mode == 1)
                mag *= mag;
            float db = Decibels::gainToDecibels(mag, -60.f);
            float y = plot.getY() + jlimit(0.f, 1.f, (18 - db) / 66) * plot.getHeight(),
                  x = plot.getX() + t * plot.getWidth();
            if (i == 0)
                curve.startNewSubPath(x, y);
            else
                curve.lineTo(x, y);
        }
        glow(g, curve, amber);
        float marker = jlimit(0.f, 1.f, std::log(cutoff / 20) / std::log(1000.f));
        g.setColour(amber.withAlpha(.2f));
        g.drawVerticalLine(int(plot.getX() + marker * plot.getWidth()), plot.getY(), plot.getBottom());
    }
    void spectrumScreen(Graphics &g, Rectangle<float> r)
    {
        lcd(g, r, cyan, "OUTPUT / REAL-TIME SPECTRUM", meterL > .001f);
        auto plot = r.reduced(8, 20);
        plot.setBottom(r.getBottom() - 7);
        Path path;
        float sr = p.visual.sampleRate.load();
        for (int i = 0; i < 250; ++i)
        {
            float t = i / 249.f, hz = 30 * std::pow(600.f, t), bin = jlimit(1.f, 510.f, hz * 1024 / sr);
            int a = jlimit(1, 510, int(bin));
            float gain = spectrum[size_t(a)] + (bin - a) * (spectrum[size_t(a + 1)] - spectrum[size_t(a)]);
            float db = Decibels::gainToDecibels(jmax(0.f, gain), -72.f);
            float x = plot.getX() + t * plot.getWidth(),
                  y = plot.getBottom() - jlimit(0.f, 1.f, (db + 72) / 72) * plot.getHeight();
            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        glow(g, path, cyan, 1.2f);
    }
    void outputScope(Graphics &g, Rectangle<float> r)
    {
        lcd(g, r, cyan, "OUTPUT / L + R OSCILLOSCOPE", meterL > .001f);
        auto plot = r.reduced(10, 20);
        plot.setBottom(r.getBottom() - 8);
        int start = 0;
        for (int i = 0; i < 300; ++i)
            if (leftData[size_t(i)] <= 0 && leftData[size_t(i + 1)] > 0)
            {
                start = i;
                break;
            }
        for (int channel = 0; channel < 2; ++channel)
        {
            auto &data = channel == 0 ? leftData : rightData;
            Path line;
            for (int i = 0; i < 600; ++i)
            {
                float t = i / 599.f;
                int at = jlimit(0, 1023, start + i);
                float x = plot.getX() + t * plot.getWidth(),
                      y = plot.getCentreY() - data[size_t(at)] * plot.getHeight() * .65f;
                if (i == 0)
                    line.startNewSubPath(x, y);
                else
                    line.lineTo(x, y);
            }
            glow(g, line, channel == 0 ? cyan : amber, 1.1f);
        }
    }

    void showPage(int page)
    {
        activePage = page;
        for (const auto &item : widgetPages)
            item.first->setVisible(item.second == page);
        synthTab.setToggleState(page == 0, dontSendNotification);
        sequenceTab.setToggleState(page == 1, dontSendNotification);
        modTab.setToggleState(page == 2, dontSendNotification);
        scaleTab.setToggleState(page == 3, dontSendNotification);
        repaint();
    }
    void pageWidget(Component &c, int page)
    {
        addAndMakeVisible(c);
        widgetPages[&c] = page;
    }
    void buildSequencePage()
    {
        buildingPage = 1;
        seqPlay.setBounds(28, 145, 100, 30);
        seqStop.setBounds(138, 145, 100, 30);
        seqRecord.setBounds(248, 145, 110, 30);
        seqPlay.setClickingTogglesState(true);
        seqRecord.setClickingTogglesState(true);
        seqPlay.onClick = [this] { p.sequencer.playing.store(seqPlay.getToggleState()); };
        seqStop.onClick = [this]
        {
            p.sequencer.playing.store(false);
            p.sequencer.recording.store(false);
        };
        seqRecord.onClick = [this]
        {
            bool record = seqRecord.getToggleState();
            p.sequencer.recording.store(record);
            if (record && !p.sequencer.stepRecord.load())
                p.sequencer.playing.store(true);
        };
        recordMode.addItemList({"LIVE RECORD", "STEP RECORD"}, 1);
        recordMode.setSelectedId(1, dontSendNotification);
        recordMode.setBounds(370, 145, 165, 30);
        recordMode.onChange = [this]
        {
            p.sequencer.stepRecord.store(recordMode.getSelectedId() == 2);
            if (recordMode.getSelectedId() == 2)
                p.sequencer.playing.store(false);
        };
        metroToggle.setName("metronome");
        metroToggle.setBounds(550, 145, 160, 30);
        metroAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
            p.state, "metronome", metroToggle);
        patterns.addItemList({"Empty", "Beat pulse", "Acid steps", "Dream arpeggio", "Chord walk"}, 1);
        patterns.setSelectedId(1, dontSendNotification);
        patterns.setBounds(1100, 145, 190, 30);
        patterns.onChange = [this] { p.sequencer.fill(patterns.getSelectedId() - 1); };
        seqClear.setBounds(1302, 145, 110, 30);
        seqClear.onClick = [this] { p.sequencer.clear(); };
        for (Component *c : std::array<Component *, 7>{&seqPlay, &seqStop, &seqRecord, &recordMode,
                                                       &metroToggle, &patterns, &seqClear})
        {
            pageWidget(*c, 1);
            c->setWantsKeyboardFocus(false);
        }
        knob("bpm", "BPM", 28, 180, amber, 104);
        knob("seqlength", "Steps", 151, 180, cyan, 104);
        knob("seqnote", "Entry note", 274, 180, cyan, 104);
        knob("seqvelocity", "Entry velocity", 397, 180, cyan, 104);
        knob("seqgate", "Gate", 520, 180, cyan, 104);
        knob("clicklevel", "Click level", 643, 180, amber, 104);
        knob("seqnotelength", "Note length", 766, 180, cyan, 104);
        choice("seqdivision", {"Quarter / 1/4", "Eighth / 1/8", "Sixteenth / 1/16", "Thirty-second / 1/32"},
               {905, 209, 195, 30});
        for (int i = 0; i < 64; ++i)
        {
            auto pad = std::make_unique<SequencePad>(p.sequencer, i);
            pad->entryNote = [this] { return int(p.v("seqnote")); };
            pad->entryVelocity = [this] { return p.v("seqvelocity"); };
            pad->entryLength = [this] { return p.v("seqnotelength"); };
            pad->select = [this](int note, float velocity)
            {
                p.setValue("seqnote", float(note));
                p.setValue("seqvelocity", velocity);
            };
            pad->setBounds(28 + (i % 16) * 86, 326 + (i / 16) * 112, 78, 96);
            pageWidget(*pad, 1);
            stepPads[size_t(i)] = std::move(pad);
        }
    }
    void buildModulationPage()
    {
        buildingPage = 2;
        StringArray destinations{
            "Off",         "Filter cutoff", "Resonance", "WT position",      "A / B mix",
            "Saw / pulse", "Pulse width",   "B detune",  "Phase modulation", "Stereo unison",
            "Drive",       "Noise",         "Sub level", "Amplitude",        "Pan"};
        for (int i = 0; i < 4; ++i)
        {
            int x = i % 2 == 0 ? 16 : 728, y = i < 2 ? 110 : 468;
            String prefix = "lfo" + String(i + 1), rateId = i == 0 ? "rate" : prefix + "rate",
                   shapeId = i == 0 ? "lfoshape" : prefix + "shape";
            choice(shapeId.toRawUTF8(), {"Sine", "Triangle", "Saw", "Square"}, {x + 490, y + 10, 200, 25});
            knob(rateId.toRawUTF8(), "Rate", x + 20, y + 204, cyan, 106);
            knob((prefix + "amount").toRawUTF8(), "Bipolar amount", x + 145, y + 204, Colour(0xffb69de8),
                 106);
            choice((prefix + "dest").toRawUTF8(), destinations, {x + 300, y + 230, 380, 32});
        }
    }
    void buildScalePage()
    {
        buildingPage = 3;
        choice("scale", ScaleEngine::names(), {40, 175, 400, 36});
        choice("root", {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"},
               {465, 175, 160, 36});
        choice("quantizedirection", {"Nearest", "Up", "Down"}, {650, 175, 200, 36});
        liveQuantize.setName("quantizelive");
        liveQuantize.setBounds(890, 175, 280, 36);
        pageWidget(liveQuantize, 3);
        liveQuantizeAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
            p.state, "quantizelive", liveQuantize);
        for (int i = 0; i < 12; ++i)
        {
            auto note = std::make_unique<TextButton>();
            note->setName("custom-note" + String(i));
            note->setBounds(40 + i * 112, 351, 100, 110);
            note->onClick = [this, i]
            {
                int mask = int(p.v("customscale")) ^ (1 << i);
                if (mask == 0)
                    mask = 1 << i;
                p.setValue("customscale", float(mask));
                p.setValue("scale", float(ScaleEngine::scales().size() - 1));
            };
            pageWidget(*note, 3);
            customNotes[size_t(i)] = std::move(note);
        }
    }
    void paintSequence(Graphics &g)
    {
        panel(g, {16, 110, 1408, 194}, "TRANSPORT / RECORDING", amber);
        g.setColour(muted);
        g.setFont(FontOptions(12.f));
        g.drawText("Click: toggle entry note. Right-click: inspect. Shift-click: clear step. Live record "
                   "overdubs at the nearest step; step record advances after key release.",
                   30, 296, 1370, 24, Justification::centredLeft);
        g.setColour(text);
        g.setFont(FontOptions(13.f));
        g.drawText("64 STEPS / " + String(p.sequencer.effectiveBpm.load(), 1) + " BPM / " +
                       (p.sequencer.recording.load() ? "RECORDING" : "READY"),
                   1120, 200, 280, 55, Justification::centred);
        g.setColour(muted);
        g.drawText("Scale: " + ScaleEngine::names()[int(p.v("scale"))] + " / " +
                       StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#",
                                   "B"}[int(p.v("root"))],
                   30, 790, 1360, 25, Justification::centredLeft);
    }
    void paintModulation(Graphics &g)
    {
        for (int i = 0; i < 4; ++i)
        {
            int x = i % 2 == 0 ? 16 : 728, y = i < 2 ? 110 : 468;
            String prefix = "lfo" + String(i + 1), rateId = i == 0 ? "rate" : prefix + "rate",
                   shapeId = i == 0 ? "lfoshape" : prefix + "shape";
            Colour accent = i % 2 == 0 ? cyan : Colour(0xffb69de8);
            panel(g, {float(x), float(y), 696, 340}, "LFO " + String(i + 1), accent);
            Rectangle<float> r(float(x + 18), float(y + 53), 672, 132);
            lcd(g, r, accent, "2 SECOND WINDOW / ASSIGNABLE MODULATION", false, false);
            auto plot = r.reduced(14, 26);
            Path path;
            for (int point = 0; point < 500; ++point)
            {
                float t = point / 499.f, wave = lfoValue(t * 2 * p.v(rateId.toRawUTF8()),
                                                         int(p.v(shapeId.toRawUTF8())), p.tables) *
                                                p.v((prefix + "amount").toRawUTF8());
                float px = plot.getX() + t * plot.getWidth(),
                      py = plot.getCentreY() - wave * plot.getHeight() * .45f;
                if (point == 0)
                    path.startNewSubPath(px, py);
                else
                    path.lineTo(px, py);
            }
            glow(g, path, accent);
            g.setColour(muted);
            g.setFont(FontOptions(11.f));
            g.drawText("DESTINATION", x + 300, y + 207, 380, 22, Justification::centredLeft);
        }
    }
    void paintScale(Graphics &g)
    {
        panel(g, {16, 110, 1408, 705}, "SCALE ENGINE", cyan, "SEQUENCER + OPTIONAL LIVE MIDI");
        int root = int(p.v("root")), scale = int(p.v("scale"));
        auto bits = scale == int(ScaleEngine::scales().size()) - 1
                        ? int(p.v("customscale"))
                        : ScaleEngine::scales()[size_t(scale)].mask;
        for (int i = 0; i < 12; ++i)
        {
            auto &note = *customNotes[size_t(i)];
            note.setButtonText(MidiMessage::getMidiNoteName(60 + (root + i) % 12, true, false, 3));
            note.setColour(TextButton::buttonColourId,
                           (bits & (1 << i)) ? Colour(0xff28636c) : Colour(0xff101b25));
            note.setColour(TextButton::textColourOffId, (bits & (1 << i)) ? text : muted);
        }
        g.setColour(text);
        g.setFont(FontOptions(18.f));
        g.drawText(ScaleEngine::names()[scale] + " / " +
                       MidiMessage::getMidiNoteName(60 + root, true, false, 3),
                   40, 255, 1250, 40, Justification::centredLeft);
        g.setColour(muted);
        g.setFont(FontOptions(14.f));
        g.drawText("Click pitch classes to build a custom scale. Quantization preserves matching note-offs "
                   "when the scale changes.",
                   40, 485, 1300, 35, Justification::centredLeft);
        g.drawText("Sequencer notes are always sent through the selected scale. Enable QUANTIZE LIVE MIDI to "
                   "constrain your keyboard as well.",
                   40, 532, 1300, 35, Justification::centredLeft);
    }

  public:
    NorynEditor(NorynProcessor &processor)
        : AudioProcessorEditor(processor), p(processor),
          keys(p.keyboard, MidiKeyboardComponent::horizontalKeyboard)
    {
        setLookAndFeel(&look);
        look.setColour(ComboBox::backgroundColourId, Colour(0xff101d24));
        look.setColour(ComboBox::outlineColourId, Colour(0xff49616d));
        look.setColour(ComboBox::textColourId, text);
        look.setColour(PopupMenu::backgroundColourId, Colour(0xff10212a));
        look.setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xff285b68));
        look.setColour(TextButton::buttonColourId, Colour(0xff243c46));
        look.setColour(TextButton::buttonOnColourId, Colour(0xff137984));
        setWantsKeyboardFocus(true);
        setResizable(true, true);
        getConstrainer()->setFixedAspectRatio(double(designWidth) / designHeight);
        setResizeLimits(1008, 651, 1728, 1116);
        setSize(designWidth, designHeight);
        knob("position", "WT Position", 29, 275, cyan);
        knob("scan", "WT Spread", 119, 275, cyan);
        knob("fm", "Phase Mod", 209, 275, cyan);
        knob("balance", "A / B Mix", 299, 275, cyan);
        knob("octavea", "A Octave", 389, 275, cyan);
        knob("detune", "Detune", 503, 275, amber, 70);
        knob("shape", "Saw / Pulse", 576, 275, amber, 70);
        knob("pulsewidth", "Pulse Width", 649, 275, amber, 70);
        knob("pwmdepth", "Pulse Edge", 722, 275, amber, 70);
        knob("sub", "Sub Level", 795, 275, amber, 70);
        knob("octaveb", "B Octave", 868, 275, amber, 70);
        knob("cutoff", "Cutoff", 980, 275, amber);
        knob("resonance", "Resonance", 1067, 275, amber);
        knob("drive", "Drive", 1154, 275, amber);
        knob("envamount", "ENV > Filter", 1241, 275, amber);
        knob("keytrack", "Key Track", 1328, 275, amber);
        choice("engine", {"Hybrid WT", "FM Operator", "Plucked String", "Drawbar Organ", "Analog Stack"},
               {116, 118, 170, 23});
        choice("family", {"Classic", "Spectral", "Formant", "Digital"}, {297, 118, 173, 23});
        choice("filtermode", {"LP 12 dB", "LP 24 dB", "High pass", "Band pass"}, {1243, 118, 169, 23});
        choice("lfoshape", {"Sine", "Triangle", "Saw", "Square"}, {914, 415, 100, 23});
        knob("attack", "Attack", 27, 539, cyan, 78);
        knob("decay", "Decay", 111, 539, cyan, 78);
        knob("sustain", "Sustain", 195, 539, cyan, 78);
        knob("release", "Release", 279, 539, cyan, 78);
        knob("fattack", "Attack", 387, 539, cyan, 78);
        knob("fdecay", "Decay", 471, 539, cyan, 78);
        knob("fsustain", "Sustain", 555, 539, cyan, 78);
        knob("frelease", "Release", 639, 539, cyan, 78);
        knob("rate", "LFO Rate", 750, 539, cyan);
        knob("depth", "LFO 1 Gain", 840, 539, cyan);

        knob("unison", "Unison", 1048, 539, amber, 70);
        knob("noise", "Noise", 1122, 539, amber, 70);
        knob("velfilter", "Vel > Filter", 1196, 539, amber, 70);
        knob("pan", "Pan", 1270, 539, amber, 70);
        knob("fmratio", "FM ratio", 1344, 539, amber, 70);
        const char *effectIds[] = {"chorus", "chorusrate", "delay", "delaytime", "feedback",
                                   "reverb", "size",       "width", "master"};
        const char *effectNames[] = {"Chorus", "Chorus Rate", "Echo Mix", "Echo Time", "Feedback",
                                     "Reverb", "Room Size",   "Width",    "Output"};
        for (int i = 0; i < 9; ++i)
            knob(effectIds[i], effectNames[i], 26 + i * 83, 704, amber, 76);
        keys.setName("keyboard");
        keys.setKeyPressBaseOctave(5);
        keys.setAvailableRange(36, 96);
        keys.setLowestVisibleKey(36);
        keys.setKeyWidth(24);
        keys.setColour(MidiKeyboardComponent::keyDownOverlayColourId, cyan.withAlpha(.6f));
        keys.setBounds(262, 835, 1162, 64);
        addAndMakeVisible(keys);
        categories.setName("categories");
        categories.addItem("All sounds", 1);
        StringArray cats;
        for (const auto &preset : factoryPresets())
            cats.addIfNotAlreadyThere(preset.category);
        cats.add("User patches");
        categories.addItemList(cats, 2);
        categories.setSelectedId(1, dontSendNotification);
        categories.setBounds(322, 22, 156, 30);
        categories.onChange = [this]
        {
            fillPresets();
            if (presets.getSelectedId() == 0 && !visiblePrograms.empty())
                load(visiblePrograms.front());
        };
        addAndMakeVisible(categories);
        presets.setName("presets");
        presets.setBounds(488, 22, 510, 36);
        presets.onChange = [this]
        {
            if (presets.getSelectedId() >= 10000)
            {
                p.loadUserPreset(presets.getSelectedId() - 10000);
                syncPreset();
            }
            else if (presets.getSelectedId() > 0)
                load(presets.getSelectedId() - 1);
        };
        addAndMakeVisible(presets);
        previous.setBounds(1008, 22, 34, 36);
        next.setBounds(1048, 22, 34, 36);
        previous.onClick = [this] { step(-1); };
        next.onClick = [this] { step(1); };
        audition.setName("audition");
        audition.setBounds(1094, 22, 136, 36);
        audition.setClickingTogglesState(true);
        audition.onClick = [this] { playPreview(); };
        reset.setBounds(1241, 22, 77, 36);
        reset.onClick = [this] { load(0); };
        panic.setBounds(1328, 22, 96, 36);
        panic.onClick = [this]
        {
            audition.setToggleState(false, dontSendNotification);
            p.preview(false);
            p.panicRequested.store(true);
        };
        for (auto *button : {&previous, &next, &audition, &reset, &panic})
        {
            button->setWantsKeyboardFocus(false);
            addAndMakeVisible(*button);
        }
        categories.setWantsKeyboardFocus(false);
        presets.setWantsKeyboardFocus(false);
        description.setFont(FontOptions(11.5f));
        description.setColour(Label::textColourId, muted);
        description.setBounds(322, 60, 1100, 21);
        addAndMakeVisible(description);
        status.setFont(FontOptions(11.f));
        status.setColour(Label::textColourId, muted);
        status.setBounds(265, 903, 1010, 22);
        addAndMakeVisible(status);
        synthTab.setBounds(322, 83, 100, 22);
        sequenceTab.setBounds(430, 83, 180, 22);
        modTab.setBounds(618, 83, 160, 22);
        scaleTab.setBounds(786, 83, 150, 22);
        TextButton *tabs[] = {&synthTab, &sequenceTab, &modTab, &scaleTab};
        for (int page = 0; page < 4; ++page)
        {
            auto *tab = tabs[page];
            tab->setClickingTogglesState(false);
            tab->setWantsKeyboardFocus(false);
            tab->onClick = [this, page] { showPage(page); };
            addAndMakeVisible(*tab);
        }
        routeShortcut.setBounds(932, 568, 82, 30);
        routeShortcut.onClick = [this] { showPage(2); };
        pageWidget(routeShortcut, 0);
        buildSequencePage();
        buildModulationPage();
        buildScalePage();
        buildingPage = 0;
        showPage(0);
        fillPresets();
        syncPreset();
        startTimerHz(30);
        resized();
    }
    ~NorynEditor() override
    {
        stopTimer();
        p.preview(false);
        setLookAndFeel(nullptr);
    }
    void refreshDisplays()
    {
        timerCallback();
    }
    void resized() override
    {
        float scale = getWidth() / float(designWidth);
        for (auto *child : getChildren())
            child->setTransform(AffineTransform::scale(scale));
    }
    void mouseDown(const MouseEvent &) override
    {
        grabKeyboardFocus();
    }
    bool keyPressed(const KeyPress &key) override
    {
        if (key == KeyPress::spaceKey)
        {
            audition.setToggleState(!audition.getToggleState(), dontSendNotification);
            playPreview();
            return true;
        }
        if (key == KeyPress::pageUpKey)
        {
            step(-1);
            return true;
        }
        if (key == KeyPress::pageDownKey)
        {
            step(1);
            return true;
        }
        return keys.keyPressed(key);
    }
    bool keyStateChanged(bool down) override
    {
        return keys.keyStateChanged(down);
    }
    void paint(Graphics &g) override
    {
        g.addTransform(AffineTransform::scale(getWidth() / float(designWidth)));
        g.fillAll(ink);
        g.setGradientFill(ColourGradient(Colour(0xff2b373e), 0, 0, Colour(0xff0a121a), 1440, 930, false));
        g.fillRoundedRectangle(3, 3, 1434, 924, 12);
        g.setColour(Colour(0xff576a74));
        g.drawRoundedRectangle(4, 4, 1432, 922, 12, 1);
        g.setColour(text);
        g.setFont(Font(FontOptions(35.f, Font::bold)));
        g.drawText("N O R Y N", 25, 14, 290, 46, Justification::centredLeft);
        g.setFont(FontOptions(10.f));
        g.setColour(cyan);
        g.drawText("HYBRID SYNTHESIZER / 96 FACTORY SOUNDS", 28, 62, 295, 21, Justification::centredLeft);
        if (activePage == 0)
        {
            panel(g, {16, 110, 465, 286}, "OSC A", cyan);
            panel(g, {491, 110, 465, 286}, "OSC B", amber, "POLYBLEP / PWM / SUB");
            panel(g, {966, 110, 458, 286}, "FILTER", amber);
            oscillator(g, {28, 153, 441, 112}, 0);
            oscillator(g, {503, 153, 441, 112}, 1);
            filter(g, {978, 153, 434, 112});
            panel(g, {16, 407, 350, 244}, "AMP ENVELOPE", cyan, "ADSR");
            panel(g, {376, 407, 350, 244}, "FILTER ENVELOPE", cyan, "ADSR");
            panel(g, {736, 407, 290, 244}, "LFO 1", cyan);
            panel(g, {1036, 407, 388, 244}, "VOICE / SPECTRUM", amber, "STEREO");
            envelope(g, {28, 450, 326, 80}, false);
            envelope(g, {388, 450, 326, 80}, true);
            lfo(g, {748, 450, 266, 80});
            spectrumScreen(g, {1048, 450, 364, 80});
            panel(g, {16, 662, 760, 154}, "EFFECTS / OUTPUT", amber, "CHORUS > PING-PONG DELAY > REVERB");
            panel(g, {786, 662, 638, 154}, "LIVE AUDIO", cyan, "2x OVERSAMPLED");
            outputScope(g, {798, 704, 544, 97});
            for (int c = 0; c < 2; ++c)
            {
                float x = 1358 + c * 23, level = c == 0 ? meterL : meterR;
                g.setColour(Colour(0xff050d12));
                g.fillRoundedRectangle(x, 708, 15, 88, 3);
                int bars = jlimit(0, 18, int((Decibels::gainToDecibels(level, -60.f) + 60) / 60 * 18));
                for (int b = 0; b < bars; ++b)
                {
                    g.setColour(b > 15 ? amber : cyan.withAlpha(.35f + b * .035f));
                    g.fillRect(x + 3, 790 - b * 4.5f, 9.f, 3.f);
                }
                g.setColour(muted);
                g.setFont(FontOptions(9.f));
                g.drawText(c == 0 ? "L" : "R", int(x), 797, 15, 14, Justification::centred);
            }
        }
        else if (activePage == 1)
            paintSequence(g);
        else if (activePage == 2)
            paintModulation(g);
        else
            paintScale(g);
        g.setColour(Colour(0xff080f16));
        g.fillRoundedRectangle(16, 831, 232, 68, 5);
        g.setColour(cyan);
        g.setFont(FontOptions(11.f));
        g.drawText("A W S E D F T G Y H U J", 28, 840, 210, 20, Justification::centredLeft);
        g.setColour(muted);
        g.drawText("SPACE: play patch / PAGE: browse", 28, 861, 210, 18, Justification::centredLeft);
        g.drawText("MIDI / sustain / mod wheel / bend", 28, 879, 210, 16, Justification::centredLeft);
        g.setColour(amber);
        g.setFont(FontOptions(10.f));
        g.drawText("NORYN 0.3 / " + String(p.visual.voices.load()) + " OF 16 VOICES", 24, 903, 232, 22,
                   Justification::centredLeft);
        g.setColour(muted);
        g.drawText(String(p.visual.sampleRate.load() / 1000, 1) + " kHz", 1310, 903, 108, 22,
                   Justification::centredRight);
    }
};
AudioProcessorEditor *NorynProcessor::createEditor()
{
    return new NorynEditor(*this);
}
AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new NorynProcessor();
}
// Test accessors use the same telemetry as the visible LCDs.
bool norynTelemetryActive(AudioProcessor *processor)
{
    auto *p = dynamic_cast<NorynProcessor *>(processor);
    return p && p->visual.left.published.load() > 0 && p->visual.right.published.load() > 0;
}
void norynRefreshEditor(AudioProcessorEditor *editor)
{
    if (auto *instance = dynamic_cast<NorynEditor *>(editor))
        instance->refreshDisplays();
}
bool norynScopeMatches(AudioProcessor *processor, const AudioBuffer<float> &audio)
{
    auto *p = dynamic_cast<NorynProcessor *>(processor);
    if (!p || audio.getNumSamples() > 1024)
        return false;
    std::array<float, 1024> samples{};
    p->visual.left.snapshot(samples.data(), audio.getNumSamples());
    for (int i = 0; i < audio.getNumSamples(); ++i)
        if (std::abs(samples[size_t(i)] - audio.getSample(0, i)) > 1.e-6f)
            return false;
    p->visual.left.snapshot(samples.data(), 1024);
    double energy = 0;
    for (float sample : samples)
        energy += sample * sample;
    return energy > .001;
}

var norynControl(AudioProcessor *processor, const var &request)
{
    auto *p = dynamic_cast<NorynProcessor *>(processor);
    if (!p)
        throw std::runtime_error("Not NORYN");
    return p->handleControl(request);
}
