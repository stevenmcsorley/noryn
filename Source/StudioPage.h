#pragma once
#include "Studio.h"
#include "UiParts.h"
#include <JuceHeader.h>
namespace noryn
{
// STUDIO page: transport, eight track lanes with waveforms, and session management.
class StudioPage : public juce::Component, private juce::Timer
{
  public:
    std::function<void()> onNeedInput;        // make sure the audio input is open and unmuted
    std::function<void(int)> onEditTrack;     // show the vocal chain for a track
    std::function<void()> onLoadDemo;         // build the Bjork "Hunter" multitrack demo
    std::function<float()> bpm;

    explicit StudioPage(Studio &s) : studio(s), cache(32)
    {
        formats.registerBasicFormats();
        for (auto &thumb : thumbs)
            thumb = std::make_unique<juce::AudioThumbnail>(256, formats, cache);
        rtz.onClick = [this] { studio.locate(0); };
        ui::styleToggle(play, ui::green);
        play.onClick = [this]
        {
            if (play.getToggleState())
                studio.play();
            else
                studio.stop();
        };
        stop.onClick = [this]
        {
            if (!studio.playing.load())
                studio.locate(0);
            studio.stop();
        };
        ui::styleToggle(record, ui::red);
        record.onClick = [this]
        {
            if (record.getToggleState())
            {
                needInputIfArmed();
                if (!studio.record())
                    record.setToggleState(false, juce::dontSendNotification);
            }
            else
                studio.stop();
        };
        ui::styleToggle(follow, ui::amber);
        follow.setToggleState(true, juce::dontSendNotification);
        follow.onClick = [this] { studio.runSequencer.store(follow.getToggleState()); };
        follow.setTooltip("The step sequencer and drums start and stop with the studio transport, from the "
                          "same bar. Turn off to play back recorded tracks alone.");
        newSession.onClick = [this]
        {
            studio.newSession(Studio::defaultRoot().getChildFile(
                "Session " + juce::Time::getCurrentTime().formatted("%Y-%m-%d %H-%M-%S")));
            refresh();
        };
        open.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser>("Open a NORYN session folder", Studio::defaultRoot());
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                 [this](const juce::FileChooser &fc)
                                 {
                                     if (fc.getResult().isDirectory() && !studio.openSession(fc.getResult()))
                                         studio.lastMessage = "That folder has no session.xml.";
                                     refresh();
                                 });
        };
        bounce.onClick = [this]
        {
            const auto dir = studio.ensureSession();
            chooser = std::make_unique<juce::FileChooser>("Bounce mix to WAV", dir.getChildFile("Mixdown.wav"), "*.wav");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                                 [this](const juce::FileChooser &fc)
                                 {
                                     if (fc.getResult() != juce::File())
                                         studio.bounce(fc.getResult().withFileExtension(".wav"));
                                 });
        };
        demo.onClick = [this]
        {
            if (onLoadDemo)
                onLoadDemo();
        };
        demo.setTooltip("Builds a full-length instrumental arrangement of Bjork's \"Hunter\" as a session: each "
                        "part on its own track, played by NORYN's synth and drums. Mix, mute, add vocals.");
        for (auto *c : std::initializer_list<juce::Component *>{&rtz, &play, &stop, &record, &follow, &newSession, &open,
                                                               &bounce, &demo, &message})
            addAndMakeVisible(c);
        message.setColour(juce::Label::textColourId, ui::muted);
        message.setFont(juce::FontOptions(11.f));
        for (int t = 0; t < studioTracks; ++t)
            rows[size_t(t)] = std::make_unique<Row>(*this, t);
        refresh();
        startTimerHz(30);
    }
    void refresh()
    {
        for (auto &row : rows)
            row->sync();
        lastGeneration = -1;
        repaint();
    }
    void resized() override
    {
        rtz.setBounds(12, 8, 44, 30);
        play.setBounds(60, 8, 74, 30);
        stop.setBounds(138, 8, 64, 30);
        record.setBounds(206, 8, 74, 30);
        follow.setBounds(484, 8, 150, 30);
        newSession.setBounds(642, 8, 96, 30);
        open.setBounds(744, 8, 76, 30);
        bounce.setBounds(826, 8, 86, 30);
        demo.setBounds(918, 8, 190, 30);
        message.setBounds(12, 44, 1100, 20);
        for (int t = 0; t < studioTracks; ++t)
            rows[size_t(t)]->place(rowY(t));
    }
    void paint(juce::Graphics &g) override
    {
        // Time display
        const double sr = juce::jmax(1.0, studio.getSampleRate()), seconds = double(studio.position.load()) / sr;
        const double beats = seconds * currentBpm() / 60;
        g.setColour(juce::Colour(0xff050b10));
        g.fillRoundedRectangle(288, 8, 188, 30, 4);
        g.setColour(studio.recording.load() ? ui::red : ui::cyan);
        g.setFont(juce::FontOptions(17.f, juce::Font::bold));
        g.drawText(juce::String(int(beats / 4) + 1) + "." + juce::String(int(beats) % 4 + 1) + "   " +
                       juce::String(int(seconds / 60)) + ":" + juce::String(int(seconds) % 60).paddedLeft('0', 2) + "." +
                       juce::String(int(seconds * 10) % 10),
                   288, 8, 188, 30, juce::Justification::centred);
        // Ruler in bars
        const auto lane = laneArea(0);
        g.setColour(ui::panelFill);
        g.fillRect(lane.getX(), 70.f, lane.getWidth(), 20.f);
        const double view = viewSeconds(), barSeconds = 240 / currentBpm();
        const int barStep = juce::jmax(1, int(std::ceil(view / barSeconds / 24)));
        g.setFont(juce::FontOptions(9.f));
        for (int bar = 0; bar * barSeconds <= view; bar += barStep)
        {
            const float x = xFor(bar * barSeconds);
            g.setColour(ui::panelEdge);
            g.drawVerticalLine(int(x), 70, 90);
            g.setColour(ui::muted);
            g.drawText(juce::String(bar + 1), int(x) + 2, 70, 40, 20, juce::Justification::centredLeft);
        }
        for (int t = 0; t < studioTracks; ++t)
        {
            const auto r = laneArea(t);
            const auto &track = studio.tracks[size_t(t)];
            g.setColour(track.armed.load() ? juce::Colour(0xff2a1517) : ui::panelFill);
            g.fillRoundedRectangle(r, 4);
            g.setColour(ui::panelEdge);
            g.drawRoundedRectangle(r, 4, 1);
            g.setColour(juce::Colour(0xff0d171e));
            g.fillRoundedRectangle(4, r.getY(), 508, r.getHeight(), 4);
            if (track.takeLength > 0)
            {
                const double start = double(track.takeStart) / sr, length = double(track.takeLength) / sr;
                auto clip = juce::Rectangle<float>(xFor(start), r.getY() + 3, float(length / view * r.getWidth()), r.getHeight() - 6)
                                .getIntersection(r);
                g.setColour((track.mute.load() ? ui::muted : ui::cyan).withAlpha(.12f));
                g.fillRoundedRectangle(clip, 3);
                g.setColour(track.mute.load() ? ui::muted : ui::cyan);
                thumbs[size_t(t)]->drawChannels(g, clip.toNearestInt(), 0, length, 1.f);
            }
            // Meter
            const float level = juce::jlimit(0.f, 1.f, track.meter.load());
            g.setColour(juce::Colour(0xff050b10));
            g.fillRect(500.f, r.getY() + 4, 8.f, r.getHeight() - 8);
            g.setColour(level > .9f ? ui::red : ui::green);
            const float h = (r.getHeight() - 8) * std::sqrt(level);
            g.fillRect(500.f, r.getBottom() - 4 - h, 8.f, h);
        }
        // Master meter under the studio level knob
        const float master = juce::jlimit(0.f, 1.f, studio.masterMeter.load());
        g.setColour(juce::Colour(0xff050b10));
        g.fillRoundedRectangle(1340, 120, 26, 560, 4);
        const float mh = 552 * std::sqrt(master);
        g.setColour(master > .95f ? ui::red : ui::cyan);
        g.fillRect(1344.f, 676 - mh, 18.f, mh);
        g.setColour(ui::muted);
        g.setFont(juce::FontOptions(9.f));
        g.drawText("MASTER", 1316, 684, 74, 14, juce::Justification::centred);
        // Playhead
        const float x = xFor(seconds);
        if (x <= laneArea(0).getRight())
        {
            g.setColour(studio.recording.load() ? ui::red : juce::Colours::white);
            g.drawVerticalLine(int(x), 70, float(rowY(studioTracks - 1) + rowHeight));
        }
    }
    void mouseDown(const juce::MouseEvent &e) override
    {
        for (int t = 0; t < studioTracks; ++t)
            if (laneArea(t).contains(e.position) || (e.position.y >= 70 && e.position.y < 90 && e.position.x >= laneArea(0).getX()))
            {
                if (e.mods.isPopupMenu() && laneArea(t).contains(e.position))
                {
                    juce::PopupMenu menu;
                    menu.addItem(1, "Undo last take", studio.tracks[size_t(t)].previousTake.existsAsFile());
                    menu.addItem(2, "Clear track", studio.tracks[size_t(t)].takeLength > 0);
                    menu.addItem(3, "Reveal take file", studio.tracks[size_t(t)].take.existsAsFile());
                    menu.showMenuAsync(juce::PopupMenu::Options(), [this, t](int choice)
                                       {
                                           if (choice == 1)
                                               studio.undoTake(t);
                                           else if (choice == 2)
                                               studio.clearTrack(t);
                                           else if (choice == 3)
                                               studio.tracks[size_t(t)].take.revealToUser();
                                           refresh();
                                       });
                    return;
                }
                // Click to locate, snapped to the nearest beat.
                const double seconds = juce::jmax(0.0, (e.position.x - laneArea(0).getX()) / laneArea(0).getWidth() * viewSeconds());
                const double beat = 60 / currentBpm();
                studio.locate(juce::int64(std::round(seconds / beat) * beat * studio.getSampleRate()));
                repaint();
                return;
            }
    }

  private:
    struct Row
    {
        StudioPage &page;
        int index;
        juce::TextEditor name;
        juce::ComboBox source;
        juce::TextButton arm{"R"}, monitor{"MON"}, mute{"M"}, solo{"S"}, fx{"FX"}, edit{"EDIT"};
        juce::Slider gain{juce::Slider::LinearHorizontal, juce::Slider::NoTextBox},
            pan{juce::Slider::LinearHorizontal, juce::Slider::NoTextBox};
        Row(StudioPage &owner, int i) : page(owner), index(i)
        {
            auto &track = page.studio.tracks[size_t(i)];
            name.setFont(juce::FontOptions(12.f));
            name.onTextChange = [this] { page.studio.tracks[size_t(index)].name = name.getText(); };
            name.onFocusLost = [this] { page.studio.saveSession(); };
            source.addItemList(trackSourceNames(), 1);
            source.onChange = [this]
            {
                page.studio.tracks[size_t(index)].source.store(source.getSelectedId() - 1);
                page.needInputIfArmed();
            };
            ui::styleToggle(arm, ui::red);
            arm.setTooltip("Arm for recording");
            arm.onClick = [this]
            {
                page.studio.tracks[size_t(index)].armed.store(arm.getToggleState());
                page.needInputIfArmed();
                page.repaint();
            };
            ui::styleToggle(monitor, ui::amber);
            monitor.setTooltip("Hear the live input through this track's chain (use headphones).");
            monitor.onClick = [this]
            {
                page.studio.tracks[size_t(index)].monitor.store(monitor.getToggleState());
                if (monitor.getToggleState() && page.onNeedInput)
                    page.onNeedInput();
            };
            ui::styleToggle(mute, ui::muted);
            mute.onClick = [this] { page.studio.tracks[size_t(index)].mute.store(mute.getToggleState()); };
            ui::styleToggle(solo, ui::amber);
            solo.onClick = [this] { page.studio.tracks[size_t(index)].solo.store(solo.getToggleState()); };
            ui::styleToggle(fx, ui::cyan);
            fx.setTooltip("Vocal chain on this track (playback and monitoring). EDIT opens it.");
            fx.onClick = [this] { page.studio.tracks[size_t(index)].fx.store(fx.getToggleState()); };
            edit.onClick = [this]
            {
                if (page.onEditTrack)
                    page.onEditTrack(index);
            };
            gain.setRange(0, 1.5, 0);
            gain.setTooltip("Track level");
            gain.onValueChange = [this] { page.studio.tracks[size_t(index)].gain.store(float(gain.getValue())); };
            gain.onDragEnd = [this] { page.studio.saveSession(); };
            gain.setDoubleClickReturnValue(true, .8);
            pan.setRange(-1, 1, 0);
            pan.setTooltip("Pan");
            pan.setDoubleClickReturnValue(true, 0);
            pan.onValueChange = [this] { page.studio.tracks[size_t(index)].pan.store(float(pan.getValue())); };
            for (auto *c : std::initializer_list<juce::Component *>{&name, &source, &arm, &monitor, &mute, &solo, &fx, &edit,
                                                                   &gain, &pan})
                page.addAndMakeVisible(c);
            juce::ignoreUnused(track);
        }
        void sync()
        {
            const auto &track = page.studio.tracks[size_t(index)];
            name.setText(track.name, juce::dontSendNotification);
            source.setSelectedId(track.source.load() + 1, juce::dontSendNotification);
            arm.setToggleState(track.armed.load(), juce::dontSendNotification);
            monitor.setToggleState(track.monitor.load(), juce::dontSendNotification);
            mute.setToggleState(track.mute.load(), juce::dontSendNotification);
            solo.setToggleState(track.solo.load(), juce::dontSendNotification);
            fx.setToggleState(track.fx.load(), juce::dontSendNotification);
            gain.setValue(track.gain.load(), juce::dontSendNotification);
            pan.setValue(track.pan.load(), juce::dontSendNotification);
        }
        void place(int y)
        {
            name.setBounds(10, y + 4, 140, 24);
            source.setBounds(10, y + 34, 140, 24);
            arm.setBounds(156, y + 4, 30, 24);
            monitor.setBounds(190, y + 4, 44, 24);
            mute.setBounds(238, y + 4, 30, 24);
            solo.setBounds(272, y + 4, 30, 24);
            fx.setBounds(306, y + 4, 36, 24);
            edit.setBounds(346, y + 4, 50, 24);
            gain.setBounds(156, y + 36, 210, 20);
            pan.setBounds(372, y + 36, 110, 20);
        }
    };
    Studio &studio;
    juce::AudioFormatManager formats;
    juce::AudioThumbnailCache cache;
    std::array<std::unique_ptr<juce::AudioThumbnail>, studioTracks> thumbs;
    std::array<std::unique_ptr<Row>, studioTracks> rows;
    juce::TextButton rtz{"|<"}, play{"PLAY"}, stop{"STOP"}, record{"REC"}, follow{"SEQ + DRUMS FOLLOW"},
        newSession{"NEW SESSION"}, open{"OPEN..."}, bounce{"BOUNCE..."}, demo{"LOAD HUNTER DEMO"};
    juce::Label message;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastGeneration = -1;
    static constexpr int rowHeight = 74;
    static int rowY(int t)
    {
        return 94 + t * (rowHeight + 2);
    }
    juce::Rectangle<float> laneArea(int t) const
    {
        return {520.f, float(rowY(t)), 776.f, float(rowHeight)}; // right column: studio level and master meter
    }
    double viewSeconds() const
    {
        const double sr = juce::jmax(1.0, studio.getSampleRate());
        return juce::jmax(60.0, double(juce::jmax(studio.sessionLength(), studio.position.load())) / sr * 1.08);
    }
    float xFor(double seconds) const
    {
        const auto lane = laneArea(0);
        return lane.getX() + float(seconds / viewSeconds()) * lane.getWidth();
    }
    float currentBpm() const
    {
        return bpm ? juce::jlimit(30.f, 300.f, bpm()) : 120.f;
    }
    void needInputIfArmed()
    {
        for (const auto &track : studio.tracks)
            if ((track.armed.load() || track.monitor.load()) && isInputSource(track.source.load()))
            {
                if (onNeedInput)
                    onNeedInput();
                return;
            }
    }
    void timerCallback() override
    {
        if (studio.generation.load() != lastGeneration)
        {
            lastGeneration = studio.generation.load();
            for (int t = 0; t < studioTracks; ++t)
            {
                const auto &take = studio.tracks[size_t(t)].take;
                if (take.existsAsFile())
                    thumbs[size_t(t)]->setSource(new juce::FileInputSource(take));
                else
                    thumbs[size_t(t)]->clear();
            }
            for (auto &row : rows)
                row->sync();
        }
        play.setToggleState(studio.playing.load(), juce::dontSendNotification);
        record.setToggleState(studio.recording.load(), juce::dontSendNotification);
        follow.setToggleState(studio.runSequencer.load(), juce::dontSendNotification);
        const auto session = studio.sessionDirectory == juce::File() ? juce::String("No session yet (one is made when you record)")
                                                                     : studio.sessionDirectory.getFileName();
        message.setText(session + (studio.lastMessage.isNotEmpty() ? "   /   " + studio.lastMessage : juce::String()),
                        juce::dontSendNotification);
        if (isVisible())
            repaint();
    }
};
} // namespace noryn
