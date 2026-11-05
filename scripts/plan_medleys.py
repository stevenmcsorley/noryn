#!/usr/bin/env python3
"""Plan section medleys: one slice per distinct song section, in song order, filling the sequencer.

Sections are detected from the whole arrangement, not just the preset's part. Each phrase (block)
gets a fingerprint of its bar-by-bar harmony plus which tracks are playing; phrases with similar
fingerprints share a section label. The span where the preset's own part plays is split into equal
regions (start to end of the song); each region contributes its most common section, preferring
labels not already used, so the medley moves through the song's different parts in order. Writes `slices` into the manifest entries; replace_song_sequences.py renders them.
"""
import argparse, collections, json, math
from pathlib import Path
import mido
import replace_song_sequences  # noqa: F401  (tolerant key-signature parsing)

STEPS = 256

def notes_of(midi):
    tb = midi.ticks_per_beat
    for index, track in enumerate(midi.tracks):
        tick = 0
        held = {}
        for msg in track:
            tick += msg.time
            if msg.type not in ('note_on', 'note_off') or msg.channel == 9:
                continue
            key = (msg.channel, msg.note)
            if msg.type == 'note_on' and msg.velocity:
                held[key] = tick
            elif key in held:
                start = held.pop(key)
                yield index, msg.channel, msg.note, start / tb, max(tick - start, 1) / tb

def plan(path, tracks, channels, beats_per_bar, bars_per_slice, slices_wanted):
    midi = mido.MidiFile(path)
    notes = list(notes_of(midi))
    track_ids = sorted({n[0] for n in notes})
    end = max(n[3] + n[4] for n in notes)
    bars = math.ceil(end / beats_per_bar)
    chroma = [[0.0] * 12 for _ in range(bars)]
    active = [set() for _ in range(bars)]
    part = [0] * bars
    for track, channel, note, start, length in notes:
        bar = int(start // beats_per_bar)
        if bar >= bars:
            continue
        chroma[bar][note % 12] += min(length, beats_per_bar)
        active[bar].add(track)
        if track in tracks and (channels is None or channel in channels):
            part[bar] += 1
    first = next(b for b in range(bars) if active[b])
    blocks = []
    for start in range(first, bars - bars_per_slice + 1, bars_per_slice):
        vec = []
        for b in range(start, start + bars_per_slice):
            norm = math.sqrt(sum(x * x for x in chroma[b])) or 1
            vec += [x / norm for x in chroma[b]]
        texture = collections.Counter(t for b in range(start, start + bars_per_slice) for t in active[b])
        vec += [.6 * texture[t] / bars_per_slice for t in track_ids]
        coverage = sum(1 for b in range(start, start + bars_per_slice) if part[b])
        blocks.append(dict(bar=start, vec=vec, coverage=coverage))
    labels = []
    for block in blocks:
        best, score = None, 0
        for label, rep in labels:
            dot = sum(a * b for a, b in zip(block['vec'], rep))
            sim = dot / ((math.sqrt(sum(a * a for a in block['vec'])) or 1) * (math.sqrt(sum(b * b for b in rep)) or 1))
            if sim > score:
                best, score = label, sim
        if best is None or score < .8:
            best = len(labels)
            labels.append((best, block['vec']))
        block['label'] = best
    need = max(1, max(b['coverage'] for b in blocks) * .75)
    eligible = [b for b in blocks if b['coverage'] >= need]
    count = collections.Counter(b['label'] for b in eligible)
    # Split the span where the part plays into equal regions, so the medley runs from the start of
    # the song to its end. Each region gives its most common section, preferring one not yet used.
    chosen, used = [], set()
    for region in range(slices_wanted):
        lo = round(region * len(eligible) / slices_wanted)
        hi = max(lo + 1, round((region + 1) * len(eligible) / slices_wanted))
        pool = [b for b in eligible[lo:hi] if b not in chosen]
        if not pool:
            continue
        pick = max(pool, key=lambda b: (b['label'] not in used, count[b['label']], -b['bar']))
        chosen.append(pick)
        used.add(pick['label'])
    chosen.sort(key=lambda b: b['bar'])
    names = {}
    for block in chosen:
        names.setdefault(block['label'], chr(ord('A') + len(names)))
    return [dict(startBeat=b['bar'] * beats_per_bar, beats=bars_per_slice * beats_per_bar,
                 section=names[b['label']], bar=b['bar'] + 1) for b in chosen], len(labels)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('manifest')
    ap.add_argument('--source-dir', required=True)
    args = ap.parse_args()
    manifest = json.loads(Path(args.manifest).read_text())
    for spec in manifest['replacements']:
        beats_per_bar = 10 if 'right place' in spec['file'] else 4
        total_beats = STEPS // spec['grid']
        if beats_per_bar == 10:
            total_beats = 60
        slices_wanted = 3 if beats_per_bar == 10 else 4
        bars_per_slice = max(1, total_beats // beats_per_bar // slices_wanted)
        slices, kinds = plan(Path(args.source_dir) / spec['file'], spec['tracks'], spec.get('channels'),
                             beats_per_bar, bars_per_slice, slices_wanted)
        spec['slices'] = slices
        spec['startBeat'] = slices[0]['startBeat']
        spec['beats'] = sum(s['beats'] for s in slices)
        spec['window'] = (f"medley of {len(slices)} sections in song order ({kinds} section types detected): "
                          + ', '.join(f"{s['section']} bar {s['bar']}" for s in slices))
        print(spec['name'][:52].ljust(52), spec['window'])
    Path(args.manifest).write_text(json.dumps(manifest, indent=2) + '\n')

if __name__ == '__main__':
    main()
