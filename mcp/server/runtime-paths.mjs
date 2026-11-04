import os from 'node:os';import path from'node:path';
export const runtimeDir=path.join(process.env.XDG_CONFIG_HOME||path.join(os.homedir(),'.config'),'NORYN','mcp');
