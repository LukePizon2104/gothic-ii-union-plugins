r"""Szybki strzal z kuszy Geralta (Wiedzmin 3, "gabriel") dla zDismember - TYLKO UZYTEK PRYWATNY (animacje CD Projekt RED).

Spec: [dokument projektowy - niepublikowany]. Klipy z animations\man\combat\man_geralt_gabriel.w2anims (zrzuty
w3_dump w anims\work\w3xbow; pierwsze z rozpoznania 23.10 w [notatki lokalne]):
- XBOW_IN    = aim_start_rp: siegniecie, chwyt kuszy, uniesienie do celowania (RotateToTarget, ProjectileAttach);
- XBOW_LOOP  = aim_idle_rp: petla celowania (zDismember wznawia ja od poczatku po koncu - bez "next");
- XBOW_SHOOT = aim_idle_rp z wypieczonym odrzutem shoot_additive_center (lokalnie: poza bazowa x delta wzgledem
  pierwszej klatki addytywu - addytyw zaczyna sie i konczy na tej samej pozie) + aim_end_rp (opuszczenie, kusza na plecy).
Kazdy klip w dwoch wariantach (te same klatki, zDismember przelacza od tej samej klatki, gdy bohater ruszy):
- postoj (cale cialo, korzen bez ruchu w poziomie, stopy stoja - make_signs.plant_feet),
- ruch (tulow i rece od BIP01 SPINE, tulow wzgledem miednicy biegu S_RUNL - jak gesty Znakow).
W3 trzyma kusze w lewej dloni (l_weapon), miecz w prawej. W Gothicu kusza (wizual przedmiotu) idzie na czas strzalu z
wezla ZS_CROSSBOW do ZS_LEFTHAND - ten slot jest animowany: w celowaniu kusza Gothica lezy jak l_weapon z W3 ze stala
poprawka obrotu (lufa i "gora" kuszy z natywnej pozy C_CBOW_5 - kusza w ZS_RIGHTHAND - ustawiona na wprost postaci),
w chwili chwytu i odlozenia slot dloni = wezel ZS_CROSSBOW na plecach (przelozenie bez skoku).
Decyzja autora 23.10: dlon siega tam, gdzie kusza wisi w Gothicu (plecy). Po tescie 23.10 (reka wygieta za plecy przy
siaganiu do poczatku wezla ZS_CROSSBOW nad prawym barkiem): dlon chwyta kusze za przod (GRIP_CM wzdluz osi -X wizualu,
blisko luku) - ta czesc kuszy wisi na srodku plecow na wysokosci ledzwi, tam gdzie W3 trzyma kabure i dokad dlon W3
i tak siega. Klatka chwytu / odlozenia = najblizsze podejscie dloni W3 do tego punktu; IK lewego ramienia (dwie kosci,
plaszczyzna lokcia z W3) tylko domyka reszte (najwyzej REACH_CAP cm), a co zostanie, przejmuje przesuniecie slotu w kilku
klatkach po chwycie (kusza "dosuwa sie" w dloni o kilka cm). W dloni kusza zostaje trzymana za przod: slot ZS_LEFTHAND ma
w kazdej klatce wlasne przesuniecie (poczatek kuszy = chwyt + GRIP_CM wzdluz osi kuszy), kolba wychodzi przy celowaniu
pod ramie / policzek. ZS_CROSSBOW jest w klipach kluczowany (poza z petli S_RUN).
Nazwy: nosniki gestow Znakow (zero nowych nazw) - wpisy pozycza zDismember na czas strzalu (Crossbow.cpp,
zdcore::XbowClipAni - ta sama tablica XBOW_ANIS). Nakladka HUMANS_ZDF_XBOW nie jest zakladana, tylko wczytywana.
Zdarzenia: tylko dzwieki (DRAWSOUND_BOW przy chwycie i odlozeniu, CROSSBOWSHOOT przy strzale) - bez DEF_* (tryb walki,
sloty, belt prowadzi plugin wedlug chwil z crossbow_w3.ini).

Uzycie: py -3.12 -I make_xbow.py   (pisze MAN / MSB do data_w3\_work\Data\Anims\_compiled i
data_w3\System\zDismember\crossbow_w3.ini; potem build_vdf.py --w3 [--nb | --km])
"""
import math, os, shutil, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import manlib, msblib
import retarget, build_overlay, sfx_events, make_pair, make_moves, make_signs
from retarget import qmat

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(ROOT, "anims", "work", "w3xbow")
SEED_DUMPS = os.environ.get("XBOW_SEED_DUMPS", "")     # opcjonalny katalog gotowych zrzutow (gdy w WORK brak zrzutu)
COMPILED = make_moves.COMPILED
INI = os.path.join(ROOT, "data_w3", "System", "zDismember", "crossbow_w3.ini")
SET = chr(92).join(["animations", "man", "combat", "man_geralt_gabriel.w2anims"])
CLIPS = {"start": "man_geralt_gabriel_aim_start_rp", "idle": "man_geralt_gabriel_aim_idle_rp",
         "add": "man_geralt_gabriel_shoot_additive_center", "end": "man_geralt_gabriel_aim_end_rp"}
OVERLAY = "HUMANS_ZDF_XBOW"
KINDS = ("IN", "LOOP", "SHOOT")
# (klip, postoj) -> nosnik; ta sama tablica w zDismember\Core\Crossbow.cpp (XbowClipAni)
XBOW_ANIS = {
    ("IN", True): "T_FOOD_RANDOM_1", ("LOOP", True): "T_FOOD_RANDOM_2", ("SHOOT", True): "T_MEAT_RANDOM_1",
    ("IN", False): "R_CHAIR_RANDOM_1", ("LOOP", False): "R_CHAIR_RANDOM_2", ("SHOOT", False): "R_CHAIR_RANDOM_3",
}
LAYER = make_signs.LAYER   # 21 - jak gesty Znakow (nad zakretami 20; gesty i kusza wykluczaja sie w pluginach)
FPS = retarget.FPS_OUT
GRAB_RANGE = (2, 8)    # klatki XBOW_IN, w ktorych szukamy najblizszego podejscia dloni W3 do chwytu (W3: kl. 4-6 @25)
POSE_S = 0.40      # koniec przejscia slotu (kusza w pozycji trzymania) - W3: poza celowania od 0,43 s
REL_BACK = 6       # klatek powrotu dloni do toru W3 po odlozeniu
GRIP_CM = -45.0    # chwyt: punkt na osi X wizualu kuszy (cm od poczatku; lufa i luk w -X: kusze G2 -48..-60)
REACH_CAP = 25.0   # cm - najwieksza korekta IK dloni (wiecej wyglada nienaturalnie - reszta w slocie)
SLIDE = 4          # klatek dosuniecia kuszy w dloni po chwycie / przed odlozeniem (reszta po IK)
TURN_S = 0.21      # W3 RotateToTarget 0-0,21 s
BLEND_IN, BLEND_NEXT = 0.12, 0.05
BLEND_OUT = 0.3
BLEND_OUT_STAND = 0.2   # jak make_signs: przejscie postoj -> ruch (StartAni wariantu ruchu wygasza wariant postoju)
ARM = ("BIP01 L UPPERARM", "BIP01 L FOREARM", "BIP01 L HAND")
CBOW_REF = "S_RUN"      # poza wezla ZS_CROSSBOW (kluczowany w S_RUN; w postawach broni nie - zostaje ostatnia)


# ---------------------------------------------------------------- zrzuty i addytyw

def dump(name):
    clip = CLIPS[name]
    os.makedirs(WORK, exist_ok=True)
    out = os.path.join(WORK, clip + ".json")
    seed = os.path.join(SEED_DUMPS, clip + ".json")
    if not os.path.exists(out) and os.path.exists(seed):
        shutil.copy2(seed, out)
    return make_pair.dump_clip(SET, clip, out)


def _locals(d, frame):
    names = [b["name"] for b in d["bones"]]
    parent = {b["name"]: b["parent"] for b in d["bones"]}
    W = {n: np.array(m) for n, m in zip(names, frame)}
    return {n: (W[n] if parent[n] is None else np.linalg.inv(W[parent[n]]) @ W[n]) for n in names}


def _compose(d, L):
    W = {}
    for b in d["bones"]:
        n, p = b["name"], b["parent"]
        if p is not None and p not in W:   # rodzic po dziecku - drugi obieg
            raise ValueError("kolejnosc kosci w zrzucie: %s przed %s" % (n, p))
        W[n] = L[n] if p is None else W[p] @ L[n]
    return [W[b["name"]].tolist() for b in d["bones"]]


def bake_additive(base, add):
    """Klatki base (od 0) z odrzutem addytywu: lokalnie L_base(k) @ (L_add(0)^-1 @ L_add(k)) na kosc. Addytyw W3
    (SAT_Additive, AdditiveType None) w zrzucie ma pozy bezwzgledne - delta wzgledem jego pierwszej klatki (ta sama poza
    na poczatku i na koncu, szczyt w kl. 2-3: lewe ramie ok. 37 st., przedramie 34 st.)."""
    assert [b["name"] for b in base["bones"]] == [b["name"] for b in add["bones"]]
    a0 = _locals(add, add["frames"][0])
    out = dict(base)
    frames = []
    for k in range(len(add["frames"])):
        lb = _locals(base, base["frames"][k % len(base["frames"])])
        la = _locals(add, add["frames"][k])
        frames.append(_compose(base, {n: lb[n] @ np.linalg.inv(a0[n]) @ la[n] for n in lb}))
    out["frames"] = frames
    return out


# ---------------------------------------------------------------- retarget z kosci kuszy W3

def retarget_clip(d, sk, grip):
    """-> klatki @25 (lokalne obroty + korzen) i obroty swiatowe l_weapon W3 (w ukladzie Gothica) w tych klatkach."""
    d = make_moves.to_entity_start(d)
    fr = retarget.retarget(d, sk, grip, False, "w3", weapon="r_weapon")
    _, mf = retarget.mixamo_frames(d, retarget.P_W3)
    ext = [(list(local) + [mf[k]["l_weapon"][0]], root) for k, (local, root) in enumerate(fr)]
    res = retarget.resample(ext, d["fps"])
    frames = [(list(local[:-1]), root) for local, root in res]
    lw = [local[-1] for local, _ in res]
    return frames, lw


def stand_variant(frames):
    """Postoj: tylko wysokosc korzenia z W3, stopy stoja (make_signs.plant_feet)."""
    return [([R.copy() for R in local], np.array([0.0, root[1], 0.0])) for local, root in frames]


def move_variant(sk, frames, ref):
    """Ruch: tulow wzgledem miednicy biegu ref (jak make_signs.build_gesture full=False)."""
    spine, pelvis = sk.idx("BIP01 SPINE"), sk.idx("BIP01 PELVIS")
    P = sk.compose(ref[0], ref[1])[pelvis][:3, :3]
    out = []
    for local, root in frames:
        W = sk.compose(local, root)
        loc = [R.copy() for R in local]
        loc[spine] = P.T @ W[spine][:3, :3]
        out.append((loc, root))
    return out


# ---------------------------------------------------------------- kusza: natywna poza, wezel na plecach

def native_aim(vfs, sk):
    """Obrot kuszy w celowaniu (swiat, postac wzdluz +Z): ZS_RIGHTHAND z C_CBOW_5 (srodek S_CBOWAIM) obrocony tak, zeby
    lufa (-X wezla) szla dokladnie na wprost i poziomo; "gora" kuszy jak w Gothicu."""
    m = manlib.Man(bytes(vfs.find("HUMANS-C_CBOW_5.MAN").data))
    local = [l[:3, :3].copy() for l in sk.local]
    root = np.array(sk.root_pos, dtype=float)
    for k, ni in enumerate(m.nodes):
        (x, y, z, w), p = m.decode(m.sample(0, k))
        local[ni] = qmat(-x, -y, -z, w)
        if sk.parent[ni] < 0:
            root = np.array(p, dtype=float)
    R = sk.compose(local, root)[sk.idx("ZS_RIGHTHAND")][:3, :3]
    muzzle = -R[:, 0]
    fix = retarget.rot_between(muzzle, np.array([0.0, 0.0, 1.0]))
    return fix @ R, muzzle


def cbow_local(vfs, sk):
    """Lokalny obrot ZS_CROSSBOW (wzgledem BIP01 SPINE2) ze sredniej petli S_RUN; przesuniecie z MDH."""
    local, _ = make_signs.mean_pose(vfs, sk, CBOW_REF)
    return local[sk.idx("ZS_CROSSBOW")]


def qavg(Rs):
    q0 = retarget.quat_from_mat(Rs[0])
    qs = [retarget.quat_from_mat(R) for R in Rs]
    q = np.sum([x if np.dot(x, q0) >= 0 else -x for x in qs], axis=0)
    return qmat(*(q / np.linalg.norm(q)))


def slerpR(A, B, t):
    return qmat(*retarget.slerp(retarget.quat_from_mat(A), retarget.quat_from_mat(B), t))


def smooth(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3.0 - 2.0 * x)


# ---------------------------------------------------------------- IK lewego ramienia

def arm_ik(sk, local, root, target, W=None):
    """Dlon (staw BIP01 L HAND) w punkt target: ramie i przedramie z IK dwoch kosci w plaszczyznie lokcia z klatki,
    obrot dloni w swiecie bez zmian (palce i slot ida za nia). -> (nowe lokalne, blad cm)."""
    ua, fa, h = (sk.idx(n) for n in ARM)
    if W is None:
        W = sk.compose(local, root)
    a, b = np.linalg.norm(sk.local[fa][:3, 3]), np.linalg.norm(sk.local[h][:3, 3])
    S, K0, Rh = W[ua][:3, 3], W[fa][:3, 3], W[h][:3, :3]
    D = target - S
    dist = np.linalg.norm(D)
    u = D / dist
    d = min(max(dist, abs(a - b) + 1e-3), a + b - 1e-3)
    p = (K0 - S) - np.dot(K0 - S, u) * u
    if np.linalg.norm(p) < 1e-6:
        p = W[ua][:3, :3] @ np.array([0.0, 0.0, -1.0])
        p = p - np.dot(p, u) * u
    v = p / np.linalg.norm(p)
    x = (a * a - b * b + d * d) / (2 * d)
    K = S + x * u + math.sqrt(max(a * a - x * x, 0.0)) * v
    Rt = W[ua][:3, :3]
    Rt = retarget.rot_between(Rt @ sk.local[fa][:3, 3], K - S) @ Rt
    Rc = Rt @ local[fa]
    Rc = retarget.rot_between(Rc @ sk.local[h][:3, 3], S + d * u - K) @ Rc
    out = [R.copy() for R in local]
    out[ua] = W[sk.parent[ua]][:3, :3].T @ Rt
    out[fa] = Rt.T @ Rc
    out[h] = Rc.T @ Rh
    W2 = sk.compose(out, root)
    return out, float(np.linalg.norm(W2[h][:3, 3] - target))


def cbow_matrix(sk, cb_local):
    M = np.eye(4)
    M[:3, :3] = cb_local
    M[:3, 3] = sk.local[sk.idx("ZS_CROSSBOW")][:3, 3]
    return M


def grip_vec():
    return np.array([GRIP_CM, 0.0, 0.0])


def back_world(sk, W, cb_local):
    """Kusza na plecach (4x4 w ukladzie modelu) w pozie W."""
    return W[sk.idx("BIP01 SPINE2")] @ cbow_matrix(sk, cb_local)


def grip_point(sk, W, cb_local):
    """Punkt chwytu kuszy wiszacej na plecach."""
    C = back_world(sk, W, cb_local)
    return C[:3, :3] @ grip_vec() + C[:3, 3]


def hand_grip(sk, W):
    """Punkt trzymania w dloni: poczatek wezla ZS_LEFTHAND z MDH (tam ma byc chwyt kuszy)."""
    h = sk.idx("BIP01 L HAND")
    return W[h][:3, :3] @ sk.local[sk.idx("ZS_LEFTHAND")][:3, 3] + W[h][:3, 3]


def closest_frame(sk, frames, cb_local, lo, hi):
    """Klatka (lo..hi) najblizszego podejscia dloni do chwytu kuszy na plecach i ta odleglosc (cm)."""
    best = None
    for k in range(max(0, lo), min(len(frames) - 1, hi) + 1):
        W = sk.compose(*frames[k])
        d = float(np.linalg.norm(hand_grip(sk, W) - grip_point(sk, W, cb_local)))
        if best is None or d < best[1]:
            best = (k, d)
    return best


def reach_weights(n, keys):
    """Waga IK (0..1) w kazdej klatce: keys = [(klatka, waga)] - gladko (smoothstep) miedzy kolejnymi punktami."""
    out = []
    for k in range(n):
        if k <= keys[0][0]:
            out.append(keys[0][1])
            continue
        if k >= keys[-1][0]:
            out.append(keys[-1][1])
            continue
        for (k0, w0), (k1, w1) in zip(keys, keys[1:]):
            if k0 <= k <= k1:
                out.append(w0 + (w1 - w0) * smooth((k - k0) / max(1, k1 - k0)))
                break
    return out


def apply_reach(sk, frames, weights, cb_local, cap=REACH_CAP):
    """Dlon ku chwytowi kuszy na plecach z waga weights[k], korekta najwyzej cap cm. -> klatki, korekty (cm),
    reszta po IK w klatkach z waga 1 (cm)."""
    out, fixes, rest = [], [], []
    h = sk.idx("BIP01 L HAND")
    for (local, root), w in zip(frames, weights):
        if w <= 1e-4:
            out.append(([R.copy() for R in local], root))
            fixes.append(0.0)
            continue
        W = sk.compose(local, root)
        d = grip_point(sk, W, cb_local) - hand_grip(sk, W)
        n = float(np.linalg.norm(d))
        if n > cap:
            d = d * (cap / n)
        p = W[h][:3, 3]
        loc, _ = arm_ik(sk, local, root, p + w * d, W)
        out.append((loc, root))
        fixes.append(float(np.linalg.norm(w * d)))
        if w > 0.999:
            W2 = sk.compose(loc, root)
            rest.append(float(np.linalg.norm(grip_point(sk, W2, cb_local) - hand_grip(sk, W2))))
    return out, fixes, rest


def slot_track(sk, frames, aim, mix, cb_local):
    """Lokalny ZS_LEFTHAND (4x4): kusza na plecach (mix 0 - dokladnie wezel ZS_CROSSBOW wyrazony w dloni) <-> kusza
    trzymana za chwyt (mix 1 - obrot jak l_weapon W3 z poprawka, poczatek kuszy = chwyt dloni - R * GRIP)."""
    h, lh = sk.idx("BIP01 L HAND"), sk.idx("ZS_LEFTHAND")
    t_lh = sk.local[lh][:3, 3]
    out = []
    for (local, root), A, m in zip(frames, aim, mix):
        W = sk.compose(local, root)
        back = np.linalg.inv(W[h]) @ back_world(sk, W, cb_local)
        held = np.eye(4)
        held[:3, :3] = retarget.ortho(W[h][:3, :3].T @ A)
        held[:3, 3] = t_lh - held[:3, :3] @ grip_vec()
        if m <= 0.0:
            S = back
        elif m >= 1.0:
            S = held
        else:
            S = np.eye(4)
            S[:3, :3] = slerpR(retarget.ortho(back[:3, :3]), held[:3, :3], m)
            S[:3, 3] = back[:3, 3] * (1 - m) + held[:3, 3] * m
        S[:3, :3] = retarget.ortho(S[:3, :3])
        out.append(S)
    return out


def with_slots(sk, frames, slots, cb_local):
    lh, cb = sk.idx("ZS_LEFTHAND"), sk.idx("ZS_CROSSBOW")
    out = []
    for (local, root), S in zip(frames, slots):
        loc = [R.copy() for R in local]
        loc[lh] = S[:3, :3].copy()
        loc[cb] = cb_local.copy()
        out.append((loc, root))
    return out


def copy_arm(sk, src, dst):
    """Ramie, przedramie, dlon i sloty z wariantu postoju do wariantu ruchu (te same klatki; wszystko wzgledem BIP01
    SPINE2 - w obu wariantach lokalne obroty kregoslupa od SPINE1 w gore sa te same)."""
    ids = [sk.idx(n) for n in ARM + ("ZS_LEFTHAND", "ZS_CROSSBOW")]
    out = []
    for (ls, _), (ld, rd) in zip(src, dst):
        loc = [R.copy() for R in ld]
        for i in ids:
            loc[i] = ls[i].copy()
        out.append((loc, rd))
    return out


def compose_slots(sk, local, root, slot):
    """Jak sk.compose, ale ZS_LEFTHAND z pelna macierza slotu (przesuniecie animowane)."""
    W = sk.compose(local, root)
    lh = sk.idx("ZS_LEFTHAND")
    W[lh] = W[sk.parent[lh]] @ slot
    return W


def nodes(sk, standing):
    base = make_signs.body_nodes(sk) if standing else make_signs.upper_nodes(sk)
    return base + [sk.idx("ZS_LEFTHAND"), sk.idx("ZS_CROSSBOW")]


def muzzle_dir(sk, local, root):
    W = sk.compose(local, root)
    R = W[sk.idx("ZS_LEFTHAND")][:3, :3]
    return -R[:, 0]


def elbow_angle(sk, local, root):
    """Kat w lokciu (st.): 180 = reka wyprostowana."""
    W = sk.compose(local, root)
    a, b, c = (W[sk.idx(n)][:3, 3] for n in ARM)
    u, v = a - b, c - b
    return math.degrees(math.acos(max(-1.0, min(1.0, float(np.dot(u, v) / (np.linalg.norm(u) * np.linalg.norm(v)))))))


def build(vfs=None, sk=None, verbose=True):
    """-> dict: klipy {(kind, standing): dict(frames, nodes, events)}, chwile (ms), diagnostyka."""
    vfs = vfs or build_overlay.game_vfs()
    sk = sk or retarget.GothicSkeleton(vfs)
    grip = make_pair.grip_local(vfs, sk)
    run = make_signs.mean_pose(vfs, sk, make_signs.RUN_REF)
    aimR, nat_muzzle = native_aim(vfs, sk)
    cbl = cbow_local(vfs, sk)
    d_start, d_idle, d_add, d_end = dump("start"), dump("idle"), dump("add"), dump("end")
    d_shoot = make_moves.concat([bake_additive(d_idle, d_add), d_end])
    n_add = int(round((len(d_add["frames"]) - 1) / d_add["fps"] * FPS))   # klatki odrzutu @25 w XBOW_SHOOT
    raw = {}
    for kind, d in (("IN", d_start), ("LOOP", d_idle), ("SHOOT", d_shoot)):
        raw[kind] = retarget_clip(d, sk, grip)
    # stala poprawka obrotu: kusza Gothica = l_weapon W3 (srednia z petli celowania) x C, tak zeby w petli lufa szla na
    # wprost jak w natywnym celowaniu
    lw_loop = raw["LOOP"][1]
    C = qavg(lw_loop).T @ aimR
    out = {"clips": {}, "diag": {}}
    k_p = int(round(POSE_S * FPS))
    timing = {}
    for kind in KINDS:
        frames, lw = raw[kind]
        n = len(frames)
        stand = make_signs.plant_feet(sk, stand_variant(frames))
        aim = [R @ C for R in lw]
        if kind == "IN":
            k_g, near = closest_frame(sk, stand, cbl, *GRAB_RANGE)
            k_p = max(k_p, k_g + SLIDE)
            w = reach_weights(n, [(0, 0.0), (k_g, 1.0), (min(n - 1, k_g + SLIDE + 2), 0.0)])
            mix = [0.0 if k <= k_g else smooth((k - k_g) / max(1, k_p - k_g)) for k in range(n)]
            ev_sfx = [(k_g + 1, "DRAWSOUND_BOW")]
            out["diag"]["k_g"] = k_g
            out["diag"]["grab_w3_cm"] = near
        elif kind == "LOOP":
            w = [0.0] * n
            mix = [1.0] * n
            ev_sfx = []
        else:
            k_r, near = closest_frame(sk, stand, cbl, n_add + 4, n - REL_BACK - 1)
            w = reach_weights(n, [(n_add, 0.0), (k_r, 1.0), (min(n - 1, k_r + REL_BACK), 0.0)])
            m0 = max(n_add, k_r - SLIDE - 3)
            mix = [1.0 if k <= m0 else 1.0 - smooth((k - m0) / max(1, k_r - m0)) for k in range(n)]
            out["diag"]["rel_w3_cm"] = near
            ev_sfx = [(1, "CROSSBOWSHOOT"), (k_r + 1, "DRAWSOUND_BOW")]
            timing["ReleaseMs"] = round(k_r / FPS * 1000)
            out["diag"]["k_r"] = k_r
        raw_stand = stand
        stand, fixes, rest = apply_reach(sk, stand, w, cbl)
        slots = slot_track(sk, stand, aim, mix, cbl)
        stand = with_slots(sk, stand, slots, cbl)
        move = copy_arm(sk, stand, move_variant(sk, frames, run))
        out["clips"][(kind, True)] = dict(frames=stand, nodes=nodes(sk, True), sfx=ev_sfx, slots=slots)
        out["clips"][(kind, False)] = dict(frames=move, nodes=nodes(sk, False), sfx=ev_sfx, slots=slots)
        out["diag"][kind] = dict(frames=n, ik_fix=max(fixes) if fixes else 0.0, ik_rest=max(rest) if rest else 0.0,
                                 weights=w, mix=mix, raw=raw_stand)
        timing[{"IN": "InMs", "LOOP": "LoopMs", "SHOOT": "ShootMs"}[kind]] = round((n - 1) / FPS * 1000)
    timing["GrabMs"] = round(out["diag"]["k_g"] / FPS * 1000)
    timing["ShootOutMs"] = round(n_add / FPS * 1000)
    timing["GripCm"] = int(round(GRIP_CM))
    timing["FireMs"] = 0
    timing["TurnMs"] = round(TURN_S * 1000)
    out["timing"] = timing
    out["diag"]["n_add"] = n_add
    out["diag"]["C"] = C
    out["diag"]["native_muzzle"] = nat_muzzle
    out["cbow_local"] = cbl
    out["sk"] = sk
    out["vfs"] = vfs
    if verbose:
        for kind in KINDS:
            dg = out["diag"][kind]
            print("%-5s %2d kl. (%.2f s), korekta IK dloni %.1f cm, reszta (dosuniecie w dloni) %.1f cm" % (
                kind, dg["frames"], (dg["frames"] - 1) / FPS, dg["ik_fix"], dg["ik_rest"]))
        print("chwyt kl. %d (dlon W3 %.1f cm od chwytu), odlozenie kl. %d (%.1f cm)" % (
            out["diag"]["k_g"], out["diag"]["grab_w3_cm"], out["diag"]["k_r"], out["diag"]["rel_w3_cm"]))
    return out


def transfer_error(sk, frames, k, cb_local, slots):
    """(cm, st.) miedzy kusza w dloni (ZS_LEFTHAND ze slotem) a kusza na plecach (ZS_CROSSBOW) w klatce k."""
    W = compose_slots(sk, *frames[k], slots[k])
    A = W[sk.idx("ZS_LEFTHAND")]
    B = back_world(sk, W, cb_local)
    R = A[:3, :3].T @ B[:3, :3]
    ang = math.degrees(math.acos(max(-1.0, min(1.0, (np.trace(R) - 1) / 2))))
    return float(np.linalg.norm(A[:3, 3] - B[:3, 3])), ang


def write_man_slots(template, frames, slots, sk, name, nodes, out_path):
    """Jak retarget.write_man, ale ZS_LEFTHAND z wlasnym przesunieciem w kazdej klatce (kusza na chwycie)."""
    lh = sk.idx("ZS_LEFTHAND")
    S = 2.1 / 65535.0
    pos, rots = [], []
    for (local, root), slot in zip(frames, slots):
        for i in nodes:
            q = retarget.quat_from_mat(slot[:3, :3] if i == lh else local[i])
            q = np.array([-q[0], -q[1], -q[2], q[3]])
            if q[3] < 0:
                q = -q
            rots.append(q)
            pos.append(root if sk.parent[i] < 0 else (slot[:3, 3] if i == lh else sk.local[i][:3, 3]))
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
    m = template
    m.name, m.frames = name, len(frames)
    m.nodes, m.nnodes = list(nodes), len(nodes)
    m.samples = samples
    m.pmin, m.pscale = float(lo), float(scale)
    m.fps = m.fps_src = FPS
    m.bbox = [float(v) for v in bb]
    m.next = ""
    if manlib.CH_EVENTS in m.raw:
        m.order = [t for t in m.order if t != manlib.CH_EVENTS]
    open(out_path, "wb").write(m.build())


def write_ini(timing):
    os.makedirs(os.path.dirname(INI), exist_ok=True)
    lines = ["; make_xbow.py - chwile szybkiego strzalu z kuszy (ms przy XbowSpeed=100) dla zDismember (Crossbow.cpp)",
             "; InMs / LoopMs / ShootMs - dlugosci XBOW_IN / LOOP / SHOOT; GrabMs - kusza z plecow do dloni (od startu IN);",
             "; FireMs - belt (od startu SHOOT); ReleaseMs - kusza na plecy (od startu SHOOT); TurnMs - obrot do celu na postoju;",
             "; ShootOutMs - poczatek odlozenia w SHOOT (wyjscie po anulowaniu PPM); GripCm - chwyt na osi X kuszy (cm)",
             "[XBOW]"]
    for k in ("InMs", "LoopMs", "ShootMs", "GrabMs", "FireMs", "ReleaseMs", "TurnMs", "ShootOutMs", "GripCm"):
        lines.append("%s=%d" % (k, timing[k]))
    open(INI, "w", newline="\r\n").write("\n".join(lines) + "\n")


def main():
    res = build()
    sk, vfs = res["sk"], res["vfs"]
    os.makedirs(COMPILED, exist_ok=True)
    for f in os.listdir(COMPILED):
        if f.upper().startswith(OVERLAY + "-") or f.upper().startswith(OVERLAY + "."):
            os.remove(os.path.join(COMPILED, f))
    template = manlib.Man(bytes(vfs.find("HUMANS-T_1HSFINISH.MAN").data))
    chunks = []
    for kind in KINDS:
        for standing in (True, False):
            c = res["clips"][(kind, standing)]
            ani = XBOW_ANIS[(kind, standing)]
            template.layer = LAYER
            write_man_slots(template, c["frames"], c["slots"], sk, ani, c["nodes"], os.path.join(COMPILED, "%s-%s.MAN" % (OVERLAY, ani)))
            template = manlib.Man(bytes(vfs.find("HUMANS-T_1HSFINISH.MAN").data))
            nf = len(c["frames"])
            bin_ = BLEND_IN if kind == "IN" else BLEND_NEXT
            bout = (BLEND_OUT_STAND if standing else BLEND_OUT) if kind == "SHOOT" else BLEND_NEXT
            ch = msblib.ani_chunk(ani, LAYER, "", bin_, bout, "M.", "ZDISM_W3.ASC", 1, nf)
            chunks.append(ch + sfx_events.events_block(sfx=c["sfx"]))
            print("%-5s %-6s %-17s %2d kl. (%.2f s)" % (kind, "postoj" if standing else "ruch", ani, nf, nf / FPS))
    build_overlay.write_overlay(OVERLAY, COMPILED, chunks, vfs)
    write_ini(res["timing"])
    for kind, k in (("IN", res["diag"]["k_g"]), ("SHOOT", res["diag"]["k_r"])):
        c = res["clips"][(kind, True)]
        e = transfer_error(sk, c["frames"], k, res["cbow_local"], c["slots"])
        print("przelozenie %s kl. %d: %.2f cm, %.2f st." % (kind, k, e[0], e[1]))
    print("crossbow_w3.ini: " + " ".join("%s=%d" % kv for kv in res["timing"].items()))


if __name__ == "__main__":
    main()
