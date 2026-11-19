"""Everything the dive is made of, as plain data: tiles, the four acts, creatures, classes, skills, perks, item bases,
affixes and uniques. No rules live here."""

# ------------------------------------------------------------------ tiles
# (sign, colour, walkable, see-through, name)
WALL, FLOOR, DOOR, OPEN, STAIRS, WATER, SHALLOW, RUBBLE, FOUNTAIN, LAVA, UPSTAIRS, CHEST, OPENCHEST, SHRINE, USED, \
    TORCH, CANDLE, ALTAR, PEW, PILLAR, TOMB, CRACKED, OPENTOMB, BIER, CORPSE, RESTLESS, BONEWALL, BONES, CRUST, NECK, BASALT, DAIS, RUNE, THRONE, \
    EDGE, HEARTH, STONEWORK, ANVIL, SLAG, RAIL, CART, CHAIN, BRAZIER, OBELISK, ASH, VENT, OBSIDIAN, WRECK, SHIELD, STATUE, \
    BANNER, FALLEN = range(52)
TILES = [
    ('#', None, False, False, 'wall'),
    ('.', None, True, True, 'floor'),
    ('+', (150, 100, 50), False, False, 'door'),
    ("'", (150, 100, 50), True, True, 'open door'),
    ('>', (255, 230, 120), True, True, 'stairs down'),
    ('~', (40, 80, 170), False, True, 'deep water'),
    ('~', (70, 120, 190), True, True, 'shallow water'),
    (',', (110, 100, 90), True, True, 'rubble'),
    ('{', (110, 190, 255), True, True, 'healing fountain'),
    ('~', (230, 90, 20), False, True, 'lava'),
    ('<', (170, 160, 130), True, True, 'stairs up'),
    ('■', (200, 150, 60), False, True, 'chest'),
    ('□', (110, 90, 60), False, True, 'open chest'),
    ('Ω', (190, 160, 255), False, True, 'shrine'),
    ('Ω', (80, 70, 100), False, True, 'spent shrine'),
    # the dressing of the rooms; the client draws the shapes (a tomb's box, a table's legs) from the tiles around
    ('¡', (255, 176, 64), False, False, 'torch'),
    ('ı', (255, 230, 160), False, True, 'candles'),
    ('‡', (220, 200, 140), False, True, 'altar'),
    ('≡', (110, 80, 50), False, True, 'pew'),
    ('○', (180, 172, 160), False, False, 'pillar'),
    ('†', (138, 132, 120), False, True, 'stone tomb'),
    ('/', (138, 132, 120), False, True, 'cracked tomb'),
    (' ', (138, 132, 120), False, True, 'open tomb'),
    ('═', (138, 106, 60), False, True, 'embalming table'),
    ('%', (154, 140, 116), False, True, 'corpse on a table'),
    ('%', (184, 168, 144), False, True, 'twitching corpse'),
    ('Ω', (200, 188, 160), False, False, 'wall of bones'),
    (',', (200, 188, 160), True, True, 'scattered bones'),
    # the Deep: crust on the lava, and the Ashen King's island of cut basalt
    ('.', (90, 58, 48), True, True, 'crust'),
    ('.', (90, 58, 48), True, True, 'thin crust'),
    ('.', (74, 77, 92), True, True, 'cut basalt'),
    ('▬', (90, 92, 110), True, True, 'dais'),
    ('Ψ', (192, 74, 22), True, True, 'burning rune'),
    ('╥', (138, 122, 160), False, True, 'throne'),
    ('→', (255, 230, 160), True, True, 'the way on'),
    # the dressing of the Deep's plates: a dwarven forge, the King's ash shrine, steam vents, the fallen clan
    ('▲', (255, 160, 48), False, True, 'forge hearth'),
    ('▓', (90, 74, 64), False, False, 'forge stones'),
    ('π', (154, 158, 168), False, True, 'anvil'),
    ('▲', (96, 80, 70), False, True, 'slag heap'),
    ('═', (106, 90, 74), True, True, 'mine rail'),
    ('▄', (122, 90, 52), False, True, 'ore cart'),
    ('§', (122, 122, 132), True, True, 'dropped chain'),
    ('☼', (255, 160, 48), False, True, 'brazier'),
    ('▲', (84, 70, 110), False, True, 'obsidian obelisk'),
    ('∙', (138, 132, 120), True, True, 'circle of ash'),
    ('o', (90, 74, 68), False, True, 'steam vent'),
    ('▲', (96, 94, 130), False, True, 'obsidian spike'),
    ('/', (138, 142, 150), True, True, 'broken axe'),
    ('◘', (106, 90, 58), True, True, 'split shield'),
    ('▬', (90, 86, 80), False, True, 'toppled statue'),
    ('¶', (122, 42, 26), False, True, 'scorched banner'),
    ('%', (154, 138, 116), False, True, 'fallen dwarf'),
]
BLOCKS_MOVE = {WALL, DOOR, WATER, LAVA, CHEST, OPENCHEST, SHRINE, USED, TORCH, CANDLE, ALTAR, PEW, PILLAR, TOMB, CRACKED,
               OPENTOMB, BIER, CORPSE, RESTLESS, BONEWALL, HEARTH, STONEWORK, ANVIL, SLAG, CART, BRAZIER, OBELISK, VENT,
               OBSIDIAN, STATUE, BANNER, FALLEN}

# ------------------------------------------------------------------ the four acts
ACTS = [
    {'name': 'The Crypt', 'wall': (120, 110, 100), 'floor': (70, 64, 60), 'mood': 'halls and corridors',
     'boss': 'gravewarden'},
    {'name': 'The Caves', 'wall': (110, 90, 70), 'floor': (72, 60, 46), 'mood': 'mixed', 'boss': 'webmother'},
    {'name': 'The Forge', 'wall': (140, 80, 60), 'floor': (80, 52, 42), 'mood': 'close-packed', 'boss': 'furnace'},
    {'name': 'The Deep', 'wall': (90, 80, 130), 'floor': (48, 44, 70), 'mood': 'mixed', 'boss': 'ashenking'},
]
FLOORS = 16
REST_AFTER = (4, 8, 12)          # a waystation follows these floors

# ------------------------------------------------------------------ creatures
# hp, dmg (lo, hi), acc, armor, speed (100 = yours), xp, ai, extras
M = {
    # act 1
    'rat': dict(name='grave rat', ch='r', col=(160, 130, 110), hp=6, dmg=(1, 3), acc=60, ar=0, spd=120, xp=2, ai='melee'),
    'shambler': dict(undead=True, name='bloated corpse', ch='z', col=(130, 160, 110), hp=16, dmg=(2, 5), acc=55, ar=1, spd=70, xp=5, ai='melee'),
    'bonearcher': dict(undead=True, name='ash-caked archer', ch='s', col=(220, 220, 200), hp=10, dmg=(2, 4), acc=60, ar=1, spd=100, xp=6,
                       ai='ranged', rng=7, shot=('-', (220, 220, 200))),
    'bat': dict(name='carrion bat', ch='b', col=(150, 110, 150), hp=5, dmg=(1, 3), acc=65, ar=0, spd=150, xp=3, ai='erratic'),
    'ghoul': dict(undead=True, name='ember-ribbed dead', ch='g', col=(170, 190, 120), hp=22, dmg=(3, 6), acc=65, ar=2, spd=100, xp=10, ai='melee',
                  gear=True),
    'gravewarden': dict(undead=True, name='the Gravewarden', ch='W', col=(230, 230, 180), hp=140, dmg=(5, 10), acc=75, ar=4, spd=100,
                        xp=120, ai='melee', boss=True, moves=('summon:shambler', 'slam')),
    # act 2
    'spider': dict(name='grease crawler', ch='a', col=(150, 120, 90), hp=14, dmg=(2, 5), acc=70, ar=1, spd=120, xp=8,
                   ai='melee', poison=3),
    'hound': dict(name='blind hound', ch='C', col=(170, 150, 130), hp=20, dmg=(3, 7), acc=70, ar=2, spd=130, xp=11, ai='melee'),
    'trog': dict(name='tunnel brute', ch='t', col=(140, 170, 160), hp=30, dmg=(4, 8), acc=65, ar=3, spd=100, xp=14, ai='melee',
                 gear=True),
    'spitter': dict(name='tar spitter', ch='j', col=(150, 210, 90), hp=16, dmg=(3, 6), acc=65, ar=1, spd=100, xp=12,
                    ai='ranged', rng=6, shot=('*', (150, 230, 90)), poison=3),
    'giantrat': dict(name='giant rat', ch='R', col=(120, 100, 90), hp=18, dmg=(3, 6), acc=65, ar=3, spd=120, xp=9,
                     ai='melee'),
    'webmother': dict(name='the Webmother', ch='A', col=(220, 180, 120), hp=260, dmg=(6, 13), acc=80, ar=6, spd=110,
                      xp=260, ai='melee', boss=True, poison=5, moves=('summon:spider', 'spit')),
    # act 3
    'imp': dict(name='scrap hurler', ch='h', col=(200, 130, 90), hp=18, dmg=(4, 8), acc=70, ar=2, spd=120, xp=16,
                ai='ranged', rng=6, shot=('*', (190, 170, 150)), fire=True),
    'cinderhound': dict(name='cinder hound', ch='C', col=(240, 110, 60), hp=30, dmg=(5, 9), acc=75, ar=3, spd=130, xp=19,
                        ai='melee', fire=True),
    'thrall': dict(undead=True, name='half-melted smith', ch='d', col=(220, 150, 110), hp=46, dmg=(6, 11), acc=70, ar=6, spd=100, xp=24,
                   ai='melee', gear=True),
    'golem': dict(name='iron golem', ch='G', col=(170, 160, 150), hp=70, dmg=(8, 14), acc=65, ar=10, spd=70, xp=30,
                  ai='melee'),
    'slag': dict(name='slag fiend', ch='S', col=(200, 90, 50), hp=60, dmg=(7, 13), acc=65, ar=8, spd=70, xp=28, ai='melee',
                 fire=True),
    'furnace': dict(name='the Furnace Lord', ch='F', col=(255, 170, 60), hp=420, dmg=(9, 17), acc=85, ar=9, spd=100,
                    xp=480, ai='melee', boss=True, fire=True, moves=('flame', 'summon:imp')),
    # act 4
    'wraith': dict(undead=True, name='ash wraith', ch='w', col=(170, 190, 230), hp=40, dmg=(7, 12), acc=80, ar=4, spd=130, xp=32, ai='melee',
                   cold=True),
    'deepknight': dict(undead=True, name='charred knight', ch='K', col=(140, 140, 200), hp=75, dmg=(9, 16), acc=80, ar=10, spd=100, xp=42,
                       ai='melee', gear=True),
    'caller': dict(name='ash caller', ch='v', col=(200, 120, 230), hp=40, dmg=(8, 14), acc=75, ar=3, spd=100, xp=40,
                   ai='ranged', rng=7, shot=('*', (200, 120, 255)), cold=True, moves=('summon:wraith',)),
    'horror': dict(undead=True, name='bone horror', ch='H', col=(190, 90, 150), hp=95, dmg=(10, 18), acc=75, ar=6, spd=110, xp=50,
                   ai='melee'),
    'ashenking': dict(name='the Ashen King', ch='&', col=(255, 120, 40), hp=750, dmg=(12, 22), acc=90, ar=12, spd=110,
                    xp=0, ai='melee', boss=True, cold=True, moves=('nova', 'summon:wraith', 'blink')),
}
SPAWNS = [   # per act: (creature, weight)
    [('rat', 4), ('shambler', 5), ('bonearcher', 3), ('bat', 3), ('ghoul', 2)],
    [('spider', 4), ('giantrat', 4), ('hound', 3), ('trog', 3), ('spitter', 3)],
    [('imp', 4), ('cinderhound', 4), ('thrall', 3), ('golem', 2), ('slag', 2)],
    [('wraith', 4), ('deepknight', 3), ('caller', 2), ('horror', 2)],
]

# champions (blue) carry one of these, uniques (gold) two or three; a pack of champions shares one
AFFIXES = {
    'Swift': 'it is quick', 'Brutal': 'it hits very hard', 'Stone Skin': 'it is hard to hurt',
    'Vampiric': 'its blows heal it', 'Fire-blooded': 'it burns, and bursts into flame when slain',
    'Regenerating': 'its wounds close', 'Venomous': 'its blows poison', 'Frost-touched': 'its cold touch numbs your arm',
}
UNIQUE_FIRST = ['Gorrath', 'Vesk', 'Old Murl', 'Ashka', 'Brannoc', 'Thessaly', 'Grimwald', 'Ulvane', 'Skarn', 'Morrow']
UNIQUE_EPITHET = ['the Gnawer', 'the Patient', 'Bonechewer', 'the Hollow', 'Ironjaw', 'the Pale', 'of the Long Teeth',
                  'the Unwashed', 'Dreadmaw', 'the Restless']

# ------------------------------------------------------------------ classes, skills, perks
# res: what the skills cost. Fury fills by striking and by being struck, and drains away out of a fight.
# skills: the first two from the start, the third at level 3, the fourth at level 6. paths: one to choose at level 5.
CLASSES = {
    'warrior': dict(name='Warrior', ch='@', col=(230, 200, 150), hp=64, mana=10, hp_lvl=10, mana_lvl=0, acc=80, res='fury',
                    start=('sword', 'leather'), skills=('cleave', 'warcry', 'charge', 'standfast'), kinds=('sword', 'axe', 'mace'),
                    paths=('berserker', 'guardian'),
                    about='The wall in front. Every blow he gives or takes fills his fury. Fury pays for his skills, and the more he has, the less he feels.'),
    'ranger': dict(name='Ranger', ch='@', col=(150, 220, 140), hp=46, mana=30, hp_lvl=7, mana_lvl=3, acc=85, res='mana',
                   start=('bow', 'leather'), skills=('volley', 'pierce', 'tumble', 'pin'), kinds=('bow',),
                   paths=('sharpshooter', 'skirmisher'),
                   about='Place and distance. Each arrow loosed without moving hits harder than the last: [f] shoots.'),
    'adept': dict(name='Adept', ch='@', col=(160, 170, 255), hp=36, mana=55, hp_lvl=5, mana_lvl=6, acc=80, res='mana',
                  start=('staff', 'robe'), skills=('firebolt', 'nova', 'blink', 'emberwall'), kinds=('staff',),
                  paths=('pyromancer', 'frostbinder'),
                  about='Fire and frost together: fire on a frozen foe shatters it for three times the harm.'),
    'lamplighter': dict(name='Lamplighter', ch='@', col=(250, 220, 120), hp=54, mana=35, hp_lvl=8, mana_lvl=4, acc=80, res='mana',
                        start=('cudgel', 'leather'), skills=('smite', 'mend', 'hallow', 'beacon'), kinds=('mace',),
                        paths=('shepherd', 'zealot'),
                        about='A lantern against the dark. Near him wounds close, his own too, and the dead burn.'),
    'runecarver': dict(name='Runecarver', ch='@', col=(120, 220, 220), hp=48, mana=45, hp_lvl=7, mana_lvl=5, acc=80, res='mana',
                       start=('staff', 'robe'), skills=('rune', 'kindle', 'ward', 'recall'), kinds=('staff', 'mace'),
                       paths=('fireward', 'stoneward'),
                       about='Reads the runes of the dead dwarves. Carves them in the ground, and they burn whoever steps on them.'),
    'cutthroat': dict(name='Cutthroat', ch='@', col=(200, 160, 200), hp=50, mana=34, hp_lvl=7, mana_lvl=3, acc=90, res='mana',
                      start=('knife', 'leather'), skills=('shadowstep', 'fan', 'smoke', 'garrote'), kinds=('dagger',),
                      paths=('duelist', 'poisoner'),
                      about='Quick and hard to hit. Strikes at openings: a foe that is busy, stunned, frozen or spent takes a bleeding critical blow.'),
}
CLASS_ORDER = ('warrior', 'ranger', 'adept', 'lamplighter', 'runecarver', 'cutthroat')
SKILL_LEVELS = (1, 1, 3, 6)
PATH_LEVEL = 5
SKILLS = {
    'cleave': dict(name='Cleave', mana=2, target=False, about='strike every foe next to you'),
    'warcry': dict(name='War Cry', mana=3, target=False, about='stun foes within 2'),
    'charge': dict(name='Charge', mana=2, target=True, about='leap at a foe up to 5 away, strike and stun it'),
    'standfast': dict(name='Stand Fast', mana=4, target=False, about='4 turns: half harm, and the foes near you turn on you'),
    'volley': dict(name='Volley', mana=7, target=False, about='an arrow at each of the 3 nearest foes'),
    'pierce': dict(name='Piercing Shot', mana=5, target=True, about='an arrow through every foe in a line'),
    'tumble': dict(name='Tumble', mana=4, target=False, about='roll 2 back from the nearest foe and shoot it, keeping your focus'),
    'pin': dict(name='Pinning Shot', mana=6, target=True, about='an arrow that holds a foe in place; a great foe it stuns'),
    'firebolt': dict(name='Firebolt', mana=4, target=True, about='a bolt of fire'),
    'nova': dict(name='Frost Nova', mana=10, target=False, about='frost bursts around you and freezes foes'),
    'blink': dict(name='Blink', mana=5, target=False, about='vanish and stand up to 5 away, far from your foes'),
    'emberwall': dict(name='Ember Wall', mana=8, target=True, about='a wall of fire across the way to a foe; foes will not cross it'),
    'smite': dict(name='Smite', mana=3, target=False, about='a holy blow at a foe next to you; the dead take far more'),
    'mend': dict(name='Mend', mana=6, target=False, about='heal yourself and every friend within 3'),
    'hallow': dict(name='Hallow', mana=7, target=False, about='the ground around you burns foes for 5 turns'),
    'beacon': dict(name='Beacon', mana=9, target=False, about='your lantern flares: foes within 4 are stunned, the dead burn'),
    'rune': dict(name='Carve Rune', mana=2, target=False, about='a rune where you stand that bursts into fire when a foe steps on it'),
    'kindle': dict(name='Kindle', mana=4, target=False, about='all your runes burst at once'),
    'ward': dict(name='Rune of Binding', mana=6, target=True, about='a rune under a foe that binds it; a great foe it stuns'),
    'recall': dict(name='Rune of Return', mana=6, target=False, about='step to your oldest rune; a rune bursts where you stood'),
    'shadowstep': dict(name='Shadowstep', mana=3, target=True, about='appear behind a foe up to 4 away; it is caught off guard'),
    'fan': dict(name='Fan of Knives', mana=5, target=False, about='knives at every foe within 2'),
    'smoke': dict(name='Smoke', mana=7, target=False, about='foes within 3 lose you for 2 turns'),
    'garrote': dict(name='Garrote', mana=6, target=False, about='choke the foe next to you: it cannot act for 2 turns, and bleeds'),
}
PERKS = {   # key: (name, text, max ranks, class or None, path it needs or None; '*' is a path itself)
    'tough': ('Toughness', '+15 life', 3, None, None),
    'brute': ('Brute', '+15% damage', 3, None, None),
    'eye': ('Keen Eye', '+8 accuracy, +4% critical', 3, None, None),
    'hide': ('Thick Hide', '+3 armour', 3, None, None),
    'well': ('Deep Well', '+15 mana, faster mana', 3, None, None),
    'blood': ('Bloodlust', 'heal 3 life on each kill', 2, None, None),
    'ward': ('Warding', '+20% resist fire, cold and poison', 2, None, None),
    'second': ('Second Wind', 'potions heal 40% more', 1, None, None),
    # warrior
    'berserker': ('Berserker', 'each point of fury: +3% damage', 1, 'warrior', '*'),
    'guardian': ('Guardian', '+4 armour; blows you take fill twice the fury', 1, 'warrior', '*'),
    'whirl': ('Whirling Steel', 'Cleave hits 25% harder and heals 2 life for each foe', 2, 'warrior', 'berserker'),
    'frenzy': ('Frenzy', 'Charge costs no fury', 1, 'warrior', 'berserker'),
    'iron': ('Iron Will', 'War Cry also shields you: half harm for 3 turns', 1, 'warrior', 'guardian'),
    'bulwark': ('Bulwark', 'Stand Fast lasts 3 turns longer', 1, 'warrior', 'guardian'),
    # ranger
    'sharpshooter': ('Sharpshooter', 'focus builds to 5, and +10% critical with bows', 1, 'ranger', '*'),
    'skirmisher': ('Skirmisher', 'a step only loses one point of focus', 1, 'ranger', '*'),
    'sniper': ('Deadeye', '+10% critical with bows', 2, 'ranger', 'sharpshooter'),
    'multi': ('Multishot', 'Volley shoots one more arrow', 2, 'ranger', 'skirmisher'),
    'quick': ('Light Feet', 'Tumble costs 2 less mana', 1, 'ranger', 'skirmisher'),
    # adept
    'pyromancer': ('Pyromancer', 'fire spells burn 30% hotter', 1, 'adept', '*'),
    'frostbinder': ('Frostbinder', 'frozen foes take 25% more from everything', 1, 'adept', '*'),
    'blast': ('Fireball', 'Firebolt bursts on impact', 1, 'adept', 'pyromancer'),
    'deepfrost': ('Deep Frost', 'Frost Nova reaches further and freezes longer', 2, 'adept', 'frostbinder'),
    # lamplighter
    'shepherd': ('Shepherd', 'your light reaches 3, and Mend heals half again as much', 1, 'lamplighter', '*'),
    'zealot': ('Zealot', 'Smite strikes 40% harder and leaps to a second foe', 1, 'lamplighter', '*'),
    'warmth': ('Warmth', 'your light heals 1 more each turn', 2, 'lamplighter', 'shepherd'),
    'pyre': ('Holy Fire', 'Hallow burns twice as hot', 1, 'lamplighter', 'zealot'),
    # runecarver
    'fireward': ('Fireward', 'runes burst wider and 30% hotter', 1, 'runecarver', '*'),
    'stoneward': ('Stoneward', 'runes also hold foes fast, and you may keep 5', 1, 'runecarver', '*'),
    'deeprunes': ('Deep Runes', 'Kindle costs nothing', 1, 'runecarver', 'fireward'),
    'heavyrunes': ('Heavy Runes', 'a held foe takes 25% more from your runes', 1, 'runecarver', 'stoneward'),
    # cutthroat
    'duelist': ('Duelist', 'each blow in a row at the same foe: +12% harm, up to 5', 1, 'cutthroat', '*'),
    'poisoner': ('Poisoner', 'your bleeding becomes poison: twice the harm, longer', 1, 'cutthroat', '*'),
    'riposte': ('Riposte', 'a foe that misses you is open to you', 1, 'cutthroat', 'duelist'),
    'venom': ('Black Venom', 'Fan of Knives poisons every foe it hits', 1, 'cutthroat', 'poisoner'),
}

# ------------------------------------------------------------------ items
SLOTS = ('weapon', 'body', 'helm', 'gloves', 'boots', 'ring', 'amulet')
# base: (name, slot, kind, tier, dmg (lo, hi) or armour)
BASES = {
    'sword': ('Short Sword', 'weapon', 'sword', 1, (3, 7)),
    'blade': ('Broad Blade', 'weapon', 'sword', 2, (5, 11)),
    'falchion': ('Falchion', 'weapon', 'sword', 3, (8, 15)),
    'greatsword': ('Greatsword', 'weapon', 'sword', 4, (11, 21)),
    'handaxe': ('Hand Axe', 'weapon', 'axe', 1, (2, 9)),
    'waraxe': ('War Axe', 'weapon', 'axe', 2, (4, 14)),
    'cleaver': ('Headsman\'s Axe', 'weapon', 'axe', 4, (8, 26)),
    'mace': ('Mace', 'weapon', 'mace', 2, (6, 10)),
    'maul': ('Maul', 'weapon', 'mace', 3, (10, 16)),
    'bow': ('Short Bow', 'weapon', 'bow', 1, (2, 6)),
    'longbow': ('Longbow', 'weapon', 'bow', 2, (4, 10)),
    'warbow': ('War Bow', 'weapon', 'bow', 3, (6, 14)),
    'greatbow': ('Great Bow', 'weapon', 'bow', 4, (9, 19)),
    'cudgel': ('Cudgel', 'weapon', 'mace', 1, (3, 6)),
    'knife': ('Knife', 'weapon', 'dagger', 1, (3, 6)),
    'dirk': ('Dirk', 'weapon', 'dagger', 2, (4, 8)),
    'kris': ('Kris', 'weapon', 'dagger', 3, (6, 12)),
    'stiletto': ('Stiletto', 'weapon', 'dagger', 4, (9, 16)),
    'staff': ('Ash Staff', 'weapon', 'staff', 1, (2, 5)),
    'rodstaff': ('Rune Staff', 'weapon', 'staff', 2, (3, 7)),
    'gnarl': ('Gnarled Staff', 'weapon', 'staff', 3, (4, 9)),
    'starstaff': ('Star Staff', 'weapon', 'staff', 4, (6, 12)),
    'robe': ('Robe', 'body', 'cloth', 1, 1),
    'leather': ('Leather Coat', 'body', 'light', 1, 3),
    'studded': ('Studded Leather', 'body', 'light', 2, 6),
    'mail': ('Mail Shirt', 'body', 'heavy', 2, 9),
    'scale': ('Scale Mail', 'body', 'heavy', 3, 13),
    'plate': ('Plate Armour', 'body', 'heavy', 4, 18),
    'cap': ('Cap', 'helm', 'helm', 1, 1),
    'helm': ('Helm', 'helm', 'helm', 2, 3),
    'greathelm': ('Great Helm', 'helm', 'helm', 3, 5),
    'gloves': ('Gloves', 'gloves', 'gloves', 1, 1),
    'gauntlets': ('Gauntlets', 'gloves', 'gloves', 3, 3),
    'boots': ('Boots', 'boots', 'boots', 1, 1),
    'greaves': ('Greaves', 'boots', 'boots', 3, 3),
    'ring': ('Ring', 'ring', 'ring', 1, 0),
    'amulet': ('Amulet', 'amulet', 'amulet', 2, 0),
}
STAFF_SPELL = {'staff': 0, 'rodstaff': 15, 'gnarl': 30, 'starstaff': 50}     # % spell power of a staff base
# affix: (stat, (lo, hi) at tier 1, grows by tier, slots it can roll on or None for all)
PREFIXES = {
    'Sharp': ('dmg', (1, 3), 1.6, ('weapon',)),
    'Cruel': ('dmg_pct', (10, 25), 1.4, ('weapon',)),
    'Sturdy': ('armour', (1, 3), 1.6, ('body', 'helm', 'gloves', 'boots')),
    'Glowing': ('mana', (5, 12), 1.5, None),
    'Blessed': ('acc', (4, 9), 1.4, None),
    'Burning': ('fire_dmg', (1, 4), 1.7, ('weapon', 'ring', 'amulet', 'gloves')),
    'Runed': ('spell', (8, 18), 1.4, ('weapon', 'amulet', 'ring', 'helm')),
}
SUFFIXES = {
    'of Vigour': ('hp', (5, 12), 1.6, None),
    'of the Lynx': ('crit', (2, 5), 1.3, ('weapon', 'gloves', 'ring', 'amulet', 'helm')),
    'of the Leech': ('leech', (3, 6), 1.3, ('weapon', 'ring', 'amulet')),
    'of Embers': ('res_fire', (10, 25), 1.2, None),
    'of the Thaw': ('res_cold', (10, 25), 1.2, None),
    'of Purity': ('res_poison', (10, 25), 1.2, None),
    'of the Wellspring': ('regen', (1, 2), 1.3, ('ring', 'amulet', 'body', 'helm')),
    'of Thorns': ('thorns', (2, 5), 1.5, ('body', 'helm')),
}
STAT_TEXT = {
    'dmg': '+{} damage', 'dmg_pct': '+{}% damage', 'armour': '+{} armour', 'mana': '+{} mana', 'acc': '+{} accuracy',
    'fire_dmg': '+{} fire damage', 'spell': '+{}% spell power', 'hp': '+{} life', 'crit': '+{}% critical',
    'leech': '{}% of damage heals you', 'res_fire': '+{}% resist fire', 'res_cold': '+{}% resist cold',
    'res_poison': '+{}% resist poison', 'regen': '+{} mana each 3 turns', 'thorns': 'attackers take {} damage',
}
RARE_A = ['Grim', 'Dusk', 'Blood', 'Storm', 'Bone', 'Ash', 'Wraith', 'Raven', 'Doom', 'Ember', 'Gloom', 'Iron']
RARE_B = {'weapon': ['Bite', 'Song', 'Fang', 'Edge', 'Thirst', 'Reaver'], 'body': ['Shell', 'Hide', 'Coat', 'Carapace'],
          'helm': ['Crown', 'Visage', 'Brow'], 'gloves': ['Grip', 'Hand', 'Clutch'], 'boots': ['Stride', 'Tread', 'Road'],
          'ring': ['Loop', 'Band', 'Coil'], 'amulet': ['Charm', 'Eye', 'Heart']}
UNIQUES = [   # name, base, fixed stats
    ('Lanternfang', 'blade', {'dmg_pct': 60, 'fire_dmg': 6, 'crit': 6}),
    ('The Ninth Bell', 'maul', {'dmg_pct': 70, 'acc': 15, 'hp': 20}),
    ('Widowstring', 'longbow', {'dmg_pct': 60, 'crit': 10, 'leech': 4}),
    ('Emberwhisper', 'rodstaff', {'spell': 70, 'mana': 30, 'res_fire': 30}),
    ('Skin of the Drowned King', 'mail', {'armour': 10, 'hp': 30, 'res_cold': 40}),
    ('Gravewalker Treads', 'boots', {'armour': 5, 'hp': 15, 'res_poison': 40}),
    ('Ring of the Ninth Door', 'ring', {'crit': 6, 'leech': 5, 'mana': 15}),
    ('Heart of Cinders', 'amulet', {'fire_dmg': 6, 'spell': 30, 'res_fire': 30, 'hp': 15}),
]
RARITY_COL = {'normal': (210, 210, 210), 'magic': (110, 140, 255), 'rare': (240, 220, 90), 'unique': (210, 160, 70)}
POTION_HEAL, POTION_MANA = 'heal', 'mana'
BELT_MAX = 6
PACK_MAX = 14
