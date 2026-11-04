import { runtimeDir } from "./runtime-paths.mjs";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  ListToolsRequestSchema,
  CallToolRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";
import { promises as fs } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const token = (
  await fs.readFile(path.join(runtimeDir, "mcp-token"), "utf8")
).trim();
const remote = new Client({ name: "noryn-local-client", version: "0.3.0" });
await remote.connect(
  new StreamableHTTPClientTransport(new URL("http://127.0.0.1:8787/mcp"), {
    requestInit: { headers: { Authorization: `Bearer ${token}` } },
  }),
);
const local = new Server(
  { name: "noryn-codemode", version: "0.3.0" },
  { capabilities: { tools: {} } },
);
local.setRequestHandler(ListToolsRequestSchema, () => remote.listTools());
local.setRequestHandler(CallToolRequestSchema, (request) =>
  remote.callTool(request.params),
);
await local.connect(new StdioServerTransport());
process.on("SIGINT", async () => {
  await remote.close();
  await local.close();
  process.exit(0);
});
process.on("SIGTERM", async () => {
  await remote.close();
  await local.close();
  process.exit(0);
});
