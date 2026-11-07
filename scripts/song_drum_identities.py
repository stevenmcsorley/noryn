"""Per-song drum identities for the song demos.

Every song gets its own drum sound: a base kit chosen for the record, then pad edits (pitch, decay, tone,
snap, model), bus drive / compression / room and swing. Songs whose MIDI source has no drum track get a
groove written for that song instead of a shared pattern preset.

Pad roles (0-based): 0 kick, 1 snare, 2 clap, 3 closed hat, 4 open hat, 5-7 toms, 8 rim, 9 cowbell,
10 cymbal, 11 shaker, 12 conga, 13 zap, 14 noise hit, 15 second kick.
Groove lanes: one string per pad, bars separated by '|'; X accent, x normal, o ghost, . rest.
The lanes repeat to the groove length.
"""

VELOCITY = {'X': 120, 'x': 96, 'o': 56}


def groove_hits(lanes, length):
    hits = []
    for pad, text in lanes.items():
        steps = text.replace('|', '')
        for step in range(length):
            ch = steps[step % len(steps)]
            if ch in VELOCITY:
                hits.append([pad, step, VELOCITY[ch]])
    return sorted(hits, key=lambda h: (h[1], h[0]))


def pad(index, **fields):
    return {'pad': {'index': index, **fields}}


# name: kit, bus (drive, compression, room), swing, pads, and for songs without drum MIDI: groove + length.
SONGS = {
    # ---------------------------------------------------------------- Burial: each its own 2-step
    'Burial Archangel': dict(
        kit='Trip-Hop (Portishead)', bus=(.55, .55, .32), swing=.62,
        pads=[pad(1, name='Reload Snare', type='Noise hit', tune=2, decay=.35, tone=.55, snap=.8, level=.8),
              pad(8, name='Crackle Rim', tune=3, decay=.25, tone=.7, level=.45),
              pad(11, name='Vinyl Hiss', decay=.25, tone=.85, level=.3)],
        length=32, groove={0: 'X.........x.....|X.........x...o.', 1: '....X.......X...',
                           3: '..x.o.x...x.o.x.', 8: '.o.....o.o.....o', 11: 'o.o.o.o.o.o.o.o.'}),
    'Burial Untrue': dict(
        kit='Minimal Clicks', bus=(.4, .5, .38), swing=.66,
        pads=[pad(0, name='Sub Thud', tune=-5, decay=.55, tone=.2, snap=.3, level=1),
              pad(8, name='Click Snare', tune=4, decay=.3, tone=.8, snap=.9, level=.8),
              pad(3, name='Tick Hat', decay=.2, tone=.9, level=.45)],
        length=32, groove={0: 'X......x..x.....|X.........x.....', 8: '....X.......X...',
                           3: 'o.x.o.x.o.x.o.xx', 14: '.......o........|..............o.'}),
    'Burial Near Dark': dict(
        kit='Trip-Hop (Portishead)', bus=(.6, .6, .28), swing=.48,
        pads=[pad(0, tune=-4, decay=.6, tone=.15, level=1),
              pad(1, name='Dark Snare', tune=-4, decay=.55, tone=.25, snap=.7),
              pad(2, name='Ghost Clap', tune=-2, decay=.6, level=.45)],
        length=32, groove={0: 'X.....x.......x.|X.........X.....', 1: '....X.......X...',
                           2: '............o...', 3: '.xx.x.xx.xx.x.x.', 10: 'o...............|................'}),
    'Burial Etched Headplate': dict(
        kit='Ambient Soft', bus=(.35, .4, .55), swing=.3,
        pads=[pad(1, name='Rain Snare', type='Noise hit', tune=-2, decay=.6, tone=.35, snap=.5, level=.75),
              pad(11, name='Drizzle', decay=.4, tone=.7, level=.45)],
        length=16, groove={0: 'X.......x.x.....', 1: '........X.......', 3: 'x.ox.ox.x.ox.ox.',
                           11: 'o.o.o.o.o.o.o.o.'}),
    'Burial Ashtray Wasp': dict(
        kit='Techno Warehouse', bus=(.45, .5, .4), swing=.35,
        pads=[pad(0, name='Muffled Kick', tune=-2, decay=.45, tone=.15, level=.95),
              pad(2, name='Wasp Clap', tune=3, decay=.4, tone=.6, snap=.7, level=.7),
              pad(4, name='Hiss Hat', decay=.35, tone=.8, level=.4)],
        length=32, groove={0: 'X...X...X...X...', 2: '....X.......X...', 4: '..x...x...x...x.',
                           11: 'oxoooxoooxoooxoo', 14: '................|...........o....'}),
    'Burial Kindred': dict(
        kit='Industrial (Nine Inch Nails)', bus=(.75, .65, .3), swing=.52,
        pads=[pad(0, name='Rattle Kick', tune=-3, decay=.4, tone=.3, snap=.6),
              pad(1, name='Shard Snare', type='Noise hit', tune=0, decay=.45, tone=.6, snap=.85, level=.8),
              pad(15, name='Boom', tune=-8, decay=.9, level=.7)],
        length=32, groove={0: 'X..x......x..x..', 1: '....X.......X..o', 3: 'x.xxx.xxx.xxx.xx',
                           15: 'X...............|................'}),
    'Burial Raver': dict(
        kit='909 House', bus=(.4, .45, .35), swing=.5,
        pads=[pad(0, name='Garage Kick', tune=-1, decay=.4, tone=.4),
              pad(1, name='Garage Snare', tune=1, decay=.35, tone=.6, snap=.8),
              pad(4, name='Skippy Hat', decay=.3, tone=.7, level=.45)],
        length=16, groove={0: 'X.....X...X.....', 1: '....X.......X...', 4: '..X...X...X...X.',
                           11: 'oooxoooxoooxooox'}),
    'Burial Shell of Light': dict(
        kit='Ambient Soft', bus=(.25, .4, .58), swing=.42,
        pads=[pad(2, name='Halo Clap', tune=-1, decay=.7, tone=.5, level=.6),
              pad(3, name='Soft Tick', decay=.25, tone=.75, level=.4)],
        length=16, groove={0: 'X.........X.....', 2: '.....x......X...', 3: 'o.o.x.o.o.o.x.o.'}),
    'Burial Rival Dealer': dict(
        kit='Breakbeat Dusty', bus=(.8, .6, .3), swing=.2,
        pads=[pad(0, name='Distorted Break Kick', tune=-1, decay=.4, snap=.8),
              pad(1, name='Rave Snare', tune=2, decay=.4, tone=.7, snap=.85),
              pad(13, name='Rave Zap', tune=0, level=.4)],
        length=32, groove={0: 'X.X.......XX....|X.X.......X.....', 1: '....X..o.o..X..o',
                           3: 'x.x.x.x.x.x.x.x.', 13: '................|...........x....'}),
    'Burial Stolen Dog': dict(
        kit='Trip-Hop (Portishead)', bus=(.3, .45, .45), swing=.56,
        pads=[pad(0, tune=-3, decay=.4, tone=.2, level=.85),
              pad(8, name='Wood Rim', tune=-2, decay=.35, tone=.4, level=.7),
              pad(11, name='Hazy Shaker', decay=.3, tone=.6, level=.45)],
        length=16, groove={0: 'X...........x...', 8: '....x.......x...', 11: 'o.o.o.o.o.o.o.o.'}),
    'Burial Come Down to Us': dict(
        kit='Synthwave Gated', bus=(.25, .5, .6), swing=.15,
        pads=[pad(1, name='Distant Snare', tune=-3, decay=.7, tone=.35, level=.7),
              pad(10, name='Swell', decay=.85, tone=.4, level=.35)],
        length=32, groove={0: 'X.....x.........', 1: '........X.......', 3: 'x.x.x.x.x.x.x.x.',
                           10: 'x...............|................'}),

    # ---------------------------------------------------------------- Depeche Mode
    'Depeche Mode A Question of Time': dict(
        kit='Linn Electro (Depeche Mode)', bus=(.65, .6, .2), swing=0,
        pads=[pad(1, name='Metal Snare', tune=3, decay=.4, tone=.8, snap=.9),
              pad(14, name='Pipe Hit', tune=2, decay=.3, tone=.7, level=.5)],
        length=64, groove={0: 'X...X...X...X...', 1: '....X.......X...', 3: 'Xxxxxxxxxxxxxxxx',
                           14: '..x.......x.....', 5: '................|................|................|............xx..',
                           6: '................|................|................|..........xx....'}),
    'Depeche Mode Black Celebration': dict(
        kit='Industrial (Nine Inch Nails)', bus=(.5, .55, .4), swing=0,
        pads=[pad(1, name='Sheet Metal', type='Noise hit', tune=-2, decay=.55, tone=.45, snap=.7, level=.8),
              pad(9, name='Anvil', tune=-3, decay=.5, tone=.6, level=.6),
              pad(0, tune=-2, decay=.5, tone=.4)],
        length=16, groove={0: 'X.....x.X.......', 1: '....X.......X...', 9: '..x.......x...x.',
                           3: 'x.x.x.x.x.x.x.x.'}),
    'Depeche Mode Master and Servant': dict(
        kit='Techno Warehouse', bus=(.55, .6, .25), swing=0,
        pads=[pad(2, name='Whip Crack', type='Noise hit', tune=5, decay=.25, tone=.9, snap=1, level=.85),
              pad(8, name='Chain Rattle', type='Shaker', tune=2, decay=.35, tone=.8, level=.5),
              pad(14, name='Hiss Strike', decay=.3, tone=.8, level=.45)],
        length=16, groove={0: 'X...X...X...X...', 2: '....X.......X...', 14: '....x.......x...',
                           3: '..x...x...x...x.', 8: '.o.o.o.o.o.o.o.o'}),
    'Depeche Mode New Life': dict(
        kit='CR-78 Vintage', bus=(.25, .35, .15), swing=0,
        pads=[pad(1, tune=4, decay=.3, tone=.6, snap=.6), pad(9, name='Cowbell', level=.45)],
        length=16, groove={0: 'X.....X.X.......', 1: '....X.......X...', 3: 'x.x.x.x.x.x.x.x.',
                           9: '..........x.....'}),
    'Depeche Mode Clean': dict(
        kit='808 Classic', bus=(.4, .45, .45),
        pads=[pad(0, name='Deep Boom', tune=-6, decay=.85, tone=.15), pad(6, name='Desert Tom', tune=-4, decay=.7)]),
    'Depeche Mode Enjoy the Silence': dict(
        kit='909 House', bus=(.3, .5, .25),
        pads=[pad(1, name='Tight Snare', tune=3, decay=.3, tone=.75, snap=.9),
              pad(11, name='Silence Shaker', decay=.25, tone=.8, level=.5)]),
    'Depeche Mode Everything Counts': dict(
        kit='Linn Electro (Depeche Mode)', bus=(.3, .4, .3),
        pads=[pad(8, name='Hammer Hit', type='Cowbell', tune=-5, decay=.35, tone=.7, level=.6),
              pad(10, name='Pipe Clang', type='Cowbell', tune=4, decay=.5, tone=.8, level=.4)]),
    'Depeche Mode Halo': dict(
        kit='Synthwave Gated', bus=(.55, .65, .55),
        pads=[pad(1, name='Gated Crack', tune=-1, decay=.5, tone=.55, snap=.85),
              pad(11, name='Tambourine', decay=.3, tone=.9, level=.45)]),
    'Depeche Mode Just Cant Get Enough': dict(
        kit='808 Classic', bus=(.2, .3, .15),
        pads=[pad(0, name='Bouncy Kick', tune=2, decay=.35, tone=.4),
              pad(1, name='Bright Snare', tune=5, decay=.3, tone=.8, snap=.8),
              pad(11, name='Synth Shaker', decay=.2, tone=.9, level=.4)]),
    'Depeche Mode Never Let Me Down Again': dict(
        kit='Synthwave Gated', bus=(.5, .6, .5),
        pads=[pad(0, name='March Kick', tune=-2, decay=.5, snap=.6),
              pad(1, name='Big Gated Snare', tune=-2, decay=.6, tone=.5, snap=.8)]),
    'Depeche Mode People Are People': dict(
        kit='Industrial (Nine Inch Nails)', bus=(.6, .55, .35),
        pads=[pad(1, name='Anvil Snare', type='Cowbell', tune=-6, decay=.45, tone=.8, snap=.9, level=.85),
              pad(11, name='Metal Shaker', decay=.25, tone=.9, level=.4)]),
    'Depeche Mode Personal Jesus': dict(
        kit='Boom Bap Lo-Fi', bus=(.45, .55, .42),
        pads=[pad(0, name='Stomp', tune=-3, decay=.5, tone=.2, snap=.5),
              pad(1, name='Stomp Clap', type='Clap', tune=-1, decay=.6, tone=.4, snap=.6, level=.9),
              pad(11, name='Tambourine', decay=.3, tone=.85, level=.4)]),
    'Depeche Mode Photographic': dict(
        kit='CR-78 Vintage', bus=(.2, .3, .25),
        pads=[pad(0, tune=4, decay=.25), pad(10, name='Tin Cymbal', tune=5, decay=.35, level=.35)]),
    'Depeche Mode Policy of Truth': dict(
        kit='Boom Bap Lo-Fi', bus=(.4, .5, .3),
        pads=[pad(1, name='Thick Snare', tune=-3, decay=.55, tone=.4, snap=.7),
              pad(3, name='Lazy Hat', decay=.3, tone=.55, level=.4)]),
    'Depeche Mode Precious': dict(
        kit='Techno Warehouse', bus=(.4, .5, .3),
        pads=[pad(1, name='Electro Snare', tune=1, decay=.4, tone=.65, snap=.8)]),
    'Depeche Mode Shake the Disease': dict(
        kit='Linn Electro (Depeche Mode)', bus=(.3, .45, .5),
        pads=[pad(1, name='Glass Snare', type='Noise hit', tune=4, decay=.4, tone=.8, snap=.7, level=.75)]),
    'Depeche Mode Strangelove': dict(
        kit='Linn Electro (Depeche Mode)', bus=(.45, .55, .22),
        pads=[pad(0, name='Punch Kick', tune=0, decay=.35, snap=.8),
              pad(1, name='Whip Snare', tune=2, decay=.35, tone=.75, snap=.95)]),
    'Depeche Mode Stripped': dict(
        kit='Industrial (Nine Inch Nails)', bus=(.55, .5, .35),
        pads=[pad(0, name='Engine Kick', tune=-5, decay=.6, tone=.2),
              pad(4, name='Steam Hat', decay=.55, tone=.6, level=.4)]),
    'Depeche Mode Walking in My Shoes': dict(
        kit='Breakbeat Dusty', bus=(.6, .6, .35),
        pads=[pad(1, name='Heavy Snare', tune=-2, decay=.55, tone=.45, snap=.8),
              pad(11, name='Tambourine', decay=.3, tone=.85, level=.4)]),
    'Depeche Mode World in My Eyes': dict(
        kit='808 Classic', bus=(.35, .45, .25),
        pads=[pad(1, name='Snap Snare', tune=3, decay=.3, tone=.75, snap=.9),
              pad(2, name='Wide Clap', decay=.45, level=.65)]),

    # ---------------------------------------------------------------- Nine Inch Nails
    'Nine Inch Nails Closer': dict(
        kit='Industrial (Nine Inch Nails)', bus=(.9, .7, .12),
        pads=[pad(0, name='Nightclubbing Kick', tune=-2, decay=.3, tone=.75, snap=1, level=1),
              pad(1, name='Fizz Snare', type='Noise hit', tune=1, decay=.4, tone=.6, snap=.9),
              pad(11, name='Static', decay=.15, tone=1, level=.35)]),
    'Nine Inch Nails Down in It': dict(
        kit='Breakbeat Dusty', bus=(.7, .6, .2),
        pads=[pad(1, name='Hip-hop Snare', tune=0, decay=.45, tone=.5, snap=.8)]),
    'Nine Inch Nails Head Like a Hole': dict(
        kit='Industrial (Nine Inch Nails)', bus=(.8, .65, .25),
        pads=[pad(1, name='Clang Snare', type='Cowbell', tune=-4, decay=.4, tone=.85, snap=1, level=.85),
              pad(12, name='Metal Perc', type='Noise hit', tune=3, decay=.25, level=.5)]),
    'Nine Inch Nails March of the Pigs': dict(
        kit='Industrial (Nine Inch Nails)', bus=(1, .75, .3),
        pads=[pad(0, name='Pig Kick', tune=1, decay=.3, snap=1),
              pad(1, name='Shredded Snare', type='Noise hit', tune=3, decay=.35, tone=.75, snap=1)]),
    'Nine Inch Nails Ruiner': dict(
        kit='Synthwave Gated', bus=(.7, .6, .35),
        pads=[pad(0, name='Ruiner Kick', tune=-3, decay=.45, snap=.9),
              pad(1, name='Crushed Gate', type='Noise hit', tune=-1, decay=.5, tone=.5, snap=.9)]),
    'Nine Inch Nails The Perfect Drug': dict(
        kit='Breakbeat Dusty', bus=(.55, .55, .25),
        pads=[pad(1, name='Jungle Snare', tune=5, decay=.3, tone=.8, snap=.9),
              pad(3, name='Fast Hat', decay=.2, tone=.8, level=.45)]),

    # ---------------------------------------------------------------- Radiohead / Thom Yorke
    'Radiohead All I Need': dict(
        kit='Ambient Soft', bus=(.3, .5, .45),
        pads=[pad(0, name='Heartbeat Kick', tune=-4, decay=.55, tone=.15)]),
    'Radiohead Everything': dict(
        kit='Minimal Clicks', bus=(.2, .35, .4),
        pads=[pad(0, name='Soft Pulse', tune=-2, decay=.45, tone=.2, level=.9)]),
    'Radiohead Idioteque': dict(
        kit='Minimal Clicks', bus=(.85, .7, .2),
        pads=[pad(0, name='Overdriven Kick', tune=-1, decay=.35, tone=.7, snap=1),
              pad(8, name='Glitch Snare', type='Noise hit', tune=2, decay=.3, tone=.7, snap=.9, level=.8)]),
    'Radiohead Kid A': dict(
        kit='Minimal Clicks', bus=(.25, .35, .5),
        pads=[pad(0, name='Toy Kick', tune=2, decay=.3, tone=.35, level=.8)]),
    'Radiohead Lotus Flower': dict(
        kit='Minimal Clicks', bus=(.35, .5, .35),
        pads=[pad(1, name='Hand Clap', type='Clap', tune=0, decay=.45, tone=.5, level=.8)]),
    'Radiohead No Surprises': dict(
        kit='Ambient Soft', bus=(.15, .35, .35),
        pads=[pad(1, name='Brushed Snare', tune=-1, decay=.45, tone=.3, snap=.3, level=.6)]),
    'Radiohead Nude': dict(
        kit='Ambient Soft', bus=(.2, .4, .45),
        pads=[pad(0, tune=-3, decay=.5, tone=.2), pad(10, name='Ride', tune=2, decay=.7, level=.35)]),
    'Radiohead Pyramid Song': dict(
        kit='Breakbeat Dusty', bus=(.35, .45, .55),
        pads=[pad(10, name='Jazz Ride', tune=3, decay=.75, tone=.6, level=.4)]),
    'Radiohead Reckoner': dict(
        kit='Breakbeat Dusty', bus=(.3, .45, .45),
        pads=[pad(11, name='Tambourine Roll', decay=.35, tone=.9, level=.55),
              pad(10, name='Cymbal Wash', decay=.6, level=.35)]),
    'Radiohead Videotape': dict(
        kit='Minimal Clicks', bus=(.2, .3, .5),
        pads=[pad(8, name='Off-grid Click', tune=-3, decay=.3, tone=.5, level=.6)]),
    'Radiohead Weird Fishes': dict(
        kit='Breakbeat Dusty', bus=(.35, .45, .3),
        pads=[pad(3, name='Fast Hat', tune=2, decay=.2, tone=.8, level=.5),
              pad(8, name='Rim Click', tune=1, decay=.3, level=.55)]),
    'Thom Yorke The Eraser': dict(
        kit='Minimal Clicks', bus=(.45, .55, .25),
        pads=[pad(0, name='Laptop Kick', tune=-2, decay=.35, tone=.5, snap=.8)]),

    # ---------------------------------------------------------------- The Cure
    'The Cure A Forest': dict(
        kit='Linn Electro (Depeche Mode)', bus=(.2, .35, .45),
        pads=[pad(1, name='Flanged Snare', tune=1, decay=.4, tone=.6), pad(3, name='Tight Hat', decay=.2, level=.45)]),
    'The Cure Close to Me': dict(
        kit='808 Classic', bus=(.2, .35, .2),
        pads=[pad(2, name='Hand Claps', decay=.4, tone=.55, level=.75), pad(1, tune=2, decay=.3)]),
    'The Cure Just Like Heaven': dict(
        kit='Breakbeat Dusty', bus=(.4, .5, .4),
        pads=[pad(1, name='Bright Snare', tune=4, decay=.4, tone=.75, snap=.85),
              pad(10, name='Crash Ride', tune=2, decay=.6, level=.4)]),
    'The Cure Lullaby': dict(
        kit='Trip-Hop (Portishead)', bus=(.3, .45, .4),
        pads=[pad(3, name='Spider Hat', tune=3, decay=.2, tone=.8, level=.4)]),
    'The Cure The Lovecats': dict(
        kit='CR-78 Vintage', bus=(.15, .3, .35),
        pads=[pad(1, name='Brush Swirl', type='Shaker', tune=-2, decay=.4, tone=.4, level=.55)]),

    # ---------------------------------------------------------------- The Verve / Richard Ashcroft / UNKLE
    'The Verve Bitter Sweet Symphony': dict(
        kit='Boom Bap Lo-Fi', bus=(.5, .6, .3),
        pads=[pad(1, name='Loop Snare', tune=-1, decay=.5, tone=.45, snap=.75)]),
    'The Verve Lucky Man': dict(
        kit='Breakbeat Dusty', bus=(.3, .45, .55),
        pads=[pad(10, name='Ride Wash', decay=.75, level=.4)]),
    'The Verve Sonnet': dict(
        kit='Ambient Soft', bus=(.2, .4, .45),
        pads=[pad(11, name='Tambourine', decay=.3, tone=.85, level=.4)]),
    'Richard Ashcroft A Song for the Lovers': dict(
        kit='Boom Bap Lo-Fi', bus=(.6, .6, .3), swing=.3,
        pads=[pad(1, name='Loop Snare', tune=-1, decay=.5, tone=.45, snap=.75)],
        length=16, groove={0: 'X.x.......X.x...', 1: '....X.......X..o', 3: 'x.x.x.x.x.x.x.x.',
                           4: '..............x.', 11: '....x.......x...'}),
    'Richard Ashcroft Break the Night with Colour': dict(
        kit='Ambient Soft', bus=(.2, .4, .4), swing=.1,
        pads=[pad(8, name='Side Stick', tune=0, decay=.3, level=.6)],
        length=16, groove={0: 'X.......X.x.....', 8: '....x.......x...', 3: 'x.x.x.x.x.x.x.x.'}),
    'UNKLE Rabbit in Your Headlights': dict(
        kit='Trip-Hop (Portishead)', bus=(.4, .5, .5),
        pads=[pad(4, name='Rain Hat', decay=.4, tone=.5, level=.35)]),

    # ---------------------------------------------------------------- Portishead
    'Portishead Roads': dict(
        kit='Trip-Hop (Portishead)', bus=(.3, .45, .55), swing=.4,
        pads=[pad(1, name='Brushed Snare', tune=-3, decay=.55, tone=.3, snap=.35, level=.65),
              pad(11, name='Brushes', decay=.35, tone=.5, level=.4)],
        length=16, groove={0: 'X.........x.....', 1: '........X.......', 11: 'o.o.o.o.o.o.o.o.'}),
    'Portishead Sour Times': dict(
        kit='Boom Bap Lo-Fi', bus=(.5, .55, .35), swing=.45,
        pads=[pad(11, name='Tambourine', decay=.3, tone=.85, level=.5),
              pad(1, name='Vinyl Snare', tune=-2, decay=.5, tone=.35)],
        length=16, groove={0: 'X......x..x.....', 1: '....X.......X...', 3: 'x.x.x.x.x.x.x.x.',
                           11: '....x.......x...'}),

    # ---------------------------------------------------------------- Father John Misty
    'Father John Misty Chloe': dict(
        kit='Ambient Soft', bus=(.15, .3, .45), swing=.6,
        pads=[pad(8, name='Brush Tap', tune=-2, decay=.35, tone=.4, level=.55),
              pad(11, name='Brush Sweep', decay=.45, tone=.45, level=.45)],
        length=16, groove={0: 'X.......X.......', 8: '....x.......x...', 11: 'x.ox.ox.x.ox.ox.'}),
    'Father John Misty Gods Favorite Customer': dict(
        kit='Breakbeat Dusty', bus=(.2, .4, .5), swing=0,
        pads=[pad(1, name='Ballad Snare', tune=-2, decay=.55, tone=.4, snap=.5)],
        length=32, groove={0: 'X.......x.......', 1: '....X.......X...', 3: 'x...x...x...x...',
                           10: 'x...............|................'}),
    'Father John Misty Mr Tillman': dict(
        kit='Ambient Soft', bus=(.25, .4, .35), swing=.55,
        pads=[pad(11, name='Shaker', decay=.3, tone=.7, level=.45)],
        length=16, groove={0: 'X.....x.X.......', 1: '....X.......X...', 3: 'x.x.x.x.x.x.x.x.',
                           11: 'oxoxoxoxoxoxoxox'}),
}
