"""Moving pictures for blows and spells, drawn over the map for a moment and never waited on.

Adapted from Avarenthia's anim.py (NilioDev/Avarenthia): bolts with a fading tail, round bursts, waves, swings,
spins, crits and blood. fx_cells(kind, kw, age) gives the cells an effect lights at that age; fx_life how long it lasts."""
import math


def _clamp(v):
    return 0 if v < 0 else 255 if v > 255 else int(v)


def scale(c, f):
    return (_clamp(c[0] * f), _clamp(c[1] * f), _clamp(c[2] * f))


def mix(a, b, f):
    return (_clamp(a[0] + (b[0] - a[0]) * f), _clamp(a[1] + (b[1] - a[1]) * f), _clamp(a[2] + (b[2] - a[2]) * f))


# ------------------------------------------------------------------ spell effects
# A small set of moving pictures that every school shares: bolts that fly with a fading tail, impacts that open out,
# life flowing back, rising light, lightning that zigzags. Never longer than about .4 s; a key skips them and the game
# never waits for one. game.fx(kind, **kw) queues one; the GameScreen draws it over the map.
# Each school has its own hand on top: rings and waves, Meteor's shadow and slanting fall, the dead that grey and
# rise, walls that go up one stone at a time, and one white frame of the sky for a great lightning stroke.
FX_MODES = ('full', 'short', 'off')
STEP = .02                # a bolt flies one cell in this long
BOLT_MAX = .12
BURST_FRAME = .05
WAVE_FRAME = .06          # kept for timing a second wave after a first
ROUND_STEP = .03          # an area opens out by real distance: one tile further in this long (it looks round)
FRONT = .04               # each sign of the front (* + ·) shows this long, then the ground glows ░ and dims
ROUND_DUR = .22
SWING_TICK, SWING_LIT = .045, .12     # a weapon's swing arc: one sign after another, each lit a moment
SPIN_TICK = .03
CRIT_TIME, BLOOD_TIME = .26, .3
STEEL_DIM, GOLD_CRIT, BLOOD_RED = (150, 152, 160), (255, 214, 90), (200, 30, 30)
CLOUD_DUR = .22           # the faint cloud beyond the square's sides: only a look, it harms nobody
DUST = (150, 140, 130)
NO_GLOW = ('sound', '~', '!')     # a howl, a gust, a war cry: only the front, no glow, no cloud
SKY_TIME = .07            # the white frame of a great lightning stroke
METEOR_SHADOW, METEOR_FALL = .08, .1
RAISE_TIME = .36
TRAIL_STEP = .03


def _line_char(dx, dy):
    """The sign of a flying arrow or a stroke of lightning, by its direction."""
    if dx == 0:
        return '|'
    if dy == 0:
        return '-'
    return '\\' if (dx > 0) == (dy > 0) else '/'


def bolt_time(n):
    return min(BOLT_MAX, STEP * max(1, n))


def fx_life(kind, kw):
    """How long (delay included) an effect of this kind stays on the screen."""
    d = kw.get('delay', 0.0)
    if kind == 'bolt':
        return d + bolt_time(len(kw['cells'])) + .06
    if kind in ('burst', 'wave') and kw.get('r', 1) >= 1:
        r = kw.get('r', 1)
        g = BURST_FRAME if kind == 'burst' and kw.get('gather') else 0
        return d + g + max(ROUND_STEP * r * 1.415 + kw.get('dur', ROUND_DUR), ROUND_STEP * r + CLOUD_DUR)
    if kind == 'burst':
        return d + BURST_FRAME * (kw.get('r', 1) + 3 + (1 if kw.get('gather') else 0))
    if kind == 'zap':
        return d + .24
    if kind == 'sky':
        return d + SKY_TIME
    if kind == 'meteor':
        return d + METEOR_SHADOW + METEOR_FALL
    if kind == 'raise':
        return d + RAISE_TIME
    if kind == 'trail':
        return d + TRAIL_STEP * len(kw['cells']) + kw.get('linger', .14)
    if kind == 'swing':
        return d + SWING_TICK * (5 if kw.get('wide') else 3) + SWING_LIT
    if kind == 'spin':
        return d + SPIN_TICK * 8 + SWING_LIT
    if kind == 'crit':
        return d + CRIT_TIME
    if kind == 'blood':
        return d + BLOOD_TIME
    return d + kw.get('dur', .28 if kind == 'drain' else .4)


def fx_fallback(kind, kw):
    """With effects off: the plain flash of old, (cells, colour, sign), or None."""
    col = kw.get('col', (255, 255, 255))
    if kind in ('bolt', 'zap'):
        return kw['cells'], col, kw.get('ch', '*')
    if kind in ('burst', 'wave'):
        x, y, r = kw['x'], kw['y'], kw.get('r', 1)
        return ([(x + dx, y + dy) for dy in range(-r, r + 1) for dx in range(-r, r + 1)
                 if not ((kw.get('hollow') or kind == 'wave') and dx == dy == 0)], col, kw.get('ch', '*'))
    if kind in ('trail', 'line'):
        return kw['cells'], col, kw.get('ch', '*')
    if kind == 'raise':
        return [(kw['x'], kw['y'])], col, kw.get('ch', 'z')
    return None                 # the sky's white frame and Meteor's shadow have no flash of old


def _wave_sign(sign, dx, dy):
    """The sign of a wave at this offset: a sound wave bends round as ( ), wind runs as ~, the rest keep theirs."""
    if sign == 'sound':
        if abs(dx) >= abs(dy):
            return '(' if dx < 0 else ')'
        return '-'
    return sign


def _round(x, y, r, col, age, signs='*+·', hollow=False, sign=None, glow=True, cloud=True, dust=False, dur=ROUND_DUR):
    """An area of radius r (what is hit is the whole square, the same range every way) drawn so it looks round: a
    front of signs runs out by real distance, the ground behind it glows ░ and dims, the corners are drawn softer,
    and a faint cloud beyond the sides rounds it off (a Meteor's is brown dust)."""
    out = []
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if hollow and dx == dy == 0:
                continue
            d = math.sqrt(dx * dx + dy * dy)
            q = age - d * ROUND_STEP
            if not 0 <= q < dur:
                continue
            soft = d > r + .5
            st = int(q / FRONT)
            if st < len(signs):
                c = mix(col, (255, 250, 220), .5) if st == 0 else _fade(col, 1 - st * .3)
                ch = _wave_sign(sign, dx, dy) if st == 0 and sign else signs[st]
                out.append((x + dx, y + dy, ch, scale(c, .72) if soft else c, None))
            elif glow:
                k = (q - FRONT * len(signs)) / max(.01, dur - FRONT * len(signs))
                out.append((x + dx, y + dy, '░', mix(col, (14, 12, 12), min(.95, .3 + .55 * k + (.15 if soft else 0))), None))
    if cloud and r >= 1:
        a = age - ROUND_STEP * r
        if 0 <= a < CLOUD_DUR:
            k = a / CLOUD_DUR
            base = mix((200, 120, 80), DUST, .3) if dust else mix(col, DUST, .5)
            rr = r * 1.415 + .7
            for dy in range(-r - 2, r + 3):
                for dx in range(-r - 2, r + 3):
                    if max(abs(dx), abs(dy)) <= r or dx * dx + dy * dy > rr * rr:
                        continue
                    if k > .6 and ((x + dx) * 73856093 ^ (y + dy) * 19349663) % 5 == 0:
                        continue                # a few puffs thin out first: a ragged edge
                    out.append((x + dx, y + dy, '░' if k < .55 else '·', mix(base, (14, 12, 12), .35 + .55 * k), None))
    return out


def slant(x, y, n=4):
    """Where a Meteor falls from: a slanting line down onto (x, y) from the upper left, ending on it."""
    return [(x - k, y - k) for k in range(n, -1, -1)]


def sky_on(kw, age):
    """Whether the sky's white frame is lit at this age."""
    a = age - kw.get('delay', 0.0)
    return 0 <= a < SKY_TIME


def _fade(col, k):
    """A dimmer shade, never so dark that it reads as a black mark on lit ground."""
    return scale(col, .45 + .55 * max(0.0, min(1.0, k)))


def fx_cells(kind, kw, age):
    """The cells an effect lights at this age: [(x, y, sign, colour, background or None)]."""
    age -= kw.get('delay', 0.0)
    if age < 0:
        return []
    col = kw.get('col', (255, 255, 255))
    out = []
    if kind == 'bolt':
        cells = kw['cells']
        if not cells:
            return out
        n = len(cells)
        head = int(age / bolt_time(n) * n)
        ch = kw.get('ch', '*')
        if ch in '-|/\\':                       # arrows and stones point the way they fly
            sx, sy = kw.get('src', cells[0])
            ex, ey = cells[-1]
            ch = _line_char((ex > sx) - (ex < sx), (ey > sy) - (ey < sy))
        for k in range(4):                      # the head and a tail of three, fading
            i = head - k
            if 0 <= i < n:
                out.append((*cells[i], ch if k == 0 else '·' if k == 1 else '∙',
                            mix(col, (255, 255, 240), .35) if k == 0 else _fade(col, 1 - k * .28), None))
        return out
    if kind == 'burst':
        x, y, r = kw['x'], kw['y'], kw.get('r', 1)
        f = int(age / BURST_FRAME)
        if kw.get('gather'):                    # a big spell draws the light in for a frame first
            if f == 0:
                rr = r + 1
                return [(x + dx, y + dy, '·', _fade(col, .55), None) for dy in range(-rr, rr + 1)
                        for dx in range(-rr, rr + 1) if max(abs(dx), abs(dy)) == rr]
            f -= 1
        signs = kw.get('signs', '*+·')
        if r >= 1:
            return _round(x, y, r, col, age - (BURST_FRAME if kw.get('gather') else 0), signs, kw.get('hollow'),
                          dust=kw.get('dust'), dur=kw.get('dur', ROUND_DUR))
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                d = max(abs(dx), abs(dy))
                if kw.get('hollow') and d == 0:
                    continue
                s = f - d
                if 0 <= s < len(signs):
                    hot = mix(col, (255, 250, 220), .5) if s == 0 else _fade(col, 1 - s * .3)
                    out.append((x + dx, y + dy, signs[s], hot, None))
        return out
    if kind == 'drain':                         # motes of life flying from the foe back to you
        cells = kw['cells']
        n = len(cells)
        if not n:
            return out
        dur = kw.get('dur', .28)
        for j in range(3):
            k = (age - j * .06) / max(.01, dur - .12)
            if 0 <= k < 1:
                i = min(n - 1, int(k * n))
                out.append((*cells[i], '∙' if j else '•', _fade(col, 1 - j * .25), None))
        return out
    if kind == 'rise':                          # small crosses of light rising off whoever is healed
        x, y = kw['x'], kw['y']
        dur = kw.get('dur', .4)
        for j, dx in enumerate((0, -1, 1)):
            k = (age - j * .07) / (dur - .14)
            if 0 <= k < 1:
                out.append((x + dx, y - 1 - int(k * 2), '+', _fade(col, 1 - k * .7), None))
        return out
    if kind == 'zap':                           # a white zigzag that fades to yellow
        cells = kw['cells']
        k = age / .24
        if k >= 1:
            return out
        c = mix((255, 255, 255), col, k)
        for i, (cx, cy) in enumerate(cells):
            out.append((cx, cy, '/' if i % 2 == 0 else '\\', c, None))
        return out
    if kind == 'wave':                          # a ring running out from (x, y): the school's sign as its front
        x, y, r = kw['x'], kw['y'], kw.get('r', 1)
        sign = kw.get('sign', '*')
        quiet = sign in NO_GLOW
        signs = '*+·' if sign == '*' else (sign if sign != 'sound' else '(') + '·'
        return _round(x, y, r, col, age, signs, True, sign=sign if sign == 'sound' else None, glow=not quiet,
                      cloud=not quiet, dur=kw.get('dur', ROUND_DUR))
    if kind == 'meteor':                        # its shadow on the ground first, then it falls in at a slant
        x, y, r = kw['x'], kw['y'], kw.get('r', 1)
        shade = (52, 46, 44)
        k = min(1.0, age / METEOR_SHADOW)
        for dy in range(-1, 2):
            for dx in range(-1, 2):
                if dx == dy == 0 or k > .5:
                    out.append((x + dx, y + dy, '░', shade if dx == dy == 0 else mix(shade, (24, 22, 20), .4), None))
        fall = age - METEOR_SHADOW
        if fall >= 0:
            cells = slant(x, y)
            head = min(len(cells) - 1, int(fall / METEOR_FALL * len(cells)))
            for j in range(3):
                i = head - j
                if 0 <= i < len(cells):
                    out.append((*cells[i], '*' if j == 0 else '·',
                                mix(col, (255, 250, 220), .5) if j == 0 else _fade(col, 1 - j * .3), None))
        return out
    if kind == 'raise':                         # a corpse greys, then gets up as what it now is
        x, y = kw['x'], kw['y']
        k = age / RAISE_TIME
        if k >= 1:
            return out
        corpse, grave = kw.get('from_col', (150, 60, 50)), (150, 170, 140)
        if k < .55:
            return [(x, y, '%', mix(corpse, grave, k / .55), None)]
        return [(x, y, kw.get('ch', 'z'), mix((235, 245, 225), col, (k - .55) / .45), None)]
    if kind == 'trail':                         # one cell after another: a wall going up, dust behind a run
        cells = kw['cells']
        ch = kw.get('ch', '*')
        linger = kw.get('linger', .14)
        for i, c in enumerate(cells):
            a = age - i * TRAIL_STEP
            if 0 <= a < linger:
                q = a / linger
                out.append((*c, ch, mix((255, 255, 245), col, min(1.0, q * 2.5)) if kw.get('build') else
                            _fade(col, 1 - q), None))
        return out
    if kind == 'line':                          # a stroke from the sky: the whole line at once, white to its colour
        cells = kw['cells']
        k = age / kw.get('dur', .4)
        if k >= 1 or not cells:
            return out
        sx, sy = cells[0]
        ex, ey = cells[-1]
        ch = kw.get('ch') or _line_char((ex > sx) - (ex < sx), (ey > sy) - (ey < sy))
        c = mix((255, 255, 255), col, min(1.0, k * 1.6))
        for cx, cy in cells:
            out.append((cx, cy, ch, c if k < .6 else _fade(c, 1 - (k - .6) / .4), None))
        return out
    if kind == 'inward':                        # a ring of motes drawn in to one spot, then gone
        x, y = kw['x'], kw['y']
        k = age / kw.get('dur', .4)
        if k >= 1:
            return out
        rr = max(0, round(3 * (1 - k)))
        if rr == 0:
            return [(x, y, '•', mix(col, (255, 255, 255), .3), None)]
        return [(x + dx, y + dy, '∙', _fade(col, .5 + k / 2), None) for dy in range(-rr, rr + 1)
                for dx in range(-rr, rr + 1) if max(abs(dx), abs(dy)) == rr and (dx + dy) % 2 == 0]
    if kind == 'swing':                         # \ | / along the side of the foe, white fading to steel
        return _swing(kw, age)
    if kind == 'spin':                          # the blade once round you
        x, y = kw['x'], kw['y']
        for i, (dx, dy) in enumerate(((1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1))):
            a = age - i * SPIN_TICK
            if 0 <= a < SWING_LIT:
                out.append((x + dx, y + dy, _line_char(dx, dy), mix((255, 255, 250), STEEL_DIM, a / SWING_LIT), None))
        return out
    if kind == 'crit':                          # a white star that turns gold and fades
        k = age / CRIT_TIME
        if k < 1:
            out.append((kw['x'], kw['y'], '*', mix((255, 255, 255), GOLD_CRIT, min(1.0, k * 2)) if k < .6
                        else _fade(GOLD_CRIT, 1 - (k - .6) / .4), None))
        return out
    if kind == 'blood':                         # a red * on the cut, then drops fly back and darken
        x, y = kw['x'], kw['y']
        sx, sy = kw.get('src', (x - 1, y))
        dx, dy = (x > sx) - (x < sx), (y > sy) - (y < sy)
        k = age / BLOOD_TIME
        if k >= 1:
            return out
        if k < .3:
            out.append((x, y, '*', BLOOD_RED, None))
        for j, (ox, oy) in enumerate(((dx, dy), (dx - dy, dy + dx), (dx + dy, dy - dx))):
            ox, oy = max(-1, min(1, ox)), max(-1, min(1, oy))
            n = 1 + int(k * 1.8)                    # one tile out, then two
            if (ox or oy) and k > .1:
                out.append((x + ox * n, y + oy * n, ',' if j == 0 else '·', _fade(BLOOD_RED, 1 - k), None))
        return out
    return out


def _swing(kw, age):
    """The cells of a swing arc: the foe's tile and its neighbours on your side, swept in turn."""
    out = []
    sx, sy = kw.get('src', (0, 0))
    seen = set()
    for tx, ty in kw.get('cells', ()):
        dx, dy = (tx > sx) - (tx < sx), (ty > sy) - (ty < sy)
        dirs = [(1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1), (0, -1), (1, -1)]
        if (dx, dy) not in dirs:
            continue
        i = dirs.index((dx, dy))
        half = 2 if kw.get('wide') else 1
        far = max(abs(tx - sx), abs(ty - sy))
        for n, k in enumerate(range(-half, half + 1)):
            ex, ey = dirs[(i + k) % 8]
            c = (sx + ex * far, sy + ey * far)
            if c in seen:
                continue
            seen.add(c)
            a = age - n * SWING_TICK
            if 0 <= a < SWING_LIT:
                out.append((c[0], c[1], _line_char(ex, ey), mix((255, 255, 250), STEEL_DIM, a / SWING_LIT), None))
    return out


