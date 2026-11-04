---
name: noryn-control
description: Control the running local NORYN synthesizer to design sounds, save presets, program its 64-step polyphonic sequencer, write chord progressions, select scales or assign four LFOs through Cloudflare Code Mode.
---

Use the NORYN `code` tool. It executes JavaScript in Cloudflare's isolated Dynamic Worker runtime, with typed `codemode` operations for the local synthesizer.

Start with `codemode.get_state({})` and `codemode.list_presets({})`. Inspect current values, parameter ranges and choice labels. Native parameter values are not normalized 0..1. Use the actual enum indices from the returned choices.

The 288-sound factory bank includes 192 original signature patches. Search returned names and descriptions for artist/song references; filter results inside Code Mode instead of returning the entire bank. Use the returned indices, not guessed program numbers. References describe inspiration, not exact recreations or artist endorsements.

Preserve transport, scale and existing steps unless the user's requested change requires modifying them. `write_sequence` defaults to preserving other steps; `replace: true` explicitly replaces the sequence. `chord_progression` generates triads or sevenths from scale degrees and does not start playback. Ask the user to choose a musical direction only when necessary; otherwise choose an appropriate scale, key and voicing.

Keep oscillator, filter, envelope and LFO graphs stationary during playback. They visualize settings. Mapped controls show their effective modulation separately from the manually set value. Never repeatedly overwrite base parameters to imitate LFO movement; assign a destination with `map_lfo` instead.

Save named sounds using `save_preset`. These are local user presets and appear under User patches. Verify the returned actual state after mutations. Use `panic` if a control task produces unwanted sustained sound. Do not automatically start transport solely to confirm settings; start or audition when the user requests playback or sound evaluation.

The local services must be running (`start.sh` in this package) and NORYN must be open. The server binds to loopback and reads credentials from private local files; never print or return tokens. If several synth instances exist, keep using the instance ID from the initial state.
