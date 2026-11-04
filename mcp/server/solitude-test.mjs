import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StreamableHTTPClientTransport } from '@modelcontextprotocol/sdk/client/streamableHttp.js';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { runtimeDir } from './runtime-paths.mjs';
const root=path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const token=(await fs.readFile(path.join(runtimeDir,'mcp-token'),'utf8')).trim();
const client=new Client({name:'noryn-solitude-pattern',version:'0.4.0'});
await client.connect(new StreamableHTTPClientTransport(new URL('http://127.0.0.1:8787/mcp'),{requestInit:{headers:{Authorization:`Bearer ${token}`}}}));
try {
 const result=await client.callTool({name:'code',arguments:{code:`async()=>{
 const before=await codemode.get_state({});const instanceId=before.instanceId;
 const bank=await codemode.list_presets({instanceId});
 const unique=(base)=>{let name=base,n=2;while(bank.user.includes(name))name=base+' '+n++;return name;};
 const swellName=unique('M83 Solitude - Swell Chord Test');
 const pianoName=unique('M83 Solitude - Electric Piano Test');
 const chords=[
 {index:0,label:'Cm7',notes:[48,55,58,60,63],length:8},
 {index:8,label:'Bbadd9',notes:[46,53,58,60,62],length:8},
 {index:16,label:'F',notes:[41,53,57,60,65],length:8},
 {index:24,label:'Fmadd9',notes:[41,53,56,60,67],length:8},
 {index:32,label:'Ebmaj7',notes:[39,51,55,58,62],length:8},
 {index:40,label:'Abmaj7',notes:[44,51,55,60,63],length:8},
 {index:48,label:'Dm7b5/Ab',notes:[44,50,53,60,62],length:8},
 {index:56,label:'G7sus4',notes:[43,50,53,60,62],length:4},
 {index:60,label:'G7',notes:[43,50,53,59,62],length:4}
 ];
 const steps=chords.map(chord=>({index:chord.index,notes:chord.notes,length:chord.length,velocity:.76,
 noteDetails:chord.notes.map((note,i)=>({note,velocity:i===0?.64:.74+i*.012,length:chord.length-(i===0?0:.2)}))}));
 if(before.factoryPreset!==290)await codemode.load_preset({index:290,instanceId});
 await codemode.transport({playing:false,recording:false,metronome:false,instanceId});
 await codemode.set_parameters({values:{bpm:61,seqlength:64,seqdivision:1,seqgate:.96,seqnotelength:1,seqrelease:.08,seqfollowrelease:0,scale:0,root:0,quantizelive:0},instanceId});
 await codemode.write_sequence({steps,replace:true,instanceId});
 await codemode.save_preset({name:swellName,instanceId});
 await codemode.load_preset({index:205,instanceId});
 await codemode.set_parameters({values:{engine:1,fmprofile:1,fm:.25,fmratio:2,balance:0,unison:0,sub:.02,attack:.003,decay:1.7,sustain:.15,release:.35,cutoff:6500,envamount:.1,chorus:.12,delay:0,reverb:.23,voicemode:0},instanceId});
 await codemode.save_preset({name:pianoName,instanceId});
 const final=await codemode.load_user_preset({name:swellName,instanceId});
 const populated=final.steps.filter(s=>s.notes.length);
 if(final.factoryPreset!==290 || populated.length!==9 || final.parameters.bpm.value!==61 || final.parameters.seqdivision.value!==1 || populated[0].notes.join(',')!=='48,55,58,60,63' || Math.abs(populated[0].noteDetails[1].length-7.8)>.01)throw new Error('Pattern recall mismatch');
 return {verified:true,loaded:swellName,pianoAlternative:pianoName,bpm:final.parameters.bpm.value,steps:64,division:'1/8',bars:8,chords:chords.map(c=>c.label),playing:final.playing,perNoteDurations:true};
}`}});
 if(result.isError)throw new Error(JSON.stringify(result));
 console.log(JSON.stringify(result));
}finally{await client.close();}
