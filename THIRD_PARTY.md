# Dependency notices

NORYN source and its local MCP plugin are licensed under AGPL-3.0-only. Copyright (C) 2026 stevenmcsorley and NORYN contributors.

## Native audio application

- **JUCE 8.0.6**, Copyright Raw Material Software Limited. This distribution uses JUCE's **AGPLv3** open-source license option. See [JUCE-LICENSE.md](licenses/JUCE-LICENSE.md) and the framework's original module headers.
- JUCE bundles dependencies including the Steinberg VST3 SDK, FLAC, Ogg/Vorbis, libpng, zlib, HarfBuzz, SheenBidi and platform support code. Their original licenses and notices are retained in the corresponding-source archive under `vendor/JUCE`. JUCE's root notice lists their locations. Unused platform modules are not necessarily linked into the Linux binaries.
- CMake fetches the exact JUCE `8.0.6` release. The source release includes that framework tree, preserving its notices. System libraries remain system dependencies and are not bundled into the Linux ZIP.

## MCP integration

The Node/TypeScript integration uses locked npm dependencies. These include `@cloudflare/codemode`, `agents`, the Model Context Protocol SDK/client/server, `zod`, TypeScript and Wrangler. Their license metadata and original notices remain in the packages installed by `npm ci`; `package-lock.json` records the complete dependency graph. No npm credentials or user authentication tokens are included.

## Presets and visuals

The factory parameter bank and native UI drawing code were created for this project. The design takes visual inspiration from the supplied NORYN concept and the SONAVYRE module. No proprietary commercial synth presets, samples, oscillator code or third-party logo assets are bundled.

## Corresponding source

The main release provides `NORYN-0.6.0-source.tar.gz`, containing this project's sources and the JUCE 8.0.6 source tree needed for the native build. Build it without fetching JUCE:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/vendor/JUCE"
cmake --build build --target Noryn_All NorynSmoke -j2
```

The optional MCP plugin installs its locked npm dependencies with `npm ci`. No public Cloudflare account or deployment is required for the local workerd setup.

## Google MSFA FM core

The optional six-operator FM structures use the operator kernels, sine lookup and algorithm router from [Google Music Synthesizer for Android](https://github.com/google/music-synthesizer-for-android), commit `f67d41d313b7dc85f6fb99e79e515cc9d208cfff`. Copyright 2012–2013 Google Inc., Apache License 2.0. The original notices are retained in `Source/ThirdParty/msfa`, with the full license in `licenses/MSFA-APACHE-2.0.txt`. `NORYN-NOTICE.md` records the small portability changes. The source archive includes this code.

NORYN supplies its own six independent operator envelope generators and original voice designs. No DX7 ROM/factory banks, artist preset files or reference recordings are bundled. This integration does not claim component-accurate DX7 emulation.
