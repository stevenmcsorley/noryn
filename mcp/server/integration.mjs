import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { promises as fs } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import assert from "node:assert/strict";
const root = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const token = (await fs.readFile(path.join(root, ".mcp-token"), "utf8")).trim();
const client = new Client({ name: "noryn-integration", version: "0.7.1" });
await client.connect(
  new StreamableHTTPClientTransport(new URL("http://127.0.0.1:8787/mcp"), {
    requestInit: { headers: { Authorization: `Bearer ${token}` } },
  }),
);
try {
  const result = await client.callTool({
    name: "code",
    arguments: {
      code: `async () => {
 const before=await codemode.get_state({});const instanceId=before.instanceId;
 const values=Object.fromEntries(Object.entries(before.parameters).map(([id,p])=>[id,p.value]));
 try {
   const signature=await codemode.load_preset({index:288,instanceId});
   if(signature.presetName!=='ODESZA Sunlit Pluck')throw new Error('Expanded preset index not loaded');
   await codemode.load_preset({index:37,instanceId});
   await codemode.set_parameters({values:{reverb:0.3,chorus:0.18,delay:0.18,seqgate:0.95,lfo1dest:0,seqlength:64,seqdivision:2,seqnote:60,seqvelocity:0.8,seqnotelength:1,clicklevel:0.4,customscale:2741},instanceId});
   await codemode.map_lfo({lfo:2,destination:1,amount:0.25,rate:0.13,shape:0,instanceId});
   await codemode.set_scale({scale:1,root:2,direction:0,live:false,instanceId});
   await codemode.chord_progression({degrees:[1,6,4,5],tonic:50,stepsPerChord:16,seventh:false,velocity:0.8,replace:true,instanceId});
   await codemode.transport({bpm:110,playing:false,recording:false,metronome:false,instanceId});
   await codemode.save_preset({name:'Code Mode Glass Orbit',instanceId});
   await codemode.load_preset({index:0,instanceId});
   const recalled=await codemode.load_user_preset({name:'Code Mode Glass Orbit',instanceId});
   if(recalled.parameters.lfo2dest.value!==1||Math.abs(recalled.parameters.lfo2rate.value-0.13)>0.001||recalled.parameters.root.value!==2||recalled.steps[0].notes.join(',')!=='50,54,57'||recalled.steps[48].notes.length!==3)throw new Error('Native preset/progression recall mismatch');
   return {verified:true,instanceId,signaturePreset:signature.presetName,preset:'Code Mode Glass Orbit',chords:recalled.steps.filter(s=>s.notes.length).length,scale:'D major',bpm:recalled.parameters.bpm.value,lfo2:{destination:recalled.parameters.lfo2dest.text,rate:recalled.parameters.lfo2rate.value}};
 } finally {
   await codemode.panic({instanceId});await codemode.load_preset({index:before.factoryPreset,instanceId});await codemode.set_parameters({values,instanceId});await codemode.write_sequence({steps:before.steps,replace:true,instanceId});
 }
}`,
    },
  });
  assert(!result.isError, JSON.stringify(result));
  console.log(JSON.stringify(result));
} finally {
  await client.close();
}
