#!/usr/bin/env python3
"""Plan each song demo's drum part for NorynSmoke --apply-song-drums.

Transcribed songs take their drums from the same community MIDI file and the same medley sections as
their synth part (channel 10, quantized to 1/16, General MIDI notes mapped to NORYN's pad roles).
Songs whose source has no drum track, and the chord studies, get a pattern preset that suits the song,
or no drums where the original has none. Requires mido.
"""
import argparse, json, re
from pathlib import Path
import mido
import replace_song_sequences  # noqa: F401  (tolerant key-signature parsing)
from song_drum_identities import SONGS, groove_hits

GM = {35: 0, 36: 0, 38: 1, 40: 1, 39: 2, 42: 3, 44: 3, 46: 4, 41: 5, 43: 5, 45: 6, 47: 6, 48: 7, 50: 7,
      37: 8, 56: 9, 49: 10, 51: 10, 52: 10, 55: 10, 57: 10, 59: 10, 54: 11, 69: 11, 70: 11,
      60: 12, 61: 12, 62: 12, 63: 12, 64: 12}

def kit_for(name):
    n = name.lower()
    if 'nine inch nails' in n: return 'Industrial (Nine Inch Nails)'
    if 'depeche' in n: return 'Linn Electro (Depeche Mode)'
    if any(k in n for k in ('idioteque', 'everything', 'kid a', 'eraser')): return 'Minimal Clicks'
    if 'radiohead' in n: return 'Breakbeat Dusty'
    if 'the cure' in n: return 'CR-78 Vintage' if 'lovecats' in n else 'Linn Electro (Depeche Mode)'
    if any(k in n for k in ('portishead', 'unkle', 'burial')): return 'Trip-Hop (Portishead)'
    if 'bitter sweet' in n: return 'Boom Bap Lo-Fi'
    if any(k in n for k in ('verve', 'ashcroft')): return 'Breakbeat Dusty'
    if 'father john misty' in n: return 'Ambient Soft'
    return '808 Classic'

# Songs without usable drum MIDI: (pattern preset, kit) or None for no drums.
FALLBACK = {
    'Radiohead Motion Picture Soundtrack': None,
    'Radiohead Daydreaming': None,
    'Depeche Mode Waiting for the Night': None,
    'Depeche Mode Black Celebration': ('Synth-pop drive (Depeche Mode)', 'Linn Electro (Depeche Mode)'),
    'Nine Inch Nails A Warm Place': None,
    'Burial Night Bus': None,
    'Father John Misty The Palace': None,
    'Burial Archangel': ('Two-step garage (Burial)', 'Trip-Hop (Portishead)'),
    'Depeche Mode Master and Servant': ('Synth-pop drive (Depeche Mode)', 'Linn Electro (Depeche Mode)'),
    'Depeche Mode New Life': ('Synth-pop drive (Depeche Mode)', 'Linn Electro (Depeche Mode)'),
    'Depeche Mode A Question of Time': ('Synth-pop drive (Depeche Mode)', 'Linn Electro (Depeche Mode)'),
    'Portishead Roads': ('Trip-hop slow (Portishead)', 'Trip-Hop (Portishead)'),
    'Portishead Sour Times': ('Trip-hop slow (Portishead)', 'Trip-Hop (Portishead)'),
    'Burial Near Dark': ('Two-step garage (Burial)', 'Trip-Hop (Portishead)'),
    'Burial Raver': ('Two-step garage (Burial)', 'Trip-Hop (Portishead)'),
    'Burial Kindred': ('Two-step garage (Burial)', 'Trip-Hop (Portishead)'),
    'Burial Come Down to Us': ('Half-time', 'Trip-Hop (Portishead)'),
    'Burial Untrue': ('Two-step garage (Burial)', 'Trip-Hop (Portishead)'),
    'Burial Shell of Light': ('Two-step garage (Burial)', 'Trip-Hop (Portishead)'),
    'Burial Etched Headplate': ('Two-step garage (Burial)', 'Trip-Hop (Portishead)'),
    'Burial Ashtray Wasp': ('Two-step garage (Burial)', 'Trip-Hop (Portishead)'),
    'Burial Rival Dealer': ('Two-step garage (Burial)', 'Trip-Hop (Portishead)'),
    'Burial Stolen Dog': ('Two-step garage (Burial)', 'Trip-Hop (Portishead)'),
    'Father John Misty Chloe': ('Bossa nova', 'Ambient Soft'),
    'Father John Misty Gods Favorite Customer': ('Rock basic', 'Ambient Soft'),
    'Father John Misty Mr Tillman': ('Rock basic', 'Ambient Soft'),
    'Richard Ashcroft A Song for the Lovers': ('Breakbeat (funky drummer feel)', 'Boom Bap Lo-Fi'),
    'Richard Ashcroft Break the Night with Colour': ('Rock basic', 'Breakbeat Dusty'),
}

def song_of(name):
    return name.split(' - ')[0]

def drum_hits(path, slices, limit=256):
    midi = mido.MidiFile(path)
    tb = midi.ticks_per_beat
    hits = {}
    offset = 0
    for s in slices:
        start, beats = s['startBeat'], s['beats']
        for track in midi.tracks:
            tick = 0
            for msg in track:
                tick += msg.time
                if msg.type != 'note_on' or msg.channel != 9 or not msg.velocity or msg.note not in GM:
                    continue
                beat = tick / tb
                if start <= beat < start + beats:
                    step = offset + int(round((beat - start) * 4))
                    if step < offset + beats * 4 and step < limit:
                        key = (GM[msg.note], step)
                        hits[key] = max(hits.get(key, 0), msg.velocity)
        offset += int(beats * 4)
    return [[pad, step, vel] for (pad, step), vel in sorted(hits.items(), key=lambda kv: (kv[0][1], kv[0][0]))], min(limit, offset)

def identity(name, entry):
    """Applies the song's own kit, sound and (for songs without drum MIDI) groove to a plan entry."""
    song = song_of(name)
    spec = SONGS.get(song)
    if spec is None:
        raise SystemExit(f'no drum identity for {song}')
    entry['kit'] = spec['kit']
    if 'groove' in spec:
        entry.pop('pattern', None)
        entry['length'] = spec['length']
        entry['hits'] = groove_hits(spec['groove'], spec['length'])
    drive, compression, room = spec['bus']
    entry['tweaks'] = [{'drive': drive, 'compression': compression, 'room': room,
                        'swing': spec.get('swing', 0)}] + spec.get('pads', [])
    return entry

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--source-dir', required=True)
    ap.add_argument('--out', required=True)
    args = ap.parse_args()
    plan = []
    songs = json.loads(Path('examples/song-sequence-replacements.json').read_text())['replacements']
    studies = json.loads(Path('examples/chord-study-sequences.json').read_text())['studies']
    for spec in songs:
        name, song = spec['name'], song_of(spec['name'])
        slices = spec.get('slices') or [{'startBeat': spec['startBeat'], 'beats': spec['beats']}]
        hits, length = drum_hits(Path(args.source_dir) / spec['file'], slices)
        if FALLBACK.get(song, 0) is None:
            plan.append({'preset': name, 'none': True})
        elif len(hits) >= 32 and song not in FALLBACK:  # sparse sections are not the song's beat
            plan.append(identity(name, {'preset': name, 'length': max(16, length), 'hits': hits, 'source': spec['file']}))
        else:
            plan.append(identity(name, {'preset': name}))
    for study in studies:
        name, song = study['name'], song_of(study['name'])
        plan.append({'preset': name, 'none': True} if FALLBACK.get(song, 0) is None else identity(name, {'preset': name}))
    Path(args.out).write_text(json.dumps(plan))
    for item in plan:
        kind = 'none' if item.get('none') else f"{len(item['hits'])} hits / {item['length']} steps " + \
            ('from MIDI' if 'source' in item else 'song groove')
        print(f"{item['preset'][:58]:58s} {item.get('kit', '')[:28]:28s} {kind}")
    print(len(plan), 'song demos;', sum(1 for i in plan if 'source' in i), 'from MIDI drums,',
          sum(1 for i in plan if 'hits' in i and 'source' not in i), 'written grooves,',
          sum(1 for i in plan if i.get('none')), 'without drums')

if __name__ == '__main__':
    main()
