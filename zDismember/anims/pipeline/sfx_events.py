r"""Zdarzenia animacji w skompilowanym MSB (blok 0xF5A0 po chunku ANI) i ich wykrywanie z ruchu szkieletu.

Formaty (jak w HUMANS.MSB): 0xF5A1 dzwiek = i32 klatka, nazwa\n, f32 zasieg (2500), f32 0;
0xF5A2 krok na podlozu = i32 klatka, nazwa\n ("RUN" - dzwiek wg materialu ziemi), f32 1000, f32 1;
0xF5A3 tag = i32 klatka + 5 linii (DEF_OPT_FRAME itd.); 0xF5AF koniec bloku. Klatki od 1 (jak pierwsza klatka ANI).
Nazwy z gry: RUN (krok), M_FALL_SMALL (upadek ciala), WHOOSH (swist), FIG_SWORDFINAL (dobicie), D_PARADE (odskok).
"""
import struct
import numpy as np


def chunk(t, body):
    return struct.pack("<HI", t, len(body)) + body


def event_sfx(frame, name, rng=2500.0):
    return chunk(0xF5A1, struct.pack("<i", frame) + name.encode("latin1") + b"\n" + struct.pack("<ff", rng, 0.0))


def event_sfx_ground(frame, name="RUN"):
    return chunk(0xF5A2, struct.pack("<i", frame) + name.encode("latin1") + b"\n" + struct.pack("<ff", 1000.0, 1.0))


def event_tag(tag, *args, frame=0):
    lines = [tag] + list(args) + [""] * (4 - len(args))
    return chunk(0xF5A3, struct.pack("<i", frame) + b"".join(s.encode("latin1") + b"\n" for s in lines))


def events_block(sfx=(), ground=(), tags=()):
    """Blok zdarzen do doklejenia za chunkiem ANI. sfx/ground: (klatka od 1, nazwa); tags: gotowe chunki event_tag."""
    events = list(tags) + [event_sfx(f, n) for f, n in sorted(sfx)] + [event_sfx_ground(f, n) for f, n in sorted(ground)]
    if not events:
        return b""
    return chunk(0xF5A0, struct.pack("<I", len(events))) + b"".join(events) + chunk(0xF5AF, b"")


def landings(ys, lift=3.0, touch=1.0):
    """Klatki (od 0) postawienia stopy: wysokosc nad najnizsza w klipie spada do 'touch' po uniesieniu ponad 'lift'."""
    base = min(ys)
    out, up = [], ys[0] - base > lift
    for k, y in enumerate(ys):
        h = y - base
        if up and h <= touch:
            out.append(k)
            up = False
        elif not up and h > lift:
            up = True
    return out


def body_fall(pelvis_y, margin=5.0, low=40.0):
    """Klatka (od 0), w ktorej miednica dochodzi do ziemi po upadku; None gdy postac nie lezy."""
    m = min(pelvis_y)
    if m > low:
        return None
    return next(k for k, y in enumerate(pelvis_y) if y <= m + margin)


def speed_peaks(points, fps, min_speed, min_gap):
    """Klatki (od 0) szczytow predkosci punktu (np. czubka ostrza) powyzej min_speed (cm/s), co najmniej min_gap klatek."""
    sp = [0.0] + [float(np.linalg.norm(points[k] - points[k - 1])) * fps for k in range(1, len(points))]
    peaks = []
    for k in range(1, len(sp) - 1):
        if sp[k] >= min_speed and sp[k] >= sp[k - 1] and sp[k] >= sp[k + 1]:
            if peaks and k - peaks[-1] < min_gap:
                if sp[k] > sp[peaks[-1]]:
                    peaks[-1] = k
                continue
            peaks.append(k)
    return peaks


def motion_sounds(frames, sk, fps=25.0, whoosh=False, fall=False):
    """Zdarzenia z ruchu klatek (lokalne obroty + korzen z ruchem): kroki obu stop, opcjonalnie upadek i swisty ostrza.
    -> (sfx, ground) z klatkami od 1."""
    W = [sk.compose(local, root) for local, root in frames]
    ground = []
    for foot in ("BIP01 L FOOT", "BIP01 R FOOT"):
        i = sk.idx(foot)
        ground += [(k + 1, "RUN") for k in landings([w[i][1, 3] for w in W], lift=4.0, touch=2.5)]
    sfx = []
    if fall:
        k = body_fall([w[sk.idx("BIP01")][1, 3] for w in W])
        if k is not None and k > 2:
            sfx.append((k + 1, "M_FALL_SMALL"))
    if whoosh:
        gi = sk.idx("ZS_RIGHTHAND")
        tips = [w[gi][:3, 3] - w[gi][:3, 0] * 90.0 for w in W]
        sfx += [(k + 1, "WHOOSH") for k in speed_peaks(tips, fps, 900.0, 10)]
    # krok tuz przy upadku albo dwa w jednej klatce - jeden wystarczy
    ground = sorted(set(ground))
    return sfx, ground
