#!/usr/bin/env python3
"""Write chord-progression demos for patches whose reference has no note-level transcription.
Progressions come from cited chord charts or analysis of official preview audio (see the manifest).
They are harmonic studies, not transcriptions: voicings and rhythms are arranged for each patch role.
Only sequence/tempo fields change; local sound edits are preserved and backed up.
"""
import argparse, json, re, time
from pathlib import Path
from replace_song_sequences import update

PITCH = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
QUALITY = {'': (0, 4, 7), 'm': (0, 3, 7), '7': (0, 4, 7, 10), 'm7': (0, 3, 7, 10), 'maj7': (0, 4, 7, 11),
           '6': (0, 4, 7, 9), 'sus4': (0, 5, 7), 'dim7': (0, 3, 6, 9), 'm7b5': (0, 3, 6, 10),
           '13': (0, 4, 10, 14, 21), 'm9': (0, 3, 7, 10, 14), 'maj9': (0, 4, 7, 11, 14), 'add9': (0, 2, 4, 7)}

def pitch_class(name):
    return (PITCH[name[0]] + name[1:].count('#') - name[1:].count('b')) % 12

def parse(symbol):
    match = re.fullmatch(r'([A-G][#b]?)([^/]*)(?:/([A-G][#b]?))?', symbol)
    if not match or match.group(2) not in QUALITY:
        raise ValueError('Unsupported chord ' + symbol)
    root = pitch_class(match.group(1))
    bass = pitch_class(match.group(3)) if match.group(3) else root
    return root, QUALITY[match.group(2)], bass

def voice(symbol, low=55):
    """Bass note in octave 2 and a close upper voicing starting at or above `low`."""
    root, intervals, bass = parse(symbol)
    upper = sorted({low + (root + i - low) % 12 + (12 if i >= 12 else 0) for i in intervals})
    return 36 + (bass - 36) % 12, upper

# Rhythms per bar: (offset in bar fraction, length in bar fraction, velocity, voices)
# voices: 'bass', 'chord', 'top' (highest chord tone, an octave up), 'arp' (cycles chord tones)
STYLES = {
    'pad': [(0, 1, .72, 'bass'), (0, 1, .68, 'chord')],
    'organ': [(0, 1, .74, 'bass'), (0, .5, .7, 'chord'), (.5, .5, .62, 'chord')],
    'piano': [(0, 1, .7, 'bass'), (0, .37, .66, 'chord'), (.375, .12, .5, 'top'), (.5, .5, .6, 'chord')],
    'comp': [(0, .5, .74, 'bass'), (.125, .25, .62, 'chord'), (.5, .5, .68, 'bass'), (.625, .25, .58, 'chord')],
    # Burial-style two-step chops: the second and fourth hits land off the grid of the beat.
    'chops': [(0, .375, .74, 'bass'), (0, .125, .7, 'chord'), (.1875, .125, .55, 'chord'),
              (.4375, .125, .62, 'chord'), (.6875, .1875, .58, 'chord')],
    'stab': [(0, .0625, .82, 'chord'), (.1875, .0625, .6, 'chord'), (.625, .0625, .72, 'chord'),
             (.625, .375, .66, 'bass')],
    'bells': [(i / 8, .25, .62 - (i % 2) * .1, 'arp') for i in range(8)],
    'flare': [(0, 1, .6, 'bass'), (0, .875, .66, 'chord'), (.5, .5, .58, 'top')],
    # Stolen Dog study: bass held 15 steps, a held chord, two off-beat chops and the top note at the bar end.
    'hazy': [(0, 15.1 / 16, .66, 'bass'), (0, 5.2 / 16, .62, 'chord'), (7 / 16, 2.7 / 16, .5, 'chord'),
             (11 / 16, 2.9 / 16, .54, 'chord'), (15 / 16, .7 / 16, .46, 'peak')],
    'hazy_lift': [(0, 15.1 / 16, .66, 'bass'), (0, 5.2 / 16, .62, 'chord'), (7 / 16, 2.7 / 16, .5, 'chord'),
                  (.5, 4 / 16, .4, 'top'), (11 / 16, 2.9 / 16, .54, 'chord'), (15 / 16, .7 / 16, .46, 'peak')],
    # Second-half variations so 16-bar loops develop instead of repeating.
    'chops_lift': [(0, .375, .74, 'bass'), (0, .125, .7, 'chord'), (.1875, .125, .55, 'chord'),
                   (.4375, .125, .62, 'chord'), (.5, .25, .5, 'top'), (.6875, .1875, .58, 'chord'),
                   (.875, .125, .52, 'top')],
    'pad_swell': [(0, 1, .66, 'bass'), (0, 1, .64, 'chord'), (.5, .5, .52, 'top')],
    'organ_push': [(0, 1, .74, 'bass'), (0, .375, .7, 'chord'), (.375, .125, .6, 'chord'), (.625, .375, .64, 'chord')],
    'stab_sparse': [(0, .0625, .8, 'chord'), (.375, .0625, .64, 'chord'), (.5, .5, .62, 'bass'),
                    (.8125, .0625, .7, 'chord')],
    'bells_fall': [(i / 8, .25, .6 - (i % 2) * .1, 'arp_down') for i in range(8)],
    'flare_rise': [(0, 1, .6, 'bass'), (0, .5, .62, 'chord'), (.5, .5, .66, 'chord'), (.75, .25, .6, 'top')],
    'piano_flow': [(0, 1, .7, 'bass'), (0, .25, .64, 'chord'), (.25, .25, .52, 'top'), (.5, .25, .6, 'chord'),
                   (.75, .25, .5, 'top')],
    'comp_drive': [(0, .25, .76, 'bass'), (.125, .125, .6, 'chord'), (.25, .25, .66, 'bass'),
                   (.375, .125, .58, 'chord'), (.5, .25, .72, 'bass'), (.625, .125, .6, 'chord'),
                   (.75, .25, .66, 'bass'), (.875, .125, .58, 'chord')],
}

def build(spec):
    """Each chord fills [start, start + bars); style events falling inside that span use its voicing."""
    steps_per_bar = spec['division']
    cells = {}
    bar = 0.0
    sections = spec.get('sections') or [dict(style=spec['style'], progression=spec['progression'])]
    chords = [(symbol, bars, section['style']) for section in sections for symbol, bars in section['progression']]
    for symbol, bars, style in chords:
        bass, upper = voice(symbol, spec.get('low', 55))
        span_end = bar + bars
        for whole in range(int(bar), int(-(-span_end // 1))):
            for offset, length, velocity, part in STYLES[style]:
                at = whole + offset
                if not bar - 1e-9 <= at < span_end - 1e-9:
                    continue
                if part == 'bass':
                    notes = [bass + 12 * spec.get('bassOctave', 0)]
                elif part == 'chord':
                    notes = upper
                elif part == 'top':
                    notes = [upper[-1] + 12]
                elif part == 'peak':
                    notes = [upper[-1]]
                else:
                    run = upper + [n + 12 for n in upper]
                    if part == 'arp_down':
                        run = run[::-1]
                    notes = [run[int(round(offset * 8)) % len(run)]]
                held = max(.25, (min(at + length, span_end) - at) * steps_per_bar - .1)  # floor in steps
                cell = cells.setdefault(int(round(at * steps_per_bar)), {})
                for note in notes:
                    if 0 <= note < 128:
                        cell[note] = dict(note=note, velocity=velocity, length=round(held, 3))
        bar = span_end
    total = int(round(bar * steps_per_bar))
    assert total <= 256, (spec['name'], total)
    pattern = []
    for index, notes in sorted(cells.items()):
        details = [notes[n] for n in sorted(notes)][:16]
        pattern.append(dict(index=index, notes=[d['note'] for d in details],
                            velocity=max(d['velocity'] for d in details),
                            length=max(d['length'] for d in details), noteDetails=details))
    return pattern, total

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('manifest')
    args = ap.parse_args()
    repo = Path(__file__).resolve().parents[1]
    manifest = json.loads(Path(args.manifest).read_text())
    backup = Path('/tmp') / ('noryn-pattern-backup-' + str(time.time_ns()))
    division = {4: 0, 8: 1, 16: 2, 32: 3}  # steps per bar -> seqdivision
    for spec in manifest['studies']:
        pattern, steps = build(spec)
        spec['steps'] = steps
        spec['populatedSteps'] = len(pattern)
        for folder in [repo / 'presets', Path.home() / '.config/NORYN/presets']:
            path = folder / (spec['name'] + '.noryn')
            if path.exists():
                update(path, pattern, spec['bpm'], steps, division[spec['division']],
                       backup / ('repo' if folder == repo / 'presets' else 'local'))
        print(spec['name'], steps, 'steps,', len(pattern), 'populated')
    Path(args.manifest).write_text(json.dumps(manifest, indent=2) + '\n')
    print('Previous patches backed up to', backup)

if __name__ == '__main__':
    main()
