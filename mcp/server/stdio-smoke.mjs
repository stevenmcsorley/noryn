import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StdioClientTransport } from "@modelcontextprotocol/sdk/client/stdio.js";
import assert from "node:assert/strict";
import path from "node:path";
import { fileURLToPath } from "node:url";
const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const client = new Client({
  name: "noryn-stdio-verification",
  version: "0.5.0",
});
await client.connect(
  new StdioClientTransport({
    command: process.execPath,
    args: [path.join(root, "server/bootstrap.mjs")],
    cwd: root,
    stderr: "pipe",
  }),
);
try {
  const tools = await client.listTools();
  assert.equal(tools.tools.length, 1);
  assert.equal(tools.tools[0].name, "code");
  const result = await client.callTool({
    name: "code",
    arguments: {
      code: 'async () => {return {transport: "stdio", runtime: "Cloudflare Code Mode", ok: true};}',
    },
  });
  assert(!result.isError, JSON.stringify(result));
  console.log(
    "PASS: portable plugin stdio startup, tool discovery, isolated code execution.",
  );
} finally {
  await client.close();
}
