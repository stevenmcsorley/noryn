# Sound-reference scope and review status

The first 192-reference bank was made with shared sound-design templates and parameter variations. It has **not** been verified as a set of faithful reconstructions. Its automated tests establish working controls, valid states and audible, bounded output; they do not establish resemblance to a recording. v0.4.0 includes the engine, stability and architectural corrections described below; individual musical references remain open to refinement.

## Concrete reference: M83 — Solitude

Two separate candidates are being developed:

- **Original Lead:** mono legato, last-note priority, glide, adjustable pitch bend and a pitch-LFO route. It needs comparison to the solo, including its performance articulation.
- **Reinterpretation Swell:** the Felsmann + Tiley version's low brass-like swell, with separate amplitude/filter contours and a layered oscillator/sub balance. A single preset does not reproduce every layer of the finished recording.

The user described the first candidates as very close and identified timbre/movement for refinement; a restrained second pass adjusted those aspects. They remain studies, not verified exact recreations. The rendered examples use original phrases/chords, not the recorded song's melody. Official preview audio used for analysis is kept outside this repository and is not distributed.

## Engine changes

- Optional nonlinear ladder filter from JUCE, alongside the existing state-variable filter.
- Curved amplitude and filter contours; LCDs use the same shaping function and remain stationary during playback.
- Per-note phase variation and slow per-voice drift.
- Square sub blend that follows oscillator A's octave, alongside the legacy MIDI-root sine sub.
- Per-channel mono/retrigger/legato with last-note priority and pitch glide.
- Oscillator B semitone interval, master fine tuning, adjustable bend range and an explicit pitch LFO destination.
- Two original stereo ensemble chorus modes with opposite-phase, bandwidth-limited delay lines. These are not component-level JUNO emulations.

Defaults preserve the previous signal path. The new models are opt-in. Functional/audio tests cover all controls and patches; those tests do not certify likeness to a recording.

## Remaining gaps across the broader bank

An optional six-operator FM path is integrated from the Apache-licensed Google MSFA operator core, with NORYN independent envelopes and original voice designs. It is covered by functional/audio checks and is not a full hardware emulation. The legacy two-operator path remains available. There is no sampled-piano/choir playback engine, dedicated hard-sync/ring-mod model or component-accurate model of every referenced synthesizer. Some artist-only references do not define a unique target sound. An architectural character pass now applies mono articulation, curved contours, phase behavior and filter/chorus choices to the 192 first-pass patches according to their role, with several record-specific corrections. This is not a reference-comparison pass. `presets/reference-review.json` tracks every entry. Every named patch needs review against an explicit part/recording; parameter changes alone cannot certify authenticity.

Useful primary references:

- [Roland's JUNO-106 architecture discussion](https://www.roland.com/uk/blog/roland-icon-series-juno-106-synthesizer/?lang=en-GB)
- [Official original Solitude audio](https://www.youtube.com/watch?v=YQVKQ794wmQ)
- [Official reinterpretation release links](https://m83.it/solitude-felsmanntiley)
- [Dexed's six-operator FM implementation](https://github.com/asb2m10/dexed)
- [Google's Apache-licensed FM synthesis core](https://github.com/google/music-synthesizer-for-android)

The Google MSFA operator core is integrated and covered by the validation suite; Dexed remains a research reference, not a bundled dependency.

## Sequencer and envelope stability correction

A confirmed integration error reapplied JUCE ADSR parameters on every audio block. That replaced the release rate calculated at note-off with a rate based on sustain. Near-zero sustain could produce a tail lasting far longer than the requested release. Envelope parameters are now updated only when needed, without disturbing an active release when unrelated ADSR controls change.

The sequencer now records per-note chord durations/velocities, schedules absolute sample deadlines, releases pending notes on Stop/Clear/pattern replacement, clears sequencer pedal states and offers an 80 ms default release limit. Long-loop event accounting, exact note-offs and the low-sustain release reproduction have dedicated tests. Physical-string buffers are no longer initialized for unrelated engines on every note-on.

## Burial — Stolen Dog study

`Burial Stolen Dog - Hazy Keys` is a saved six-operator keys interpretation with a 64-step, four-bar chord pattern at 115.5 BPM. Subtle pitch/filter LFOs, ensemble, echo and reverb supply movement. The arranged C#m7–Eadd9–F#m9–Dmaj9 progression uses longer bass gates beneath shorter upper-chord chops. Native preset recall and independent note lengths were verified through Cloudflare Code Mode. This is a representative harmonic/rhythmic study, not a complete transcription or a reconstruction of Burial's sample processing. The [official recording](https://burial.bandcamp.com/track/stolen-dog) is the reference; reference audio is not bundled.
