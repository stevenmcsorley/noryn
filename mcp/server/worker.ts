import { DynamicWorkerExecutor } from "@cloudflare/codemode";
import { codeMcpServer } from "@cloudflare/codemode/mcp";
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { createLegacyMcpHandler } from "agents/mcp";
import { z } from "zod";
interface Env {
  LOADER: WorkerLoader;
  NORYN_BRIDGE_URL: string;
  NORYN_BRIDGE_TOKEN: string;
  NORYN_MCP_TOKEN: string;
}
const instanceId = z
  .string()
  .optional()
  .describe(
    "Optional instance ID from get_state; defaults to the latest running synth.",
  );
function upstream(env: Env) {
  const server = new McpServer({ name: "noryn", version: "0.3.0" });
  async function call(op: string, args: unknown, id?: string) {
    const response = await fetch(`${env.NORYN_BRIDGE_URL}/api`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Authorization: `Bearer ${env.NORYN_BRIDGE_TOKEN}`,
      },
      body: JSON.stringify({ op, args, instanceId: id }),
    });
    const result = (await response.json()) as Record<string, unknown>;
    if (!response.ok || result.error)
      throw new Error(String(result.error || "Native control failed"));
    return {
      structuredContent: result,
      content: [{ type: "text" as const, text: JSON.stringify(result) }],
    };
  }
  server.registerTool(
    "get_state",
    {
      description:
        "Inspect actual live NORYN parameters, ranges, preset, transport and all 64 steps. Read before changing a sound.",
      inputSchema: { instanceId },
    },
    async ({ instanceId }) => call("state", {}, instanceId),
  );
  server.registerTool(
    "list_presets",
    {
      description:
        "List all 96 factory sounds plus Init and locally saved user patches.",
      inputSchema: { instanceId },
    },
    async ({ instanceId }) => call("presets", {}, instanceId),
  );
  server.registerTool(
    "set_parameters",
    {
      description:
        "Set native parameter values by ID. Values must be in the actual ranges returned by get_state. This is not normalized 0..1 input.",
      inputSchema: {
        values: z.record(z.string(), z.number().finite()),
        instanceId,
      },
    },
    async ({ values, instanceId }) =>
      call("setParameters", { values }, instanceId),
  );
  server.registerTool(
    "load_preset",
    {
      description:
        "Load a factory sound by index, preserving sequence, scale and tempo.",
      inputSchema: { index: z.number().int().min(0).max(96), instanceId },
    },
    async ({ index, instanceId }) => call("loadPreset", { index }, instanceId),
  );
  server.registerTool(
    "save_preset",
    {
      description:
        "Save the actual edited sound, sequencer and scale as a named local user preset.",
      inputSchema: { name: z.string().min(1).max(80), instanceId },
    },
    async ({ name, instanceId }) => call("savePreset", { name }, instanceId),
  );
  server.registerTool(
    "load_user_preset",
    {
      description: "Recall a previously saved local user preset by exact name.",
      inputSchema: { name: z.string().min(1).max(80), instanceId },
    },
    async ({ name, instanceId }) =>
      call("loadUserPreset", { name }, instanceId),
  );
  const step = z.object({
    index: z.number().int().min(0).max(63),
    notes: z.array(z.number().int().min(0).max(127)).max(16),
    velocity: z.number().min(0.01).max(1).optional(),
    length: z
      .number()
      .min(0.05)
      .max(64)
      .optional()
      .describe(
        "Duration in sequencer steps, multiplied by global gate. Use 16 for a chord held over a bar of sixteenths.",
      ),
  });
  server.registerTool(
    "write_sequence",
    {
      description:
        "Write any subset of a 64-step polyphonic sequence. For chord progressions put chord MIDI notes at chosen steps and set length. replace=false preserves other steps.",
      inputSchema: {
        steps: z.array(step).max(64),
        replace: z.boolean().default(false),
        instanceId,
      },
    },
    async ({ steps, replace, instanceId }) =>
      call("sequence", { steps, replace }, instanceId),
  );
  server.registerTool(
    "chord_progression",
    {
      description:
        "Create diatonic triad or seventh-chord progressions from scale degrees 1..7. In minor, supply the minor scale intervals explicitly. Does not start playback.",
      inputSchema: {
        degrees: z.array(z.number().int().min(1).max(7)).min(1).max(16),
        tonic: z.number().int().min(24).max(84).default(48),
        intervals: z
          .array(z.number().int().min(0).max(11))
          .length(7)
          .default([0, 2, 4, 5, 7, 9, 11]),
        stepsPerChord: z.number().int().min(1).max(64).default(16),
        seventh: z.boolean().default(false),
        velocity: z.number().min(0.01).max(1).default(0.8),
        replace: z.boolean().default(true),
        instanceId,
      },
    },
    async ({
      degrees,
      tonic,
      intervals,
      stepsPerChord,
      seventh,
      velocity,
      replace,
      instanceId,
    }) => {
      if (degrees.length * stepsPerChord > 64)
        throw new Error("Progression exceeds 64 steps.");
      const notesFor = (degree: number) =>
        [0, 2, 4, ...(seventh ? [6] : [])].map((offset) => {
          const d = degree - 1 + offset;
          return tonic + intervals[d % 7] + 12 * Math.floor(d / 7);
        });
      const steps = degrees.map((degree, index) => ({
        index: index * stepsPerChord,
        notes: notesFor(degree),
        velocity,
        length: stepsPerChord,
      }));
      return call("progression", { steps, replace }, instanceId);
    },
  );
  server.registerTool(
    "transport",
    {
      description:
        "Set sequence play/record, BPM, live-vs-step recording and metronome. Recording live incoming MIDI quantizes it to the current step grid.",
      inputSchema: {
        playing: z.boolean().optional(),
        recording: z.boolean().optional(),
        stepRecord: z.boolean().optional(),
        bpm: z.number().min(30).max(300).optional(),
        metronome: z.boolean().optional(),
        instanceId,
      },
    },
    async ({ instanceId, ...args }) => call("transport", args, instanceId),
  );
  server.registerTool(
    "set_scale",
    {
      description:
        "Set scale index from parameter range/catalog, tonic 0=C..11=B, quantize direction 0=nearest/1=up/2=down, live MIDI quantization and optional custom twelve-bit pitch-class mask.",
      inputSchema: {
        scale: z.number().int().min(0).max(36),
        root: z.number().int().min(0).max(11).optional(),
        direction: z.number().int().min(0).max(2).optional(),
        live: z.boolean().optional(),
        mask: z.number().int().min(1).max(4095).optional(),
        instanceId,
      },
    },
    async ({ instanceId, ...args }) => call("scale", args, instanceId),
  );
  server.registerTool(
    "map_lfo",
    {
      description:
        "Set one of four LFOs. Shapes: 0 sine,1 triangle,2 saw,3 square. Destinations: 0 off,1 cutoff,2 resonance,3 WT position,4 A/B mix,5 saw/pulse,6 pulse width,7 B detune,8 phase modulation,9 unison,10 drive,11 noise,12 sub,13 amplitude,14 pan.",
      inputSchema: {
        lfo: z.number().int().min(1).max(4),
        rate: z.number().min(0.03).max(30).optional(),
        shape: z.number().int().min(0).max(3).optional(),
        destination: z.number().int().min(0).max(14).optional(),
        amount: z.number().min(-1).max(1).optional(),
        instanceId,
      },
    },
    async ({ instanceId, ...args }) => call("lfo", args, instanceId),
  );
  server.registerTool(
    "panic",
    {
      description:
        "Stop the sequencer, recording, held notes and effect tails.",
      inputSchema: { instanceId },
    },
    async ({ instanceId }) => call("panic", {}, instanceId),
  );
  return server;
}
export default {
  async fetch(request: Request, env: Env, ctx: ExecutionContext) {
    if (
      !env.NORYN_MCP_TOKEN ||
      request.headers.get("Authorization") !== `Bearer ${env.NORYN_MCP_TOKEN}`
    )
      return new Response("Unauthorized", { status: 401 });
    if (new URL(request.url).pathname === "/health")
      return Response.json({ name: "noryn-codemode", version: "0.3.0" });
    const server = await codeMcpServer({
      server: upstream(env),
      executor: new DynamicWorkerExecutor({ loader: env.LOADER }),
    });
    return createLegacyMcpHandler(server, { route: "/mcp" })(request, env, ctx);
  },
} satisfies ExportedHandler<Env>;
