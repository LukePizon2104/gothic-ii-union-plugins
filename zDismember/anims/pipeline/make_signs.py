r"""Gesty Znakow Geralta (Wiedzmin 3) dla zQuickSign - TYLKO UZYTEK PRYWATNY (animacje CD Projekt RED).

Kazdy gest (5 Znakow, z mieczem i bez broni) ma dwa warianty, oba na wlasnej warstwie LAYER (nizej uzasadnienie):
- postoj (SIGN_ANIS_STAND, nakladka HUMANS_ZDF_SIGNS_STAND): caly Geralt z W3 - korzen, miednica, nogi, tulow i rece -
  w miejscu: korzen bez ruchu w poziomie, stopy stoja tam, gdzie na poczatku gestu (W3 robi wypad o 30-90 cm - po samym
  wyzerowaniu korzenia stopy slizgalyby sie o 30-55 cm), kolana prowadzi IK dwoch kosci w plaszczyznie kolana z W3.
  Postawa pod spodem nie ma znaczenia, wiec dlon idzie do przodu takze z gardy W3 (miednica bokiem: 77 st. w 1H,
  -54 st. w 2H), z postaw zEBA (do 57 st., piesci HUMANS_REMASTER) i natywnych.
- ruch (SIGN_ANIS, nakladka HUMANS_ZDF_SIGNS): tylko tulow i rece (od BIP01 SPINE w dol), nogi dalej biegna. Tulow
  ustawiony wzgledem miednicy biegu (srednia z petli S_RUNL; w biegu wszystkich stylow - natywnych, zEBA, W3 - miednica
  patrzy do przodu, srednio +-3 st.) tak, jak w W3 wzgledem kierunku postaci.
Oba warianty powstaja z tych samych klatek W3: ta sama dlugosc i chwila pchniecia dloni - zQuickSign gra wariant postoju,
gdy bohater stoi, a gdy ruszy w trakcie gestu, przelacza na wariant ruchu od tej samej klatki.

Nazwy (nakladka moze tylko podmienic istniejaca nazwe z HUMANS): 18 losowych animacji przy meblach i jedzeniu (cale
HUMANS) i 2 gesty strazy z Gothica 1 (T_LGUARD_GREET, T_HGUARD_GREET) - wszystkie gra tylko AI postaci NPC (stany ZS_*
w skryptach), nigdy sterowany bohater. Obie nakladki zQuickSign zaklada raz, gdy bohater stoi, i nie zdejmuje: zalozenie
i zdjecie nakladki zatrzymuje w silniku wszystkie animacje postaci (warstwy 0..99999), co w biegu albo skoku hamowalo
bohatera.

Tryb alternatywny (23.10, przytrzymanie F dluzej niz SignAltHold w zDismember): Aard (fala dookola) i Yrden (pulapka) maja
gest "w ziemie" w miejscu (SIGN_ANIS_ALT, w nakladce postoju - caly Geralt, stopy stoja): od zamachu przez dotkniecie
ziemi do wstania, wariant broni wedlug postawy (bez broni / miecz rp - garda 1H, sztylet / miecz lp - garda 2H). Na czas
gestu bohatera trzyma w miejscu zDismember (blokada ruchu), skutek w chwili dotkniecia ziemi (ALT_BEFORE klatek od startu
- SignAltCastMs w zQuickSign). Pozostale Znaki w trybie alternatywnym graja gest trybu zwyklego.
Yrden w trybie zwyklym w ruchu (tylko tulow i rece nad biegnacymi nogami): tulow pochylony do przodu tyle, ile W3 kuca
(GROUND_SIGNS) - dlon schodzi w strone ziemi, a nie na wysokosc pasa (wygladalo jak reka do przodu).

Uzycie: py -3.12 -I make_signs.py   (wypisuje chwile pchniecia dloni - SignCastMs w zQuickSign.ini - i dotkniecia
ziemi w trybie alternatywnym - SignAltCastMs / SignAltGestureMs)
"""
import math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import manlib, msblib
import retarget, build_overlay, sfx_events, make_pair, make_moves
from retarget import qmat

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(ROOT, "anims", "work", "w3signs")
COMPILED = make_moves.COMPILED
SET = chr(92).join(["animations", "man", "combat", "man_geralt_signs.w2anims"])
# znak -> (bez broni, z mieczem w prawej rece)
SIGNS = {
    "AARD": ("man_geralt_aard_front", "man_ger_sword_aard_front_rp_sword"),
    "IGNI": ("man_geralt_ignii_front", "man_ger_sword_ignii_front_rp_sword"),
    "QUEN": ("man_geralt_quen_front", "man_ger_sword_quen_front_rp_sword"),
    "YRDEN": ("man_geralt_yrden_ground", "man_ger_sword_yrden_ground_rp_sword"),
    "AXII": ("man_geralt_axii_release", "man_ger_sword_axii_release_rp_sword"),
}
OVERLAY = "HUMANS_ZDF_SIGNS"              # ruch: tulow i rece
OVERLAY_STAND = "HUMANS_ZDF_SIGNS_STAND"  # postoj: cale cialo w miejscu
# (znak, z mieczem) -> nazwa animacji; te same tablice sa w zQuickSign.cpp (SIGN_ANIS, SIGN_ANIS_STAND)
SIGN_ANIS = {
    ("AARD", False): "R_CHAIR_RANDOM_1", ("AARD", True): "R_CHAIR_RANDOM_2",
    ("IGNI", False): "R_CHAIR_RANDOM_3", ("IGNI", True): "R_CHAIR_RANDOM_4",
    ("QUEN", False): "T_BAUMSAEGE_RANDOM_1", ("QUEN", True): "T_REPAIR_RANDOM_1",
    ("YRDEN", False): "T_STOVE_RANDOM_1", ("YRDEN", True): "T_FOODHUGE_RANDOM_1",
    ("AXII", False): "T_RICE_RANDOM_1", ("AXII", True): "T_RICE_RANDOM_2",
}
SIGN_ANIS_STAND = {
    ("AARD", False): "T_FOOD_RANDOM_1", ("AARD", True): "T_FOOD_RANDOM_2",
    ("IGNI", False): "T_MEAT_RANDOM_1", ("IGNI", True): "T_JOINT_RANDOM_1",
    ("QUEN", False): "T_POTION_RANDOM_1", ("QUEN", True): "T_POTION_RANDOM_2",
    ("YRDEN", False): "T_POTION_RANDOM_3", ("YRDEN", True): "T_PRAY_RANDOM",
    ("AXII", False): "T_LGUARD_GREET", ("AXII", True): "T_HGUARD_GREET",
}
# Tryb alternatywny "w ziemie" (W3: fala Aard dookola, pulapka Yrden): znak -> klipy (bez broni, miecz rp, miecz lp).
# rp = prawa noga z przodu jak garda 1H stylu W3 (i sztylet), lp = lewa jak garda 2H (make_attacks: 1H _rp, 2H _lp).
# man_geralt_aard_ground_sword ma te same klatki co man_ger_sword_aard_ground_rp_sword.
ALT_SIGNS = {
    "AARD": ("man_geralt_aard_ground", "man_ger_sword_aard_ground_rp_sword", "man_ger_sword_aard_ground_lp_sword"),
    "YRDEN": ("man_geralt_yrden_ground", "man_ger_sword_yrden_ground_rp_sword", "man_ger_sword_yrden_ground_lp_sword"),
}
ALT_WEAPONS = ("bez", "1H", "2H")   # indeks klasy broni jak w zQuickSign (SignWeaponClass: 0 bez, 1 sztylet/1H, 2 2H)
QS_ORDER = ("AARD", "IGNI", "QUEN", "YRDEN", "AXII")   # enum SIGN_* w zQuickSign.cpp (kolejnosc list w ini)
# (znak, klasa broni) -> nazwa w nakladce postoju; te same w zQuickSign.cpp (SIGN_ANIS_ALT). Gesty strazy z Gothica 1
# (T_LGUARD_* / T_HGUARD_*): prawdziwe animacje warstwy 2 w HUMANS Steam / New Balance / Kronik, poza nakladkami (nasze,
# zEBA HUMANS_REMASTER, NB); COMEIN / ALLRIGHT / NOENTRY nie gra nikt (wanilia, NB, Kroniki), LOOKAROUND tylko NPC (SELF
# w stanach ZS_*) - nigdy bohater (OTHER / HERO), w odroznieniu od np. R_SCRATCHHEAD / T_GREETGRD w Kronikach.
SIGN_ANIS_ALT = {
    ("AARD", 0): "T_LGUARD_COMEIN", ("AARD", 1): "T_LGUARD_ALLRIGHT", ("AARD", 2): "T_LGUARD_NOENTRY",
    ("YRDEN", 0): "T_HGUARD_COMEIN", ("YRDEN", 1): "T_HGUARD_NOENTRY", ("YRDEN", 2): "T_HGUARD_LOOKAROUND",
}
# Koniec trybu podtrzymywanego (23.10): strumien Igni i aktywna tarcza Quen koncza sie klipem W3 "stop" (wyjscie z pozy
# trzymania - stop_match.py: man_geralt_*_stop zaczyna sie najblizej pozy trzymania, z mieczem *_stop_rp_sword jak gest
# zwykly), w wariancie ruchu (tulow i rece nad nogami biegu - w podtrzymaniu bohater chodzi). Gra je zDismember
# (SgChannelStopName w Signs.cpp - te same nazwy) zamiast dogrywania gestu zwyklego od klatki trzymania.
# (znak, z mieczem) -> nazwa w nakladce ruchu; nazwy: gesty strazy G1 (tylko NPC w ZS_*) i taniec ZS_Dance (tylko NPC).
STOP_SIGNS = {
    "IGNI": ("man_geralt_ignii_stop", "man_ger_sword_ignii_stop_rp_sword"),
    "QUEN": ("man_geralt_quen_stop", "man_ger_sword_quen_stop_rp_sword"),
}
SIGN_ANIS_STOP = {
    ("IGNI", False): "T_LGUARD_STRETCH", ("IGNI", True): "T_LGUARD_CHANGELEG",
    ("QUEN", False): "T_LGUARD_SCRATCH", ("QUEN", True): "T_DANCE_09",
}
STOP_SETTLE = 3.0       # cm/kl. ruchu lewej dloni wzgledem tulowia - koniec klipu stop (dalej W3 tylko powoli dochodzi do spoczynku)
BLEND_IN_STOP = 0.15    # z pozy trzymania (inna o 10-16 st. srednio) - przejscie bez szarpniecia
# Tryb zwykly (23.10, zQuickSign SignCastSync): skutek Znaku z koncem pchniecia dloni - pierwsza klatka po najszybszym
# ruchu dloni, w ktorej dlon (wzgledem tulowia) zwalnia ponizej PUSH_END_FRAC szczytu; SignCastSyncMs = najpozniejszy
# z wariantow (bez broni / z mieczem) danego Znaku.
PUSH_END_FRAC = 0.25
ALT_BEFORE = 11   # klatek od startu gestu do dotkniecia ziemi (wszystkie warianty) - SignAltCastMs = 440 ms
ALT_AFTER = 16    # klatek po dotknieciu (przyklek, wstanie) - gest 28 kl. = SignAltGestureMs 1120 ms
ALT_CONTACT_EPS = 6.0   # cm nad najnizszym polozeniem lewej dloni po pchnieciu = dotkniecie ziemi
BLEND_OUT_ALT = 0.3     # bohater juz stoi (koniec blokady ruchu) - wygaszenie do postawy pod spodem
# Znaki "w ziemie" w trybie zwyklym: w wariancie ruchu tulow pochylony do przodu o kat rosnacy z przysiadem W3, tak zeby
# w chwili dotkniecia dlon byla GROUND_REACH cm nad ziemia biegu (nogi biegna, wiec bez pochylenia dlon zostaje na
# wysokosci pasa); kat najwyzej GROUND_BEND_MAX.
GROUND_SIGNS = ("YRDEN",)
GROUND_REACH = 30.0
GROUND_BEND_MAX = 60.0
RUN_REF = "S_RUNL"   # petla biegu - miednica odniesienia tulowia w wariancie ruchu
# Warstwa gestu (ta sama stala SIGN_LAYER w zQuickSign.cpp). Nie 20: w HUMANS to zakrety (cale cialo) i gesty dialogowe,
# a silnik w kazdej klatce ruchu gasi warstwe 20 - FadeOutAnisLayerRange(20, 20) w oCAniCtrl_Human::_Forward/_Backward/
# Turn/TurnDegrees/StopTurnAnis/StrafeLeft/Right/ToggleWalkMode/CheckWaterLevel, StopAnisLayerRange(20, 20) w EV_RemoveWeapon
# (stad "gest zatrzymany" w kazdej klatce). Nie 2..8: oCNpc::StandUp (dobycie/schowanie broni, Interrupt) gasi 2..8, a 2 i 4
# zajmuja atak w biegu/trafienie/dobycie broni i celowanie z luku. 9 i 11 to patrzenie/wskazywanie. 21 = pierwsza wolna warstwa
# nad wszystkimi z HUMANS: gest przykrywa tez zakret w miejscu (warstwa 20), a gasza go tylko przerwania postaci
# (Interrupt 2..1000, StartAIState/Enable 2..256) i zdjecie/zalozenie nakladki (0..99999) - jak kazda animacje.
LAYER = 21
TAIL = 8         # klatek po pchnieciu dloni (potem wygaszenie blend_out)
LEAD = 8          # klatek zamachu przed pchnieciem (W3 ma ok. 0,45 s - za dlugo na szybki rzut)
BLEND_IN, BLEND_OUT = 0.1, 0.3
# Wariant postoju gasnie szybciej: gdy bohater ruszy w trakcie gestu, StartAni wariantu ruchu (ta sama warstwa) wygasza go
# z tym czasem (zCModel::FadeOutAni - blendOut wygaszanej animacji), a przez ten czas nogi sa jeszcze czesciowo z gestu.
BLEND_OUT_STAND = 0.2
LEGS = (("BIP01 L THIGH", "BIP01 L CALF", "BIP01 L FOOT"), ("BIP01 R THIGH", "BIP01 R CALF", "BIP01 R FOOT"))


def upper_nodes(sk):
    """Kregoslup od BIP01 SPINE w dol drzewa (bez slotow ZS_ poza ZS_RIGHTHAND - miecz w dloni)."""
    spine = sk.idx("BIP01 SPINE")
    return [i for i in range(len(sk.names)) if retarget._descends(sk, i, spine)
            and (not sk.names[i].startswith("ZS_") or sk.names[i] == "ZS_RIGHTHAND")]


def body_nodes(sk):
    """Cale cialo (jak finishery W3): wszystkie wezly bez slotow ZS_ poza ZS_RIGHTHAND."""
    return [i for i, n in enumerate(sk.names) if not n.startswith("ZS_") or n == "ZS_RIGHTHAND"]


def mean_pose(vfs, sk, ani=RUN_REF):
    """Srednia poza petli z HUMANS: kazda kosc ze sredniego kwaternionu wszystkich klatek, korzen ze sredniej pozycji."""
    m = manlib.Man(bytes(vfs.find("HUMANS-%s.MAN" % ani).data))
    local = [l[:3, :3].copy() for l in sk.local]
    root = np.array(sk.root_pos, dtype=float)
    for k, ni in enumerate(m.nodes):
        qs, ps = [], []
        for f in range(m.frames):
            (x, y, z, w), p = m.decode(m.sample(f, k))
            q = retarget.quat_from_mat(qmat(-x, -y, -z, w))
            qs.append(q if not qs or np.dot(q, qs[0]) >= 0 else -q)
            ps.append(p)
        q = np.sum(qs, axis=0)
        local[ni] = qmat(*(q / np.linalg.norm(q)))
        if sk.parent[ni] < 0:
            root = np.mean(np.array(ps, dtype=float), axis=0)
    return local, root


def plant_feet(sk, frames):
    """Stopy stoja tam, gdzie w pierwszej klatce (korzen juz w miejscu): udo i lydka z IK dwoch kosci w plaszczyznie
    kolana z klatki W3, stopa z obrotem z pierwszej klatki (palce ida za nia). Gdy noga nie siega stopy (W3 unosi biodra
    przy wykroku, a stopa zostaje), korzen idzie nizej o tyle, ile trzeba. Obroty miednicy i reszty ciala bez zmian."""
    W0 = sk.compose(*frames[0])
    out = []
    for local, root in frames:
        local = [R.copy() for R in local]
        W = sk.compose(local, root)
        drop = 0.0
        for names in LEGS:
            t, c, f = (sk.idx(n) for n in names)
            reach = np.linalg.norm(sk.local[c][:3, 3]) + np.linalg.norm(sk.local[f][:3, 3]) - 0.5
            D = W0[f][:3, 3] - W[t][:3, 3]
            h2 = D[0] * D[0] + D[2] * D[2]
            if h2 < reach * reach:
                drop = min(drop, D[1] + math.sqrt(reach * reach - h2))
        if drop < 0.0:
            root = root + np.array([0.0, drop, 0.0])
            W = sk.compose(local, root)
        for names in LEGS:
            t, c, f = (sk.idx(n) for n in names)
            a, b = np.linalg.norm(sk.local[c][:3, 3]), np.linalg.norm(sk.local[f][:3, 3])
            H, T, K0 = W[t][:3, 3], W0[f][:3, 3], W[c][:3, 3]
            u = (T - H) / np.linalg.norm(T - H)
            d = min(max(np.linalg.norm(T - H), abs(a - b) + 1e-3), a + b - 1e-3)
            p = (K0 - H) - np.dot(K0 - H, u) * u           # kierunek zgiecia kolana jak w W3
            if np.linalg.norm(p) < 1e-6:
                p = W[t][:3, :3] @ np.array([0.0, 0.0, 1.0]) - np.dot(W[t][:3, :3] @ np.array([0.0, 0.0, 1.0]), u) * u
            v = p / np.linalg.norm(p)
            x = (a * a - b * b + d * d) / (2 * d)
            K = H + x * u + math.sqrt(max(a * a - x * x, 0.0)) * v
            Rt = W[t][:3, :3]
            Rt = retarget.rot_between(Rt @ sk.local[c][:3, 3], K - H) @ Rt
            Rc = Rt @ local[c]
            Rc = retarget.rot_between(Rc @ sk.local[f][:3, 3], H + d * u - K) @ Rc
            local[t] = W[sk.parent[t]][:3, :3].T @ Rt
            local[c] = Rt.T @ Rc
            local[f] = Rc.T @ W0[f][:3, :3]
        out.append((local, root))
    return out


def _rot_x(deg):
    """Obrot wokol osi X postaci (w prawo); dodatni kat pochyla gore (+Y) do przodu (+Z)."""
    a = math.radians(deg)
    c, s = math.cos(a), math.sin(a)
    return np.array([[1.0, 0.0, 0.0], [0.0, c, -s], [0.0, s, c]])


def _smoothstep(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3.0 - 2.0 * x)


def ground_pose(sk, frame, ref, bend=0.0):
    """Lokalne obroty wariantu ruchu nalozonego na poze ref (bieg): tulow i rece z gestu, reszta z ref; bend = dodatkowe
    pochylenie tulowia do przodu (st.) wokol osi X postaci w stawie BIP01 SPINE."""
    spine, pelvis = sk.idx("BIP01 SPINE"), sk.idx("BIP01 PELVIS")
    P = sk.compose(ref[0], ref[1])[pelvis][:3, :3]
    local = [R.copy() for R in ref[0]]
    for i in upper_nodes(sk):
        local[i] = frame[0][i]
    if bend:
        local[spine] = P.T @ _rot_x(bend) @ P @ local[spine]
    return local


def hand_over_feet(sk, local, root, floor=None):
    """Wysokosc lewej dloni nad wezlem stopy na ziemi (cm): w pozie W3 nad nizsza stopa, w biegu Gothica nad floor
    (run_floor - stopa podparcia; w sredniej pozie biegu obie stopy sa w gorze)."""
    W = sk.compose(local, root)
    if floor is None:
        floor = min(W[sk.idx(n)][1, 3] for n in ("BIP01 L FOOT", "BIP01 R FOOT"))
    return W[sk.idx("BIP01 L HAND")][1, 3] - floor


def run_floor(vfs, sk, ani=RUN_REF):
    """Wysokosc wezla stopy podparcia w petli biegu (najnizsza stopa ze wszystkich klatek MAN), w ukladzie modelu."""
    m = manlib.Man(bytes(vfs.find("HUMANS-%s.MAN" % ani).data))
    feet = (sk.idx("BIP01 L FOOT"), sk.idx("BIP01 R FOOT"))
    low = None
    for f in range(m.frames):
        local = [l[:3, :3].copy() for l in sk.local]
        root = np.array(sk.root_pos, dtype=float)
        for k, ni in enumerate(m.nodes):
            (x, y, z, w), p = m.decode(m.sample(f, k))
            local[ni] = qmat(-x, -y, -z, w)
            if sk.parent[ni] < 0:
                root = np.array(p, dtype=float)
        W = sk.compose(local, root)
        y = min(W[i][1, 3] for i in feet)
        low = y if low is None else min(low, y)
    return low


def bend_to_ground(sk, frames, pelvis_y, ref, floor):
    """Wariant ruchu Znaku "w ziemie": tulow pochylony do przodu o kat = bend x smoothstep(przysiad W3) - 0 na poczatku
    gestu, najwiecej w najnizszym polozeniu miednicy W3. bend (do GROUND_BEND_MAX) dobrany tak, zeby najnizej dlon byla
    GROUND_REACH cm nad stopa podparcia biegu (floor, pozy ref). -> (klatki, bend, dlon najnizej przed / po pochyleniu)."""
    spine, pelvis = sk.idx("BIP01 SPINE"), sk.idx("BIP01 PELVIS")
    P = sk.compose(ref[0], ref[1])[pelvis][:3, :3]
    p0, pmin = pelvis_y[0], min(pelvis_y)
    w = [_smoothstep((p0 - p) / (p0 - pmin)) if p0 - pmin > 1.0 else 0.0 for p in pelvis_y]

    def lowest(bend):
        return min(hand_over_feet(sk, ground_pose(sk, f, ref, bend * w[k]), ref[1], floor) for k, f in enumerate(frames))

    before = lowest(0.0)
    lo, hi = 0.0, GROUND_BEND_MAX
    if before <= GROUND_REACH:
        hi = 0.0
    elif lowest(hi) < GROUND_REACH:
        for _ in range(24):
            mid = 0.5 * (lo + hi)
            lo, hi = (mid, hi) if lowest(mid) > GROUND_REACH else (lo, mid)
    bend = hi
    out = []
    for k, (local, root) in enumerate(frames):
        loc = [R.copy() for R in local]
        loc[spine] = P.T @ _rot_x(bend * w[k]) @ P @ loc[spine]
        out.append((loc, root))
    return out, bend, before, lowest(bend)


def build_gesture(dump, sk, grip, weapon, ref, full=False, ground=False, floor=None):
    """-> dict(frames @25 (pelne lokalne obroty), nodes, release = klatka pchniecia dloni).
    full=False (ruch): tylko tulow i rece; tulow wzgledem miednicy pozy ref (bieg) tak, jak w W3 wzgledem kierunku postaci.
    full=True (postoj): cale cialo z W3 w miejscu - korzen bez ruchu w poziomie, stopy stoja (plant_feet); ref bez znaczenia.
    Oba warianty z tych samych klatek W3 - ta sama dlugosc i chwila pchniecia (zQuickSign przelacza je w trakcie gestu).
    ground=True (Znak "w ziemie", tylko wariant ruchu): tulow pochylony do przodu z przysiadem W3 (bend_to_ground; floor =
    run_floor biegu ref) - w dict takze bend (st.) i dlon najnizej nad stopa biegu przed / po pochyleniu (reach0, reach)."""
    d = make_moves.to_entity_start(dump)
    fr = retarget.resample(retarget.retarget(d, sk, grip, False, "w3", weapon="r_weapon" if weapon else None), d["fps"])
    spine, pelvis, hand = sk.idx("BIP01 SPINE"), sk.idx("BIP01 PELVIS"), sk.idx("BIP01 L HAND")
    P = None if full else sk.compose(ref[0], ref[1])[pelvis][:3, :3]   # miednica Gothica w biegu
    out, tips, pel = [], [], []
    for local, root in fr:
        W = sk.compose(local, root)
        loc = [R.copy() for R in local]
        if full:
            out.append((loc, np.array([0.0, root[1], 0.0])))   # w miejscu: tylko wysokosc korzenia z W3
        else:
            loc[spine] = P.T @ W[spine][:3, :3]                 # tulow wzgledem kierunku postaci jak w W3
            out.append((loc, root))
        tips.append(W[hand][:3, 3] - W[spine][:3, 3])          # dlon wzgledem tulowia (bez ruchu calej postaci)
        pel.append(W[pelvis][1, 3])
    v = [np.linalg.norm(tips[k + 1] - tips[k]) for k in range(len(tips) - 1)]
    release = int(np.argmax(v[:max(2, len(v) * 2 // 3)])) + 1   # najszybszy ruch dloni w pierwszych 2/3 klipu
    start = max(0, release - LEAD)
    end = min(len(out), release + TAIL + 1)
    frames = out[start:end]
    # koniec pchniecia: dlon zwalnia ponizej PUSH_END_FRAC szczytu (v[k] = ruch z klatki k do k+1) - skutek Znaku
    peak = release - 1
    push_end = next((k for k in range(peak + 1, len(v)) if v[k] < PUSH_END_FRAC * v[peak]), end - 1)
    g = dict(nodes=body_nodes(sk) if full else upper_nodes(sk), release=release - start,
             push_end=min(push_end, end - 1) - start)
    if full:
        frames = plant_feet(sk, frames)
    elif ground:
        frames, g["bend"], g["reach0"], g["reach"] = bend_to_ground(sk, frames, pel[start:end], ref, floor)
    g["frames"] = frames
    return g


def build_alt_gesture(dump, sk, grip, weapon):
    """Gest trybu alternatywnego "w ziemie": caly Geralt z W3 w miejscu jak wariant postoju (korzen bez ruchu w poziomie,
    stopy stoja), od ALT_BEFORE klatek przed dotknieciem ziemi lewa dlonia do ALT_AFTER po nim (przyklek i wstanie).
    Dotkniecie = pierwsza klatka po pchnieciu, w ktorej dlon jest najwyzej ALT_CONTACT_EPS cm nad swoim najnizszym
    polozeniem. -> dict(frames, nodes, release (pchniecie), contact (= ALT_BEFORE), hand (dlon nad stopami w dotknieciu),
    pelvis_drop (przysiad miednicy w dotknieciu, cm))."""
    d = make_moves.to_entity_start(dump)
    fr = retarget.resample(retarget.retarget(d, sk, grip, False, "w3", weapon="r_weapon" if weapon else None), d["fps"])
    spine, pelvis, hand = sk.idx("BIP01 SPINE"), sk.idx("BIP01 PELVIS"), sk.idx("BIP01 L HAND")
    out, tips, hy = [], [], []
    for local, root in fr:
        W = sk.compose(local, root)
        out.append(([R.copy() for R in local], np.array([0.0, root[1], 0.0])))
        tips.append(W[hand][:3, 3] - W[spine][:3, 3])
        hy.append(W[hand][1, 3])
    v = [np.linalg.norm(tips[k + 1] - tips[k]) for k in range(len(tips) - 1)]
    release = int(np.argmax(v[:max(2, len(v) * 2 // 3)])) + 1
    low = min(hy[release:])
    contact = next(k for k in range(release, len(hy)) if hy[k] <= low + ALT_CONTACT_EPS)
    start, end = contact - ALT_BEFORE, contact + ALT_AFTER + 1
    assert start >= 0 and end <= len(out), (start, end, len(out))
    frames = plant_feet(sk, out[start:end])
    W0, Wc = sk.compose(*frames[0]), sk.compose(*frames[ALT_BEFORE])
    return dict(frames=frames, nodes=body_nodes(sk), release=release - start, contact=ALT_BEFORE,
                hand=hand_over_feet(sk, *frames[ALT_BEFORE]), pelvis_drop=W0[pelvis][1, 3] - Wc[pelvis][1, 3])


def build_stop_gesture(dump, sk, grip, weapon, ref):
    """Klip W3 *_stop (koniec strumienia Igni / tarczy Quen) w wariancie ruchu: tulow i rece, tulow wzgledem miednicy
    biegu ref jak w build_gesture; od poczatku klipu (poza trzymania) do STOP_SETTLE (+2 kl.) - dalej W3 tylko stoi.
    -> dict(frames, nodes, settle)."""
    d = make_moves.to_entity_start(dump)
    fr = retarget.resample(retarget.retarget(d, sk, grip, False, "w3", weapon="r_weapon" if weapon else None), d["fps"])
    spine, pelvis, hand = sk.idx("BIP01 SPINE"), sk.idx("BIP01 PELVIS"), sk.idx("BIP01 L HAND")
    P = sk.compose(ref[0], ref[1])[pelvis][:3, :3]
    out, tips = [], []
    for local, root in fr:
        W = sk.compose(local, root)
        loc = [R.copy() for R in local]
        loc[spine] = P.T @ W[spine][:3, :3]
        out.append((loc, root))
        tips.append(W[hand][:3, 3] - W[spine][:3, 3])
    v = [np.linalg.norm(tips[k + 1] - tips[k]) for k in range(len(tips) - 1)]
    moving = [k for k, x in enumerate(v) if x > STOP_SETTLE]
    settle = min(len(out), max(8, (moving[-1] if moving else 0) + 3))
    return dict(frames=out[:settle], nodes=upper_nodes(sk), settle=settle, release=0)


def gesture_chunk(vfs, sk, ani, g, overlay=OVERLAY, blend_out=None, blend_in=BLEND_IN, sfx=True):
    template = manlib.Man(bytes(vfs.find("HUMANS-T_1HSFINISH.MAN").data))
    template.layer = LAYER
    retarget.write_man(template, g["frames"], sk, ani, g["nodes"], os.path.join(COMPILED, "%s-%s.MAN" % (overlay, ani)))
    nf = len(g["frames"])
    if blend_out is None:
        blend_out = BLEND_OUT_STAND if overlay == OVERLAY_STAND else BLEND_OUT
    chunk = msblib.ani_chunk(ani, LAYER, "", blend_in, blend_out, "M.", "ZDISM_W3.ASC", 1, nf)
    return chunk + (sfx_events.events_block(sfx=[(max(1, g["release"]), "WHOOSH")]) if sfx else b""), nf


def main():
    vfs = build_overlay.game_vfs()
    sk = retarget.GothicSkeleton(vfs)
    grip = make_pair.grip_local(vfs, sk)
    run = mean_pose(vfs, sk, RUN_REF)
    floor = run_floor(vfs, sk, RUN_REF)
    os.makedirs(WORK, exist_ok=True)
    os.makedirs(COMPILED, exist_ok=True)
    for f in os.listdir(COMPILED):   # stare nakladki po jednej na Znak i stare MAN-y gestow (inne nazwy)
        u = f.upper()
        if u.startswith(("HUMANS_ZDF_SIGN_", OVERLAY + "-", OVERLAY_STAND + "-")):
            os.remove(os.path.join(COMPILED, f))
    chunks = {OVERLAY: [], OVERLAY_STAND: []}
    push_end = {}   # znak -> najpozniejszy koniec pchniecia (kl.) z wariantow - SignCastSyncMs
    for sign, (bare, sword) in SIGNS.items():
        for clip, weapon in ((sword, True), (bare, False)):
            d = make_pair.dump_clip(SET, clip, os.path.join(WORK, clip + ".json"))
            timing = set()
            for overlay, table, full in ((OVERLAY_STAND, SIGN_ANIS_STAND, True), (OVERLAY, SIGN_ANIS, False)):
                g = build_gesture(d, sk, grip, weapon, run, full, ground=not full and sign in GROUND_SIGNS, floor=floor)
                ani = table[(sign, weapon)]
                chunk, nf = gesture_chunk(vfs, sk, ani, g, overlay)
                chunks[overlay].append(chunk)
                timing.add((nf, g["release"], g["push_end"]))
                push_end[sign] = max(push_end.get(sign, 0), g["push_end"])
                print("%-6s %-5s %-6s %-20s <- %-38s %2d kl. (%.2f s), pchniecie kl. %d = %d ms, koniec pchniecia kl. %d%s" % (
                    sign, "miecz" if weapon else "bez", "postoj" if full else "ruch", ani, clip, nf, nf / retarget.FPS_OUT,
                    g["release"], round(g["release"] / retarget.FPS_OUT * 1000), g["push_end"],
                    ", pochylenie %.0f st. (dlon najnizej %.0f -> %.0f cm nad stopami)" % (g["bend"], g["reach0"], g["reach"])
                    if "bend" in g else ""))
            assert len(timing) == 1, (sign, weapon, timing)   # przelaczenie w trakcie gestu od tej samej klatki
    # tryb alternatywny "w ziemie" - w nakladce postoju (caly Geralt w miejscu)
    for sign, clips in ALT_SIGNS.items():
        for wc, clip in enumerate(clips):
            d = make_pair.dump_clip(SET, clip, os.path.join(WORK, clip + ".json"))
            g = build_alt_gesture(d, sk, grip, wc > 0)
            ani = SIGN_ANIS_ALT[(sign, wc)]
            chunk, nf = gesture_chunk(vfs, sk, ani, g, OVERLAY_STAND, BLEND_OUT_ALT)
            chunks[OVERLAY_STAND].append(chunk)
            print("%-6s %-5s alt    %-20s <- %-38s %2d kl. (%.2f s), pchniecie kl. %d, ziemia kl. %d = %d ms "
                  "(dlon %.0f cm nad stopami, przysiad %.0f cm)" % (
                      sign, ALT_WEAPONS[wc], ani, clip, nf, nf / retarget.FPS_OUT, g["release"], g["contact"],
                      round(g["contact"] / retarget.FPS_OUT * 1000), g["hand"], g["pelvis_drop"]))
    # koniec podtrzymania Igni / Quen - klip W3 *_stop w nakladce ruchu (gra zDismember)
    for sign, (bare, sword) in STOP_SIGNS.items():
        for clip, weapon in ((bare, False), (sword, True)):
            d = make_pair.dump_clip(SET, clip, os.path.join(WORK, clip + ".json"))
            g = build_stop_gesture(d, sk, grip, weapon, run)
            ani = SIGN_ANIS_STOP[(sign, weapon)]
            chunk, nf = gesture_chunk(vfs, sk, ani, g, OVERLAY, BLEND_OUT, BLEND_IN_STOP, sfx=False)
            chunks[OVERLAY].append(chunk)
            print("%-6s %-5s stop   %-20s <- %-38s %2d kl. (%.2f s)" % (
                sign, "miecz" if weapon else "bez", ani, clip, nf, nf / retarget.FPS_OUT))
    print("zQuickSign.ini: SignCastSyncMs=%s  (koniec pchniecia dloni, kolejnosc AARD,IGNI,QUEN,YRDEN,AXII)" % ",".join(
        str(round(push_end[s] / retarget.FPS_OUT * 1000)) for s in QS_ORDER))
    print("zQuickSign.ini: SignAltCastMs=%s  SignAltGestureMs=%s  (kolejnosc AARD,IGNI,QUEN,YRDEN,AXII)" % (
        ",".join(str(round(ALT_BEFORE / retarget.FPS_OUT * 1000)) if s in ALT_SIGNS else "0" for s in QS_ORDER),
        ",".join(str(round((ALT_BEFORE + ALT_AFTER + 1) / retarget.FPS_OUT * 1000)) if s in ALT_SIGNS else "0"
                 for s in QS_ORDER)))
    for overlay, ch in chunks.items():
        build_overlay.write_overlay(overlay, COMPILED, ch, vfs)


if __name__ == "__main__":
    main()
