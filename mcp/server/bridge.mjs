import{runtimeDir}from'./runtime-paths.mjs';
import http from 'node:http';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import crypto from 'node:crypto';
import { fileURLToPath } from 'node:url';
const project=path.dirname(path.dirname(fileURLToPath(import.meta.url)));
export const controlRoot=process.env.NORYN_CONTROL_DIR||path.join(process.env.XDG_CONFIG_HOME||path.join(os.homedir(),'.config'),'NORYN','control');
export async function instances(root=controlRoot){
  let files;try{files=await fs.readdir(root);}catch{return [];}
  const result=[];
  for(const file of files.filter(x=>/^[0-9a-f-]+\.json$/i.test(x))){try{const info=JSON.parse(await fs.readFile(path.join(root,file),'utf8'));if(info.id!==file.slice(0,-5)||!Number.isInteger(info.pid)||info.pid<=0)continue;process.kill(info.pid,0);const base=path.join(root,info.id);if(info.inbox!==path.join(base,'inbox')||info.outbox!==path.join(base,'outbox'))continue;result.push({...info,mtime:(await fs.stat(path.join(root,file))).mtimeMs});}catch{}}
  return result.sort((a,b)=>b.mtime-a.mtime);
}
export async function dispatch(op,args={},instanceId,root=controlRoot){
  const available=await instances(root);const instance=instanceId?available.find(x=>x.id===instanceId):available[0];if(!instance)throw new Error('No running NORYN 0.3 instance. Launch the updated synth first.');
  const id=crypto.randomUUID()+'.json';const incoming=path.join(instance.inbox,id),outgoing=path.join(instance.outbox,id);
  const pending=incoming+'.pending';await fs.writeFile(pending,JSON.stringify({op,args}),{mode:0o600,flag:'wx'});await fs.rename(pending,incoming);
  const deadline=Date.now()+5000;
  try{while(Date.now()<deadline){try{const result=JSON.parse(await fs.readFile(outgoing,'utf8'));await fs.unlink(outgoing);if(result.error)throw new Error(result.error);return result;}catch(error){if(error.code!=='ENOENT')throw error;}await new Promise(resolve=>setTimeout(resolve,20));}throw new Error('NORYN control timed out; its message loop may be busy.');}
  finally{await fs.unlink(incoming+'.pending').catch(()=>{});await fs.unlink(incoming).catch(()=>{});await fs.unlink(outgoing).catch(()=>{});}
}
export const operations=new Set(['state','presets','setParameters','loadPreset','savePreset','loadUserPreset','sequence','progression','transport','scale','lfo','panic']);
async function main(){
  const tokenFile=path.join(runtimeDir,'bridge-token');await fs.mkdir(runtimeDir,{recursive:true,mode:0o700});let token;try{token=(await fs.readFile(tokenFile,'utf8')).trim();}catch{token=crypto.randomBytes(32).toString('hex');await fs.writeFile(tokenFile,token,{mode:0o600});}
  const server=http.createServer(async(req,res)=>{
    const reply=(code,body)=>{res.writeHead(code,{'Content-Type':'application/json','Cache-Control':'no-store'});res.end(JSON.stringify(body));};
    if(req.headers.authorization!==`Bearer ${token}`)return reply(401,{error:'Unauthorized'});
    if(req.method==='GET'&&req.url==='/instances')return reply(200,{instances:(await instances()).map(({id,pid,version})=>({id,pid,version}))});
    if(req.method!=='POST'||req.url!=='/api')return reply(404,{error:'Unknown route'});
    try{let text='';for await(const chunk of req){text+=chunk;if(Buffer.byteLength(text)>131072){reply(413,{error:'Request too large'});req.destroy();return;}}const body=JSON.parse(text);if(!operations.has(body.op))return reply(400,{error:'Unknown operation'});const result=await dispatch(body.op,body.args,body.instanceId);reply(200,result);}catch(error){reply(400,{error:error.message});}
  });
  server.listen(Number(process.env.NORYN_BRIDGE_PORT||8788),'127.0.0.1',()=>console.error('NORYN native control bridge listening on 127.0.0.1:8788'));
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url))main().catch(error=>{console.error(error);process.exit(1);});
