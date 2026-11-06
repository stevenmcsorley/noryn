import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StreamableHTTPClientTransport } from '@modelcontextprotocol/sdk/client/streamableHttp.js';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import { runtimeDir } from './runtime-paths.mjs';
const token=(await fs.readFile(path.join(runtimeDir,'mcp-token'),'utf8')).trim();
const client=new Client({name:'noryn-stolen-dog-study',version:'1.0.1'});
await client.connect(new StreamableHTTPClientTransport(new URL('http://127.0.0.1:8787/mcp'),{requestInit:{headers:{Authorization:`Bearer ${token}`}}}));
try {
 const result=await client.callTool({name:'code',arguments:{code:`async()=>{
 const before=await codemode.get_state({});const instanceId=before.instanceId;
 const bank=await codemode.list_presets({instanceId});
 let name='Burial Stolen Dog - Hazy Keys',suffix=2;
 while(bank.user.includes(name))name='Burial Stolen Dog - Hazy Keys '+suffix++;
 await codemode.transport({playing:false,recording:false,metronome:false,instanceId});
 await codemode.load_preset({index:205,instanceId});
 await codemode.set_parameters({values:{engine:1,fmprofile:12,fm:.23,fmratio:2,balance:.025,
 position:.35,shape:.85,pulsewidth:.42,sub:.025,unison:.12,detune:4,octavea:0,octaveb:0,
 filtercharacter:1,filtermode:0,cutoff:2450,resonance:.17,drive:.075,envamount:.19,
 attack:.028,decay:.82,sustain:.22,release:.42,envcurve:.55,fattack:.015,fdecay:.48,fsustain:.25,frelease:.4,
 choruscharacter:1,chorus:.43,chorusrate:.16,delay:.26,delaytime:.25974,feedback:.38,reverb:.43,size:.84,
 width:1.42,master:.69,phasefree:.55,analogdrift:.2,noise:0,velfilter:.25,keytrack:.25,
 voicemode:0,glidetime:0,bendrange:2,lfo1dest:15,lfo1amount:.065,rate:.17,lfoshape:0,depth:1,
 lfo2dest:1,lfo2amount:.035,lfo2rate:.09,lfo2shape:1,lfo3dest:0,lfo4dest:0,
 bpm:115.5,seqlength:64,seqdivision:2,seqgate:.87,seqrelease:.1,seqfollowrelease:0,seqnotelength:1,
 scale:0,root:0,quantizelive:0},instanceId});
 const voicings=[[37,56,59,61,64],[40,56,59,64,66],[42,57,61,64,68],[38,54,57,61,64]];
 const steps=[];
 for(let bar=0;bar<4;bar++){
  const notes=voicings[bar],top=notes.slice(1),base=bar*16;
  steps.push({index:base,notes,velocity:.74,length:5.2,
   noteDetails:notes.map((note,i)=>({note,velocity:i===0?.49:.72+i*.012,length:i===0?15:5.2-i*.08}))});
  steps.push({index:base+7,notes:top,velocity:.52,length:2.6});
  steps.push({index:base+11,notes:top,velocity:.66,length:2.8});
  steps.push({index:base+15,notes:[top[top.length-1]],velocity:.38,length:.6});
 }
 await codemode.write_sequence({steps,replace:true,instanceId});
 await codemode.save_preset({name,instanceId});
 const recalled=await codemode.load_user_preset({name,instanceId});
 if(recalled.steps.filter(s=>s.notes.length).length!==16 || recalled.parameters.fmprofile.value!==12 ||
    Math.abs(recalled.parameters.bpm.value-115.5)>.001 || recalled.steps[0].notes.join(',')!=='37,56,59,61,64' ||
    recalled.steps[0].noteDetails[0].length!==15)throw new Error('Stolen Dog study recall failed');
 return {verified:true,preset:name,bpm:115.5,steps:64,bars:4,division:'1/16',chords:['C#m7','Eadd9','F#m9','Dmaj9'],
  synthesis:'six-operator hazy keys; original interpretation',playing:recalled.playing,perNoteDurations:true};
}`}});
 if(result.isError)throw new Error(JSON.stringify(result));console.log(JSON.stringify(result));
} finally {await client.close();}
