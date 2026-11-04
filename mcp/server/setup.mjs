import {promises as fs} from 'node:fs';import path from 'node:path';import crypto from 'node:crypto';import{fileURLToPath}from'node:url';import{runtimeDir}from'./runtime-paths.mjs';
const root=path.dirname(path.dirname(fileURLToPath(import.meta.url)));await fs.mkdir(runtimeDir,{recursive:true,mode:0o700});
async function token(name,legacy){const file=path.join(runtimeDir,name);try{return(await fs.readFile(file,'utf8')).trim();}catch{let value;try{value=(await fs.readFile(path.join(root,legacy),'utf8')).trim();}catch{value=crypto.randomBytes(32).toString('hex');}await fs.writeFile(file,value,{mode:0o600});return value;}}
const bridge=await token('bridge-token','.bridge-token'),mcp=await token('mcp-token','.mcp-token');
await fs.writeFile(path.join(root,'.dev.vars'),`NORYN_BRIDGE_TOKEN=${bridge}\nNORYN_MCP_TOKEN=${mcp}\n`,{mode:0o600});
// Legacy private files remain useful for standalone development test commands.
await fs.writeFile(path.join(root,'.bridge-token'),bridge,{mode:0o600});await fs.writeFile(path.join(root,'.mcp-token'),mcp,{mode:0o600});
await fs.writeFile(path.join(root,'mcp.local.json'),JSON.stringify({mcpServers:{noryn:{command:'node',args:[path.join(root,'server/bootstrap.mjs')]}}},null,2),{mode:0o600});
console.error('Local credentials and MCP client configuration prepared.');
