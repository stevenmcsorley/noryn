# NORYN 0.7 — Hybrid Synthesizer

Native Linux VST3 instrument and standalone synthesizer. The interface takes cues from the supplied NORYN concept and the SONAVYRE module's dark panels and colored traces. This is an original instrument built with JUCE and custom DSP, not a circuit emulation of a particular commercial synth.

## v0.7.1 changes

- **Black-box recorder for the white-noise report.** NORYN now watches its own output. If it turns into loud broadband noise for half a second, or a safety guard has to reset non-finite audio, it saves three files to `~/.config/NORYN/incidents/`: a WAV of the last 4 seconds, a `.noryn` file of the exact state (sound, sequence, scale; load it like a user preset) and a `.txt` summary (preset, sequencer state, voices, every parameter, and in the standalone app the audio device, buffer size, xrun count and CPU load). It only records; it never mutes, so deliberately noisy sounds are unaffected. At most one report per 30 seconds is kept, and only the newest 20.
- **Noise investigation.** New test modes: `NorynSmoke . --noise-hunt N seed` plays user presets at 44.1 kHz / 512 with the sequencer running and changes presets, sequencer settings, engine, record/play state, keyboard notes and knobs mid-play; `NorynSmoke . --probe "<preset>" out.wav <note> id=value...` renders one held note. Every user preset render now also fails if the recorder reports it as noise.

## v0.7.0 changes

- **26 new song presets.** Nine Inch Nails (8 from 7 songs), The Cure (5), Portishead (2), UNKLE (1), The Verve (3) and Richard Ashcroft (2), each with a whole-song demo; see *Song-reference demo sequences*.
- **Stolen Dog rebuilt.** Bar-synchronous analysis of the official preview confirms 115.5 BPM and an F#m9 – Dadd9 – C#m7 – C#m/E (A/E) cycle; the study now starts on F#m9, runs 16 bars (256 steps) and joins level balancing.
- **Chord-study fix.** Short chord hits (Burial stabs, chops, bells) were stretched to almost four steps by a note-length floor applied in bars instead of steps; they are now as short as written.
- Section medleys handle sparse parts (bells, fills) by accepting any phrase where the part plays.
- All 85 user presets pass the render check; the 82 song demos are distinct and loudness-balanced.

## v0.6.0 changes

- **Section-medley song demos.** All 46 transcription-based demos now run through the whole song: each fills the 256-step sequencer with slices of the song's distinct sections in order (start, middle, end) instead of one excerpt. Daydreaming moves to 1/16 steps so it gets 16 bars. Details under *Song-reference demo sequences*.
- **MIDI control** (introduced in 0.5.0): plug-and-play MIDI inputs in the standalone app, a MIDI panel, and MIDI learn for every knob plus PLAY and RECORD, saved across presets and sessions. Tested with a first-generation KORG microKEY, which connects automatically.
- Loudness rebalanced for the new demos; all 64 user presets pass the render check.

## v0.5.0 changes

- **256-step sequencer.** Loops run up to 256 steps (16 bars of sixteenths). The grid pages through 64 steps at a time and **follows the playhead** during playback and step recording, so later steps can be watched, performed and overdubbed live. A 256-step overview strip shows the whole pattern and jumps between pages.
- **MIDI.** The standalone app now enables every connected MIDI input automatically, including devices plugged in while it runs; the new **MIDI** button lists inputs (switch any off) and opens the full Audio/MIDI settings. **MIDI learn:** right-click any knob, PLAY or RECORD, choose *MIDI Learn* and move a controller. Mappings are listed and cleared in the MIDI panel, and saved globally so they survive preset changes. In a DAW, notes and controllers arrive through the host's MIDI routing.
- **Audio stability.** Blocks larger than the size announced in `prepareToPlay` are now rendered in prepared-size pieces instead of overrunning the oversampler, chorus and metronome buffers (hosts may deliver these). A guard silences and resets voices and effect state if a non-finite sample ever appears, so one bad sample can no longer leave delay/reverb/chorus feeding constant noise. See *Known issue* below.
- **Song demos rebuilt** for all 47 earlier artist patches and **13 new patches** (see *Song-reference demo sequences*). All 64 user presets are rendered in CI-style checks for finite, bounded audio, clean stops and balanced loudness.

### Known issue: occasional white noise in the standalone app

Reported in live use, mostly with the step sequencer running: audio sometimes turns to loud white noise. It has **not been reproduced** offline. What has been ruled out:

- **Synth or effects running away.** The filters, chorus, delay and reverb are stable or tanh-limited, and non-finite audio is caught and reset (since 0.6.0). 600 randomized sequencer sessions (presets, sequencer length/rate/gate, engine, record/play, notes and knobs changed mid-play) produced no noise. The few loud, bright windows found were tonal sound with the filter opened, not noise.
- **The noise oscillator.** Even at full level through an open filter on an eight-note chord it reaches about RMS 0.11, so a louder wall of noise is not a sound NORYN makes on purpose.
- **Audio-driver underruns staying stuck.** NORYN's standalone ALSA output was reproduced with the same settings into PipeWire. A 250 ms stall and two seconds at 115% CPU both produced dropouts and crackle, and the output recovered cleanly as soon as processing caught up.

Remaining suspects are outside the synth's sound path: the audio device or driver, or controller data. One example would be a USB hub dropping out and sending a burst of MIDI that moves learned controls. If it happens, the black-box recorder (0.7.1) saves what NORYN was outputting and its full state to `~/.config/NORYN/incidents/`. If those files show clean audio while you heard noise, the fault is after NORYN. A 512 or 1024 sample buffer is still recommended.

## v0.4.0 changes

The factory bank expands to **290 sounds plus Init**. This release adds original musical-reference studies, two Solitude candidates, an optional six-operator FM path, phase-linked DCO architecture, nonlinear ladder filtering, curved contours, ensemble chorus and mono/legato glide. **87 parameters** are exposed; every control has a UI binding.

A critical ADSR integration bug is fixed: small sustain values could stretch short release times into long tails. Sequencer notes now retain independent chord durations and velocities; Stop/Clear reliably release notes; playback uses sample deadlines; and an 80 ms sequencer release limit prevents pad tails accumulating. The running-build fix was confirmed by the user.

The artist/song names indicate **original interpretations**, not extracted patches, commercial emulations or verified exact reconstructions. The Solitude candidates were described as very close in listening feedback; timbre and movement were then refined. The broader bank has received architectural corrections and remains open to reference-specific refinement. See [the sound review](AUTHENTICITY.md) for concrete limits.

## Screenshots

| Synthesizer | 256-step sequencer |
| --- | --- |
| ![Synthesizer](preview.png) | ![Sequencer](sequencer.png) |
| **Four LFOs** | **Scale engine** |
| ![Four LFOs](lfos.png) | ![Scale engine](scales.png) |

[Voice Design controls](character.png) expose the new models. The new [Signature Collection browser](signature-bank.png) supports artist and song search. Listen to the [48-second signature demo](signature-demo.wav).

## Launch

Run `./run.sh`. Build outputs:

- `build/Noryn_artefacts/Release/Standalone/NORYN`
- `build/Noryn_artefacts/Release/VST3/NORYN.vst3` — copy the complete bundle into `~/.vst3/` and rescan your DAW.

MIDI inputs are enabled automatically in the standalone app; use the **MIDI** button to switch inputs off, open Audio/MIDI settings or map controllers. Play a MIDI controller, click the piano, or use A/W/S/E/D/F/T/G/Y/H/U/J. Space auditions the current patch; Page Up/Down browse. STOP clears notes, transport, recording and effect tails. Pitch bend spans two semitones; sustain and velocity work. CC1 opens the filter by up to three octaves; it does not create a hidden LFO route.

## Knobs and displays

All knobs use a 250-pixel full sweep. Hold Shift before dragging for fine adjustment. Values **clamp at minimum and maximum**; they do not wrap around. Double-click resetting is disabled. Right-click for an explicit reset. Values can be typed with units, e.g. `350 Hz`, `4 kHz`, `120 ms`, `1.5 s`, `35 %` or `-12 ct`.

A violet outer marker/arc shows the effective value of a control being modulated. The manually set knob value stays fixed. Hovering identifies the LFO sources. Only explicitly assigned LFO destinations show modulation. Other LFOs can independently target other controls; inspect all four assignments if several controls move.

The oscillator, filter, ADSR and LFO graphs are **setting views**, not audio scopes: they stay still when a note plays and change only with their relevant controls. OSC B never changes OSC A's display. The LFO graph shows a fixed two-second window and responds to rate, waveform and source gain/amount. OSC A's static neighboring traces show wavetable spread. OSC B's pulse edge changes the rounded/square character of its pulse. Controls that do not apply to the selected engine are dimmed.

Only the dedicated stereo output scope, FFT spectrum and level meters follow playback. These update at 30 fps from actual audio; they are not decorative waveform animations. The spectrum uses a 1,024-point Hann window. The filter graph approximates the response at the base cutoff/resonance/mode; it is not a measured response analyzer.

## Sound-reference review

The artist/song bank is undergoing a full reference review after listening feedback. The two Solitude studies were described by the user as very close, with timbre and movement still needing refinement. The other interpretations have received an architectural character pass, but are not certified reconstructions. [AUTHENTICITY.md](AUTHENTICITY.md) and [the per-patch review record](presets/reference-review.json) state the remaining limitations. Automated control/audio checks do not establish likeness to a recording.

## Voice Design

**VOICE DESIGN** adds independent/phase-linked DCO architecture, legacy SVF/nonlinear ladder filtering, curved contours, variable note-start phase, slow voice drift, square-sub blend, mono/retrigger/legato with last-note priority, portamento, B semitone interval, fine tuning and adjustable bend range. Two stereo ensemble chorus modes complement the legacy chorus. An explicit Pitch destination extends LFO routing; only applicable, assigned controls show modulation.

The optional six-operator FM structures use Google's Apache-licensed MSFA operator kernels/router with six independent NORYN envelopes and original voice designs. Legacy two-operator FM remains the default for older states. The fixed-level FM waveform preview is not an audio animation. These models improve available sound design; they do not claim circuit-identical hardware emulation.

## Sounds

290 editable factory patches plus Init: the original 96 sounds, 192 musical-reference interpretations and two Solitude studies across Pads, Basses, Leads, Keys, Plucks, Textures, Motion and Brass & Strings. Search by sound name, artist, song or playing tip; combine words to narrow results. Categories filter the browser. Arrows cycle within a category. PLAY PATCH auditions a suitable note or chord. [PRESETS.md](PRESETS.md) lists the bank.

The Signature Collection includes funk/disco and synth-pop bass, acid/techno, Reese/modern bass, classic solo leads, poly brass, festival/trance, melodic plucks, house/garage keys, FM keys/bells, organs, vintage pads, cinematic ambience, vocal/digital textures, rhythmic motion and organic plucks. References range from Stevie Wonder, Herbie Hancock, Kraftwerk, Vangelis, Jean-Michel Jarre, Prince, Depeche Mode and New Order to Daft Punk, Tiesto, Armin van Buuren, deadmau5, Avicii, Skrillex, Disclosure, Bicep, Four Tet and many more. Every new patch has a musical reference and playing tip in its description (hover to read the full text); MCP listings include descriptions too.

These are original NORYN interpretations of musical palettes, not extracted commercial patches, recordings, exact recreations or artist endorsements. They use only the instrument's synthesis engines. The original program numbers 0–96 stay unchanged for existing projects. [PRESETS.md](PRESETS.md) lists every patch and reference.

Five synthesis engines make the bank more varied:

- **Hybrid WT:** four interpolated 16-frame wavetable families: Classic, Spectral, Formant and Digital. Seven harmonic bands; static frame spread across unison layers; phase modulation.
- **FM Operator:** the legacy carrier/modulator voice plus twelve optional six-operator structures with independent envelopes, original ratios and feedback routing, for tines, bells, mallets, organs and bass.
- **Plucked String:** a noise-excited, fractionally tuned feedback delay with damping for string-like plucks.
- **Drawbar Organ:** additive partial mixtures and a fast sustained contour.
- **Analog Stack:** detuned triangle/saw layers alongside the polyBLEP analog oscillator.

Six oscillators per note at full unison, 16-note polyphony, stereo spread, independent oscillator octaves, sine sub and noise. Oscillator B blends saw and variable-width pulse, with adjustable pulse edge. A resonant state-variable filter offers 12/24 dB low pass, high pass and band pass, drive, key tracking, velocity and an independent ADSR. The oscillator/drive/filter path runs at 2x sample rate and reports its latency to the host.

Stereo chorus, variable-time ping-pong echo, feedback, algorithmic reverb, stereo width and output gain complete the chain. Output soft saturation bounds peaks below full scale. The master also controls metronome output. Preset loudness depends on pitch, velocity, voicing and envelope stages.

The legacy 24 dB mode cascades state-variable stages. Voice Design also offers a nonlinear ladder model. Extreme phase/FM settings can still alias despite oversampling. These are custom algorithms; “best free synth DSP” is not a claim this project makes.

## Four LFOs

Open **4 LFO ROUTING** or use ROUTE LFO. Each LFO has its own rate, shape, bipolar amount and one destination:

Off, filter cutoff, resonance, WT position, A/B mix, saw/pulse blend, pulse width, B detune, phase-modulation index, unison, drive, noise, sub, amplitude, pan or pitch.

There are **no legacy hidden sends** to the filter or oscillators. Off means no modulation. Changing a destination removes the old assignment. LFO 1 additionally has a source-gain control on the Synth page; it scales that LFO's assigned amount. WT Spread and Pulse Edge are static tone controls, not extra LFO sends. Older session states are migrated to this explicit-routing model.

LFOs are per-voice and restart with a note. Modulation indicators show the most recently played active voice's effective values. Shape displays do not move with LFO phase or incoming audio.

## 256-step sequencer

Open **256 STEP SEQUENCER**:

- PLAY / STOP SEQ control transport. BPM spans 30–300.
- Length spans 1–256 steps. The pad grid shows 64 steps at a time; the **1-64**, **65-128**, **129-192** and **193-256** buttons switch pages, and the footer shows the length and playing step. With **FOLLOW PLAYHEAD** on (the default), the grid turns to the page being played, or to the step-record cursor, so you can watch and overdub live on any page. Choosing a page during playback switches follow off so you can edit another page; tick it again to resume. The strip under the footer shows all 256 steps (filled steps, loop length, visible page, playhead); click it to jump to a page. Division is quarter, eighth, sixteenth or thirty-second notes. Note-on/off events use sample deadlines, without scanning every MIDI note on every audio sample.
- Click a pad to toggle the entry note. Right-click a pad to inspect its note/velocity. Shift-click clears it. Pads can hold chords; MIDI recording and MCP can enter several notes in a step.
- Entry velocity, note length in steps, and global gate control articulation. Each recorded chord note retains its own velocity and duration. Pads show length; hover to inspect every note. Right-click also recalls its entry length.
- **Release limit** defaults to 80 ms for sequencer voices, so a short gate does not inherit a pad's multi-second tail. Enable **PATCH RELEASE** for the preset's full tail. Live keyboard envelopes are independent.
- Stop, Clear, pattern replacement and removal of sounding notes release pending sequence notes. Stop also clears sequencer sustain/sostenuto state.
- **LIVE RECORD:** enable RECORD to start transport and overdub incoming MIDI on the nearest step. Held note durations are recorded per note in steps; the global gate remains a playback articulation control.
- **STEP RECORD:** RECORD accepts a note or chord into the selected step; releasing its keys advances the cursor. Transport stays stopped.
- METRONOME and Click Level provide a quarter-note click with a bar accent. The beat clock remains independent of pattern length.
- Empty, Beat pulse, Acid steps, Dream arpeggio and Chord walk provide starting patterns.

Playback uses MIDI channel 16 internally so its note-offs do not stop live channel-1 notes. Tempo is internal; DAW transport/PPQ sync and sequencer MIDI output are not implemented. The sequencer, scale and tempo are preserved when changing factory sounds. Full session state and user presets recall the pattern and control settings; transport does not auto-start on reopening.

## Scale engine

**SCALE ENGINE** includes all twelve roots and 37 choices: chromatic, major/minor and their modes, harmonic/melodic minor, major/minor pentatonic, blues, whole-tone, both diminished forms, harmonic/double-harmonic major, Phrygian/Lydian dominant, altered, Hungarian, Neapolitan, Persian, Japanese pentatonic forms, Egyptian, bebop, augmented, tritone and Custom.

Click pitch classes to build any nonempty custom twelve-tone scale. Choose nearest, up or down quantization. Sequencer notes use the scale; live MIDI quantization is optional. Matching note-offs are retained when root/scale changes, and colliding quantized notes use reference counts to avoid stuck or prematurely released notes.

## Solitude chord test

Two saved user presets are bundled and installed by `run.sh` without overwriting existing files:

- **M83 Solitude - Swell Chord Test:** the current swell study with an eight-bar chord test.
- **M83 Solitude - Electric Piano Test:** a synthesized six-operator electric-piano alternative with the same pattern.

Select either under User patches, open **256 STEP SEQUENCER**, and press **PLAY**. The arrangement uses 64 eighth-note steps at 61 BPM: Cm7, Bbadd9, F, Fm(add9), Ebmaj7, Abmaj7, Dm7b5/Ab, G7sus4 and G7. The final two chords share the last bar. Quantization is chromatic to retain the borrowed major/dominant notes. Each chord note has its own gate length and velocity. This is a piano-style harmonic test arrangement, not a complete transcription of the recorded performance.

The [JSON pattern](examples/solitude-chords.json) and [MIDI file](<examples/Solitude Chord Study.mid>) can be reused in a DAW. The pattern was created and recalled through the real Cloudflare Code Mode MCP tool. `mcp/server/solitude-test.mjs` can create new uniquely named test copies in a running instance; it replaces that instance's current pattern and leaves transport stopped.

## Stolen Dog keys test

**Burial Stolen Dog - Hazy Keys** is a saved user preset with a 16-bar, 256-step pattern at 115.5 BPM. Select it under User patches, open **256 STEP SEQUENCER**, and press **PLAY**. Bar-synchronous analysis of the official preview shows a steady four-bar cycle, F#m9 – Dadd9 – C#m7 – C#m/E, with A/E answering in every second pass; the study starts on F#m9 and its second eight bars move F#m9 over A with a light top-line lift. Six-operator keys, a softened ladder filter, subtle pitch movement, ensemble and tempo echo create the hazy texture. Each bar holds the bass for 15 steps under a 5-step chord, two off-beat chops and a closing top note; sequence release is limited to 100 ms.

The study uses C#m7, Eadd9, F#m9 and Dmaj9 with chromatic quantization. It is an original synthesized interpretation informed by [Burial’s recording](https://burial.bandcamp.com/track/stolen-dog), not a sampled recreation or complete transcription. The [JSON pattern](examples/stolen-dog-study.json), [MIDI file](<examples/Stolen Dog Hazy Keys Study.mid>) and native preset are bundled. [Listen to the synthesized study](stolen-dog-study.wav). No reference audio is distributed. `mcp/server/stolen-dog-test.mjs` creates and verifies a uniquely named copy through Cloudflare Code Mode; it replaces the running instance’s current pattern and leaves playback stopped.

## User presets and MCP

User patches are saved locally in `~/.config/NORYN/presets` and appear in the browser under User patches. Session saves include all 87 automatable parameters, preset identity and the complete pattern. Preset creation/loading is available through MCP; the standalone Options menu also supports sound-state saving.

The [NORYN Control plugin](mcp/README.md) uses the real Cloudflare Code Mode SDK and Dynamic Worker sandbox. One `code` MCP tool composes native operations for inspecting/editing sounds, saving presets, programming sequences/chord progressions, choosing scales, mapping LFOs and transport. It runs locally through Cloudflare's workerd runtime; no hosted Cloudflare deployment or account is required for this local setup.

The native bridge processes commands on the message thread and uses private per-instance directories. It does not poll files or wait on MCP in the audio callback. Local HTTP services bind to loopback and require private bearer tokens; credentials are excluded from packages. The plugin bootstrap installs its locked Node dependencies and starts the local services if needed. Open the synth before calling its native operations.

`Code Mode Glass Orbit` is a saved example created and recalled through Code Mode: a D-major I–vi–IV–V progression at 110 BPM, using an FM key sound and LFO 2 on cutoff. Select it under User patches, then open the sequencer to play it.

## Build and verify

Use the repository or matching source archive for builds. Requires CMake 3.22+, C++17, Git and internet for the initial JUCE 8.0.6 fetch. Debian/Ubuntu dependencies include ALSA, X11/Xrandr/Xinerama/Xcursor, FreeType, Fontconfig and OpenGL development packages.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Noryn_All NorynSmoke -j2
NORYN_NO_MCP=1 xvfb-run -a build/NorynSmoke_artefacts/Release/NorynSmoke .
```

The checks cover 68 sound controls with rendered audio comparisons; every UI parameter binding and minimum; disabled wraparound/accidental reset; all 290 presets and five engines; signature parameter bounds, unique states, output levels and artist/song search; full parameter/pattern recall; 64-step timing at 120/240 BPM, 256-step recall, loop wrap and playhead follow; MIDI learn for parameters and transport; oversized host blocks; live and chord step recording; exhaustive scale/root/direction/MIDI-note bounds; collision-safe note-offs; matching scope/audio samples; static setting graphs; OSC-display independence; LFO rate/depth graph response; moving modulation indicators with fixed base values; destination isolation and no audio modulation with routes Off; and finite output at 44.1/48/96 kHz with 16/64/127/512-sample blocks.

The ADSR integration now preserves the release rate established at note-off. Previously, reapplying unchanged parameters each block could stretch a low-sustain patch into an extremely long tail. Regression checks cover sustain values near zero, edits during release, exact per-note gates, Stop/Clear, five-minute loop accounting and bounded short-gate audio. Physical-string excitation is initialized only for the physical-string engine.

Reports: `validation.json`, `control-sensitivity.csv`, `native-knob-verification.json`. Screenshots: `preview.png`, `sequencer.png`, `lfos.png`, `scales.png`, `preview-small.png`. `demo.wav` samples the eight original preset categories. `signature-demo.wav` is a 48-second tour of the 16 new signature families, using original notes/chords rather than song melodies. `preset-levels.csv` reports RMS and peak for every tested patch. Native mouse testing also verifies that dragging 70% below the minimum stays at zero after release/double-click. Offline verification does not replace listening or validation across DAWs.

NORYN is released under [AGPL-3.0](LICENSE). JUCE uses its AGPLv3 open-source option in this build; dependency notices are in [THIRD_PARTY.md](THIRD_PARTY.md). Binary releases include the license and a matching source archive with JUCE 8.0.6. Windows/macOS binaries require builds on those platforms. User wavetable import, a multi-slot arbitrary modulation matrix and host tempo sync remain future work.

## Radiohead — Everything in Its Right Place studies

Two saved User patches are included: **Radiohead Everything - Pulsing Keys** and **Radiohead Everything - Vocal Texture**. Each recalls a 64-step chord study at 136 BPM. Select a patch, open the sequencer and press PLAY. The keys use rounded analog tones; the vocal texture uses the formant wavetable with slow position/filter modulation. These are original synthesized studies; the vocal patch approximates the processed-vocal character with synthesis. Pattern JSON files are in `examples/radiohead-keys.json` and `examples/radiohead-vocal.json`.

## Additional Burial studies

Five saved User patches add original sounds and 64-step arrangements: **Archangel - Ghost Choir** (132 BPM), **Near Dark - Shadow Keys** (135), **Raver - Night Organ** (135), **Kindred - Broken Chord** (134), and **Come Down to Us - Warm Halo** (100). Names begin with `Burial` in the browser. Select one, open the sequencer and press PLAY. The pack spans formant choir, FM keys, organ, dark analog stabs and a slow pad. These are musical inspirations with original chord arrangements, rather than transcriptions or verified reconstructions. Pattern data is in `examples/burial-pack.json`.

## Radiohead and Burial expansion

Twelve further User patches each recall a 64-step original arrangement. Radiohead studies: **Idioteque - Frozen Chords**, **Kid A - Glass Music Box**, **All I Need - Low Gravity**, **Motion Picture Soundtrack - Organ**, **Weird Fishes - Interlocking Pluck**, and **Daydreaming - Distant Keys**. Burial studies: **Untrue - Hollow Choir**, **Shell of Light - Amber Keys**, **Etched Headplate - Rain Bells**, **Ashtray Wasp - Fractured Stab**, **Rival Dealer - Signal Flare**, and **Night Bus - Empty Streets**. Browser names include the artist prefix. Open the sequencer and press PLAY after selecting a patch. These are original synthesized interpretations with original arrangements, not verified song recreations. Pattern data is in `examples/radiohead-burial-expansion.json`.

## Father John Misty studies

Four User patches include original 64-step arrangements: **The Palace - Warm Piano** (72 BPM), **Chloe - Velvet Strings** (86), **Gods Favorite Customer - Sunday Organ** (80), and **Mr Tillman - Soft Electric Keys** (100). Names begin with `Father John Misty`. Select a patch, open the sequencer and press PLAY. Piano and strings are synthesized approximations; these are palette studies with original chords, not transcriptions or verified recreations. Their tempos are arrangement choices. JSON patterns are in `examples/father-john-misty-studies.json`. Album references: [God’s Favorite Customer](https://www.subpop.com/releases/father_john_misty/gods_favorite_customer) and [Chloë and the Next 20th Century](https://fatherjohnmisty.bandcamp.com/album/chlo-and-the-next-20th-century).

## Depeche Mode studies

24 saved User patches span bass, bells, metallic strikes, choir, strings, organ, plucks and leads. Each has an original 64-step arrangement and its own tempo. Select a patch beginning with `Depeche Mode`, open the sequencer and press PLAY. These are synthesized musical inspirations with original arrangements, not verified song reconstructions. Tempos are arrangement choices. Pattern data: `examples/depeche-mode-studies.json`.

- **Enjoy the Silence - Dark Bass** — 113 BPM
- **Enjoy the Silence - Cathedral Pad** — 113 BPM
- **Personal Jesus - Machine Bass** — 130 BPM
- **Personal Jesus - Metallic Strike** — 130 BPM
- **Never Let Me Down Again - Driving Bass** — 106 BPM
- **Never Let Me Down Again - Wide Strings** — 106 BPM
- **Strangelove - Bright Bell** — 118 BPM
- **Strangelove - Pulse Bass** — 118 BPM
- **Policy of Truth - Muted Bass** — 114 BPM
- **Policy of Truth - Glass Hook** — 114 BPM
- **World in My Eyes - Resonant Bass** — 122 BPM
- **World in My Eyes - Digital Air** — 122 BPM
- **Everything Counts - Clockwork Pluck** — 116 BPM
- **Everything Counts - Reed Organ** — 116 BPM
- **People Are People - Iron Bell** — 120 BPM
- **People Are People - Punch Bass** — 120 BPM
- **Black Celebration - Shadow Choir** — 108 BPM
- **Black Celebration - Low Pulse Bass** — 108 BPM
- **Waiting for the Night - Quiet Arp** — 94 BPM
- **Clean - Sub Bass** — 100 BPM
- **Walking in My Shoes - Haunted Pad** — 94 BPM
- **Precious - Silver Keys** — 110 BPM
- **Just Cant Get Enough - Bright Lead** — 128 BPM
- **Shake the Disease - Cold Pluck** — 108 BPM

## Song-reference demo sequences

81 Depeche Mode, Radiohead, Thom Yorke, Nine Inch Nails, The Cure, Portishead, UNKLE, The Verve, Richard Ashcroft, Father John Misty and Burial user patches recall song-specific demos of up to 256 steps (16 bars of sixteenths or 32 bars of eighths; *Everything in Its Right Place* uses six 10/4 bars). They replace the earlier generic arrangements described above, and no two share a pitch pattern. Select one under User patches, open **256 STEP SEQUENCER** and press **PLAY**.

- **65 note-level excerpts** (all Depeche Mode, Radiohead, Thom Yorke, Nine Inch Nails, The Cure, Portishead, UNKLE and The Verve patches, plus Burial *Archangel*): quantized excerpts of community MIDI or Online Sequencer transcriptions, matched to the patch role (bass part for bass patches, hook, piano or pad part for others). Each demo is a **section medley**: `scripts/plan_medleys.py` fingerprints every 4-bar phrase of the whole arrangement (bar-by-bar harmony plus which instruments play), groups similar phrases into section types, splits the stretch where the preset's own part plays into four regions from start to end, and takes the most representative phrase from each, preferring section types not yet used. The slices are stitched in song order (4 × 4 bars at 1/16; 4 × 8 bars for the 1/8 patches; three 2-bar sections for *Everything in Its Right Place*). Section boundaries are detected, not hand-marked, and a slice can start mid-phrase in songs with pickups or irregular bars. Sources, tracks, windows and file hashes: `examples/song-sequence-replacements.json`; rebuild with `scripts/replace_song_sequences.py`.
- **16 chord studies**: the four Father John Misty and two Richard Ashcroft patches use 32-bar intro/verse/chorus progressions from cited chord charts (Ashcroft tempos measured from the official previews). The other ten Burial patches use progressions estimated from the official preview audio and are low confidence; their second eight bars vary the rhythm. Data: `examples/chord-study-sequences.json`; rebuild with `scripts/write_chord_studies.py`.
- **13 new patches**: Radiohead *No Surprises - Music Box Bells*, *Pyramid Song - Sunken Piano*, *Videotape - Tape Piano*, *Reckoner - Falling Strings*, *Nude - Round Bass*, *Lotus Flower - Rubber Bass*; Thom Yorke *The Eraser - Loop Piano*; Depeche Mode *Master and Servant - Whip Riff*, *Stripped - Analog Bass*, *Halo - Choir Strings*, *New Life - Synthpop Lead*, *Photographic - Early Sequence*, *A Question of Time - Drive Bass*. Each starts from a related patch's sound with its own changes (`examples/new-reference-presets.json`, `scripts/add_reference_presets.py`).
- **Nine Inch Nails** (8 patches from 7 songs, community MIDI from BitMidi): *Closer - Squelch Bass* and *Closer - Dark Keys*, *Head Like a Hole - Saw Bassline*, *A Warm Place - Glass Pad*, *The Perfect Drug - Broken Piano*, *March of the Pigs - Saw Riff*, *Ruiner - Polysynth Lead* and *Down in It - Toy Bells*, voiced with more drive and grit than their template patches.
- **The Cure** (BitMidi): *A Forest - Rotary Riff*, *Lullaby - Pizzicato Strings*, *Just Like Heaven - Chime Lead*, *Close to Me - Breath Keys*, *The Lovecats - Walking Bass*. **Portishead** (Online Sequencer transcriptions, which cover only part of each song): *Roads - Tremolo Rhodes*, *Sour Times - Spy Riff*. **UNKLE**: *Rabbit in Your Headlights - Night Piano*. **The Verve** (BitMidi): *Bitter Sweet Symphony - Orchestral Hook*, *Lucky Man - Swelling Strings*, *Sonnet - Mellotron Flute*. **Richard Ashcroft** (no transcription available; chord-chart studies): *A Song for the Lovers - Warm Strings* (77 BPM), *Break the Night with Colour - Piano* (80 BPM).
- Loudness is balanced from rendered measurements (`scripts/balance_levels.py`); run `NorynSmoke <dir> --user-presets` to re-render every demo and write `user-preset-levels.csv`.

None have been listening-verified against the recordings. The excerpts reproduce melodies and bass lines from the named songs; they are included as playing demos of the sounds.
