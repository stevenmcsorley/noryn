# NORYN Control — Cloudflare Code Mode MCP

This local plugin controls the actual NORYN 0.3 native synth. It uses `@cloudflare/codemode`'s `codeMcpServer` and `DynamicWorkerExecutor`, running in Cloudflare workerd through Wrangler. One MCP `code` tool exposes typed `codemode` operations. This is a local runtime, not a deployed Cloudflare service.

## Connect

Requires Node 22+ and the running NORYN synth. A portable plugin manifest and `mcp.json` are included. Import this plugin folder through your MCP/plugin host, or configure the equivalent stdio server:

```json
{
  "mcpServers": {
    "noryn": {
      "command": "node",
      "args": ["/absolute/path/to/noryn/mcp/server/bootstrap.mjs"]
    }
  }
}
```

The bootstrap installs locked dependencies if missing, creates private local credentials, starts local services if needed, and connects the stdio proxy. It sends only MCP protocol traffic on stdout. Runtime diagnostics and tokens live under `~/.config/NORYN/mcp`; tokens are never packaged or returned by tools. `mcp.local.json` is a generated, machine-local config containing the current absolute entrypoint.

For development, `./start.sh` runs the broker at `127.0.0.1:8788` and Code Mode at `127.0.0.1:8787/mcp`. Both HTTP endpoints require the generated credentials. The native synth publishes a private per-instance command inbox; the broker selects the latest live instance unless an instance ID is supplied.

## Operations

- `get_state`: actual parameters, ranges, labels, choices, transport and 64 steps.
- `list_presets`, `load_preset`: 96 factory sounds plus Init.
- `set_parameters`: actual native values, validated before application.
- `save_preset`, `load_user_preset`: named local sound/pattern/scale states.
- `write_sequence`: polyphonic steps, velocity and duration; preserve or replace.
- `chord_progression`: scale-degree triads/sevenths, custom scale intervals and voicings.
- `map_lfo`: four independent LFOs, 14 destinations, bipolar amounts.
- `set_scale`: 37 scales/custom mask, root and quantize direction, optional live MIDI.
- `transport`: tempo, play/record mode, metronome.
- `panic`: stop notes, transport, recording and effect tails.

Example for the `code` tool:

```js
async () => {
  const state = await codemode.get_state({});
  const instanceId = state.instanceId;
  await codemode.load_preset({ index: 37, instanceId });
  await codemode.map_lfo({ lfo: 2, destination: 1, rate: 0.13, amount: 0.25, instanceId });
  await codemode.set_scale({ scale: 1, root: 2, direction: 0, live: false, instanceId });
  await codemode.chord_progression({
    degrees: [1, 6, 4, 5], tonic: 50, stepsPerChord: 16,
    seventh: false, velocity: 0.8, replace: true, instanceId
  });
  await codemode.transport({ bpm: 110, playing: false, instanceId });
  return codemode.save_preset({ name: "Glass Orbit", instanceId });
}
```

Inspect current parameter ranges and enum labels before editing. Source graphs remain static; mapped controls show effective modulation separately from their base value. Routes Off do not leave hidden oscillator/filter modulation active.

## Verify

```sh
npm ci
npm run check
npm test
node server/smoke.mjs --native
node server/stdio-smoke.mjs
```

`server/integration.mjs` tests native parameter edits, scale selection, LFO assignment, chord programming and saving/recalling `Code Mode Glass Orbit`, restoring the previous live settings afterward. It does not start playback.

Public cloud hosting is optional and not configured here. A remote Worker would need an authenticated reachable native bridge and proper authorization; a deployed Worker cannot directly reach this machine's loopback address. The local setup keeps the synth and control plane on the same computer.

Reference: [Cloudflare's Code Mode MCP guide](https://developers.cloudflare.com/agents/model-context-protocol/guides/build-codemode-mcp-server/).
