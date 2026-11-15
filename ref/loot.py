"""Items: rolling them, naming them, describing them. An item is a plain dict, so it goes over the wire as it is."""
import itertools
import random

from . import data as D

_ids = itertools.count(1)


def tier_for(depth):
    """Item tier for a floor: 1 on the first floors, 4 in the Deep, sometimes one higher or lower."""
    t = 1 + (depth - 1) // 4
    r = random.random()
    if r < .15:
        t -= 1
    elif r > .85:
        t += 1
    return max(1, min(4, t))


def _roll(rng, grow, tier):
    lo, hi = rng
    f = grow ** (tier - 1)
    return max(1, round(random.uniform(lo * f, hi * f)))


def make(base, rarity='normal', tier=None):
    name, slot, kind, btier, val = D.BASES[base]
    it = {'id': next(_ids), 'base': base, 'name': name, 'slot': slot, 'kind': kind, 'tier': tier or btier,
          'rarity': rarity, 'stats': {}}
    if slot == 'weapon':
        it['dmg'] = list(val)
        if kind == 'staff' and D.STAFF_SPELL.get(base):
            it['stats']['spell'] = D.STAFF_SPELL[base]
    else:
        it['ar'] = val
    return it


def _add_affix(it, table, used):
    choices = [k for k, (stat, _r, _g, slots) in table.items()
               if k not in used and (slots is None or it['slot'] in slots)]
    if not choices:
        return None
    k = random.choice(choices)
    stat, rng, grow, _slots = table[k]
    it['stats'][stat] = it['stats'].get(stat, 0) + _roll(rng, grow, it['tier'])
    used.add(k)
    return k


def roll(depth, kinds=None, rarity=None, at_least='normal'):
    """A random item for this floor. kinds: weapon kinds the party can use (other weapons are not rolled)."""
    tier = tier_for(depth)
    if rarity is None:
        r = random.random()
        u = .004 + depth * .0015
        rarity = 'unique' if r < u else 'rare' if r < u + .06 + depth * .006 else 'magic' if r < .45 + depth * .01 else 'normal'
    order = ['normal', 'magic', 'rare', 'unique']
    if order.index(rarity) < order.index(at_least):
        rarity = at_least
    if rarity == 'unique':
        pool = [u for u in D.UNIQUES if D.BASES[u[1]][1] != 'weapon' or not kinds or D.BASES[u[1]][2] in kinds]
        name, base, stats = random.choice(pool)
        it = make(base, 'unique', max(D.BASES[base][3], tier))
        it['name'] = name
        for k, v in stats.items():
            it['stats'][k] = it['stats'].get(k, 0) + v
        it['value'] = 400 + 100 * it['tier']
        return it
    bases = [b for b, (_n, slot, kind, bt, _v) in D.BASES.items()
             if bt <= tier and bt >= tier - 2 and (slot != 'weapon' or not kinds or kind in kinds)]
    if rarity == 'normal':
        bases = [b for b in bases if D.BASES[b][1] not in ('ring', 'amulet')] or bases
    # weapons and body armour a little more often
    weights = [3 if D.BASES[b][1] in ('weapon', 'body') else 1 for b in bases]
    base = random.choices(bases, weights)[0]
    it = make(base, rarity, tier)
    if D.BASES[base][3] < tier:                     # an old base found deep is made better
        k = 1 + .25 * (tier - D.BASES[base][3])
        if 'dmg' in it:
            it['dmg'] = [round(it['dmg'][0] * k), round(it['dmg'][1] * k)]
        else:
            it['ar'] = round(it['ar'] * k)
    used = set()
    if rarity == 'magic':
        pre = _add_affix(it, D.PREFIXES, used) if random.random() < .7 else None
        suf = _add_affix(it, D.SUFFIXES, used) if (not pre or random.random() < .5) else None
        it['name'] = ' '.join(x for x in (pre, it['name'], suf) if x)
    elif rarity == 'rare':
        for _ in range(random.randint(3, 4)):
            _add_affix(it, random.choice((D.PREFIXES, D.SUFFIXES)), used)
        it['name'] = '%s %s' % (random.choice(D.RARE_A), random.choice(D.RARE_B[it['slot']]))
        it['basename'] = D.BASES[base][0]
    it['value'] = (10 + 15 * it['tier']) * {'normal': 1, 'magic': 3, 'rare': 7}[rarity]
    return it


def lines(it):
    """What the item says about itself, line by line."""
    out = []
    if it.get('basename'):
        out.append(it['basename'])
    if 'dmg' in it:
        out.append('damage %d-%d' % tuple(it['dmg']))
    elif it.get('ar'):
        out.append('armour %d' % it['ar'])
    for k, v in it['stats'].items():
        out.append(D.STAT_TEXT[k].format(v))
    return out


def short(it):
    if 'dmg' in it:
        return '%s (%d-%d)' % (it['name'], *it['dmg'])
    if it.get('ar'):
        return '%s [%d]' % (it['name'], it['ar'])
    return it['name']
