r"""Medytacja W3 -> nakladka HUMANS_ZDF_W3_MEDITATE (TYLKO uzytek prywatny - animacje CD Projekt RED).

Uzycie: py -3.12 -I make_meditate.py [--variant 1|2]
Klipy z animations\interaction\environment\man_meditation.w2anims (wypakowane bundle_extract.py do w3_uncook):
  T_STAND_2_PRAY        <- meditation_start_long   stoi -> kleczy (wejscie z postawy S_RUN Gothica)
  S_PRAY                <- meditation_idle01        petla kleczenia (domkniecie bez szwu)
  T_SLEEPGROUND_2_STAND <- meditation_stop_long     kleczy -> stoi (powrot do postawy S_RUN)
Nazwy MUSZA istniec w bazowym HUMANS (Steam, NB i Kroniki) - nakladka z nowa nazwa animacji jest w calosci
odrzucana przez silnik ("ani in overlay not present in base"); T_PRAY_2_STAND to w HUMANS tylko alias.
Wariant 2: meditation_start_long_2 / idle_2 / stop_long_2 (dluzsze).
Korzen jak w natywnym kleczeniu Gothica (T_STAND_2_PRAY: flaga M., korzen zjezdza w dol i do tylu wzgledem voba):
wzgledem postaci (to_entity_start), bez wymuszania miejsca. Plugin zaklada nakladke tylko na czas medytacji
(spec: [dokument projektowy - niepublikowany]).
"""
import argparse, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import manlib, msblib
import retarget, build_overlay, make_pair, make_moves, make_signs
from retarget import qmat

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(ROOT, "anims", "work", "w3meditate")
COMPILED = make_moves.COMPILED
SET = chr(92).join(["animations", "interaction", "environment", "man_meditation.w2anims"])
OVERLAY = "HUMANS_ZDF_W3_MEDITATE"
ANI_START, ANI_LOOP, ANI_STOP = "T_STAND_2_PRAY", "S_PRAY", "T_SLEEPGROUND_2_STAND"
VARIANTS = {
    1: ("meditation_start_long", "meditation_idle01", "meditation_stop_long"),
    2: ("meditation_start_long_2", "meditation_idle_2", "meditation_stop_long_2"),
}
STAND = "S_RUN"      # postawa Gothica, z ktorej sie kleka i do ktorej sie wstaje
ENTRY = 10           # klatek przejscia z postawy Gothica w 1. klatke W3 (0,4 s)
EXIT = 10            # klatek przejscia z ostatniej klatki W3 do postawy Gothica
# 24.10 stopy i podloze. Retarget przenosi same obroty, a nogi Gothica maja inne proporcje niz W3 (udo 39 vs 49 cm,
# lydka 49 vs 44, stopa 26 vs 18, biodra 18 vs 21 cm): w kleku W3 (siad na pietach, stopy pod posladkami) dluzsza lydka
# Gothica sprowadza kostki do srodka - stopy 5,6 cm od siebie (osie kostka->palce 3,6 cm; W3 16-18 cm, natywne S_PRAY
# 22 cm) przy szerokosci stopy siatki 14,6 cm = stopy wchodza w siebie. Do tego krotsze udo podnosi kolana (13 vs 7 cm)
# i caly klek wisial ~13 cm nad podlozem (najnizszy punkt siatki 9 cm, a stojacy S_RUN -3,9 cm).
FEET_MIN = 18.0      # min. odleglosc osi stop (kostka->palce) i lydek L-R w cm; stopa siatki 14,6 cm szerokosci
FEET_SOFT = 4.0      # miekkie wejscie rozsuniecia (cm) - bez zalamania ruchu na granicy
BODY_MESH = "HUM_BODY_NAKED0.MDM"   # siatka do pomiaru podloza (najnizszy wierzcholek)
KNEE_R = 12.0        # promien (cm) wokol stawu kolanowego - wierzcholki kolana
LIFT_BAND = (10.0, 25.0)   # roznica wysokosci kolano-stopa (cm): ponizej pelne zrownanie stopy z kolanem, powyzej nic
LIFT_MAX = 45.0      # max. dodatkowy obrot stopy w kostce (st.)
FLOOR_SIGMA = 1.5    # wygladzanie (gauss, klatki @25) przesuniecia korzenia do podloza przy zmianie punktu podparcia
FLOOR_PEN = 2.0      # ... przy czym punkt podparcia najwyzej tyle cm pod podlozem (chwilowo, przy ladowaniu kolana)
LIFT_R = 4           # promien wygladzania obrotu stop w czasie (klatki @25)
ROLL_MAX = 10.0      # max. przechyl miednicy wyrownujacy podparcie nog (st.)
KNEEL_BAND = (9.0, 20.0)   # kolano nad najnizszym punktem ciala (cm): ponizej klek (pelny obrot stopy), powyzej nic


class Body:
    """Skinning siatki ciala (soft skin: wierzcholek = suma wag * macierz wezla @ pozycja lokalna)."""

    def __init__(self, vfs, sk, mdm=BODY_MESH):
        import zenkit
        m = zenkit.ModelMesh.load(vfs.find(mdm))
        rows = [[(w.index, w.weight, (w.position.x, w.position.y, w.position.z)) for w in ws]
                for s in m.meshes for ws in s.weights]
        k = max(len(r) for r in rows)
        self.sk = sk
        self.idx = np.zeros((len(rows), k), int)
        self.w = np.zeros((len(rows), k))
        self.pos = np.zeros((len(rows), k, 3))
        for i, r in enumerate(rows):
            for j, (ni, w, p) in enumerate(r):
                self.idx[i, j], self.w[i, j], self.pos[i, j] = ni, w, p
        self.main = self.idx[np.arange(len(rows)), self.w.argmax(axis=1)]   # dominujacy wezel wierzcholka

    def skin(self, local, root):
        W = np.array(self.sk.compose(local, root))[self.idx]
        p = np.einsum("nkij,nkj->nki", W[..., :3, :3], self.pos) + W[..., :3, 3]
        return (p * self.w[..., None]).sum(axis=1)

    def lowest(self, frame):
        return float(self.skin(*frame)[:, 1].min())


def seg_dist(p1, q1, p2, q2):
    """Najmniejsza odleglosc odcinkow p1-q1 i p2-q2."""
    d1, d2, r = q1 - p1, q2 - p2, p1 - p2
    a, e, f, c, b = d1 @ d1, d2 @ d2, d2 @ r, d1 @ r, d1 @ d2
    den = a * e - b * b
    s = float(np.clip((b * f - c * e) / den, 0, 1)) if den > 1e-9 else 0.0
    t = (b * s + f) / e
    if t < 0:
        t, s = 0.0, float(np.clip(-c / a, 0, 1))
    elif t > 1:
        t, s = 1.0, float(np.clip((b - c) / a, 0, 1))
    return float(np.linalg.norm(p1 + s * d1 - p2 - t * d2))


def leg_points(sk, W, side):
    return [W[sk.idx("BIP01 %s %s" % (side, n))][:3, 3] for n in ("THIGH", "CALF", "FOOT", "TOE0")]


def feet_gap(sk, frame):
    """Odstep stop: min. odleglosc osi stop (kostka->palce) i lydek (kolano->kostka) lewej i prawej nogi."""
    W = sk.compose(*frame)
    L, R = leg_points(sk, W, "L"), leg_points(sk, W, "R")
    return min(seg_dist(L[2], L[3], R[2], R[3]), seg_dist(L[1], L[2], R[1], R[2]),
               seg_dist(L[2], L[3], R[1], R[2]), seg_dist(L[1], L[2], R[2], R[3]))


def leg_ik(sk, frame, side, target):
    """Dwukoscowe IK nogi: kostka do 'target' (model), kolano w plaszczyznie biodro-cel-dawne kolano, biodro w miejscu;
    stopa zachowuje obrot swiatowy (palce ida za nia)."""
    local, root = frame
    W = sk.compose(local, root)
    h, c, f = (sk.idx("BIP01 %s %s" % (side, n)) for n in ("THIGH", "CALF", "FOOT"))
    H, K, A = W[h][:3, 3], W[c][:3, 3], W[f][:3, 3]
    l1, l2 = np.linalg.norm(K - H), np.linalg.norm(A - K)
    d = np.linalg.norm(target - H)
    d = min(max(d, abs(l1 - l2) + 1e-3), l1 + l2 - 1e-3)
    e = (target - H) / np.linalg.norm(target - H)
    a = (l1 * l1 - l2 * l2 + d * d) / (2 * d)
    pole = (K - H) - ((K - H) @ e) * e
    pole /= np.linalg.norm(pole)
    K2 = H + a * e + np.sqrt(max(l1 * l1 - a * a, 0.0)) * pole
    T = H + d * e
    R1 = retarget.rot_between(K - H, K2 - H)
    Wt = R1 @ W[h][:3, :3]
    A1 = K2 + R1 @ (A - K)
    Wc = retarget.rot_between(A1 - K2, T - K2) @ R1 @ W[c][:3, :3]
    out = list(local)
    p = sk.parent[h]
    out[h] = W[p][:3, :3].T @ Wt
    out[c] = Wt.T @ Wc
    out[f] = Wc.T @ W[f][:3, :3]
    return out, root


def spread_feet(sk, frame, min_gap=FEET_MIN, soft=FEET_SOFT):
    """Rozsuwa kostki w bok (os biodra L-R w poziomie) tak, by odstep stop byl >= min_gap. Cel z miekkim wejsciem
    (gap + (min_gap - gap + soft)^2 / 4soft w pasie +-soft) - poza pasem poza nietknieta, w srodku gladko (C1)."""
    gap0 = feet_gap(sk, frame)
    deficit = min_gap - gap0
    if deficit <= -soft:
        return frame
    goal = gap0 + (deficit if deficit >= soft else (deficit + soft) ** 2 / (4 * soft))
    for _ in range(6):
        gap = feet_gap(sk, frame)
        push = goal - gap
        if push < 0.05:
            break
        W = sk.compose(*frame)
        u = W[sk.idx("BIP01 L THIGH")][:3, 3] - W[sk.idx("BIP01 R THIGH")][:3, 3]
        u[1] = 0.0
        u /= np.linalg.norm(u)
        for side, sgn in (("L", 1.0), ("R", -1.0)):
            A = W[sk.idx("BIP01 %s FOOT" % side)][:3, 3]
            frame = leg_ik(sk, frame, side, A + sgn * 0.5 * push * u)
    return frame


def _leg_lows(body, sk, frame, side, P=None):
    """(najnizszy punkt kolana, najnizszy punkt stopy) siatki jednej nogi; kolano = wierzcholki uda/lydki do
    KNEE_R cm od stawu kolanowego (w stojacej postawie ~60 cm nad stopa, w kleku W3 ~5 cm)."""
    P = body.skin(*frame) if P is None else P
    W = sk.compose(*frame)
    K = W[sk.idx("BIP01 %s CALF" % side)][:3, 3]
    knee = np.isin(body.main, [sk.idx("BIP01 %s THIGH" % side), sk.idx("BIP01 %s CALF" % side)]) & \
        (np.linalg.norm(P - K, axis=1) < KNEE_R)
    foot = np.isin(body.main, [sk.idx("BIP01 %s FOOT" % side), sk.idx("BIP01 %s TOE0" % side)])
    return float(P[knee, 1].min()) if knee.any() else 1e9, float(P[foot, 1].min())


def _rot_foot(sk, frame, side, ang):
    """Stopa obrocona o 'ang' (rad) w stawie skokowym wokol poziomej osi bioder L-R (palce w gore / w dol, bez
    przesuwania w bok - odstep stop z spread_feet zostaje)."""
    local, root = frame
    W = sk.compose(local, root)
    c, f = (sk.idx("BIP01 %s %s" % (side, n)) for n in ("CALF", "FOOT"))
    n = W[sk.idx("BIP01 L THIGH")][:3, 3] - W[sk.idx("BIP01 R THIGH")][:3, 3]
    n[1] = 0.0
    if np.linalg.norm(n) < 1e-6:
        return frame
    out = list(local)
    out[f] = W[c][:3, :3].T @ retarget._axis_rot(n, ang) @ W[f][:3, :3]
    return out, root


def lift_angle(body, sk, frame, side):
    """Klek z kolanem na ziemi: stopa Gothica jest o 8 cm dluzsza niz W3, wiec palce w siadzie na pietach siegaja
    nizej niz kolana (kolana 5-8 cm nad podlozem, gdy palce na nim). Gdy kolano jest blisko poziomu stopy (roznica <
    LIFT_BAND[1]), stopa obraca sie w kostce (bardziej wyprostowana) az jej najnizszy punkt zrowna sie z kolanem;
    w pasie LIFT_BAND czesciowo (smoothstep), stojace nogi (kolano ~60 cm nad stopa) bez zmian.
    Zwraca obrot ze znakiem (rad) dla _rot_foot."""
    lo, hi = LIFT_BAND
    k0, f0 = _leg_lows(body, sk, frame, side)
    gap = k0 - f0
    if gap <= 0.05 or gap >= hi:
        return 0.0
    u = min(1.0, max(0.0, (hi - gap) / (hi - lo)))
    want = f0 + gap * u * u * (3 - 2 * u)
    foot_low = lambda a: _leg_lows(body, sk, _rot_foot(sk, frame, side, a), side)[1]
    sgn = 1.0 if foot_low(0.02) > foot_low(-0.02) else -1.0
    a_lo, a_hi = 0.0, math.radians(LIFT_MAX)
    if foot_low(sgn * a_hi) <= want:
        return sgn * a_hi
    for _ in range(18):
        mid = 0.5 * (a_lo + a_hi)
        if foot_low(sgn * mid) < want:
            a_lo = mid
        else:
            a_hi = mid
    return sgn * a_lo


def pelvis_roll(sk, frame, ang):
    """Miednica (z nogami) przechylona o 'ang' (rad) wokol poziomej osi przod-tyl przez staw miednicy; tulow (SPINE)
    zachowuje obrot swiatowy - gora ciala sie nie przechyla."""
    if abs(ang) < 1e-9:
        return frame
    local, root = frame
    W = sk.compose(local, root)
    pel, spine = sk.idx("BIP01 PELVIS"), sk.idx("BIP01 SPINE")
    u = W[sk.idx("BIP01 L THIGH")][:3, 3] - W[sk.idx("BIP01 R THIGH")][:3, 3]
    fwd = np.array([u[2], 0.0, -u[0]])
    Wp = retarget._axis_rot(fwd, ang) @ W[pel][:3, :3]
    out = list(local)
    out[pel] = W[sk.parent[pel]][:3, :3].T @ Wp
    out[spine] = Wp.T @ W[spine][:3, :3]
    return out, root


def leg_low(body, sk, frame, side, P=None):
    """Najnizszy punkt podparcia nogi (kolano albo stopa)."""
    return min(_leg_lows(body, sk, frame, side, P))


def roll_angle(body, sk, frame):
    """Przechyl miednicy wyrownujacy podparcie obu nog (klek W3 jest asymetryczny - prawa noga ~3 cm wyzej)."""
    diff = lambda a: leg_low(body, sk, pelvis_roll(sk, frame, a), "R") - leg_low(body, sk, pelvis_roll(sk, frame, a), "L")
    d0 = diff(0.0)
    if abs(d0) < 0.05:
        return 0.0
    sgn = 1.0 if abs(diff(0.02)) < abs(d0) else -1.0
    lo, hi = 0.0, math.radians(ROLL_MAX)
    if diff(sgn * hi) * d0 > 0:
        return sgn * hi
    for _ in range(20):
        mid = 0.5 * (lo + hi)
        if diff(sgn * mid) * d0 > 0:
            lo = mid
        else:
            hi = mid
    return sgn * 0.5 * (lo + hi)


def kneel_weight(body, sk, frame, side):
    """1 = noga kleczy (kolano przy najnizszym punkcie ciala), 0 = kolano wyzej niz KNEEL_BAND[1] nad nim."""
    P = body.skin(*frame)
    k0, _ = _leg_lows(body, sk, frame, side, P)
    lo, hi = KNEEL_BAND
    u = min(1.0, max(0.0, (hi - (k0 - float(P[:, 1].min()))) / (hi - lo)))
    return u * u * (3 - 2 * u)


def lift_toes(sk, frame, angles):
    """Stopy obrocone o katy {strona: rad} (wygladzone w czasie w fix_legs)."""
    for side, a in angles.items():
        if abs(a) > 1e-9:
            frame = _rot_foot(sk, frame, side, a)
    return frame


def _envelope(o, left, right, r):
    """Gladka obwiednia gorna przesuniec 'o' (s >= o w kazdej klatce = nic pod podlozem): maksimum w oknie +-2r, potem
    dwa razy srednia w oknie +-r (kazda srednia bierze tylko wartosci >= o danej klatki). left/right = ciag dalszy
    przed/po klipie (sasiednie klipy lancucha), zeby obwiednia na stykach liczyla sie z tego samego materialu."""
    seq = np.concatenate([left, o, right])
    d = np.array([seq[max(0, i - 2 * r):i + 2 * r + 1].max() for i in range(len(seq))])
    for _ in range(2):
        d = np.array([d[max(0, i - r):i + r + 1].mean() for i in range(len(d))])
    return d[len(left):len(left) + len(o)]


def _pin(s, o, first=None, last=None, n=4):
    """Wartosci na koncach wymuszone (styk z sasiednim klipem) liniowo na n klatkach, potem s >= o."""
    s = np.array(s, dtype=float)
    for val, idx in ((first, 0), (last, -1)):
        if val is None:
            continue
        delta = val - s[idx]
        for k in range(min(n, len(s))):
            w = 1.0 - k / float(n)
            s[k if idx == 0 else len(s) - 1 - k] += delta * w
    return np.maximum(s, o)


def _chain_envelope(seqs, r, end=None):
    """Obwiednie gorne (start, petla, wstawanie) lancucha: petla okresowo, start i wstawanie z sasiednimi klipami jako
    ciagiem dalszym i z wartoscia petli kl. 0 wymuszona na styku (te same pozy na stykach = te same wartosci);
    'end' = wartosc wymuszona na ostatniej klatce wstawania (postawa STAND)."""
    a, l, b = (np.asarray(x, dtype=float) for x in seqs)
    pad = 3 * r
    s_l = _envelope(l, l[-pad:], l[:pad], r)
    s_a = _pin(_envelope(a, np.full(pad, a[0]), l[:pad], r), a, last=s_l[0], n=2 * r)
    s_b = _pin(_envelope(b, l[-pad:], np.full(pad, b[-1]), r), b, first=s_l[0], last=end, n=2 * r)
    return s_a, s_l, s_b


def _gauss(seq, left, right, sig):
    r = int(3 * sig) + 1
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sig) ** 2)
    k /= k.sum()
    x = np.concatenate([left[-r:] if len(left) >= r else np.full(r, seq[0]), seq,
                        right[:r] if len(right) >= r else np.full(r, seq[-1])])
    return np.convolve(x, k, "valid")


def _chain_floor(seqs, end, sig=FLOOR_SIGMA, pen=FLOOR_PEN):
    """Przesuniecia korzenia do podloza (start, petla, wstawanie): 'o' (punkt podparcia dokladnie na podlozu) wygladzone
    gaussem (petla okresowo, start/wstawanie z sasiednimi klipami jako ciagiem dalszym), ale nie nizej niz o - pen;
    na stykach wartosci petli kl. 0 / 'end' (postawa STAND). Czyste przypiecie (s = o) dawalo szarpniecie korzenia
    przy ladowaniu / odrywaniu kolana (przysp. 3,7 cm/kl^2 wobec 1,3 w W3), obwiednia bez przenikania - unoszenie
    do 12 cm; gauss 1,5 kl. + 2 cm: przysp. ~1,5, chwilowo 2,5 cm nad / 2 cm pod podlozem, w kleku ~0."""
    a, l, b = (np.asarray(x, dtype=float) for x in seqs)
    r = int(3 * sig) + 1
    s_l = np.maximum(_gauss(l, l[-r:], l[:r], sig), l - pen)
    s_a = _pin(np.maximum(_gauss(a, [], l[:r], sig), a - pen), a - pen, last=s_l[0], n=2 * r)
    s_b = _pin(np.maximum(_gauss(b, l[-r:], [], sig), b - pen), b - pen, first=s_l[0], last=end, n=2 * r)
    return s_a, s_l, s_b


def fix_legs(sk, body, chain, floor):
    """Stopy rozsuniete i palce podniesione (funkcje pozy - te same pozy na stykach klipow daja te same wyniki), potem
    korzen w pionie tak, by punkty podparcia (kolana / stopy / palce - najnizsze wierzcholki siatki) lezaly na podlozu
    (_chain_floor), petla okresowo, styki klipow wspolne."""
    spread = [[spread_feet(sk, f) for f in c] for c in chain]
    angles, weights = {}, {}
    for side in "LR":
        full = lift_angle(body, sk, spread[1][0], side)   # pelny obrot stopy w kleku petli
        w = _chain_envelope([[kneel_weight(body, sk, f, side) for f in c] for c in spread], LIFT_R, end=0.0)
        weights[side] = [np.clip(x, 0.0, 1.0) for x in w]
        angles[side] = [full * x for x in weights[side]]
    lifted = [[lift_toes(sk, f, {s: angles[s][ci][k] for s in "LR"}) for k, f in enumerate(c)]
              for ci, c in enumerate(spread)]
    roll = roll_angle(body, sk, lifted[1][0])   # przechyl miednicy w kleku petli, w czasie z waga obu kleczacych nog
    # rozsuniecie -> palce -> przechyl -> rozsuniecie (obrot stopy / miednicy minimalnie zmienia odstep stop)
    start, loop, stop = ([spread_feet(sk, pelvis_roll(sk, f, roll * min(weights["L"][ci][k], weights["R"][ci][k])))
                          for k, f in enumerate(c)] for ci, c in enumerate(lifted))
    o = [np.array([floor - body.lowest(f) for f in c]) for c in (start, loop, stop)]
    s_start, s_loop, s_stop = _chain_floor(o, end=max(o[2][-1], 0.0))
    out = []
    for c, s in ((start, s_start), (loop, s_loop), (stop, s_stop)):
        out.append([(f[0], np.asarray(f[1], dtype=float) + np.array([0.0, v, 0.0])) for f, v in zip(c, s)])
    return tuple(out)


def build_chain(vfs, sk, grip, stand, dumps):
    """(start, petla, wstawanie) z trzech zrzutow W3: retarget, domkniecie petli, przejscia z/do postawy Gothica,
    rozsuniete stopy i podparcie na podlozu (podloze = najnizszy punkt siatki w postawie STAND)."""
    start, loop, stop = (clip_frames(d, sk, grip) for d in dumps)
    loop = close_loop(loop)
    start = ease_to(ease_from(stand, start, ENTRY), loop[0], 4)
    stop = ease_to(ease_from(loop[0], stop, 4), stand, EXIT)
    body = Body(vfs, sk)
    return fix_legs(sk, body, (start, loop, stop), body.lowest(stand))


def clip_frames(dump, sk, grip):
    """Klatki @25 (obroty lokalne, korzen) bez broni, korzen wzgledem postaci (Trajectory kl. 0 w (0,0))."""
    d = make_moves.to_entity_start(dump)
    return retarget.resample(retarget.retarget(d, sk, grip, False, "w3", weapon=None), d["fps"])


def _blend(a, b, w):
    qa = [retarget.quat_from_mat(R) for R in a[0]]
    qb = [retarget.quat_from_mat(R) for R in b[0]]
    local = [qmat(*retarget.slerp(x, y, w)) for x, y in zip(qa, qb)]
    root = np.asarray(a[1], dtype=float) * (1 - w) + np.asarray(b[1], dtype=float) * w
    return local, root


def ease_from(pose, frames, n):
    """n pierwszych klatek przechodzi z pozy 'pose' w klip (smoothstep; obroty i caly korzen), reszta bez zmian."""
    out = []
    for k, f in enumerate(frames):
        if k < n:
            u = (k + 1) / float(n + 1)
            out.append(_blend(pose, f, u * u * (3 - 2 * u)))
        else:
            out.append(f)
    return out


def ease_to(frames, pose, n):
    """Klip + n klatek przejscia z ostatniej klatki do pozy 'pose' (smoothstep; obroty i caly korzen)."""
    out = list(frames)
    last = frames[-1]
    for k in range(1, n + 1):
        u = k / float(n)
        out.append(_blend(last, pose, u * u * (3 - 2 * u)))
    return out


def close_loop(frames):
    """Petla bez szwu: niedomkniecie (obroty kazdej kosci i korzen) ostatniej klatki wzgledem pierwszej rozlozone
    liniowo na cala petle; korzen zostaje wzgledem postaci (nie zerujemy przesuniecia - ciaglosc z kleczeniem)."""
    n = len(frames) - 1
    err = [make_moves.rotvec(A.T @ B) for A, B in zip(frames[0][0], frames[n][0])]
    rerr = np.asarray(frames[n][1], dtype=float) - np.asarray(frames[0][1], dtype=float)
    out = []
    for k in range(n):
        u = k / float(n)
        local = [R.copy() if np.linalg.norm(e) < 1e-9 else R @ make_moves.expmap(-u * e) for R, e in zip(frames[k][0], err)]
        out.append((local, np.asarray(frames[k][1], dtype=float) - u * rerr))
    return out


def chunk(vfs, sk, frames, ani, nxt, blend_in, blend_out, flags="M."):
    path = os.path.join(COMPILED, "%s-%s.MAN" % (OVERLAY, ani))
    nf = make_pair.write_man(vfs, sk, frames, ani, path)
    return msblib.ani_chunk(ani, 1, nxt, blend_in, blend_out, flags, "ZDISM_W3.ASC", 1, nf), nf


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--variant", type=int, default=1, choices=sorted(VARIANTS))
    a = ap.parse_args()
    vfs = build_overlay.game_vfs()
    sk = retarget.GothicSkeleton(vfs)
    grip = make_pair.grip_local(vfs, sk)
    stand = make_signs.mean_pose(vfs, sk, STAND)
    os.makedirs(WORK, exist_ok=True)
    os.makedirs(COMPILED, exist_ok=True)
    for f in os.listdir(COMPILED):
        if f.upper().startswith(OVERLAY + "-") or f.upper().startswith(OVERLAY + "."):
            os.remove(os.path.join(COMPILED, f))
    c_start, c_loop, c_stop = VARIANTS[a.variant]
    dump = lambda c: make_pair.dump_clip(SET, c, os.path.join(WORK, c + ".json"))
    start, loop, stop = build_chain(vfs, sk, grip, stand, [dump(c) for c in (c_start, c_loop, c_stop)])
    print("stopy: min. odstep %.1f cm (prog %.0f), korzen kleku %.1f cm" % (
        min(feet_gap(sk, f) for f in start + loop + stop), FEET_MIN, loop[0][1][1]))

    chunks = []
    for frames, ani, nxt, bi, bo in ((start, ANI_START, ANI_LOOP, 0.2, 0.0), (loop, ANI_LOOP, ANI_LOOP, 0.0, 0.0),
                                     (stop, ANI_STOP, STAND, 0.1, 0.2)):
        ch, nf = chunk(vfs, sk, frames, ani, nxt, bi, bo)
        chunks.append(ch)
        r0, r1 = frames[0][1], frames[-1][1]
        print("%-22s %4d kl. (%.2f s)  korzen %s -> %s" % (ani, nf, nf / retarget.FPS_OUT,
              np.round(r0, 1).tolist(), np.round(r1, 1).tolist()))
    build_overlay.write_overlay(OVERLAY, COMPILED, chunks, vfs)
    print("nakladka", OVERLAY, "->", COMPILED)


if __name__ == "__main__":
    main()
