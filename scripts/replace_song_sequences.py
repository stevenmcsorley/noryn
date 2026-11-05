#!/usr/bin/env python3
"""Replace a patch's demo using an explicitly selected MIDI track/excerpt.
Requires mido. Source arrangements are supplied separately, never downloaded implicitly.
Only sequence/tempo fields change; local sound edits are preserved and backed up.
"""
import argparse, collections, hashlib, json, math, shutil, struct, time
from pathlib import Path
import xml.etree.ElementTree as ET
import mido
from mido.midifiles import meta as _meta

class _LenientKeys(dict):
    # Some community files carry invalid key signatures; key metadata is irrelevant to note extraction.
    def __missing__(self, key):
        return 'C'

_meta._key_signature_decode = _LenientKeys(_meta._key_signature_decode)

def extract(source, tracks, start, beats, grid, transpose=0, channels=None):
    midi=mido.MidiFile(source); events=[]
    for track in tracks:
        tick=0; held=collections.defaultdict(collections.deque)
        for msg in midi.tracks[track]:
            tick+=msg.time; at=tick/midi.ticks_per_beat
            if getattr(msg, 'channel', None) is not None and channels is not None and msg.channel not in channels:continue
            if msg.type=='note_on' and msg.velocity and msg.channel!=9:
                held[(msg.channel,msg.note)].append((at,msg.velocity))
            elif msg.type=='note_off' or (msg.type=='note_on' and not msg.velocity):
                q=held[(msg.channel,msg.note)]
                if q:
                    on,vel=q.popleft()
                    if start<=on<start+beats:
                        index=int(math.floor((on-start)*grid+.5))
                        if index<round(beats*grid):
                            events.append((index,msg.note+transpose,vel/127,max(.08,min(at,start+beats)-on)*grid))
    cells=collections.defaultdict(dict)
    for i,n,v,d in events:
        if not 0<=n<128:raise ValueError('Transposed note out of range')
        old=cells[i].get(n)
        if old is None or d>old['length']:cells[i][n]=dict(note=n,velocity=round(v,5),length=round(max(.05,min(256,d)),5))
    result=[]
    for i,notes in sorted(cells.items()):
        details=[notes[n] for n in sorted(notes)]
        if len(details)>16:raise ValueError('Too many simultaneous notes')
        result.append(dict(index=i,notes=sorted(notes),velocity=max(x['velocity'] for x in details),length=max(x['length'] for x in details),noteDetails=details))
    if not result:raise ValueError('Empty source excerpt')
    return result

def update(path, pattern, bpm, length, division, backup):
    raw=path.read_bytes(); magic,size=struct.unpack('<II',raw[:8]);assert magic==0x21324356
    root=ET.fromstring(raw[8:8+size].rstrip(b'\0'))
    values=dict(bpm=bpm,seqlength=length,seqdivision=division,seqgate=1,scale=0,quantizelive=0)
    for p in root:
        if p.tag=='PARAM' and p.get('id') in values:p.set('value',str(values[p.get('id')]))
    seq=root.find('Sequence64');seq.clear()
    for cell in pattern:
        notes=cell['notes'];s=ET.SubElement(seq,'Step',index=str(cell['index']),low=format(sum(1<<n for n in notes if n<64),'x'),high=format(sum(1<<(n-64) for n in notes if n>=64),'x'),velocity=str(cell['velocity']),length=str(cell['length']))
        for n in cell['noteDetails']:ET.SubElement(s,'Note',**{k:str(v) for k,v in n.items()})
    backup.mkdir(parents=True,exist_ok=True);shutil.copy2(path,backup/path.name)
    encoded=ET.tostring(root,encoding='utf-8');path.write_bytes(struct.pack('<II',magic,len(encoded))+encoded+b'\0')

def main():
    ap=argparse.ArgumentParser();ap.add_argument('manifest');ap.add_argument('--source-dir',required=True);args=ap.parse_args()
    repo=Path(__file__).resolve().parents[1];manifest=json.loads(Path(args.manifest).read_text());backup=Path('/tmp')/('noryn-pattern-backup-'+str(time.time_ns()))
    for spec in manifest['replacements']:
        source=Path(args.source_dir)/spec['file'];pattern=extract(source,spec['tracks'],spec['startBeat'],spec['beats'],spec['grid'],spec.get('transpose',0),spec.get('channels'))
        steps=round(spec['beats']*spec['grid']);assert 1<=steps<=256
        spec['pattern']=pattern;spec['sourceSha256']=hashlib.sha256(source.read_bytes()).hexdigest()
        spec['status']='community-transcription-excerpt';spec['listeningVerified']=False
        for folder in [repo/'presets',Path.home()/'.config/NORYN/presets']:
            p=folder/(spec['name']+'.noryn')
            if p.exists():update(p,pattern,spec['bpm'],steps,{1:0,2:1,4:2,8:3}[spec['grid']],backup/('repo' if folder==repo/'presets' else 'local'))
        print(spec['name'],len(pattern),'populated steps')
    (repo/'examples/song-sequence-replacements.json').write_text(json.dumps(manifest,indent=2)+'\n');print('Previous patches backed up to',backup)
if __name__=='__main__':main()
