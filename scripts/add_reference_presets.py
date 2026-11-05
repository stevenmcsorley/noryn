#!/usr/bin/env python3
"""Create song-reference user presets by cloning a related patch's sound and applying overrides.
The sequence is left empty here; replace_song_sequences.py fills it from the manifest excerpt.
Existing presets are never overwritten unless --force is given.
"""
import argparse, json, struct
from pathlib import Path
import xml.etree.ElementTree as ET

def load(path):
    raw = path.read_bytes()
    magic, size = struct.unpack('<II', raw[:8])
    return magic, ET.fromstring(raw[8:8 + size].rstrip(b'\0'))

def save(path, magic, root):
    encoded = ET.tostring(root, encoding='utf-8')
    path.write_bytes(struct.pack('<II', magic, len(encoded)) + encoded + b'\0')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('manifest')
    ap.add_argument('--force', action='store_true')
    args = ap.parse_args()
    repo = Path(__file__).resolve().parents[1]
    for spec in json.loads(Path(args.manifest).read_text())['presets']:
        magic, root = load(repo / 'presets' / (spec['template'] + '.noryn'))
        known = {p.get('id') for p in root if p.tag == 'PARAM'}
        unknown = set(spec['sound']) - known
        if unknown:
            raise ValueError(f"{spec['name']}: unknown parameters {sorted(unknown)}")
        for p in root:
            if p.tag == 'PARAM' and p.get('id') in spec['sound']:
                p.set('value', str(spec['sound'][p.get('id')]))
        root.set('userPresetIndex', '-1')
        root.find('Sequence64').clear()
        for folder in [repo / 'presets', Path.home() / '.config/NORYN/presets']:
            folder.mkdir(parents=True, exist_ok=True)
            target = folder / (spec['name'] + '.noryn')
            if target.exists() and not args.force:
                print('kept existing', target)
                continue
            save(target, magic, root)
        print(spec['name'], '<-', spec['template'])

if __name__ == '__main__':
    main()
