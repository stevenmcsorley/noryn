#!/usr/bin/env python3
"""Balance song-reference preset loudness from `NorynSmoke <dir> --user-presets` measurements.
Aims for ~0.04 loop RMS without pushing peaks past ~0.3: master gain first (max 1), then step velocities.
Run after replace_song_sequences.py / write_chord_studies.py, which reset velocities from their sources.
"""
import csv, struct, sys
from pathlib import Path
import xml.etree.ElementTree as ET

SKIP = ('M83', 'Code Mode')

def main(levels_csv):
    repo = Path(__file__).resolve().parents[1]
    for row in csv.DictReader(open(levels_csv)):
        name = row['preset']
        if name.startswith(SKIP):
            continue
        gain = min(.04 / float(row['loop_rms']), .3 / float(row['loop_peak']))
        if .8 < gain < 1.25:
            continue
        for folder in [repo / 'presets', Path.home() / '.config/NORYN/presets']:
            path = folder / (name + '.noryn')
            if not path.exists():
                continue
            raw = path.read_bytes()
            magic, size = struct.unpack('<II', raw[:8])
            root = ET.fromstring(raw[8:8 + size].rstrip(b'\0'))
            master = next(p for p in root if p.tag == 'PARAM' and p.get('id') == 'master')
            old = float(master.get('value'))
            new = min(1.0, old * gain)
            master.set('value', f'{new:.4f}')
            rest = gain * old / new
            if rest > 1.05:
                for step in root.find('Sequence64'):
                    step.set('velocity', f"{min(1, float(step.get('velocity')) * rest):.4f}")
                    for note in step:
                        note.set('velocity', f"{min(1, float(note.get('velocity')) * rest):.4f}")
            encoded = ET.tostring(root, encoding='utf-8')
            path.write_bytes(struct.pack('<II', magic, len(encoded)) + encoded + b'\0')
        print(f'{name}: x{gain:.2f}')

if __name__ == '__main__':
    main(sys.argv[1])
