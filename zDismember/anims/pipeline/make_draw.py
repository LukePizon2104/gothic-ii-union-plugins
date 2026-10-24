r"""Dobywanie i chowanie miecza zza plecow (Wiedzmin 3) dla zDismember. TYLKO DO UZYTKU PRYWATNEGO (animacje CD Projekt
RED) - wynik trafia do data_w3 / zDismember_W3.vdf, nigdy do wydania. Spec: [dokument projektowy - niepublikowany].

Uzycie: py -3.12 -I make_draw.py [--grab 0.30] [--ready 0.50] [--release 0.40] [--arm 0.22]
Nakladki (zaklada je plugin tylko bohaterowi - BackSwords.cpp; nazwy MUSZA istniec w bazowym HUMANS Steam / NB / Kronik,
inaczej silnik odrzuca cala nakladke - sprawdzone: te same definicje we wszystkich trzech grach):
  HUMANS_ZDF_DRAW_1H / _2H      pozycja A (stal Geralta, man_geralt_sword_unsheathe_steel / sheathe_steel)
  HUMANS_ZDF_DRAW_1H_B / _2H_B  pozycja B (srebro, *_silver) - plugin podmienia nimi wpisy nakladek A (bez zakladania)
  HUMANS_ZDF_DRAW_1H_BELT       natywne dobycie 1H przy pasie (kopia z HUMANS) - plugin skaluje tempo (BeltDrawSpeed)
Animacje (jak w HUMANS.MSB - przebieg silnika: DrawWeapon1 -> S_xH -> DrawWeapon2, RemoveWeapon1 -> S_xH -> RemoveWeapon2):
  T_RUN_2_xH        postawa bez broni -> dlon na rekojesci za plecami (chwyt), nastepna S_xH
  S_xH              jedna klatka chwytu (wspolna dla dobycia i chowania)
  T_xH_2_xHRUN      wyciagniecie i garda stylu W3; DEF_FIGHTMODE xH + DEF_DRAWSOUND w 1. klatce (bron do reki jak natywnie)
  T_xHRUN_2_xH      garda -> miecz w pochwie; DEF_UNDRAWSOUND przy wsuwaniu, DEF_FIGHTMODE "" w przedostatniej klatce
                    (bron do slotu w chwili puszczenia rekojesci; natywne animacje nie maja zdarzen w ostatniej klatce
                    w przod - bez pewnosci, ze silnik je odpali), nastepna S_xH
  T_xH_2_RUN        reka wraca, postawa bez broni (w HUMANS alias T_RUN_2_xH wstecz - tu prawdziwa animacja), nastepna S_RUN
  T_MOVE_2_xHMOVE   dobycie w biegu (warstwa 2, tulow i rece nad biegiem), DEF_FIGHTMODE xH w chwili chwytu
  T_xHMOVE_2_MOVE   chowanie w biegu (warstwa 2), DEF_FIGHTMODE "" w chwili puszczenia
Pozycja na plecach: pochwa W3 (r_weapon w klatkach przed chwytem) przeniesiona na szkielet Gothica wzgledem BIP01 SPINE2,
obrot broni W3 -> bron Gothica z fazy, w ktorej miecz jest w dloni. Plugin ustawia w tym miejscu wezel slotu (ZS_SWORD dla
1H na plecach, ZS_LONGSWORD dla 2H, ZS_BOW dla drugiego miecza) - zapis PosA / PosB w backswords_w3.ini. Przelozenie bez
skoku: w chwili chwytu / puszczenia slot dloni ZS_RIGHTHAND jest w animacji ustawiony tak, ze miecz w dloni lezy dokladnie
tam, gdzie wisial na plecach, i w SLOT_BLEND klatkach wraca do zwyklego chwytu Gothica (koniec animacji = zwykly chwyt).
Tempo: chwyt w --grab s, gotowosc (OnWeaponDrawReady W3) w --ready s, potem przejscie do gardy; chowanie: puszczenie
w --release s, reka w dol w --arm s. Plugin skaluje fpsRate (BackDrawSpeed / BackSheatheSpeed, 100 = jak tu).
"""
import argparse, math, os, struct, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import manlib, msblib
import retarget, build_overlay, make_pair, make_moves, make_attacks, make_signs, recovery, sfx_events
from sfx_events import event_tag

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(ROOT, "anims", "work", "w3draw")
COMPILED = make_moves.COMPILED
INI = os.path.join(ROOT, "data_w3", "System", "zDismember", "backswords_w3.ini")
SET = make_moves.SWORD_SET
FPS = retarget.FPS_OUT

# pozycja -> (dobycie, chowanie); chwile zdarzen W3 z w2anims (CExtAnimEvent: OnWeaponDrawReady, ItemSync ILA_Holster,
# dzwiek cmb_weapon_sheathe_fast) - [notatki lokalne]
POSITIONS = {
    "A": ("man_geralt_sword_unsheathe_steel", "man_geralt_sword_sheathe_steel"),
    "B": ("man_geralt_sword_unsheathe_silver", "man_geralt_sword_sheathe_silver"),
}
W3_EVENTS = {
    "man_geralt_sword_unsheathe_steel": {"ready": 0.7305},
    "man_geralt_sword_unsheathe_silver": {"ready": 0.749},
    "man_geralt_sword_sheathe_steel": {"sound": 0.708, "holster": 1.091},
    "man_geralt_sword_sheathe_silver": {"sound": 0.711, "holster": 1.062},
}
OVERLAYS = {("1H", "A"): "HUMANS_ZDF_DRAW_1H", ("2H", "A"): "HUMANS_ZDF_DRAW_2H",
            ("1H", "B"): "HUMANS_ZDF_DRAW_1H_B", ("2H", "B"): "HUMANS_ZDF_DRAW_2H_B"}
BELT = "HUMANS_ZDF_DRAW_1H_BELT"
STAND = "S_RUN"            # postawa bez broni Gothica (poczatek dobycia, koniec chowania)
RUN_REF = "S_RUNL"         # miednica odniesienia tulowia w wariantach w biegu (warstwa 2)
SHEATHE_START = 0.20       # s klipu chowania W3 pomijane na poczatku (garda prawie bez ruchu)
SHEATHE_TAIL = 0.45        # s klipu chowania po puszczeniu (reka wraca do pasa)
HAND_BLEND = 6             # klatek (@25) przejscia dloni: naturalna W3 <-> obrocona pod ostrze (retarget weapon)
SLOT_BLEND = 6             # klatek powrotu slotu dloni z przelozenia do zwyklego chwytu
ENTRY = 3                  # klatek wejscia z postawy Gothica w 1. klatke W3
JOIN = 4                   # klatek przejscia konca chowania w poze chwytu S_xH
REC = 3                    # klatek przejscia konca chowania w postawe bez broni
GUARD_REC = 3              # klatek przejscia konca dobycia w garde stylu W3 (1H; 2H - guard_recovery)
DEPTH = 4.0                # cm: miecz blizej plecow niz pochwa W3 przeniesiona wzgledem BIP01 SPINE2 (--depth)


def names_for(w):
    return dict(draw1="T_RUN_2_%s" % w, hold="S_%s" % w, draw2="T_%s_2_%sRUN" % (w, w), stance="S_%sRUN" % w,
                sheathe1="T_%sRUN_2_%s" % (w, w), sheathe2="T_%s_2_RUN" % w,
                move_draw="T_MOVE_2_%sMOVE" % w, move_sheathe="T_%sMOVE_2_MOVE" % w)


# ---------------------------------------------------------------- klip W3

def dump(clip):
    os.makedirs(WORK, exist_ok=True)
    return make_pair.dump_clip(SET, clip, os.path.join(WORK, clip + ".json"))


def weapon_rel_torso(d):
    """Pozycja r_weapon wzgledem torso3 (cm) w kazdej klatce zrodla."""
    names = [b["name"] for b in d["bones"]]
    wi, ti = names.index("r_weapon"), names.index("torso3")
    return [(np.linalg.inv(np.array(f[ti])) @ np.array(f[wi]))[:3, 3] * 100.0 for f in d["frames"]]


def grab_frame(d, eps=1.5):
    """Klatka zrodla, w ktorej miecz rusza z pochwy (r_weapon przestaje stac wzgledem torso3 - dlon go trzyma)."""
    p = weapon_rel_torso(d)
    for k in range(1, len(p)):
        if np.linalg.norm(p[k] - p[0]) > eps:
            return k
    raise ValueError("miecz nie rusza z pochwy")


def release_frame(d, fast=150.0, still=50.0):
    """Klatka zrodla, w ktorej miecz wsunal sie do pochwy (po ostatnim szybkim ruchu predkosc wzgledem torso3 < still)."""
    p = weapon_rel_torso(d)
    v = [0.0] + [np.linalg.norm(p[k] - p[k - 1]) * d["fps"] for k in range(1, len(p))]
    last = max(k for k in range(len(v)) if v[k] > fast)
    return next((k for k in range(last, len(v)) if v[k] < still), len(v) - 1)


def qavg(Rs):
    qs = [retarget.quat_from_mat(R) for R in Rs]
    s = np.zeros(4)
    for q in qs:
        s += q if np.dot(q, qs[0]) >= 0 else -q
    return retarget.qmat(*(s / np.linalg.norm(s)))


def back_pose(d, fr_w, sk, grab):
    """Miecz w pochwie W3 w ukladzie BIP01 SPINE2 Gothica (4x4, os ostrza = -X jak w slotach Gothica).
    Obrot: r_weapon W3 (P_W3) x mapa bron W3 -> bron Gothica (srednia z klatek z mieczem w dloni po chwycie);
    przesuniecie: r_weapon wzgledem torso3 przeniesione na obrot SPINE2 z retargetu. Srednia z klatek przed chwytem."""
    Pw = retarget.P_W3
    names = [b["name"] for b in d["bones"]]
    wi, ti = names.index("r_weapon"), names.index("torso3")
    S2, RH = sk.idx("BIP01 SPINE2"), sk.idx("ZS_RIGHTHAND")
    rot = lambda k, i: Pw @ retarget.ortho(np.array(d["frames"][k][i])[:3, :3]) @ Pw.T
    pos = lambda k, i: Pw @ np.array(d["frames"][k][i])[:3, 3] * 100.0
    hand = range(grab + 5, min(len(fr_w), grab + 20))
    Q = qavg([rot(k, wi).T @ sk.compose(*fr_w[k])[RH][:3, :3] for k in hand])
    Rs, ps = [], []
    for k in range(0, grab):
        G = sk.compose(*fr_w[k])[S2][:3, :3]
        Rs.append(G.T @ rot(k, wi) @ Q)
        ps.append(G.T @ (pos(k, wi) - pos(k, ti)))
    M = np.eye(4)
    M[:3, :3] = qavg(Rs)
    M[:3, 3] = np.mean(ps, axis=0)
    return M


def retarget_pair(d, sk, grip):
    """(naturalna dlon W3, dlon obrocona pod ostrze r_weapon) - klatki zrodla, korzen wzgledem startu postaci."""
    e = make_moves.to_entity_start(d)
    return (retarget.retarget(e, sk, grip, False, "w3", weapon=None),
            retarget.retarget(e, sk, grip, False, "w3", weapon="r_weapon"))


def mix_hand(sk, natural, aligned, weight):
    """Klatki zrodla: dlon (i jej drzewo) z wagi weight(k) - 0 naturalna, 1 pod ostrze; reszta ciala bez zmian."""
    hand = sk.idx("BIP01 R HAND")
    out = []
    for k, ((ln, rn), (la, _)) in enumerate(zip(natural, aligned)):
        w = weight(k)
        local = [R.copy() for R in ln]
        if w > 0.0:
            local[hand] = retarget.qmat(*retarget.slerp(retarget.quat_from_mat(ln[hand]), retarget.quat_from_mat(la[hand]), w))
        out.append((local, np.array(rn, dtype=float)))
    return out


def smooth(u):
    u = min(1.0, max(0.0, u))
    return u * u * (3.0 - 2.0 * u)


# ---------------------------------------------------------------- czas

def warp(frames, fps_src, keys):
    """Klatki zrodla (fps_src) -> klatki @25 wg mapy czasu: keys = [(klatka wyjscia, czas zrodla s), ...] rosnaco,
    liniowo miedzy punktami. Obroty slerp, korzen lerp."""
    quats = [[retarget.quat_from_mat(R) for R in f[0]] for f in frames]
    to, ts = [k[0] for k in keys], [k[1] * fps_src for k in keys]
    out = []
    for j in range(int(to[-1]) + 1):
        out.append(make_attacks._sample(frames, quats, min(float(np.interp(j, to, ts)), len(frames) - 1)))
    return out


def blend_pose(a, b, w):
    qa = [retarget.quat_from_mat(R) for R in a[0]]
    qb = [retarget.quat_from_mat(R) for R in b[0]]
    return ([retarget.qmat(*retarget.slerp(x, y, w)) for x, y in zip(qa, qb)],
            np.asarray(a[1], dtype=float) * (1 - w) + np.asarray(b[1], dtype=float) * w)


def ease_from(pose, frames, n):
    """Klatka 0 = pose, w n klatkach przejscie w klip (smoothstep); korzen w poziomie z klipu."""
    out = []
    for k, f in enumerate(frames):
        if k < n:
            w = smooth(k / float(n))
            p = (pose[0], np.array([f[1][0], pose[1][1], f[1][2]]))
            out.append(blend_pose(p, f, w))
        else:
            out.append(f)
    return out


def ease_into(frames, pose, n):
    """Ostatnie n klatek przechodzi w pose (ostatnia = pose dokladnie); korzen w poziomie z klipu, wysokosc z pose."""
    out = list(frames)
    L = len(frames)
    for k in range(max(0, L - n), L):
        w = smooth((k - (L - n - 1)) / float(n))
        f = frames[k]
        p = (pose[0], np.array([f[1][0], pose[1][1], f[1][2]]))
        out[k] = blend_pose(f, p, w)
    last = frames[-1]
    out[-1] = ([R.copy() for R in pose[0]], np.array([last[1][0], pose[1][1], last[1][2]]))
    return out


# ---------------------------------------------------------------- slot dloni

def handover_weight(k, handover, blend):
    """Waga przelozenia slotu dloni: 1 w klatce handover, narasta / opada liniowo-gladko przez blend klatek, 0 dalej.
    Przed chwytem (dobycie) i po puszczeniu (chowanie) dlon jest pusta - slot nie jest widoczny, ale wraca do chwytu,
    zeby wezel nie odjezdzal od dloni (ramka modelu)."""
    d = abs(k - handover)
    return 0.0 if d >= blend else 1.0 - smooth(d / float(blend))


def slot_track(sk, frames, grip, M_back, handover, blend):
    """Lokalny ZS_RIGHTHAND (4x4) w kazdej klatce: w klatce handover miecz w dloni = miecz na plecach (M_back wzgledem
    SPINE2), w blend klatkach przed i po - przejscie od / do zwyklego chwytu Gothica (grip, przesuniecie z MDH)."""
    RH, S2 = sk.idx("ZS_RIGHTHAND"), sk.idx("BIP01 SPINE2")
    hand = sk.parent[RH]
    std = standard_slot(sk, grip)
    out = []
    for k, f in enumerate(frames):
        w = handover_weight(k, handover, blend)
        if w <= 0.0:
            out.append(std.copy())
            continue
        W = sk.compose(*f)
        over = np.linalg.inv(W[hand]) @ W[S2] @ M_back
        q = retarget.slerp(retarget.quat_from_mat(std[:3, :3]), retarget.quat_from_mat(retarget.ortho(over[:3, :3])), w)
        S = np.eye(4)
        S[:3, :3] = retarget.qmat(*q)
        S[:3, 3] = std[:3, 3] * (1 - w) + over[:3, 3] * w
        out.append(S)
    return out


def apply_slot(sk, frames, slots):
    RH = sk.idx("ZS_RIGHTHAND")
    out = []
    for (local, root), S in zip(frames, slots):
        local = [R.copy() for R in local]
        local[RH] = S[:3, :3].copy()
        out.append((local, root))
    return out


def standard_slot(sk, grip):
    S = np.eye(4)
    S[:3, :3] = grip
    S[:3, 3] = sk.local[sk.idx("ZS_RIGHTHAND")][:3, 3]
    return S


# ---------------------------------------------------------------- zapis

def write_man(vfs, sk, frames, slots, name, path, nodes, layer=1):
    """Jak retarget.write_man, ale ZS_RIGHTHAND z wlasnym przesunieciem w kazdej klatce (slots) - przelozenie miecza."""
    S = 2.1 / 65535.0
    m = manlib.Man(bytes(vfs.find("HUMANS-T_1HSFINISH.MAN").data))
    RH = sk.idx("ZS_RIGHTHAND")
    pos, rots = [], []
    for (local, root), slot in zip(frames, slots):
        for i in nodes:
            R = slot[:3, :3] if i == RH else local[i]
            q = retarget.quat_from_mat(R)
            q = np.array([-q[0], -q[1], -q[2], q[3]])
            if q[3] < 0:
                q = -q
            rots.append(q)
            pos.append(root if sk.parent[i] < 0 else (slot[:3, 3] if i == RH else sk.local[i][:3, 3]))
    pos = np.array(pos)
    lo, hi = pos.min(), pos.max()
    scale = (hi - lo) / 65535.0 or 1e-6
    samples = []
    for q, p in zip(rots, pos):
        r = tuple(int(max(0, min(65535, round(v / S + 32767)))) for v in q[:3])
        pp = tuple(int(max(0, min(65535, round((v - lo) / scale)))) for v in p)
        samples.append(r + pp)
    pts = []
    for local, root in frames:
        W = sk.compose(local, root)
        pts += [w[:3, 3] - frames[0][1] for w in W]
    pts = np.array(pts)
    bb = list(pts.min(axis=0) - 25.0) + list(pts.max(axis=0) + 25.0)
    m.name, m.layer, m.frames = name, layer, len(frames)
    m.nodes, m.nnodes = list(nodes), len(nodes)
    m.samples = samples
    m.pmin, m.pscale = float(lo), float(scale)
    m.fps = m.fps_src = FPS
    m.bbox = [float(v) for v in bb]
    m.next = ""
    if manlib.CH_EVENTS in m.raw:
        m.order = [t for t in m.order if t != manlib.CH_EVENTS]
    open(path, "wb").write(m.build())
    return len(frames)


def ani(name, layer, nxt, nf, blend_in, blend_out, tags=(), sfx=(), ground=()):
    ch = msblib.ani_chunk(name, layer, nxt, blend_in, blend_out, "M.", "ZDISM_W3.ASC", 1, nf)
    if tags or sfx or ground:
        ch += sfx_events.events_block(sfx=sfx, ground=ground, tags=tags)
    return ch


# ---------------------------------------------------------------- budowa

class Built:
    """Wspolne klatki jednej pozycji (A / B) dla obu typow broni (1H i 2H rozni tylko koniec dobycia - garda stylu)."""


def in_place(frames):
    """Korzen bez ruchu w poziomie (dobycie / chowanie w miejscu - bez skoku korzenia miedzy czesciami i bez
    przesuwania voba); wysokosc z klipu. -> (klatki, najwieksze przesuniecie miednicy W3 w poziomie, cm)."""
    r0 = np.asarray(frames[0][1], dtype=float)
    drift = max(math.hypot(r[0] - r0[0], r[2] - r0[2]) for _, r in frames)
    return [(local, np.array([0.0, r[1], 0.0])) for local, r in frames], drift


def build_position(vfs, sk, grip, P, a):
    clip_d, clip_s = POSITIONS[P]
    b = Built()
    stand = make_signs.mean_pose(vfs, sk, STAND)
    b.stand = ([R.copy() for R in stand[0]], np.array([0.0, stand[1][1], 0.0]))
    # dobycie: dlon naturalna do chwytu, potem w HAND_BLEND klatkach wyjscia pod ostrze; czas: chwyt -> a.grab,
    # gotowosc -> a.ready (przelicznik klatek zrodla na wyjscie z nachylenia mapy czasu po chwycie)
    dd = dump(clip_d)
    fps = dd["fps"]
    g_src = grab_frame(dd)
    nat, ali = retarget_pair(dd, sk, grip)
    g_out = int(round(a.grab * FPS))
    r_out = int(round(a.ready * FPS))
    t_ready = W3_EVENTS[clip_d]["ready"]
    slope = (t_ready - g_src / fps) / ((r_out - g_out) / FPS)   # s zrodla na s wyjscia
    hb = HAND_BLEND / FPS * slope * fps
    src = mix_hand(sk, nat, ali, lambda k: 0.0 if k <= g_src else smooth((k - g_src) / hb))
    b.M = back_pose(dd, ali, sk, g_src)
    b.M[:3, 3] += np.array([0.0, 0.0, a.depth])   # blizej plecow (lokalne +Z SPINE2 = do przodu ciala)
    frames = warp(src, fps, [(0, 0.0), (g_out, g_src / fps), (r_out, t_ready)])
    frames, b.drift_d = in_place(frames)
    frames = ease_from(b.stand, frames, ENTRY)
    b.draw, b.grab = frames, g_out
    b.g_src, b.t_ready = g_src, t_ready
    # chowanie: dlon pod ostrze do puszczenia (ostatnie HAND_BLEND klatek wyjscia przed nim wraca do naturalnej)
    ds = dump(clip_s)
    fps_s = ds["fps"]
    rel_src = release_frame(ds)
    nat_s, ali_s = retarget_pair(ds, sk, grip)
    rel_out = int(round(a.release * FPS))
    arm_out = rel_out + int(round(a.arm * FPS))
    t_rel = rel_src / fps_s
    slope_s = (t_rel - SHEATHE_START) / (rel_out / FPS)
    hb_s = HAND_BLEND / FPS * slope_s * fps_s
    src_s = mix_hand(sk, nat_s, ali_s, lambda k: 1.0 if k <= rel_src - hb_s else 0.0 if k >= rel_src else
                     1.0 - smooth((k - (rel_src - hb_s)) / hb_s))
    frames_s = warp(src_s, fps_s, [(0, SHEATHE_START), (rel_out, t_rel), (arm_out, min(t_rel + SHEATHE_TAIL, (len(src_s) - 1) / fps_s))])
    frames_s, b.drift_s = in_place(frames_s)
    b.sheathe, b.release = frames_s, rel_out
    b.rel_src, b.snd_src = rel_src, W3_EVENTS[clip_s]["sound"]
    b.snd_out = int(round(np.interp(b.snd_src, [SHEATHE_START, t_rel], [0, rel_out])))
    b.clip_d, b.clip_s = clip_d, clip_s
    return b


def finish_draw(sk, grip, b, guard, w):
    """-> (T_RUN_2_xH, S_xH, T_xH_2_xHRUN) jako listy (klatki, sloty): chwyt = koniec pierwszej czesci; druga czesc wraca
    do gardy stylu W3 (1H slerp, 2H przejsciem bezwladnosciowym z rozlozonym obrotem miednicy)."""
    frames = b.draw
    g = b.grab
    part2 = frames[g:]
    if w == "2H":
        part2 = make_moves.guard_recovery(sk, part2, guard, GUARD_REC + 2)
    else:
        part2 = recovery.append_recovery(part2, guard, GUARD_REC)
    full = frames[:g] + part2
    slots = slot_track(sk, full, grip, b.M, g, SLOT_BLEND)
    full = apply_slot(sk, full, slots)
    one = [full[g]], [slots[g]]
    return (full[:g + 1], slots[:g + 1]), one, (full[g:], slots[g:])


def finish_sheathe(sk, grip, b, guard, hold):
    """-> (T_xHRUN_2_xH, T_xH_2_RUN): pierwsza z gardy (wejscie ENTRY klatek), koniec przechodzi w poze chwytu S_xH
    (JOIN klatek) - puszczenie w ostatniej klatce; druga z pozy chwytu do postawy bez broni (koniec REC klatek)."""
    frames = b.sheathe
    r = b.release
    a = ease_from(guard, frames[:r + 1], ENTRY)
    a = ease_into(a, hold[0][0], JOIN)
    rest = ease_from(hold[0][0], frames[r:], JOIN)
    rest = ease_into(rest, b.stand, REC)
    full = a[:-1] + rest
    slots = slot_track(sk, full, grip, b.M, r, SLOT_BLEND)
    slots[r] = hold[1][0]
    full = apply_slot(sk, full, slots)
    full[r] = hold[0][0]
    return (full[:r + 1], slots[:r + 1]), (full[r:], slots[r:])


def upper_body(sk, frames, ref):
    """Tulow i rece wzgledem miednicy biegu (make_signs.build_gesture ruch): BIP01 SPINE w ukladzie miednicy ref."""
    spine, pelvis = sk.idx("BIP01 SPINE"), sk.idx("BIP01 PELVIS")
    Pr = sk.compose(ref[0], ref[1])[pelvis][:3, :3]
    out = []
    for local, root in frames:
        W = sk.compose(local, root)
        loc = [R.copy() for R in local]
        loc[spine] = Pr.T @ W[spine][:3, :3]
        out.append((loc, root))
    return out


def native_belt(vfs):
    """Natywne dobycie / chowanie 1H z HUMANS (chunki ANI ze zdarzeniami + MAN-y) pod nakladka BELT; T_1H_2_RUN (w HUMANS
    alias T_RUN_2_1H wstecz) jako prawdziwa animacja z MAN-em T_RUN_2_1H. -> lista chunkow."""
    want = ("T_RUN_2_1H", "S_1H", "T_1H_2_1HRUN", "T_1HRUN_2_1H", "T_MOVE_2_1HMOVE", "T_1HMOVE_2_MOVE")
    d = bytes(vfs.find("HUMANS.MSB").data)
    chunks, cur, raw_run = [], None, None
    for t, raw, body in build_overlay.chunks(d):
        if t == 0xF520:
            a = build_overlay.parse_ani(body)
            cur = a["name"] if a["name"] in want else None
            if cur:
                chunks.append(raw)
                if cur == "T_RUN_2_1H":
                    raw_run = a
        elif cur and 0xF5A0 <= t <= 0xF5AF:
            chunks.append(raw)
        else:
            cur = None
    assert raw_run, "brak T_RUN_2_1H w HUMANS.MSB"
    a = raw_run
    chunks.append(msblib.ani_chunk("T_1H_2_RUN", a["layer"], "S_RUN", 0.0, 0.1, a["flags"], a["asc"], a["first"], a["last"],
                                   direction="R", fps=a["fps"]))
    os.makedirs(COMPILED, exist_ok=True)
    for n in want:
        open(os.path.join(COMPILED, "%s-%s.MAN" % (BELT, n)), "wb").write(bytes(vfs.find("HUMANS-%s.MAN" % n).data))
    m = manlib.Man(bytes(vfs.find("HUMANS-T_RUN_2_1H.MAN").data))
    m.name = "T_1H_2_RUN"
    open(os.path.join(COMPILED, "%s-T_1H_2_RUN.MAN" % BELT), "wb").write(m.build())
    return chunks


def mat_line(M):
    return " ".join("%.5f" % v for v in M[:3, :].reshape(-1))


def write_ini(pos, timing):
    os.makedirs(os.path.dirname(INI), exist_ok=True)
    lines = ["; zDismember - miecze na plecach (make_draw.py, animacje W3 - TYLKO prywatnie). Nie edytowac recznie:",
             "; przestawienia w grze przez [ZDISMEMBER] BackAdjustA / BackAdjustB (x y z cm, rx ry rz st. w ukladzie miecza).",
             "; PosA / PosB = miecz na plecach wzgledem BIP01 SPINE2 (macierz 3x4 wierszami, przesuniecie w 4. kolumnie, cm).",
             "[BACKSWORDS]"]
    for P, M in pos.items():
        lines.append("Pos%s=%s" % (P, mat_line(M)))
    for k, v in timing.items():
        lines.append("%s=%s" % (k, v))
    data = ("\r\n".join(lines) + "\r\n").encode("ascii")
    open(INI, "wb").write(data)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--grab", type=float, default=0.30, help="s do chwytu rekojesci (T_RUN_2_xH)")
    ap.add_argument("--ready", type=float, default=0.50, help="s do gotowosci W3 (potem przejscie do gardy)")
    ap.add_argument("--release", type=float, default=0.40, help="s chowania do puszczenia rekojesci")
    ap.add_argument("--arm", type=float, default=0.22, help="s reki w dol po puszczeniu")
    ap.add_argument("--depth", type=float, default=DEPTH, help="cm blizej plecow niz pochwa W3 (kosci W3 leza blizej "
                    "plecow niz BIP01 SPINE2 Gothica; natywny ZS_LONGSWORD jest 6,5 cm plycej niz pochwa stali)")
    a = ap.parse_args()
    vfs = build_overlay.game_vfs()
    sk = retarget.GothicSkeleton(vfs)
    grip = make_pair.grip_local(vfs, sk)
    run = make_signs.mean_pose(vfs, sk, RUN_REF)
    upper = make_signs.upper_nodes(sk)
    body = make_signs.body_nodes(sk)
    os.makedirs(COMPILED, exist_ok=True)
    for f in os.listdir(COMPILED):
        if f.upper().startswith("HUMANS_ZDF_DRAW_"):
            os.remove(os.path.join(COMPILED, f))
    guards = {}
    for w in ("1H", "2H"):
        st = make_attacks.STYLES[w]
        gd = make_pair.dump_clip(make_moves.SWORD_SET, make_attacks.guard_clip(w),
                                 os.path.join(make_moves.WORK, make_attacks.guard_clip(w) + ".json"))
        g0 = make_attacks.style_guard(gd, sk, grip, w)[0]
        guards[w] = ([R.copy() for R in g0[0]], np.array([0.0, g0[1][1], 0.0]))
    pos, timing = {}, {}
    for P in POSITIONS:
        b = build_position(vfs, sk, grip, P, a)
        pos[P] = b.M
        print("pozycja %s: chwyt %s kl. %d zrodla (%.3f s) -> kl. %d, gotowosc %.3f s -> kl. %d; puszczenie %s kl. %d (%.3f s) "
              "-> kl. %d, dzwiek kl. %d; miednica W3 w poziomie (zerowana) %.1f / %.1f cm" % (
                  P, b.clip_d, b.g_src, b.g_src / 30.0, b.grab, b.t_ready, int(round(a.ready * FPS)), b.clip_s, b.rel_src,
                  b.rel_src / 30.0, b.release, b.snd_out, b.drift_d, b.drift_s))
        G = sk.compose(*b.stand)[sk.idx("BIP01 SPINE2")] @ b.M
        print("   na plecach (postawa S_RUN): rekojesc %s, sztych (90 cm) %s" % (
            np.round(G[:3, 3], 1), np.round((G @ np.array([-90.0, 0, 0, 1]))[:3], 1)))
        for w in ("1H", "2H"):
            n = names_for(w)
            ov = OVERLAYS[(w, P)]
            (d1, s1), (h, hs), (d2, s2) = finish_draw(sk, grip, b, guards[w], w)
            (r1, rs1), (r2, rs2) = finish_sheathe(sk, grip, b, guards[w], (h, hs))
            mp = lambda nm: os.path.join(COMPILED, "%s-%s.MAN" % (ov, nm))
            chunks = []
            nf = write_man(vfs, sk, d1, s1, n["draw1"], mp(n["draw1"]), body)
            chunks.append(ani(n["draw1"], 1, n["hold"], nf, 0.1, 0.0))
            nf = write_man(vfs, sk, h, hs, n["hold"], mp(n["hold"]), body)
            chunks.append(ani(n["hold"], 1, n["hold"], nf, 0.0, 0.0))
            nf2 = write_man(vfs, sk, d2, s2, n["draw2"], mp(n["draw2"]), body)
            _, ground = sfx_events.motion_sounds(d2, sk)
            chunks.append(ani(n["draw2"], 1, n["stance"], nf2, 0.0, 0.1,
                              tags=[event_tag("DEF_FIGHTMODE", w, frame=1), event_tag("DEF_DRAWSOUND", frame=1)], ground=ground))
            nr1 = write_man(vfs, sk, r1, rs1, n["sheathe1"], mp(n["sheathe1"]), body)
            snd = max(1, min(nr1 - 1, b.snd_out + 1))
            chunks.append(ani(n["sheathe1"], 1, n["hold"], nr1, 0.1, 0.0,
                              tags=[event_tag("DEF_UNDRAWSOUND", frame=snd), event_tag("DEF_FIGHTMODE", "", frame=nr1 - 1)]))
            nr2 = write_man(vfs, sk, r2, rs2, n["sheathe2"], mp(n["sheathe2"]), body)
            chunks.append(ani(n["sheathe2"], 1, STAND, nr2, 0.0, 0.2))
            # w biegu: cale dobycie / chowanie, tylko tulow i rece nad biegiem
            md = upper_body(sk, d1[:-1] + d2, run)
            ms = [s for s in s1[:-1] + s2]
            nm = write_man(vfs, sk, md, ms, n["move_draw"], mp(n["move_draw"]), upper, layer=2)
            chunks.append(ani(n["move_draw"], 2, "", nm, 0.2, 0.2,
                              tags=[event_tag("DEF_FIGHTMODE", w, frame=b.grab + 1), event_tag("DEF_DRAWSOUND", frame=b.grab + 1)]))
            mr = upper_body(sk, r1[:-1] + r2, run)
            mrs = [s for s in rs1[:-1] + rs2]
            nmr = write_man(vfs, sk, mr, mrs, n["move_sheathe"], mp(n["move_sheathe"]), upper, layer=2)
            chunks.append(ani(n["move_sheathe"], 2, "", nmr, 0.2, 0.2,
                              tags=[event_tag("DEF_UNDRAWSOUND", frame=snd), event_tag("DEF_FIGHTMODE", "", frame=b.release + 1)]))
            build_overlay.write_overlay(ov, COMPILED, chunks, vfs)
            timing["Draw%s%s" % (w, P)] = "%d %d %d" % (len(d1), nf2, len(d1) - 1 + nf2)
            timing["Sheathe%s%s" % (w, P)] = "%d %d %d" % (nr1, nr2, nr1 - 1 + nr2)
            print("   %s: %s %d kl. + %s %d kl. = %.2f s (chwyt %.2f s); %s %d kl. (puszczenie %.2f s) + %s %d kl. = %.2f s; "
                  "w biegu %d / %d kl." % (ov, n["draw1"], len(d1), n["draw2"], nf2, (len(d1) - 1 + nf2) / FPS, b.grab / FPS,
                                           n["sheathe1"], nr1, b.release / FPS, n["sheathe2"], nr2, (nr1 - 1 + nr2) / FPS, nm, nmr))
    build_overlay.write_overlay(BELT, COMPILED, native_belt(vfs), vfs)
    print("%s: natywne dobycie 1H z HUMANS (tempo skaluje plugin)" % BELT)
    write_ini(pos, timing)
    print("ini:", INI)


if __name__ == "__main__":
    main()
