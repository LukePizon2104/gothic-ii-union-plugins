r"""Uniki i przewroty z Wiedzmina 3 dla zDismember. TYLKO DO UZYTKU PRYWATNEGO (animacje CD Projekt RED) - wynik
trafia do data_w3 / zDismember_W3.vdf, nigdy do wydania.

Uzycie: py -3.12 -I make_moves.py [--dodge-scale 0.6] [--roll-scale 0.75] [--roll-b-air 0.6]
Wynik (data_w3): nakladki HUMANS_ZDF_MV_<ID> (T_1HPARADEJUMPB, T_2HPARADEJUMPB, T_FISTPARADEJUMPB - animacja w
miejscu) i System\zDismember\moves_w3.ini z torem (x w prawo, z do przodu, cm), oknem nietykalnosci i czasem powrotu
sterowania. Uniki: man_geralt_sword_dodge_*_350m (3,5 m, skracane). Przewroty F/L/R: przewrot w przod (roll_start_f +
roll_end_f) obrocony w strone ruchu - cialo obraca sie, vob (i kamera) zostaje. Przewrot w tyl: osobny klip W3 z obrotem
przodem do przeciwnika przy wstawaniu (ROLL_B_CLIP).
Kontry (COUNTER_L/R, BASH_F, KICK_F): wejscie z gardy stylu W3 (osobno 1H / 2H), odbicie, riposta / repel; sklejenia
bezwladnosciowe (inertial_append) z klatkami wejscia wybranymi po najblizszej pozie; w ini hit= (chwila ciosu) i reach=
(od voba do punktu kontaktu - plugin celuje wypad tak, by przeciwnik byl wtedy w zasiegu).
Ataki z wypadem: z wyskoku (JUMPATK_F, _F2..., JUMPATK2H_F, _F2...) i w biegu (RUNATK_F/_F2, RUNATK2H_F/_F2) - warianty
na bron, plugin wybiera losowo sposrod pasujacych do odleglosci celu.
"""
import argparse, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import msblib
import retarget, build_overlay, make_pair, recovery, sfx_events
from make_pair import bone_frame, moved

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(ROOT, "anims", "work", "w3moves")
DATA = os.path.join(ROOT, "data_w3")
COMPILED = os.path.join(DATA, "_work", "Data", "Anims", "_compiled")
INI = os.path.join(DATA, "System", "zDismember", "moves_w3.ini")
SWORD_SET = r"animations\man\combat\man_geralt_sword.w2anims"
ANIS = (("T_1HPARADEJUMPB", "S_1HRUN"), ("T_2HPARADEJUMPB", "S_2HRUN"), ("T_FISTPARADEJUMPB", "S_FISTRUN"))

DODGES = {"L": "man_geralt_sword_dodge_left_350m", "R": "man_geralt_sword_dodge_right_350m",
          "B": "man_geralt_sword_dodge_back_350m", "F": "man_geralt_sword_dodge_forward_350m"}
ROLL = ("man_ger_sword_roll_start_f", "man_ger_sword_roll_end_f")
ROLL_YAW = {"F": 0.0, "L": 90.0, "R": -90.0}   # obrot w Blenderze (wokol Z): +90 = przod -> lewo
# Przewrot w tyl (ROLL_B): klip W3 z obrotem - Geralt z gardy obraca sie do skoku, przewraca w tyl i wstajac obraca
# sie z powrotem przodem do przeciwnika (koniec w gardzie rp jak na starcie). Wczesniej ROLL_B = przewrot w przod
# obrocony o 180 st.: cialo konczylo tylem do wroga, a doklejone przejscie do postawy (0,24 s) obracalo je o ok. 160 st.
# w miejscu. Klip ma 7 m i w locie do 14 m/s (kamera Gothica nie nadaza) - faza ze stopami w gorze (skok i przewrot)
# skrocona x ROLL_B_AIR (feet_up / air_compress), kroki na ziemi bez zmian (bez slizgu stop): ok. 5 m jak przewrot w przod.
ROLL_B_CLIP = "man_geralt_sword_dodge_roll_flip_rp_b_01"
ROLL_B_AIR = 0.6
FEET_UP = 12.0   # cm - najnizszy punkt stop wyzej niz tyle nad najnizszym w calym klipie = stopy w gorze


def rot_z(deg, t=(0.0, 0.0)):
    a = math.radians(deg)
    M = np.eye(4)
    M[:2, :2] = [[math.cos(a), -math.sin(a)], [math.sin(a), math.cos(a)]]
    M[:2, 3] = t
    return M


def to_entity_start(dump):
    """Vob (Trajectory kl. 0) w (0,0), przod wzdluz +Y Blendera (= +Z Gothica); Trajectory w spoczynku ma yaw -90."""
    t = bone_frame(dump, "Trajectory")
    yaw = math.degrees(math.atan2(t[1][0], t[0][0]))
    R = rot_z(-90.0 - yaw)
    p = R[:2, :2] @ t[:2, 3]
    return moved(dump, rot_z(-90.0 - yaw, (-p[0], -p[1])))


def concat(dumps):
    """Kolejne klipy dosuniete tak, ze Trajectory nastepnego zaczyna sie tam, gdzie skonczyl poprzedni."""
    out = dict(dumps[0])
    frames = list(dumps[0]["frames"])
    for d in dumps[1:]:
        last = np.array(frames[-1][[b["name"] for b in d["bones"]].index("Trajectory")])
        first = bone_frame(d, "Trajectory")
        shift = np.eye(4)
        shift[:2, 3] = last[:2, 3] - first[:2, 3]
        frames += moved(d, shift)["frames"][1:]
    out["frames"] = frames
    return out


REC_FRAMES = 6   # przejscie do postawy Gothica po ruchu (0,24 s)

# Kontra = odbicie (blok W3) + riposta (szybkie ciecie W3) - bez obrotu wokol przeciwnika (kontry W3 kreca Geralta o
# 150-240 st., kamera Gothica krecilaby sie razem z nim). L = odbicie z lewej i ciecie w lewo, R = w prawo.
# BASH / KICK = odbicie i repel W3 (pchniecie barkiem / kopniecie - plugin zamiast obrazen przewraca przeciwnika
# do tylu). Klipy repel zaczynaja sie w pozie parady (w W3 ida po odbiciu); hit = klatka uderzenia @25 w klipie repel
# (bark / stopa najdalej z przodu).
# contact = czym kontra siega przeciwnika (zasieg reach= w ini): ostrze, bark (tulow), stopa.
COUNTERS = {
    "COUNTER_L": dict(parry="man_geralt_sword_parry_f_leftup_rp", attack="man_geralt_sword_attack_fast_1_rp_40ms", contact="blade"),
    "COUNTER_R": dict(parry="man_geralt_sword_parry_f_rightup_rp", attack="man_geralt_sword_attack_fast_4_rp_40ms", contact="blade"),
    "BASH_F": dict(parry="man_geralt_sword_parry_f_leftup_rp", attack="man_geralt_sword_repel_rp_bash", hit=12, start=0, step=0.6,
                   contact="body"),
    "KICK_F": dict(parry="man_geralt_sword_parry_f_rightup_rp", attack="man_geralt_sword_repel_rp_kick", hit=10, start=0, step=0.6,
                   contact="leg"),
}
COUNTER_PARRY = 8          # klatek odbicia (0,32 s) - koniec szukany w COUNTER_PARRY_RANGE po najlepszym sklejeniu
COUNTER_PARRY_RANGE = (7, 11)
COUNTER_TAIL = 10          # klatek po ciosie
COUNTER_STEPS = (0.3, 0.5)  # odrzut odbicia, wypad riposty (zeby siegnac przeciwnika)
COUNTER_ENTRY = 6          # klatek wejscia z gardy W3 w odbicie (krocej = szarpniecie wieksze niz w samych klipach W3)
COUNTER_BLEND = 6          # klatek przejscia odbicie -> riposta / repel
COUNTER_WINDUP = 8         # riposta: co najmniej tyle klatek zamachu przed ciosem (wejscie w klip najwyzej hit - 8)
# kosci oceniane przy wyborze klatek sklejenia (poza i predkosc) i w testach plynnosci
JOIN_BONES = ("BIP01", "BIP01 PELVIS", "BIP01 SPINE", "BIP01 SPINE1", "BIP01 SPINE2", "BIP01 NECK", "BIP01 HEAD",
              "BIP01 L THIGH", "BIP01 R THIGH", "BIP01 L CALF", "BIP01 R CALF", "BIP01 L UPPERARM", "BIP01 R UPPERARM",
              "BIP01 L FOREARM", "BIP01 R FOREARM", "BIP01 L HAND", "BIP01 R HAND")


def rotvec(R):
    """Obrot 3x3 -> os * kat (rad)."""
    c = max(-1.0, min(1.0, (np.trace(R) - 1.0) / 2.0))
    a = math.acos(c)
    if a < 1e-7:
        return np.zeros(3)
    if a > math.pi - 1e-3:   # okolo 180 st.: os z czesci symetrycznej
        M = (R + np.eye(3)) / 2.0
        i = int(np.argmax(np.diag(M)))
        axis = M[:, i] / math.sqrt(max(M[i, i], 1e-12))
        return axis / np.linalg.norm(axis) * a
    return np.array([R[2, 1] - R[1, 2], R[0, 2] - R[2, 0], R[1, 0] - R[0, 1]]) / (2.0 * math.sin(a)) * a


def expmap(v):
    a = float(np.linalg.norm(v))
    if a < 1e-9:
        return np.eye(3)
    k = v / a
    K = np.array([[0.0, -k[2], k[1]], [k[2], 0.0, -k[0]], [-k[1], k[0], 0.0]])
    return np.eye(3) + math.sin(a) * K + (1.0 - math.cos(a)) * K @ K


def hermite_offset(x0, v0, n, k):
    """Przesuniecie po k klatkach: start x0 z predkoscia v0 (na klatke), po n klatkach 0 z zerowa predkoscia."""
    s = min(1.0, k / float(n))
    return (2 * s ** 3 - 3 * s ** 2 + 1) * x0 + (s ** 3 - 2 * s ** 2 + s) * n * v0


BLEND_OVERSHOOT = math.radians(20.0)   # bezwladnosc konca poprzedniej czesci przenosi poze najwyzej o tyle


def inertial_append(prev, frames, s0, s1, scale, n, xz_inertia=True):
    """Dokleja frames[s0+1..s1] za prev (lista (lokalne obroty, korzen)) z przejsciem bezwladnosciowym: roznica pozy
    frames[s0] wzgledem ostatniej klatki prev (i roznica predkosci) wygasa w n klatkach krzywa Hermite'a - poza i
    predkosc ciagle na sklejeniu, bez zamrazania ostatniej pozy i bez przeskokow slerpa przez 180 st.
    Korzen w poziomie dosuniety do prev, ruch skalowany 'scale'; wysokosc i (opcjonalnie) predkosc pozioma tez wygasaja."""
    a1 = prev[-1]
    a0 = prev[-2] if len(prev) > 1 else prev[-1]
    b0 = frames[s0]
    b1 = frames[min(s0 + 1, len(frames) - 1)]
    cap = BLEND_OVERSHOOT / (4.0 / 27.0) / n   # najwieksze h10 = 4/27
    x0, v0 = [], []
    for i in range(len(a1[0])):
        x = rotvec(b0[0][i].T @ a1[0][i])
        v = rotvec(a0[0][i].T @ a1[0][i]) - rotvec(b0[0][i].T @ b1[0][i])
        nv = float(np.linalg.norm(v))
        x0.append(x)
        v0.append(v * (cap / nv) if nv > cap else v)
    base = np.array(a1[1], dtype=float)
    r_s0 = np.array(b0[1], dtype=float)

    def root_of(f):
        r = np.array(frames[f][1], dtype=float)
        return np.array([base[0] + (r[0] - r_s0[0]) * scale, r[1], base[2] + (r[2] - r_s0[2]) * scale])

    rx0 = base - root_of(s0)
    rv0 = (np.array(a1[1], dtype=float) - np.array(a0[1], dtype=float)) - (root_of(min(s0 + 1, len(frames) - 1)) - root_of(s0))
    if not xz_inertia:
        rx0[0] = rx0[2] = rv0[0] = rv0[2] = 0.0
    out = list(prev)
    for k, f in enumerate(range(s0 + 1, s1 + 1), start=1):
        local = [R @ expmap(hermite_offset(x, v, n, k)) if k < n else R.copy() for R, x, v in zip(frames[f][0], x0, v0)]
        out.append((local, root_of(f) + (hermite_offset(rx0, rv0, n, k) if k < n else 0.0)))
    return out


def join_cost(sk, a0, a1, b0, b1, w_vel=0.5):
    """Koszt sklejenia: rozbieznosc pozy (st.) i predkosci (st./kl.) kosci JOIN_BONES."""
    cost = 0.0
    for name in JOIN_BONES:
        i = sk.idx(name)
        cost += math.degrees(np.linalg.norm(rotvec(b0[0][i].T @ a1[0][i])))
        cost += w_vel * math.degrees(np.linalg.norm(rotvec(a0[0][i].T @ a1[0][i]) - rotvec(b0[0][i].T @ b1[0][i])))
    return cost


def best_join(sk, pf, af, e_range, s_range):
    """(koniec pierwszego klipu, wejscie w drugi) o najmniejszym koszcie sklejenia."""
    best = None
    for e in range(max(1, e_range[0]), min(e_range[1], len(pf) - 1) + 1):
        for s in range(max(0, s_range[0]), min(s_range[1], len(af) - 2) + 1):
            c = join_cost(sk, pf[e - 1], pf[e], af[s], af[s + 1])
            if best is None or c < best[0]:
                best = (c, e, s)
    return best[1], best[2]


def inertial_recovery(frames, target, n):
    """frames + n klatek do pozy target (postawa): ruch konca wygasa bezwladnie, ostatnia klatka = target dokladnie;
    korzen stoi w poziomie (animacja w miejscu), wysokosc przechodzi do postawy."""
    t_local, t_root = target
    last_root = np.array(frames[-1][1], dtype=float)
    static = [([R.copy() for R in t_local], np.array([last_root[0], t_root[1], last_root[2]]))] * (n + 1)
    out = inertial_append(frames, static, 0, n, 0.0, n, xz_inertia=False)
    out[-1] = ([R.copy() for R in t_local], np.array([last_root[0], t_root[1], last_root[2]]))
    return out


GUARD_OVERLAP = 4   # garda parady 2H: przejscie do niej zaczyna sie najwyzej tyle klatek przed koncem ruchu
# Animacje dla gardy parady 2H (lp), ktore koncza sie przejsciem do niej (guard_recovery) w wariancie _W3 ruchow
GUARD_BLEND_ANIS = ("T_2HPARADEJUMPB",)


def guard_blend_cost(sk, frames, lo):
    """Szarpniecie konca animacji od klatki lo (postac na koncu stoi - ostatnia klatka powtorzona): najwieksza predkosc
    kosci JOIN_BONES w ukladzie modelu (st./kl.) + najwieksza zmiana tej predkosci + najwieksza predkosc obrotu korzenia
    BIP01 (obrot calej postaci w miejscu)."""
    ids = [sk.idx(n) for n in JOIN_BONES]
    b0 = sk.idx("BIP01")
    seq = list(frames[max(0, lo - 1):]) + [frames[-1]]
    W = [[M[:3, :3] for M in sk.compose(l, np.zeros(3))] for l, _ in seq]
    vel = [[math.degrees(1.0) * rotvec(W[k][i].T @ W[k + 1][i]) for i in ids] for k in range(len(seq) - 1)]
    speed = max(float(np.linalg.norm(v)) for vk in vel for v in vk)
    acc = max(float(np.linalg.norm(vel[k + 1][j] - vel[k][j])) for k in range(len(vel) - 1) for j in range(len(ids)))
    turn = max(math.degrees(float(np.linalg.norm(rotvec(W[k][b0].T @ W[k + 1][b0])))) for k in range(len(seq) - 1))
    return speed + acc + turn


def guard_recovery(sk, frames, target, n, overlap=GUARD_OVERLAP):
    """frames + n klatek do gardy target jak inertial_recovery, ale przejscie moze zaczac sie m <= overlap klatek przed
    koncem: ostatnie m klatek klipu zastepuje dluzsze przejscie (n + m klatek) - dlugosc animacji bez zmian (cancel i tor
    w ini te same). Ruchy, ciecia i kontry W3 koncza sie w postawie rp, a garda parady 2H jest lp: obrot o 90-166 st. w n
    klatkach krecil postacia w miejscu. m wybrane jak w best_join - po najmniejszym szarpnieciu (guard_blend_cost
    w tym samym oknie dla kazdego m; m = 0 to zwykle inertial_recovery)."""
    L = len(frames)
    lo = max(1, L - overlap - 2)
    best = None
    for m in range(0, max(0, min(overlap, L - 2)) + 1):
        full = inertial_recovery(frames[:L - m], target, n + m)
        cost = guard_blend_cost(sk, full, lo)
        if best is None or cost < best[0] - 1e-9:
            best = (cost, full)
    return best[1]


def build_counter(parry_dump, attack_dump, sk, grip, hit=None, start=None, step=None, entry=None):
    """-> (klatki z ruchem korzenia @25, indeks klatki ciosu, indeksy sklejen). Bez przejscia do postawy (dokleja write_move).
    hit/start: klatka uderzenia i poczatku drugiej czesci (domyslnie: ciecie wykryte po ostrzu, wejscie wybrane po
    najblizszej pozie i predkosci). entry: poza wyjsciowa (garda W3) - z niej wchodzi odbicie (COUNTER_ENTRY klatek).
    Sklejenia bezwladnosciowe (inertial_append): bez zamrozenia pozy i bez przeskokow."""
    import make_attacks
    pf, _ = make_attacks.prep_attack(parry_dump, sk, grip)
    af, ahit = make_attacks.prep_attack(attack_dump, sk, grip)
    if hit is not None:
        ahit = hit
    if start is None:   # riposta: wejscie w zamach nie pozniej niz COUNTER_WINDUP klatek przed ciosem
        s_range = (0, max(0, ahit - COUNTER_WINDUP))
    else:               # repel: klip zaczyna sie w pozie parady - wejscie blisko poczatku
        s_range = (start, min(start + 3, ahit - 6))
    e, s0 = best_join(sk, pf, af, COUNTER_PARRY_RANGE, s_range)
    if entry is not None:
        g_local, g_root = entry
        p0 = np.array(pf[0][1], dtype=float)
        out = [([R.copy() for R in g_local], np.array([p0[0], g_root[1], p0[2]]))]
        out = inertial_append(out, pf, 0, e, COUNTER_STEPS[0], COUNTER_ENTRY, xz_inertia=False)
    else:
        r0 = np.array(pf[0][1], dtype=float)
        out = [(l, np.array([r0[0] + (r[0] - r0[0]) * COUNTER_STEPS[0], r[1], r0[2] + (r[2] - r0[2]) * COUNTER_STEPS[0]]))
               for l, r in pf[:e + 1]]
    join = len(out)
    s1 = min(len(af) - 1, ahit + COUNTER_TAIL)
    out = inertial_append(out, af, s0, s1, COUNTER_STEPS[1] if step is None else step, COUNTER_BLEND)
    return out, join + (ahit - s0) - 1, [join]


REACH_BLADE = 0.5   # punkt kontaktu ostrza: polowa klingi (srodek zamachu, nie sam sztych)


def counter_reach(sk, motion, hit, contact):
    """Zasieg kontry (cm): jak daleko przed vobem (korzen w poziomie) jest punkt kontaktu w chwili ciosu - srodek
    klingi, bark / tulow albo stopa; maksimum z klatek hit-1..hit+1 (cios trwa chwile)."""
    names = {"body": ("BIP01 SPINE2", "BIP01 L CLAVICLE", "BIP01 R CLAVICLE", "BIP01 L UPPERARM", "BIP01 R UPPERARM"),
             "leg": ("BIP01 L FOOT", "BIP01 R FOOT", "BIP01 L TOE0", "BIP01 R TOE0")}
    gi = sk.idx("ZS_RIGHTHAND")
    blade = make_pair.BLADE_CM[0] + REACH_BLADE * (make_pair.BLADE_CM[1] - make_pair.BLADE_CM[0])
    best = 0.0
    for k in range(max(0, hit - 1), min(len(motion), hit + 2)):
        local, root = motion[k]
        W = sk.compose(local, root)
        if contact == "blade":
            pts = [W[gi][:3, 3] - W[gi][:3, 0] * blade]
        else:
            pts = [W[sk.idx(n)][:3, 3] for n in names[contact] if n in sk.names]
        best = max([best] + [float(p[2] - root[2]) for p in pts])
    return round(best, 1)


# Atak z wyskoku (JUMPATK_F 1H, JUMPATK2H_F 2H): Rend W3 (attack_heavy_special). W W3 to atak ladowany: _start =
# krok i ladowanie z mieczem nad glowa (2,1 s), _end = wypad z podskokiem i ciecie z gory (ok. 0,5 s do ciosu) i powrot.
# Bierzemy sam _end z wejsciem z gardy W3 (bez ladowania) i dokladamy wyskok korzenia (polsinus od odbicia do ladowania
# tuz przed ciosem) - vob zostaje na ziemi (plugin prowadzi go po torze z kolizja), w powietrzu jest tylko cialo.
# 1H z klipu _rp, 2H z _lp (postawa lp jak reszta stylu 2H) - osobne sekcje, bo tory sie roznia (1,8 / 2,2 m).
# Wejscie z gardy W3: 1H przejscie w ukladzie modelu (world_entry, 8 kl.) - garda 1H ma tulow obrocony o 107 st.
# wzgledem Rend i lokalne przejscie przerzucalo miecz nad glowa; 2H z gardy parady (miecz przed soba) tez world_entry
# (8 kl.; lokalnie w 5 kl. kosci obracaly sie do 80 st./kl.).
# Warianty (_F2, _F3 - plugin losuje przy kazdym uzyciu, z celem sposrod tych, ktorych wypad pasuje do odleglosci):
# ciecia W3 z wlasnym wyskokiem (lift=0, cios = najszybsze opadanie sztychu - hit="descent"). 1H lzejsze i szybsze:
# fast_7_rp (wyskok z obrotem i ciecie, cios 0,3 s), fast_far_forward_1_rp (dlugi wypad z wyskokiem i obrotem - na cel
# z daleka); 2H ciezsze: strong_10_lp (wyskok i ciecie znad glowy), strong_5_lp (obrot z mieczem przy ziemi, wyskok
# dolozony od klatki takeoff i ciezkie ciecie z gory; speed=0.9 - ciezej, dlon w ciosie ponizej 135 st./kl.).
# strong_9_lp (uderzenie w ziemie) odpada: miecz spada z gory do ziemi w jednej klatce (175 st./kl.) i laduje z boku
# tuz przy nogach.
JUMP_ATTACKS = {
    "JUMPATK_F": dict(weapon="1H", clip="man_geralt_sword_attack_heavy_special_rp_end", mode="world", n=8),
    "JUMPATK_F2": dict(weapon="1H", clip="man_geralt_sword_attack_fast_7_rp_40ms", mode="world", n=4, lift=0.0,
                       hit="descent", start=2),   # start=2: w klipie W3 glowa przeskakuje o 140 st. w 2. klatce
    "JUMPATK_F3": dict(weapon="1H", clip="man_geralt_sword_attack_fast_far_forward_1_rp_50ms", mode="inertial", n=5,
                       lift=0.0, hit="descent"),
    # world n=8: z gardy parady 2H (miecz przed soba) w Rend lokalnie w 5 kl. kosci obracaly sie do 80 st./kl.
    "JUMPATK2H_F": dict(weapon="2H", clip="man_geralt_sword_attack_heavy_special_lp_end", mode="world", n=8),
    # start=3, n=6: strong_10_lp zaczyna z mieczem za barkiem, a garda parady 2H trzyma go przed soba - wejscie od 1. klatki
    # przerzucalo sztych lukiem nad glowa i z powrotem (fala 113 -> 212 -> 149 cm); od 3. klatki miecz idzie z gardy
    # prosto w gore do ciecia
    "JUMPATK2H_F2": dict(weapon="2H", clip="man_geralt_sword_attack_strong_10_lp_70ms", mode="inertial", n=6, lift=0.0,
                         hit="descent", start=3),
    "JUMPATK2H_F3": dict(weapon="2H", clip="man_geralt_sword_attack_strong_5_lp_70ms", mode="inertial", n=5,
                         hit="descent", takeoff=9, speed=0.9),
}
JUMP_LIFT = 35.0     # cm - szczyt wyskoku korzenia ponad klip W3
JUMP_TAKEOFF = 3     # klatka @25 odbicia (przysiad klipu W3 konczy sie ok. 3. klatki)
JUMP_LAND = 1        # ladowanie tyle klatek przed ciosem (ciecie z ziemi, nie z powietrza)
JUMP_ENTRY = 5       # klatek wejscia z gardy W3 w klip (domyslnie)
JUMP_TAIL = 12       # klatek po ciosie (dalej przejscie do postawy)
JUMP_HIT_HEIGHT = 125.0   # cm - cios = pierwsza klatka po szczycie zamachu, w ktorej sztych schodzi ponizej (piers)


def continuous_rotvec(R, prev):
    """rotvec(R), a przy obrocie bliskim 180 st. ta z dwoch rownowaznych postaci (os * kat albo -os * (360 - kat)),
    ktora lezy blizej prev (wynik z poprzedniej klatki) - os nie przeskakuje na przeciwna miedzy klatkami (lewe ramie
    gardy W3 1H jest obrocone o ok. 180 st. wzgledem Rend: przejscie szarpalo nim dwa razy o 80-150 st. na klatke)."""
    a = math.acos(max(-1.0, min(1.0, (np.trace(R) - 1.0) / 2.0)))
    v = rotvec(R)
    nv = float(np.linalg.norm(v))
    if nv < 1e-9:
        return v
    v = v / nv * a
    if prev is not None:
        alt = v / a * (a - 2.0 * math.pi)
        if np.linalg.norm(alt - prev) < np.linalg.norm(v - prev):
            return alt
    return v


def world_entry(sk, entry, frames, s1, n):
    """Wejscie z pozy entry (lokalne obroty, korzen) w klip frames[0..s1]: przez n klatek poza przechodzi w klip
    w ukladzie modelu (obroty kosci w swiecie, nie lokalne; waga smoothstep - start i koniec bez skoku predkosci).
    Lokalne przejscie (inertial_append) przy tulowiu obroconym naraz z ramieniem prowadzi rece lukiem: garda W3 1H ->
    Rend (tulow 107 st., ramie 90 st.) przerzucala miecz nad glowa, choc w obu pozach jest nisko z tylu. Kosci o tym
    samym obrocie lokalnym w obu pozach (staly chwyt ZS_RIGHTHAND) zostaja bez zmian. Korzen: xz z klipu, roznica
    wysokosci wygasa razem z poza. -> lista (lokalne, korzen) dlugosci s1+1, klatka 0 = entry."""
    g_local, g_root = entry
    Gw = [M[:3, :3] for M in sk.compose(g_local, np.zeros(3))]
    dy = float(g_root[1]) - float(frames[0][1][1])
    prev = [None] * len(Gw)
    out = []
    for k in range(s1 + 1):
        local, root = frames[k]
        s = min(1.0, k / float(n))
        w = s * s * (3.0 - 2.0 * s)
        r = np.array([root[0], root[1] + dy * (1.0 - w), root[2]], dtype=float)
        if w <= 0.0 or w >= 1.0:
            out.append(([R.copy() for R in (g_local if w <= 0.0 else local)], r))
            continue
        Fw = [M[:3, :3] for M in sk.compose(local, np.zeros(3))]
        Bw = []
        for i, (G, F) in enumerate(zip(Gw, Fw)):
            v = continuous_rotvec(G.T @ F, prev[i])
            prev[i] = v
            Bw.append(G @ expmap(w * v))
        blended = []
        for i in range(len(local)):
            if np.allclose(g_local[i], local[i], atol=1e-4):
                blended.append(local[i].copy())
                Bw[i] = (Bw[sk.parent[i]] if sk.parent[i] >= 0 else np.eye(3)) @ local[i]
            else:
                blended.append(Bw[i] if sk.parent[i] < 0 else Bw[sk.parent[i]].T @ Bw[i])
        out.append((blended, r))
    return out


def jump_lift(n, takeoff, land, lift):
    """Uniesienie korzenia (cm) w klatkach 0..n-1: polsinus od odbicia do ladowania, poza tym 0."""
    out = [0.0] * n
    for k in range(takeoff + 1, min(land, n)):
        out[k] = lift * math.sin(math.pi * (k - takeoff) / float(land - takeoff))
    return out


def blade_tips(sk, frames):
    """Sztych (ostrze = -X ZS_RIGHTHAND, BLADE_CM[1] od dloni) w kolejnych klatkach (lokalne obroty, korzen)."""
    gi = sk.idx("ZS_RIGHTHAND")
    out = []
    for local, root in frames:
        W = sk.compose(local, root)
        out.append(W[gi][:3, 3] - W[gi][:3, 0] * make_pair.BLADE_CM[1])
    return out


STRIKE_FRONT = 50.0   # cm - sztych co najmniej tyle przed korzeniem, by ruch liczyl sie jako cios (nie zamach z boku)


def strike_index(sk, frames, lo=2):
    """Klatka ciosu: najszybszy ruch sztychu do nastepnej klatki, gdy sztych jest STRIKE_FRONT przed postacia."""
    tips = blade_tips(sk, frames)
    sp = [float(np.linalg.norm(tips[k + 1] - tips[k])) if k >= lo and tips[k][2] - frames[k][1][2] > STRIKE_FRONT else 0.0
          for k in range(len(frames) - 1)]
    return int(np.argmax(sp))


def build_jump_attack(dump, sk, grip, entry=None, lift=JUMP_LIFT, mode="inertial", n=JUMP_ENTRY, hit_mode="chop", start=0,
                      takeoff=JUMP_TAKEOFF, speed=1.0):
    """-> (klatki @25 z ruchem korzenia, indeks klatki ciosu, indeks szczytu wyskoku). Bez przejscia do postawy
    (dokleja write_move). Cios (hit_mode): "chop" = pierwsza klatka po szczycie zamachu nad glowa, w ktorej sztych
    (ostrze = -X ZS_RIGHTHAND) schodzi ponizej JUMP_HIT_HEIGHT - ciecie z gory na wysokosci piersi (Rend); "descent" =
    klatka najszybszego opadania sztychu przed postacia po szczycie wyskoku klipu (ciecia W3 z wyskokiem).
    entry = garda W3, wejscie w n klatek: mode "world" (world_entry) albo "inertial" (inertial_append). Klip od klatki
    start (@25), tempo x speed, dolozony wyskok (lift cm) od klatki takeoff do ladowania tuz przed ciosem.
    Szczyt = najwyzej uniesiony korzen."""
    d = to_entity_start(dump)
    af = retarget.resample(retarget.retarget(d, sk, grip, False, "w3", weapon="r_weapon"), d["fps"] * speed)[start:]
    tip3 = blade_tips(sk, af)
    tips = [float(t[1]) for t in tip3]
    if hit_mode == "descent":   # po szczycie wyskoku klipu (fast_far_forward tnie tez wczesniej, w doskoku)
        apex = max(2, int(np.argmax([r[1] for _, r in af])))
        drop = [tips[k - 1] - tips[k] if k > apex and tip3[k][2] - af[k][1][2] > 0.0 else -1e9 for k in range(len(af))]
        hit = int(np.argmax(drop))
    else:
        top = int(np.argmax(tips))
        hit = next((k for k in range(top + 1, len(af)) if tips[k] < JUMP_HIT_HEIGHT), min(top + 2, len(af) - 2))
    hit = min(hit, len(af) - 2)
    s1 = min(len(af) - 1, hit + JUMP_TAIL)
    if entry is not None and mode == "world":
        out = world_entry(sk, entry, af, s1, n)
    elif entry is not None:
        g_local, g_root = entry
        p0 = np.array(af[0][1], dtype=float)
        first = [([R.copy() for R in g_local], np.array([p0[0], g_root[1], p0[2]]))]
        out = inertial_append(first, af, 0, s1, 1.0, n, xz_inertia=False)
    else:
        out = [(l, np.array(r, dtype=float)) for l, r in af[:s1 + 1]]
    land = hit - JUMP_LAND
    up = jump_lift(len(out), takeoff, land, lift)
    out = [(l, np.array([r[0], r[1] + u, r[2]])) for (l, r), u in zip(out, up)]
    peak = int(np.argmax([r[1] for _, r in out[:hit + 1]]))
    return out, hit, peak


def build_jump_variant(dump, sk, grip, entry, c):
    """Atak z wyskoku wg wpisu JUMP_ATTACKS (lift, hit, mode, n, start, takeoff, speed) -> jak build_jump_attack."""
    return build_jump_attack(dump, sk, grip, entry=entry, lift=c.get("lift", JUMP_LIFT), mode=c.get("mode", "inertial"),
                             n=c.get("n", JUMP_ENTRY), hit_mode=c.get("hit", "chop"), start=c.get("start", 0),
                             takeoff=c.get("takeoff", JUMP_TAKEOFF), speed=c.get("speed", 1.0))


# Atak w biegu (RUNATK_F 1H, RUNATK2H_F 2H): plugin przejmuje atak w biegu gry (EV_ATTACKRUN) w stylu W3 i gra caly
# ruch W3 - cialo z nogami, wypad po torze z kolizja, cios w chwili hit= - zamiast T_xHATTACKMOVE (sama gorna polowa nad
# biegnacymi nogami Gothica). Ciecia W3 z wypadu to piruety z biodrami: tulow wyciety na prosto biegnace nogi albo
# skrecal sie o 400+ st., albo po przycieciu zostawal ten sam plaski zamach w lewo dla 1H i 2H.
# Wejscie z pozy truchtu W3 (S_xHRUNL stylu W3, make_attacks.run_pose), koniec w gardzie W3 (overlayW3). Na bron wariant
# z daleka (_F) i z bliska (_F2) - plugin bierze ten, ktorego wypad do ciosu pasuje do odleglosci celu (bez slizgu nog).
# 1H szybkie: approach_attack_1 (ciecie Geralta z rozbiegu z obrotem - klip zaczyna sie w biegu, start=1 pomija
# skok toru w 1. klatce), fast_8_rp (obrot i szybkie ciecie, 0,4 s). 2H ciezkie: strong_far_forward_1_lp (doskok,
# wyskok i ciezkie ciecie z obrotem; start=2 bez stania na poczatku), strong_4_lp (obrot i ciecie znad glowy).
RUN_ATTACKS = {
    "RUNATK_F": dict(weapon="1H", clip="man_geralt_sword_approach_attack_1", start=1),
    "RUNATK_F2": dict(weapon="1H", clip="man_geralt_sword_attack_fast_8_rp_40ms"),
    "RUNATK2H_F": dict(weapon="2H", clip="man_geralt_sword_attack_strong_far_forward_1_lp_80ms", start=2),
    "RUNATK2H_F2": dict(weapon="2H", clip="man_geralt_sword_attack_strong_4_lp_70ms"),
}
RUN_ENTRY = 5    # klatek wejscia z pozy truchtu W3 w klip (bezwladnosciowo)
RUN_TAIL = 10    # klatek po ciosie (dalej przejscie do gardy)


def build_run_attack(dump, sk, grip, entry, start=0, speed=1.0, n=RUN_ENTRY):
    """-> (klatki @25 z ruchem korzenia, indeks klatki ciosu). Klip od klatki start (@25), tempo x speed, wejscie z pozy
    entry (lokalne obroty, korzen) w n klatek bez zatrzymania ruchu korzenia; cios = strike_index. Bez przejscia do
    postawy (dokleja write_move)."""
    d = to_entity_start(dump)
    af = retarget.resample(retarget.retarget(d, sk, grip, False, "w3", weapon="r_weapon"), d["fps"] * speed)[start:]
    hit = strike_index(sk, af)
    s1 = min(len(af) - 1, hit + RUN_TAIL)
    g_local, g_root = entry
    p0 = np.array(af[0][1], dtype=float)
    out = [([R.copy() for R in g_local], np.array([p0[0], g_root[1], p0[2]]))]
    return inertial_append(out, af, 0, s1, 1.0, n, xz_inertia=False), hit


def build_run_variant(dump, sk, grip, entry, c):
    """Atak w biegu wg wpisu RUN_ATTACKS (start, speed) -> jak build_run_attack."""
    return build_run_attack(dump, sk, grip, entry, start=c.get("start", 0), speed=c.get("speed", 1.0))


def feet_up(motion, sk, lift=FEET_UP):
    """Klatki ze stopami w gorze (skok, przewrot): najnizszy punkt stop (FOOT, TOE0) wyzej niz 'lift' cm nad najnizszym
    w calym klipie (ziemia klipu). motion: (lokalne obroty, korzen)."""
    idx = [sk.idx(n) for n in ("BIP01 L FOOT", "BIP01 R FOOT", "BIP01 L TOE0", "BIP01 R TOE0")]
    low = []
    for local, root in motion:
        W = sk.compose(local, root)
        low.append(min(float(W[i][1, 3]) for i in idx))
    ground = min(low)
    return [h - ground > lift for h in low]


def air_compress(motion, sk, scale, lift=FEET_UP):
    """Ruch korzenia w poziomie skrocony (x scale) tylko miedzy klatkami ze stopami w gorze (feet_up w k-1 i k - klatka
    odbicia i ladowania zostaje pelna) - w locie i w przewrocie nic nie stoi na ziemi, wiec krotszy tor nie slizga stop;
    kroki na ziemi bez zmian. Wysokosc korzenia i obroty kosci bez zmian. -> klatki (lokalne, korzen) z nowym torem."""
    up = feet_up(motion, sk, lift)
    out = [(motion[0][0], np.array(motion[0][1], dtype=float))]
    x, z = float(motion[0][1][0]), float(motion[0][1][2])
    for k in range(1, len(motion)):
        a, b = motion[k - 1][1], motion[k][1]
        f = scale if up[k] and up[k - 1] else 1.0
        x += (float(b[0]) - float(a[0])) * f
        z += (float(b[2]) - float(a[2])) * f
        out.append((motion[k][0], np.array([x, float(b[1]), z])))
    return out


def build_move_full(dump, sk, grip, scale, iframe_share, yaw=0.0, cancel_share=0.9, air=1.0, length=None):
    """-> (klatki w miejscu @25, klatki z ruchem korzenia, tor x,z w cm, okno nietykalnosci, koniec ruchu w s).
    Ogon klipu W3 (powrot do postawy W3) uciety tam, gdzie wraca sterowanie - dalej przejscie do postawy Gothica.
    scale 1.0 = tor dokladnie pod stopami (bez slizgu); air < 1 = krotszy tor tylko ze stopami w gorze (air_compress).
    length (s, z przejsciem do postawy): ruch dluzszy przyspieszony do tej dlugosci w miejscach wolniejszych niz
    najszybsze miejsce klipu (retime) - ta sama droga i te same klatki, krocej; nietykalnosc liczona po udziale drogi."""
    dump = moved(to_entity_start(dump), rot_z(yaw))
    frames = retarget.resample(retarget.retarget(dump, sk, grip, False, "w3", weapon="r_weapon", wrist_limit=75.0), dump["fps"])
    if air != 1.0:
        frames = air_compress(frames, sk, air)
    base = np.array([frames[0][1][0], 0.0, frames[0][1][2]])
    traj_of = lambda fr: [((r[0] - base[0]) * scale, (r[2] - base[2]) * scale) for _, r in fr]
    traj = traj_of(frames)
    fps = retarget.FPS_OUT
    dist = [math.hypot(x, z) for x, z in traj]
    total = max(dist[-1], 1e-3)
    k_share = lambda s, dl=dist: next(k for k, d in enumerate(dl) if d >= s * total)
    cut = min(len(frames), k_share(cancel_share) + int(0.1 * fps) + 1)
    frames = frames[:cut]
    if length is not None:
        n_out = int(round(length * fps)) - REC_FRAMES
        if n_out < len(frames):
            frames = retime(frames, sk, n_out, start=RETIME_START)
    traj = traj_of(frames)
    dist = [math.hypot(x, z) for x, z in traj]
    iframes = (0.03, round(k_share(iframe_share, dist) / fps, 2))
    cancel = round((len(frames) + REC_FRAMES) / fps, 2)
    return make_pair.inplace(frames), frames, traj, iframes, cancel


# ---------- Krotsze uniki i przewroty (decyzja autora 17.10) ----------
# Dlugosc z przejsciem do postawy: unik ok. 0,85-0,9 s (bylo do 1,2 s), przewrot ok. 1,0 s (bylo 1,44 s), przewrot w tyl
# (salto 5,2 m) 1,68 -> 1,28 s. Droga ta sama: retime przyspiesza wolne fazy (wstawanie, dreptanie do
# gardy) najwyzej do tempa najszybszego miejsca samego klipu - kosci i korzen nie szybsze niz w W3, bez przeskokow pozy.
# DODGE_F 0,92: unik w przod obraca cialo do 48 st./kl., przy 0,88 s koniec w gardzie parady 2H krecil sie ponad 30 st./kl.
# ROLL_B 1,28 (bylo 1,68 - tez 0,4 s krocej): salto wstajac obraca cialo przodem do wroga; przy 1,12 s ten obrot szedl
# 35-45 st./kl. tuz przed garda parady 2H (wyraznie krecenie w miejscu)
EVADE_LENGTH = {"DODGE": 0.88, "DODGE_F": 0.92, "ROLL": 1.0, "ROLL_B": 1.28}
RETIME_START = 2     # pierwsze klatki (wybicie z postawy) bez zmian
RETIME_SMOOTH = 2    # tempo wygladzone (srednia w skali log) na +-tyle odcinkow - bez szarpniecia przy zmianie tempa
RETIME_MAX = 3.0     # najwyzej tyle razy szybciej w jednym miejscu
# Okno wyjscia (exit= w ini): od tej chwili atak / blok / kolejny unik z bufora przerywa ruch. Po nietykalnosci, po
# glownej drodze (udzial) i ze stopa na ziemi (najnizszy punkt stop nizej niz EXIT_FOOT nad ziemia modelu).
EXIT_SHARE = {"DODGE": 0.75, "ROLL": 0.85}
EXIT_FOOT = 6.0


def evade_length(mid):
    """Docelowa dlugosc ruchu (s) albo None (bez przycinania)."""
    if mid in EVADE_LENGTH:
        return EVADE_LENGTH[mid]
    return EVADE_LENGTH.get(mid.split("_", 1)[0])


def exit_share(mid):
    return EXIT_SHARE.get(mid.split("_", 1)[0], 0.75)


def _sample_motion(motion, quats, s):
    """Klatka w chwili s (indeks ulamkowy): slerp obrotow, liniowo korzen."""
    n = len(motion)
    a = min(int(math.floor(s)), n - 1)
    b = min(a + 1, n - 1)
    u = s - a
    if u < 1e-9 or a == b:
        return ([R.copy() for R in motion[a][0]], np.array(motion[a][1], dtype=float))
    local = [retarget.qmat(*retarget.slerp(qa, qb, u)) for qa, qb in zip(quats[a], quats[b])]
    root = np.array(motion[a][1], dtype=float) * (1 - u) + np.array(motion[b][1], dtype=float) * u
    return local, root


def motion_speeds(motion, sk):
    """Na odcinek miedzy klatkami: (najszybsza kosc JOIN_BONES w ukladzie modelu, st./kl.; krok korzenia w poziomie, cm)."""
    ids = [sk.idx(n) for n in JOIN_BONES]
    W = [[M[:3, :3] for M in sk.compose(l, np.zeros(3))] for l, _ in motion]
    w, v = [], []
    for k in range(len(motion) - 1):
        w.append(max(math.degrees(float(np.linalg.norm(rotvec(W[k][i].T @ W[k + 1][i])))) for i in ids))
        a, b = motion[k][1], motion[k + 1][1]
        v.append(math.hypot(float(b[0]) - float(a[0]), float(b[2]) - float(a[2])))
    return w, v


def retime(motion, sk, n_out, start=RETIME_START, rmax=RETIME_MAX, smooth=RETIME_SMOOTH):
    """Ten sam ruch (klatki z korzeniem) w n_out klatkach: odcinki od 'start' przyspieszone tak, by najszybsza kosc
    i korzen doszly najwyzej do c x tempa najszybszego odcinka klipu (c dobrane bisekcja do dlugosci; szybkie odcinki
    zostaja w tempie 1). Tempo wygladzone, czasy przeskalowane do dokladnej dlugosci. Pierwsza i ostatnia klatka bez
    zmian (ta sama droga), odcinki przed 'start' tez. n_out >= len(motion) -> kopia."""
    n = len(motion)
    if n_out >= n or n < 3:
        return [([R.copy() for R in l], np.array(r, dtype=float)) for l, r in motion]
    start = max(0, min(start, n - 2))
    w, v = motion_speeds(motion, sk)
    wp, vp = max(max(w), 1e-6), max(max(v), 1e-6)

    def rates(c):
        r = []
        for i in range(n - 1):
            if i < start:
                r.append(1.0)
                continue
            cap = min(c * wp / max(w[i], 1e-6), c * vp / max(v[i], 1e-6))
            r.append(min(rmax, max(1.0, cap)))
        # wygladzenie w skali log (tylko odcinki od 'start'; poczatek trzyma tempo 1)
        lg = [math.log(x) for x in r]
        out = []
        for i in range(n - 1):
            if i < start:
                out.append(1.0)
                continue
            win = [lg[j] for j in range(max(0, i - smooth), min(n - 1, i + smooth + 1))]
            out.append(math.exp(sum(win) / len(win)))
        return out

    want = float(n_out - 1)
    lo, hi = 0.0, 1.0
    if sum(1.0 / x for x in rates(hi)) > want:
        hi = 4.0   # w granicy tempa klipu nie starcza - dalej najwyzej rmax (test pilnuje predkosci)
    for _ in range(50):
        mid = (lo + hi) / 2
        if sum(1.0 / x for x in rates(mid)) > want:
            lo = mid
        else:
            hi = mid
    d = [1.0 / x for x in rates(hi)]
    fixed = float(start)
    rest = sum(d[start:])
    k = (want - fixed) / rest if rest > 1e-9 else 1.0
    d = [x if i < start else x * k for i, x in enumerate(d)]
    T = [0.0]
    for x in d:
        T.append(T[-1] + x)
    quats = [[retarget.quat_from_mat(R) for R in l] for l, _ in motion]
    out, i = [], 0
    for j in range(n_out):
        t = float(j)
        while i < n - 2 and T[i + 1] < t - 1e-9:
            i += 1
        s = i + min(1.0, max(0.0, (t - T[i]) / d[i])) if d[i] > 1e-9 else float(i)
        out.append(_sample_motion(motion, quats, min(s, n - 1.0)))
    out[-1] = ([R.copy() for R in motion[-1][0]], np.array(motion[-1][1], dtype=float))
    return out


def foot_down(frame, sk, tol=EXIT_FOOT):
    """Stopa na ziemi: najnizszy punkt stop (FOOT, TOE0) nizej niz tol nad ziemia modelu (y = 0)."""
    local, root = frame
    W = sk.compose(local, root)
    return min(float(W[sk.idx(n)][1, 3]) for n in ("BIP01 L FOOT", "BIP01 R FOOT", "BIP01 L TOE0", "BIP01 R TOE0")) < tol


def evade_exit(motion, traj, iframes, sk, share):
    """Okno wyjscia (s): pierwsza klatka po nietykalnosci, po 'share' drogi i ze stopa na ziemi; bez takiej - ostatnia."""
    fps = retarget.FPS_OUT
    dist = [math.hypot(x, z) for x, z in traj]
    total = max(dist[-1], 1e-3)
    k0 = int(math.ceil(iframes[1] * fps - 1e-6))
    for k in range(max(0, k0), len(motion)):
        if dist[k] >= share * total - 1e-6 and foot_down(motion[k], sk):
            return round(k / fps, 2)
    return round((len(motion) - 1) / fps, 2)


def build_move(dump, sk, grip, scale, iframe_share, yaw=0.0, cancel_share=0.9):
    """-> (klatki w miejscu @25, tor x,z w cm, okno nietykalnosci, czas konca ruchu)."""
    inplace, _, traj, iframes, cancel = build_move_full(dump, sk, grip, scale, iframe_share, yaw, cancel_share)
    return inplace, traj, iframes, cancel


ROLL_IFRAMES, ROLL_CANCEL = 0.7, 0.95   # przewroty: nietykalnosc do 70% drogi, koniec ruchu przy 95%


def build_roll_back(dump, sk, grip, scale=1.0, air=ROLL_B_AIR, length=None):
    """Przewrot w tyl z klipu ROLL_B_CLIP (bez obrotu toru - klip sam idzie w tyl i konczy przodem) -> jak
    build_move_full."""
    return build_move_full(dump, sk, grip, scale, ROLL_IFRAMES, 0.0, ROLL_CANCEL, air=air, length=length)


def write_move(vfs, sk, mid, frames, motion, traj, iframes, cancel, source, kind, hit=None, per_ani=None, reach=None,
               w3_targets=None, exit=None):
    """Nakladka ruchu: osobny MAN na bron (przejscie do innej postawy), kroki i dzwiek ruchu, sekcja ini.
    per_ani: {animacja: klatki w miejscu} - wariant na bron (kontra wchodzi z gardy stylu W3 1H / 2H); kontry koncza
    sie przejsciem bezwladnosciowym (inertial_recovery), reszta jak dotad (recovery.append_recovery).
    w3_targets: {animacja: poza gardy W3} - druga nakladka <nazwa>_W3 z koncem w gardzie stylu W3 (overlayW3= w ini;
    plugin bierze ja, gdy postawa bohatera pochodzi z nakladki HUMANS_ZDF_W3_*) - bez przeskoku garda Gothica -> W3."""
    name = "HUMANS_ZDF_MV_%s" % mid
    sfx, ground = sfx_events.motion_sounds(motion, sk, fall=(kind == "ROLL"))
    if kind == "DODGE":
        sfx.append((2, "D_PARADE"))
    if kind == "COUNTER":
        sfx += [(2, "D_PARADE"), (max(1, hit - 1), "WHOOSH")]
    if kind in ("JUMPATK", "RUNATK"):
        sfx += [(max(1, hit - 1), "WHOOSH")]

    def variant(ovl, targets):
        chunks = []
        for ani, stance in ANIS:
            src = (per_ani or {}).get(ani, frames)
            target = targets.get(ani) or recovery.stance_pose(vfs, sk, stance)
            if ani in targets and ani in GUARD_BLEND_ANIS:   # garda parady 2H: obrot do niej rozlozony (guard_recovery)
                rec = guard_recovery(sk, src, target, REC_FRAMES)
            elif kind in ("COUNTER", "JUMPATK", "RUNATK"):
                rec = inertial_recovery(src, target, REC_FRAMES)
            else:
                rec = recovery.append_recovery(src, target, REC_FRAMES)
            nf = make_pair.write_man(vfs, sk, rec, ani, os.path.join(COMPILED, "%s-%s.MAN" % (ovl, ani)))
            chunks.append(msblib.ani_chunk(ani, 1, stance, 0.1, 0.1, "M.", "ZDISM_W3.ASC", 1, nf)
                          + sfx_events.events_block(sfx=sfx, ground=ground))
        build_overlay.write_overlay(ovl, COMPILED, chunks, vfs)

    variant(name, {})
    if w3_targets:
        variant(name + "_W3", w3_targets)
    traj = list(traj) + [traj[-1]] * REC_FRAMES
    return (mid, [
        "overlay=%s" % name] + (["overlayW3=%s_W3" % name] if w3_targets else []) + [
        "trajFps=%g" % retarget.FPS_OUT,
        "traj=%s" % " ".join("%.1f,%.1f" % xz for xz in traj),
        "iframes=%g,%g" % iframes, "cancel=%g" % cancel,
    ] + (["hit=%g" % round(hit / retarget.FPS_OUT, 2)] if hit is not None else []) + (
        ["reach=%g" % reach] if reach is not None else []) + (
        ["exit=%g" % exit] if exit is not None else []) + [
        "; zrodlo W3: %s, %d kl. @25 (+%d przejscia), droga %.0f cm, kroki %d" % (
            source, len(frames), REC_FRAMES, math.hypot(*traj[-1]), len(ground)),
    ])


def lunge_section(vfs, sk, mid, motion, hit, clip, kind, w3_targets):
    """Atak z wypadem (z wyskoku / w biegu): nakladka w miejscu, tor z ruchu korzenia, hit=, reach= ostrza, bez
    nietykalnosci (Rend i ataki z rozbiegu w W3 tez jej nie maja) -> sekcja ini."""
    frames = make_pair.inplace(motion)
    base = motion[0][1]
    traj = [(r[0] - base[0], r[2] - base[2]) for _, r in motion]
    cancel = round((len(frames) + REC_FRAMES) / retarget.FPS_OUT, 2)
    reach = counter_reach(sk, motion, hit, "blade")
    return write_move(vfs, sk, mid, frames, motion, traj, (0.0, 0.0), cancel, clip, kind, hit, reach=reach,
                      w3_targets=w3_targets)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dodge-scale", type=float, default=1.0, help="1.0 = tor pod stopami (mniej = krocej, ale slizg)")
    ap.add_argument("--roll-scale", type=float, default=1.0)
    ap.add_argument("--roll-b-air", type=float, default=ROLL_B_AIR, help="przewrot w tyl: skrocenie fazy ze stopami w gorze")
    a = ap.parse_args()
    vfs = build_overlay.game_vfs()
    sk = retarget.GothicSkeleton(vfs)
    grip = make_pair.grip_local(vfs, sk)
    os.makedirs(WORK, exist_ok=True)
    os.makedirs(COMPILED, exist_ok=True)
    dump = lambda c: make_pair.dump_clip(SWORD_SET, c, os.path.join(WORK, c + ".json"))

    import make_attacks
    # garda stylu W3 (S_1HRUN / S_2HRUN nakladek HUMANS_ZDF_W3_1H / _2H; 2H = garda parady parry_idle_lp) - wejscie kontr
    # i atakow z wyskoku, koniec wariantow _W3: kazda bron ma w nakladce _W3 swoja animacje (T_1H/T_2HPARADEJUMPB), wiec
    # 2H konczy w swojej gardzie
    guards = {w: make_attacks.style_guard(dump(make_attacks.guard_clip(w)), sk, grip, w)[0] for w in ("1H", "2H")}
    w3_targets = {"T_1HPARADEJUMPB": guards["1H"], "T_2HPARADEJUMPB": guards["2H"]}
    sections = []
    for d, clip in DODGES.items():
        mid = "DODGE_" + d
        frames, motion, traj, iframes, cancel = build_move_full(dump(clip), sk, grip, a.dodge_scale, 0.5,
                                                                length=evade_length(mid))
        sections.append(write_move(vfs, sk, mid, frames, motion, traj, iframes, cancel, clip, "DODGE",
                                   w3_targets=w3_targets, exit=evade_exit(motion, traj, iframes, sk, exit_share(mid))))
    roll = concat([dump(c) for c in ROLL])
    for d, yaw in ROLL_YAW.items():
        mid = "ROLL_" + d
        frames, motion, traj, iframes, cancel = build_move_full(roll, sk, grip, a.roll_scale, ROLL_IFRAMES, yaw, ROLL_CANCEL,
                                                                length=evade_length(mid))
        sections.append(write_move(vfs, sk, mid, frames, motion, traj, iframes, cancel, " + ".join(ROLL), "ROLL",
                                   w3_targets=w3_targets, exit=evade_exit(motion, traj, iframes, sk, exit_share(mid))))
    frames, motion, traj, iframes, cancel = build_roll_back(dump(ROLL_B_CLIP), sk, grip, a.roll_scale, a.roll_b_air,
                                                            length=evade_length("ROLL_B"))
    sections.append(write_move(vfs, sk, "ROLL_B", frames, motion, traj, iframes, cancel, ROLL_B_CLIP, "ROLL",
                               w3_targets=w3_targets, exit=evade_exit(motion, traj, iframes, sk, exit_share("ROLL_B"))))

    for mid, c in COUNTERS.items():
        pc, ac = c["parry"], c["attack"]
        per_ani, motion, hit = {}, None, None
        for ani, w in (("T_1HPARADEJUMPB", "1H"), ("T_2HPARADEJUMPB", "2H")):
            m, h, _ = build_counter(dump(pc), dump(ac), sk, grip, c.get("hit"), c.get("start"), c.get("step"), entry=guards[w])
            per_ani[ani] = make_pair.inplace(m)
            if motion is None:
                motion, hit = m, h
        frames = per_ani["T_1HPARADEJUMPB"]   # piesci (bez kontr) jak 1H
        base = motion[0][1]
        traj = [(r[0] - base[0], r[2] - base[2]) for _, r in motion]
        fps = retarget.FPS_OUT
        iframes = (0.03, round(hit / fps, 2))
        cancel = round((len(frames) + REC_FRAMES) / fps, 2)
        reach = counter_reach(sk, motion, hit, c["contact"])
        sections.append(write_move(vfs, sk, mid, frames, motion, traj, iframes, cancel, pc + " + " + ac, "COUNTER", hit,
                                   per_ani=per_ani, reach=reach, w3_targets=w3_targets))

    # atak z wyskoku: osobne sekcje na bron i wariant (1H _rp / 2H _lp), wszystkie animacje nakladki z tego samego klipu
    for mid, c in JUMP_ATTACKS.items():
        motion, hit, _ = build_jump_variant(dump(c["clip"]), sk, grip, guards[c["weapon"]], c)
        sections.append(lunge_section(vfs, sk, mid, motion, hit, c["clip"], "JUMPATK", w3_targets))
    # atak w biegu: wejscie z pozy truchtu W3 (w stylu W3 bohater biegnie truchtem W3), bez nietykalnosci
    run = make_attacks.run_pose(make_pair.dump_clip(make_attacks.LOCO_SET, make_attacks.RUN_POSE_CLIP, os.path.join(
        make_attacks.LOCO_WORK, make_attacks.RUN_POSE_CLIP + ".json")), sk, grip)
    for mid, c in RUN_ATTACKS.items():
        motion, hit = build_run_variant(dump(c["clip"]), sk, grip, run, c)
        sections.append(lunge_section(vfs, sk, mid, motion, hit, c["clip"], "RUNATK", w3_targets))
    os.makedirs(os.path.dirname(INI), exist_ok=True)
    text = ("; Uniki i przewroty z Wiedzmina 3 - TYLKO UZYTEK PRYWATNY (animacje CD Projekt RED), nie do wydania.\n"
            "; Generowane przez anims\\pipeline\\make_moves.py. traj: pary x,z (cm, x w prawo, z do przodu) co 1/trajFps s;\n"
            "; iframes: nietykalnosc od,do (s); cancel: koniec ruchu z przejsciem do postawy (s); exit: od tej chwili (s)\n"
            "; atak / blok / kolejny unik z bufora przerywa unik / przewrot (stopa na ziemi po glownej drodze).\n")
    for mid, lines in sections:
        text += "\n[%s]\n%s\n" % (mid, "\n".join(lines))
    open(INI, "w", encoding="ascii", newline="\n").write(text)
    for mid, lines in sections:
        print(mid, " ".join(x for x in lines if x.startswith(("iframes", "cancel", "hit", "reach", "exit", "overlayW3"))))


if __name__ == "__main__":
    main()
