import{existsSync,openSync}from'node:fs';import{promises as fs}from'node:fs';import{spawn,spawnSync}from'node:child_process';import path from'node:path';import{fileURLToPath}from'node:url';import{runtimeDir}from'./runtime-paths.mjs';
const root=path.dirname(path.dirname(fileURLToPath(import.meta.url)));
if(!existsSync(path.join(root,'node_modules/@cloudflare/codemode/package.json'))){const installed=spawnSync('npm',['ci','--no-audit','--no-fund'],{cwd:root,stdio:['ignore','ignore','inherit']});if(installed.status!==0)process.exit(installed.status||1);}
const prepared=spawnSync(process.execPath,[path.join(root,'server/setup.mjs')],{cwd:root,stdio:['ignore','ignore','inherit']});if(prepared.status!==0)process.exit(1);
const token=(await fs.readFile(path.join(runtimeDir,'mcp-token'),'utf8')).trim();
async function ready(){try{const response=await fetch('http://127.0.0.1:8787/health',{headers:{Authorization:`Bearer ${token}`},signal:AbortSignal.timeout(1000)});return response.ok&&(await response.json()).name==='noryn-codemode';}catch{return false;}}
if(!await ready()){
 const log=openSync(path.join(runtimeDir,'runtime.log'),'a',0o600);const runtimeProcess=spawn('bash',[path.join(root,'start.sh')],{cwd:root,detached:true,stdio:['ignore',log,log]});runtimeProcess.unref();
 const deadline=Date.now()+45000;while(Date.now()<deadline&&!await ready())await new Promise(resolve=>setTimeout(resolve,300));if(!await ready())throw new Error('NORYN Code Mode did not start; check ~/.config/NORYN/mcp/runtime.log');
}
const proxy=spawn(process.execPath,[path.join(root,'server/stdio.mjs')],{cwd:root,stdio:'inherit'});proxy.on('exit',code=>process.exit(code||0));process.on('SIGTERM',()=>proxy.kill('SIGTERM'));process.on('SIGINT',()=>proxy.kill('SIGINT'));
