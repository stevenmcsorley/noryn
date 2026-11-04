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
    ScopedJuceInitialiser_GUI initialise;
    try
    {
        File directory = argc > 1 ? File::getCurrentWorkingDirectory().getChildFile(argv[1])
                                  : File::getCurrentWorkingDirectory();
        directory.createDirectory();
        testScaleEngine();
        testSequencer();
        Harness h;
        require(h.p->getNumPrograms() == 97, "Expected 96 factory presets plus Init");
        auto started = Time::getMillisecondCounterHiRes();
        String csv = "parameter,normalized_low,normalized_high,relative_audio_difference\n";
        float minimumDifference = 100;
        String weakest;
        int audibleControls = 0;
        for (const auto &pair : h.parameters)
        {
            if (pair.first.startsWith("seq") || pair.first == "bpm" || pair.first == "clicklevel" ||
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
        String catalog = "# NORYN factory bank\n\n96 factory sounds plus Init. All patches are editable.\n\n";
        for (int i = 0; i < h.p->getNumPrograms(); ++i)
        {
            auto name = h.p->getProgramName(i);
            require(names.insert(name).second, "Duplicate preset name");
            h.p->setCurrentProgram(i);
            testedEngines.insert(
                int(h.parameters.at("engine")->convertFrom0to1(h.parameters.at("engine")->getValue())));
            auto sound = h.render(.9, .8f, {48, 60});
            double rms = std::sqrt(energy(sound) / (sound.getNumSamples() * 2));
            require(rms > .00015, "Silent preset: " + name);
            quietest = jmin(quietest, rms);
            loudest = jmax(loudest, rms);
            catalog += "- " + String(i) + ": " + name + "\n";
            if (i % 12 == 0)
                std::cout << "Presets checked: " << i << std::endl;
        }
        require(testedEngines.size() == 5, "Factory sounds do not cover all five voice engines");
        directory.getChildFile("PRESETS.md").replaceWithText(catalog);
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
                    combo->setSelectedItemIndex(2, sendNotificationSync);
                    require(std::abs(h.parameters.at(combo->getName())
                                         ->convertFrom0to1(h.parameters.at(combo->getName())->getValue()) -
                                     2) < .01f,
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
        for (int destination : {1, 3, 6, 0})
        {
            h.value("lfo1dest", float(destination));
            routeMidi.clear();
            h.p->processBlock(routeAudio, routeMidi);
            norynRefreshEditor(editor.get());
            for (auto *child : editor->getChildren())
                if (auto *knob = dynamic_cast<Slider *>(child))
                {
                    String id = knob->getName();
                    if (id == "cutoff" || id == "position" || id == "pulsewidth")
                    {
                        bool expected = destination == 1   ? id == "cutoff"
                                        : destination == 3 ? id == "position"
                                        : destination == 6 ? id == "pulsewidth"
                                                           : false;
                        require(bool(knob->getProperties()["showModulation"]) == expected,
                                "LFO routed/indicated the wrong destination: " + id);
                    }
                }
        }
        h.p->setCurrentProgram(1);
        h.prepare();
        norynRefreshEditor(editor.get());
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

        auto imageFile = directory.getChildFile("preview.png");
        imageFile.deleteFile();
        auto stream = imageFile.createOutputStream();
        PNGImageFormat png;
        require(stream && png.writeImageToStream(image, *stream), "Preview write failed");
        stream.reset();
        for (const auto &page : std::vector<std::pair<String, String>>{{"64 STEP SEQUENCER", "sequencer.png"},
                                                                       {"4 LFO ROUTING", "lfos.png"},
                                                                       {"SCALE ENGINE", "scales.png"}})
        {
            button(page.first)->onClick();
            norynRefreshEditor(editor.get());
            auto pageImage = editor->createComponentSnapshot(editor->getLocalBounds());
            auto file = directory.getChildFile(page.second);
            file.deleteFile();
            auto out = file.createOutputStream();
            require(out && png.writeImageToStream(pageImage, *out), "Page preview write failed");
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
        double elapsed = (Time::getMillisecondCounterHiRes() - started) / 1000;
        String report = "{\n  \"version\": \"0.3.0\",\n  \"factory_presets\": 96,\n  \"tested_controls\": " +
                        String(audibleControls) + ",\n  \"weakest_control\": \"" + weakest +
                        "\",\n  \"minimum_relative_audio_difference\": " + String(minimumDifference, 6) +
                        ",\n  \"quietest_preset_rms\": " + String(quietest, 6) +
                        ",\n  \"loudest_preset_rms\": " + String(loudest, 6) +
                        ",\n  \"state_recall\": true,\n  \"gui_bindings\": true,\n  \"audio_telemetry\": "
                        "true,\n  \"stress_sample_rates\": [44100, 48000, 96000],\n  \"stress_block_sizes\": "
                        "[16, 64, 127, 512],\n  \"elapsed_seconds\": " +
                        String(elapsed, 2) +
                        ",\n  \"dense_16_voice_realtime_ratio\": " + String(realtimeRatio, 4) + "\n}\n";
        directory.getChildFile("validation.json").replaceWithText(report);
        std::cout << "PASS: " << audibleControls
                  << " audible synthesis controls, 96 presets, GUI bindings, live telemetry, state recall, "
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
