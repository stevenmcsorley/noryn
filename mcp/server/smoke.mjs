import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { promises as fs } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import assert from "node:assert/strict";
const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const token = (await fs.readFile(path.join(root, ".mcp-token"), "utf8")).trim();
const client = new Client({ name: "noryn-verification", version: "1.0.2" });
await client.connect(
  new StreamableHTTPClientTransport(new URL("http://127.0.0.1:8787/mcp"), {
    requestInit: { headers: { Authorization: `Bearer ${token}` } },
  }),
);
try {
  const list = await client.listTools();
  assert.equal(list.tools.length, 1);
  assert.equal(list.tools[0].name, "code");
  console.log("MCP discovery: one Cloudflare Code Mode tool.");
  const sandbox = await client.callTool({
    name: "code",
    arguments: { code: "async () => { return { answer: 6 * 7 }; }" },
  });
  assert(!sandbox.isError, JSON.stringify(sandbox));
  console.log("Cloudflare isolated execution:", JSON.stringify(sandbox));
  if (process.argv.includes("--native")) {
    const result = await client.callTool({
      name: "code",
      arguments: {
        code: "async () => { const state = await codemode.get_state({}); const bank = (await codemode.list_presets({instanceId: state.instanceId})).factory; const signatures = bank.filter(p => p.description?.startsWith(\"Inspired by \")); const daft = bank.filter(p => /daft punk/i.test(p.name + \" \" + p.description)); if (state.version !== \"1.0.2\" || bank.length !== 291 || signatures.length !== 192 || daft.length < 6) throw new Error(\"Signature bank or searchable references missing\"); return { version: state.version, presets: bank.length, signaturePresets: signatures.length, daftPunkMatches: daft.length, parameters: Object.keys(state.parameters).length, steps: state.steps.length }; }",
      },
    });
    assert(!result.isError, JSON.stringify(result));
    console.log("Live native verification:", JSON.stringify(result));
  }
} finally {
  await client.close();
}
