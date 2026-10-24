r"""Style walki z Wiedzmina 3 dla zDismember. TYLKO DO UZYTKU PRYWATNEGO (animacje CD Projekt RED) - wynik trafia do
data_w3 / zDismember_W3.vdf, nigdy do wydania.

Uzycie: py -3.12 -I make_attacks.py [--only 1H|2H]
Wynik: nakladki HUMANS_ZDF_W3_1H (szybkie ciecia: T_1HATTACKL/R + kombinacja S_1HATTACK) i HUMANS_ZDF_W3_2H (mocne
ciecia: T_2HATTACKL/R + S_2HATTACK). Plugin naklada styl na bohatera ponad nakladki talentu / zExtendedBattleAnimations
(sam od 60% talentu albo wymuszony w cyklu stylow). Ataki uruchamia system walki Gothica, wiec animacje maja ruch
korzenia (flaga M - przesuwa silnik, z kolizja) i zdarzenia jak oryginal: DEF_HIT_LIMB, DEF_OPT_FRAME (trafienie),
DEF_HIT_END, DEF_WINDOW (kombinacja), swist WHOOSH, kroki RUN; na koncu przejscie do postawy stylu (S_xHRUN = garda W3:
1H 1. klatka ciecia L z oddechem, 2H petla parady Geralta parry_idle_lp - ciecia, kombinacja i blok 2H wchodza z niej
i wracaja do niej przejsciem bezwladnosciowym).
Kierunki jak w Gothicu: L = zamach z prawej w lewo, R = z lewej w prawo.
Atak w biegu (T_xHATTACKMOVE) nie jest podmieniany: w stylu W3 plugin gra zamiast niego caly ruch W3 (RUNATK_* z
make_moves.py), bez pluginu zostaje atak w biegu gry.
"""
import argparse, json, math, os, struct, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import msblib
import retarget, build_overlay, make_pair, make_moves, recovery, sfx_events
from sfx_events import event_tag

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
COMPILED = make_moves.COMPILED
TAIL_S = 0.55   # po ciosie zostaje tyle sekund (reszta klipu W3 to powrot do postawy)


def attack_ani_chunk(name, nframes, hit, ground=()):
    """ANI + blok zdarzen jak w HUMANS_1HST1: wyjscie 0,2, bez nastepnej (przejscie wybiera silnik); kroki z ruchu."""
    tags = [event_tag("DEF_HIT_LIMB", "ZS_RIGHTHAND"), event_tag("DEF_OPT_FRAME", str(hit)),
            event_tag("DEF_HIT_END", str(nframes)), event_tag("DEF_WINDOW", "%d %d" % (min(hit + 3, nframes), nframes))]
    ani = msblib.ani_chunk(name, 1, "", 0.1, 0.2, "M.", "ZDISM_W3.ASC", 1, nframes)   # wejscie 0,1: inna poza startowa niz postawa Gothica
    return ani + sfx_events.events_block(sfx=[(max(1, hit - 2), "WHOOSH")], ground=ground, tags=tags)


def strike_frame(dump, blade=(0.0, 1.0, 0.0)):
    """Klatka (zrodla) najszybszego ruchu czubka ostrza przed postacia. blade = os ostrza w kosci r_weapon: domyslnie +Y
    (tak policzone ciecia, kombinacje i kontry), wlasciwa os ostrza W3 to retarget.BLADE_AXIS["w3"] (+Z)."""
    names = [b["name"] for b in dump["bones"]]
    wi, ti = names.index("r_weapon"), names.index("Trajectory")
    tips = []
    for f in dump["frames"]:
        W, T = np.array(f[wi]), np.array(f[ti])
        tips.append((W[:3, 3] + W[:3, :3] @ np.asarray(blade) * 0.95 - np.array([T[0][3], T[1][3], 0.0])) * 100)
    sp = [np.linalg.norm(tips[k + 1] - tips[k]) if tips[k][1] > 20 else 0.0 for k in range(len(tips) - 1)]
    return int(np.argmax(sp[2:])) + 2


WINDUP_RAMP = 4      # klatek @25 przed ciosem, w ktorych przyspieszony zamach zwalnia gladko do tempa klipu
WINDUP_KMAX = 2.2    # najwiekszy mnoznik tempa zamachu


def _sample(frames, quats, s):
    """Poza w chwili s (klatki zrodla, ulamek): slerp obrotow, lerp korzenia; calkowite s = klatka zrodla."""
    a = int(math.floor(s + 1e-6))
    if abs(s - a) < 1e-4 or a >= len(frames) - 1:
        a = min(a, len(frames) - 1)
        return [R.copy() for R in frames[a][0]], np.array(frames[a][1], dtype=float)
    u = s - a
    local = [retarget.qmat(*retarget.slerp(qa, qb, u)) for qa, qb in zip(quats[a], quats[a + 1])]
    return local, np.asarray(frames[a][1], dtype=float) * (1 - u) + np.asarray(frames[a + 1][1], dtype=float) * u


def retime_windup(frames, hit, target, ramp=WINDUP_RAMP, kmax=WINDUP_KMAX):
    """Zamach (klatki @25 przed ciosem hit) przyspieszony tak, by cios wypadl w klatce target -> (klatki, nowy hit).
    Tempo k (staly mnoznik, <= kmax) od poczatku do ramp klatek przed ciosem, potem gladko (smoothstep) do 1 w chwili
    ciosu; od ciosu klip bez zmian. Mocne ciecia W3 (2H) maja cios w 0,68 s, szybkie (1H) w 0,36 s, Gothic ~0,2 s."""
    if hit <= target or hit < 1:
        return frames, hit
    r = min(ramp, hit)
    s_a = hit - r
    steps = 64

    def ramp_out(k):   # czas wyjscia zuzyty na rampe przy tempie k (calka ds / v)
        return sum(r / steps / (k + (1.0 - k) * (u * u * (3.0 - 2.0 * u))) for u in ((j + 0.5) / steps for j in range(steps)))

    t_hit = lambda k: s_a / k + ramp_out(k)
    lo, hi = 1.0, kmax
    if t_hit(hi) > target:
        k = hi
    else:
        for _ in range(60):
            mid = 0.5 * (lo + hi)
            lo, hi = (mid, hi) if t_hit(mid) > target else (lo, mid)
        k = 0.5 * (lo + hi)
    # odwrotna mapa czasu w rampie: tablica (czas wyjscia, czas zrodla)
    tab_t, tab_s, acc = [s_a / k], [float(s_a)], s_a / k
    for j in range(steps):
        u = (j + 0.5) / steps
        acc += r / steps / (k + (1.0 - k) * (u * u * (3.0 - 2.0 * u)))
        tab_t.append(acc)
        tab_s.append(s_a + r * (j + 1) / steps)
    T_hit = tab_t[-1]
    quats = [[retarget.quat_from_mat(R) for R in f[0]] for f in frames]
    out = []
    n_out = int(math.floor(T_hit + (len(frames) - 1 - hit) + 1e-6)) + 1
    for j in range(n_out):
        if j <= tab_t[0]:
            s = j * k
        elif j < T_hit:
            s = float(np.interp(j, tab_t, tab_s))
        else:
            s = hit + (j - T_hit)
        out.append(_sample(frames, quats, min(s, len(frames) - 1)))
    return out, int(round(T_hit))


ENTRY_FRAMES = 5   # klatek wejscia z gardy parady (2H) w ciecie W3 - w trakcie zamachu, bez dokladania klatek


def enter_from(guard, frames, s1, scale, n=ENTRY_FRAMES):
    """frames[0..s1] z wejsciem z pozy guard (lokalne obroty, korzen): klatka 0 = garda (korzen w poziomie jak klip),
    roznica pozy i wysokosci wygasa bezwladnosciowo w n klatkach w trakcie klipu (make_moves.inertial_append) - dlugosc
    i chwila ciosu bez zmian, od klatki n poza klipu. Ruch korzenia w poziomie x scale."""
    g_local, g_root = guard
    p0 = np.array(frames[0][1], dtype=float)
    first = [([R.copy() for R in g_local], np.array([p0[0], g_root[1], p0[2]]))]
    return make_moves.inertial_append(first, frames, 0, s1, scale, n, xz_inertia=False)


def build_attack(dump, sk, grip, step_scale, entry=None, windup=None):
    """-> (klatki @25 z ruchem korzenia skroconym do step_scale, klatka trafienia 1-based). entry = garda, z ktorej
    ciecie wchodzi przejsciem bezwladnosciowym (enter_from); bez niej klatka 0 = 1. klatka klipu W3. windup = klatka
    (@25, od 0), w ktorej ma wypasc cios - zamach przyspieszony (retime_windup)."""
    frames, hit = prep_attack(dump, sk, grip, windup)
    fps = retarget.FPS_OUT
    frames = frames[:min(len(frames), hit + int(TAIL_S * fps) + 1)]
    if entry is not None:
        return enter_from(entry, frames, len(frames) - 1, step_scale), hit + 1
    r0 = frames[0][1]
    out = [(local, np.array([r0[0] + (r[0] - r0[0]) * step_scale, r[1], r0[2] + (r[2] - r0[2]) * step_scale])) for local, r in frames]
    return out, hit + 1


COMBO_CANDIDATES = ["man_geralt_sword_attack_fast_%d_%s_40ms" % (i, s) for i in range(1, 8) for s in ("rp", "lp")] + [
    "man_geralt_sword_attack_fast_8_rp_40ms", "man_geralt_sword_attack_fast_9_lp_40ms"]
SEG_TAIL = 16   # klatek @25 po ciosie w srodkowych segmentach (okno na kolejny cios ~0,6 s jak w Gothicu)
SKIP = 3        # klatek postawy wyjsciowej pomijanych na poczatku kolejnego ciecia
JOIN = 5        # klatek przejscia (slerp) miedzy segmentami


def slerp_mat(A, B, w):
    return retarget.qmat(*retarget.slerp(retarget.quat_from_mat(A), retarget.quat_from_mat(B), w))


def angle(A, B):
    return math.degrees(math.acos(max(-1.0, min(1.0, (np.trace(A.T @ B) - 1.0) / 2.0))))


def prep_attack(dump, sk, grip, windup=None):
    """-> (klatki @25 od startu voba, indeks klatki ciosu @25). windup = docelowy indeks ciosu (retime_windup)."""
    src_hit = strike_frame(dump)
    d = make_moves.to_entity_start(dump)
    frames = retarget.resample(retarget.retarget(d, sk, grip, False, "w3", weapon="r_weapon"), d["fps"])
    hit = min(len(frames) - 1, int(round(src_hit / d["fps"] * retarget.FPS_OUT)))
    return retime_windup(frames, hit, windup) if windup is not None else (frames, hit)


def build_combo(dumps, sk, grip, step_scale, nhits, start_filter=None, entry=None, windup=None, first=None,
                open_windup=None):
    """Kombinacja z nhits ciec W3 (kolejnosc wg ciaglosci pozy, kazde inne) -> (klatki @25, zdarzenia 1-based).
    entry = garda: pierwsze ciecie wchodzi z niej bezwladnosciowo (enter_from, te same klatki i zdarzenia).
    windup = indeks ciosu w kazdym cieciu (przyspieszony zamach, retime_windup).
    first = (nazwa, zrzut, windup): pierwsze ciecie narzucone (spoza dumps, z wlasnym zamachem), reszta dobierana od niego.
    open_windup = indeks ciosu tylko pierwszego ciecia (wybranego z dumps) - retime nie rusza pozy po ciosie, wiec
    kolejnosc dobrana po ciaglosci pozy zostaje wazna."""
    prepped = {c: prep_attack(d, sk, grip, windup) for c, d in dumps.items()}
    if first:
        prepped[first[0]] = prep_attack(first[1], sk, grip, first[2])
    names = list(prepped)
    seg_end = {c: min(len(f) - 1, h + SEG_TAIL) for c, (f, h) in prepped.items()}
    dist = {(a, b): sum(angle(x, y) for x, y in zip(prepped[a][0][seg_end[a]][0], prepped[b][0][SKIP][0]))
            for a in names for b in names if a != b}
    # najlepszy lancuch: zachlannie od kazdego startu (pelny przeglad 17^4 niepotrzebny)
    best = None
    starts = [first[0]] if first else [c for c in names if not start_filter or start_filter in c] or names
    for start in starts:
        seq, cost = [start], 0.0
        while len(seq) < nhits:
            nxt = min((b for b in names if b not in seq), key=lambda b: dist[(seq[-1], b)])
            cost += dist[(seq[-1], nxt)]
            seq.append(nxt)
        if best is None or cost < best[0]:
            best = (cost, seq)
    seq = best[1]
    if open_windup is not None and not first:
        prepped[seq[0]] = prep_attack(dumps[seq[0]], sk, grip, open_windup)
        seg_end[seq[0]] = min(len(prepped[seq[0]][0]) - 1, prepped[seq[0]][1] + SEG_TAIL)

    out, ev = [], {"opt": [], "end": [], "win": [], "clips": seq}
    for k, c in enumerate(seq):
        frames, hit = prepped[c]
        s0 = 0 if k == 0 else SKIP
        s1 = seg_end[c] if k < nhits - 1 else min(len(frames) - 1, hit + int(TAIL_S * retarget.FPS_OUT))
        r_start = frames[s0][1]
        base = out[-1][1] if out else np.array([r_start[0], 0.0, r_start[2]])
        prev_local = out[-1][0] if out else None
        g0 = len(out)
        if k == 0 and entry is not None:
            out = enter_from(entry, frames, s1, step_scale)
        else:
            for i, f in enumerate(range(s0, s1 + 1)):
                local, r = frames[f]
                if prev_local is not None and i < JOIN:
                    w = (i + 1) / (JOIN + 1)
                    local = [slerp_mat(a, b, w) for a, b in zip(prev_local, local)]
                root = np.array([base[0] + (r[0] - r_start[0]) * step_scale, r[1], base[2] + (r[2] - r_start[2]) * step_scale])
                out.append((local, root))
        opt = g0 + (hit - s0) + 1
        end = len(out)
        ev["opt"].append(opt)
        ev["end"].append(end)
        ev["win"] += [min(opt + 2, end), end]
    return out, ev


def chain(segments, join=JOIN, root_join=False):
    """segments: [(klatki, od, do, skala kroku)] -> (klatki, indeksy poczatkow segmentow). Kazdy kolejny segment wchodzi
    z ostatniej pozy poprzedniego przez 'join' klatek (slerp); korzen ciagly, ruch w poziomie skalowany.
    root_join: korzen (wysokosc i ruch w poziomie) tez przechodzi od ostatniej pozycji przez 'join' klatek (waga
    smoothstep - bez skoku predkosci) - inaczej wysokosc i predkosc korzenia klipu zaczynaja sie od razu."""
    out, starts = [], []
    for frames, s0, s1, scale in segments:
        r_start = frames[s0][1]
        base = out[-1][1] if out else np.array([r_start[0], 0.0, r_start[2]])
        prev_local = out[-1][0] if out else None
        prev_root = np.array(out[-1][1], dtype=float) if out else None
        starts.append(len(out))
        for i, f in enumerate(range(s0, s1 + 1)):
            local, r = frames[f]
            root = np.array([base[0] + (r[0] - r_start[0]) * scale, r[1], base[2] + (r[2] - r_start[2]) * scale])
            if prev_local is not None and i < join:
                w = (i + 1) / (join + 1)
                local = [slerp_mat(a, b, w) for a, b in zip(prev_local, local)]
                if root_join:
                    ws = w * w * (3.0 - 2.0 * w)
                    root = prev_root + (root - prev_root) * ws
            out.append((local, root))
    return out, starts


# Blok W3 (parady Gothica T_xHPARADE_0 / _A2 / _A3 = trzy kolejne odbicia): odbicia Geralta z gornej lewej, gornej prawej
# i dolnej lewej; 1H z postawy rp, 2H z lp. Klip W3 zaczyna sie w pozie bloku - z gardy wchodzimy przejsciem.
PARADES = {"1H": ["man_geralt_sword_parry_f_leftup_rp", "man_geralt_sword_parry_f_rightup_rp", "man_geralt_sword_parry_f_leftdown_rp"],
           "2H": ["man_geralt_sword_parry_f_leftup_lp", "man_geralt_sword_parry_f_rightup_lp", "man_geralt_sword_parry_f_leftdown_lp"]}
PARRY_FRAMES = 10   # odbicie: 0,4 s klipu W3 (dalej odrzut i powrot W3)


def build_parade(dump, sk, grip, guard, step_scale, smooth=False):
    """Blok: garda -> (4 kl. przejscia) odbicie W3 -> przejscie do gardy. Odrzut W3 (1 m) skrocony do step_scale.
    smooth (garda parady 2H): korzen tez przechodzi w 4 kl. (chain root_join) i powrot bezwladnosciowy
    (make_moves.inertial_recovery); obroty na wejsciu dalej slerpem - odbicia lp zaczynaja sie prawie w parry_idle_lp,
    a wejscie bezwladnosciowe kasowalo predkosc poczatku odbicia (szybszy ruch potem, do 150 st./kl.)."""
    frames, _ = prep_attack(dump, sk, grip)
    out, _ = chain([([guard], 0, 0, 0.0), (frames, 0, min(PARRY_FRAMES, len(frames) - 1), step_scale)], join=4,
                   root_join=smooth)
    if smooth:
        return make_moves.inertial_recovery(out, guard, REC_FRAMES)
    return recovery.append_recovery(out, guard, REC_FRAMES)


def parade_ani_chunk(name, nframes, ground=()):
    """Parada jak w HUMANS: okno od poczatku prawie do konca (kolejne odbicie), dzwiek odbicia D_PARADE."""
    tags = [event_tag("DEF_WINDOW", "0 %d" % max(1, nframes - 4))]
    ani = msblib.ani_chunk(name, 1, "", 0.1, 0.2, "M.", "ZDISM_W3.ASC", 1, nframes)
    return ani + sfx_events.events_block(sfx=[(4, "D_PARADE")], ground=ground, tags=tags)


def combo_ani_chunk(name, nframes, ev, ground=()):
    """S_xHATTACK jak w nakladkach talentu: nastepna = ona sama, silnik konczy na HIT_END bez wcisniecia w oknie;
    wyjscie 0,25 s (zatrzymanie po niepelnej kombinacji przechodzi miekko w postawe)."""
    tags = [event_tag("DEF_HIT_LIMB", "ZS_RIGHTHAND"), event_tag("DEF_OPT_FRAME", " ".join(map(str, ev["opt"]))),
            event_tag("DEF_HIT_END", " ".join(map(str, ev["end"]))), event_tag("DEF_WINDOW", " ".join(map(str, ev["win"])))]
    ani = msblib.ani_chunk(name, 1, name, 0.1, 0.25, "M.", "ZDISM_W3.ASC", 1, nframes)
    return ani + sfx_events.events_block(sfx=[(max(1, o - 2), "WHOOSH") for o in ev["opt"]], ground=ground, tags=tags)


REC_FRAMES = 6   # przejscie do postawy Gothica na koncu ciecia / kombinacji

# Style W3: 1H = szybkie ciecia (Geralt, jedna reka), 2H = mocne ciecia (zamach z wiekszym rozmachem).
# Ciecia boczne wybrane po kierunku zamachu czubka ostrza (L = z prawej w lewo, jak oryginal Gothica).
# Garda (postawa S_xHRUN stylu): 1H = 1. klatka ciecia L z oddechem (guard_loop); 2H = "guard" - garda parady Geralta,
# prawdziwa petla W3 parry_idle_lp (idle_loop, miecz uniesiony przed soba). Wariant lp, bo z postawy lp startuja wszystkie
# ciecia 2H (strong_*_lp: miednica -54 st., parry_idle_lp -73 st., parry_idle_rp +77 st. = 130 st. obrotu na starcie
# kazdego ciosu), parady 2H (parry_f_*_lp zaczynaja sie prawie dokladnie w parry_idle_lp) i ataki z wypadem 2H (_lp).
# Ciecia 2H wchodza z gardy przejsciem bezwladnosciowym (ENTRY_FRAMES), konce wracaja do niej (inertial_recovery).
# Tempo: mocne ciecia W3 maja cios w 0,68 s (szybkie 1H w 0,36 s) - 2H z oryginalnym zamachem bylo wyraznie wolniejsze
# od 1H. windup = klatka ciosu po przyspieszeniu zamachu: 2H cios 0,08 s po 1H, odstep w kombinacji 25 kl. (1,0 s; 1H 23,
# zEBA 2HST3 ~27, natywne 2HST2 40-48). hits = ciec w kombinacji S_xHATTACK (silnik czyta do 10 z DEF_OPT_FRAME).
# combo_open = pierwsze ciecie kombinacji z innego klipu (zestaw, klip, windup): 2H otwiera lekkie ciecie NPC z mieczem
# dwurecznym "combo_start" (21.10: mocne ciecie Geralta trafialo po 0,48 s, natywne 2HST1 po 0,24 s); dalsze ciecia
# dobierane jak dotad po ciaglosci pozy. open_windup = przyspieszony zamach tylko pierwszego ciecia kombinacji (1H:
# szybkie ciecie Geralta trafialo po 0,40 s, natywne 1HST1 po 0,28 s).
NPC_2H_SET = r"animations\man\combat\man_sword_2handed.w2anims"
STYLES = {
    "1H": {"overlay": "HUMANS_ZDF_W3_1H", "stance": "S_1HRUN", "step": 0.3, "combo_step": 0.25, "start": "_rp_",
           "L": "man_geralt_sword_attack_fast_1_rp_40ms", "R": "man_geralt_sword_attack_fast_4_rp_40ms",
           "combo": COMBO_CANDIDATES, "hits": 4, "open_windup": 6},
    "2H": {"overlay": "HUMANS_ZDF_W3_2H", "stance": "S_2HRUN", "step": 0.3, "combo_step": 0.25, "start": "_lp_",
           "L": "man_geralt_sword_attack_strong_7_lp_70ms", "R": "man_geralt_sword_attack_strong_3_lp_70ms",
           "combo": ["man_geralt_sword_attack_strong_%d_%s_70ms" % (i, s) for i in range(1, 11) for s in ("rp", "lp")],
           "guard": "man_geralt_sword_parry_idle_lp", "hits": 6, "windup": 11,
           "combo_open": (NPC_2H_SET, "man_npc_sword_2hand_attack_combo_start_lll_1", 6)},
}


# Alternatywne ciecia boczne 2H (menu "Ciosy boczne 2H", W3Side2H=1 "W3 szybkie"): lekkie ciecia NPC z mieczem
# dwurecznym W3 (man_sword_2handed - obie dlonie na rekojesci), zamach przyspieszony do ciosu w kl. 8 (0,32 s, okno
# kolejnego ciosu 0,44 s; mocne ciecia Geralta 0,48 / 0,60 s, natywne 2HST2 0,20 / 0,56, zEBA 2HST3B 0,20 / 0,44).
# Kierunki sprawdzone azymutem czubka ostrza (swing_sweep, skalibrowany na HUMANS): L = l_2 (z prawej w lewo),
# R = l_left_1 (z lewej w prawo). Nakladka HUMANS_ZDF_W3_2HS ma TYLKO T_2HATTACKL/R - plugin nie zaklada jej na model,
# tylko wstawia jej animacje w miejsce bocznych ciec HUMANS_ZDF_W3_2H (AtkTempo.cpp). Wejscie z gardy parady i powrot
# do niej jak w stylu 2H.
SIDE_WORK = os.path.join(ROOT, "anims", "work", "w3_2h")   # zrzuty klipow spoza zestawu Geralta
SIDE_ALTS = {
    "2HS": {"style": "2H", "overlay": "HUMANS_ZDF_W3_2HS", "set": NPC_2H_SET,
            "L": "man_npc_sword_2hand_attack_l_2", "R": "man_npc_sword_2hand_attack_l_left_1", "windup": 7},
}


def swing_sweep(frames, sk, hit, w=3):
    """Zmiana azymutu czubka ostrza (st.) wokol miednicy w klatkach [hit-w, hit+w] (hit od 0) w ukladzie modelu Gothica
    (przod +Z, lewa -X, ostrze = -X ZS_RIGHTHAND, 90 cm). Ujemna = zamach z prawej w lewo (T_xHATTACKL), dodatnia =
    z lewej w prawo (T_xHATTACKR) - tak wychodza natywne ciecia HUMANS / HUMANS_1HST2 / HUMANS_2HST2."""
    wi, pi = sk.idx("ZS_RIGHTHAND"), sk.idx("BIP01 PELVIS")
    az = []
    for f in range(max(0, hit - w), min(len(frames) - 1, hit + w) + 1):
        W = sk.compose(*frames[f])
        tip = W[wi][:3, 3] + W[wi][:3, :3] @ np.array([-90.0, 0.0, 0.0])
        r = tip - W[pi][:3, 3]
        az.append(math.degrees(math.atan2(r[0], r[2])))
    return sum(((b - a + 180.0) % 360.0) - 180.0 for a, b in zip(az, az[1:]))


def build_side_alt(vfs, sk, grip, key):
    """Nakladka z samymi cieciami bocznymi (SIDE_ALTS[key]) w stylu SIDE_ALTS[key]["style"] (garda, wejscie, powrot)."""
    alt = SIDE_ALTS[key]
    w = alt["style"]
    st = dict(STYLES[w], L=alt["L"], R=alt["R"], windup=alt["windup"])
    gdump = make_pair.dump_clip(make_moves.SWORD_SET, guard_clip(w), os.path.join(make_moves.WORK, guard_clip(w) + ".json"))
    stance = style_guard(gdump, sk, grip, w)[0]
    os.makedirs(SIDE_WORK, exist_ok=True)
    chunks = []
    for side in ("L", "R"):
        ani = "T_%sATTACK%s" % (w, side)
        d = make_pair.dump_clip(alt["set"], alt[side], os.path.join(SIDE_WORK, alt[side] + ".json"))
        frames, hit, ground = style_attack(d, sk, grip, st, side, stance)
        nf = make_pair.write_man(vfs, sk, frames, ani, os.path.join(COMPILED, "%s-%s.MAN" % (alt["overlay"], ani)))
        chunks.append(attack_ani_chunk(ani, nf, hit, ground))
        print("%s %s <- %s: %d kl., trafienie kl. %d (%.2f s), kierunek %+.0f st." % (
            alt["overlay"], ani, alt[side], nf, hit, hit / 25.0, swing_sweep(frames, sk, hit - 1)))
    build_overlay.write_overlay(alt["overlay"], COMPILED, chunks, vfs)


GUARD_FRAMES = 60   # petla postawy W3: 2,4 s oddechu


def guard_clip(w):
    """Klip W3, z ktorego powstaje garda stylu w (2H: parry_idle_lp, 1H: ciecie L)."""
    return STYLES[w].get("guard") or STYLES[w]["L"]


def style_guard(dump, sk, grip, w, n=GUARD_FRAMES):
    """Petla gardy stylu w z zrzutu klipu guard_clip(w): 1H guard_loop (n klatek), 2H idle_loop (petla klipu W3)."""
    if STYLES[w].get("guard"):
        return idle_loop(dump, sk, grip)
    return guard_loop(dump, sk, grip, n)


def idle_loop(dump, sk, grip):
    """Petla postawy z klipu W3, ktory sam jest petla (parry_idle): @25 bez klatki domykajacej (ta sama chwila co 1.).
    Domkniecie bez szwu: niedomkniecie ostatniej klatki (obrot kazdej kosci i korzen) rozlozone liniowo na cala petle -
    klatka 0 bez zmian, domykajaca = klatka 0, wiec ostatnia przechodzi w pierwsza jak sasiednie klatki. Korzen w miejscu:
    klatka 0 w (0, y, 0), kolysanie miednicy w poziomie zostaje (przenoszenie ciezaru). Staly chwyt jak w retarget."""
    d = make_moves.to_entity_start(dump)
    fr = retarget.resample(retarget.retarget(d, sk, grip, False, "w3", weapon="r_weapon"), d["fps"])
    n = len(fr) - 1
    err = [make_moves.rotvec(A.T @ B) for A, B in zip(fr[0][0], fr[n][0])]   # B = A @ exp(err)
    r0 = np.array(fr[0][1], dtype=float)
    rerr = np.array(fr[n][1], dtype=float) - r0
    out = []
    for k in range(n):
        u = k / float(n)
        local = [R.copy() if np.linalg.norm(e) < 1e-9 else R @ make_moves.expmap(-u * e) for R, e in zip(fr[k][0], err)]
        r = np.array(fr[k][1], dtype=float) - u * rerr
        out.append((local, np.array([r[0] - r0[0], r[1], r[2] - r0[2]])))
    return out


def guard_loop(dump, sk, grip, n=GUARD_FRAMES):
    """Postawa gardy W3 (1. klatka ciecia - z niej rusza kazdy atak stylu) z oddechem: tulow pochyla sie o +-1,2 st.
    wokol osi bocznej, biodra o +-0,6 cm w pionie; sinus przez cala petle - ostatnia klatka przechodzi w pierwsza."""
    d = make_moves.to_entity_start(dump)
    local0, root0 = retarget.retarget(d, sk, grip, False, "w3", weapon="r_weapon")[0]
    W = sk.compose(local0, root0)
    si = sk.idx("BIP01 SPINE1")
    Ws = W[si][:3, :3]
    out = []
    for k in range(n):
        ph = 2 * math.pi * k / n
        local = [R.copy() for R in local0]
        a = math.radians(1.2) * math.sin(ph)
        Rx = np.array([[1, 0, 0], [0, math.cos(a), -math.sin(a)], [0, math.sin(a), math.cos(a)]])
        local[si] = local0[si] @ (Ws.T @ Rx @ Ws)   # obrot wokol bocznej osi swiata (X) w ukladzie kosci
        out.append((local, np.array([0.0, root0[1] + 0.6 * math.sin(ph), 0.0])))
    return out


# Atak w biegu gry (T_1HATTACKMOVE / T_2HATTACKMOVE): oCAIHuman::FightMelee przy biegu do przodu i ataku wysyla
# EV_ATTACKRUN, oCNpc::EV_AttackRun gra ja na warstwie 2 (tulow i rece) nad biegiem. Tulow W3 wyciety na te nogi nie
# wyglada dobrze (ciecia W3 z wypadu to piruety z biodrami), wiec styl jej nie podmienia - w stylu W3 plugin przejmuje
# EV_ATTACKRUN i gra caly ruch W3 (make_moves.RUN_ATTACKS) z wejsciem z pozy truchtu W3 (run_pose).
LOCO_SET = r"animations\man\combat\man_geralt_sword_locomotion.w2anims"
LOCO_WORK = os.path.join(ROOT, "anims", "work", "w3loco")   # wspolny cache z make_loco.py
RUN_POSE_CLIP = "combat_locomotion_run_cycle_fast_forward"   # trucht W3 z mieczem (S_xHRUNL stylu, make_loco SW_RUN)


def humans_attack_timing(vfs, name):
    """(klatka trafienia DEF_OPT_FRAME, liczba klatek) animacji ataku z bazowego HUMANS.MSB."""
    ani = None
    for t, _, body in build_overlay.chunks(bytes(vfs.find("HUMANS.MSB").data)):
        if t == 0xF520:
            a = build_overlay.parse_ani(body)
            ani = a if a["name"] == name else None
        elif ani and t == 0xF5A3:
            lines = body[4:].decode("latin1").split("\n")
            if lines[0] == "DEF_OPT_FRAME":
                return int(lines[1].split()[0]), ani["last"] - ani["first"] + 1
    raise KeyError(name)


def run_pose(dump, sk, grip):
    """Poza biegu stylu (1. klatka truchtu W3 z mieczem, w miejscu) - wejscie ataku w biegu."""
    local, root = retarget.retarget(make_moves.to_entity_start(dump), sk, grip, False, "w3", weapon="r_weapon")[0]
    return [R.copy() for R in local], np.array([0.0, root[1], 0.0])


def back_to_guard(sk, st, frames, stance):
    """Koniec animacji stylu -> postawa: przy gardzie parady (2H) bezwladnosciowo z obrotem rozlozonym do
    make_moves.GUARD_OVERLAP klatek przed koncem (make_moves.guard_recovery - ciecia 2H koncza w postawie rp, garda jest
    lp; dlugosc bez zmian), 1H jak dotad (slerp)."""
    if st.get("guard"):
        return make_moves.guard_recovery(sk, frames, stance, REC_FRAMES)
    return recovery.append_recovery(frames, stance, REC_FRAMES)


def style_attack(dump, sk, grip, st, side, stance):
    """Ciecie stylu (side L/R) -> (klatki z przejsciem do postawy, klatka trafienia 1-based, kroki). Przy gardzie parady
    (2H) wejscie z niej bezwladnosciowo; 1H zaczyna sie w 1. klatce ciecia (= garda 1H)."""
    frames, hit = build_attack(dump, sk, grip, st["step"], entry=stance if st.get("guard") else None,
                               windup=st.get("windup"))
    _, ground = sfx_events.motion_sounds(frames, sk)
    return back_to_guard(sk, st, frames, stance), hit, ground


def combo_first(st):
    """Narzucone pierwsze ciecie kombinacji stylu (build_combo first) z st["combo_open"] albo None."""
    if not st.get("combo_open"):
        return None
    cset, clip, wind = st["combo_open"]
    os.makedirs(SIDE_WORK, exist_ok=True)
    return clip, make_pair.dump_clip(cset, clip, os.path.join(SIDE_WORK, clip + ".json")), wind


def style_combo(dumps, sk, grip, st, stance):
    """Kombinacja stylu S_xHATTACK -> (klatki z przejsciem do postawy, zdarzenia, kroki); wejscie i koniec jak ciecia."""
    frames, ev = build_combo(dumps, sk, grip, st["combo_step"], st["hits"], st["start"],
                             entry=stance if st.get("guard") else None, windup=st.get("windup"), first=combo_first(st),
                             open_windup=st.get("open_windup"))
    _, ground = sfx_events.motion_sounds(frames, sk)
    frames = back_to_guard(sk, st, frames, stance)
    ev["end"][-1] = ev["win"][-1] = len(frames)
    return frames, ev, ground


def build_style(vfs, sk, grip, w, st):
    dump = lambda c: make_pair.dump_clip(make_moves.SWORD_SET, c, os.path.join(make_moves.WORK, c + ".json"))
    # postawa stylu (S_xHRUN) = garda W3 (1H: 1. klatka ciecia L, 2H: petla parady); ciecia wychodza z niej i wracaja
    guard = style_guard(dump(guard_clip(w)), sk, grip, w)
    nf = make_pair.write_man(vfs, sk, guard, st["stance"], os.path.join(COMPILED, "%s-%s.MAN" % (st["overlay"], st["stance"])))
    chunks = [msblib.ani_chunk(st["stance"], 1, st["stance"], 0.1, 0.1, "MI", "ZDISM_W3.ASC", 1, nf)]
    stance = guard[0]
    for side in ("L", "R"):
        ani = "T_%sATTACK%s" % (w, side)
        frames, hit, ground = style_attack(dump(st[side]), sk, grip, st, side, stance)
        nf = make_pair.write_man(vfs, sk, frames, ani, os.path.join(COMPILED, "%s-%s.MAN" % (st["overlay"], ani)))
        chunks.append(attack_ani_chunk(ani, nf, hit, ground))
        print("%s <- %s: %d kl., trafienie kl. %d (%.2f s), krokow %d" % (ani, st[side], nf, hit, hit / 25.0, len(ground)))
    for k, clip in enumerate(PARADES[w]):
        ani = "T_%sPARADE_0%s" % (w, ("", "_A2", "_A3")[k])
        frames = build_parade(dump(clip), sk, grip, stance, st["step"], smooth=bool(st.get("guard")))
        _, ground = sfx_events.motion_sounds(frames, sk)
        nf = make_pair.write_man(vfs, sk, frames, ani, os.path.join(COMPILED, "%s-%s.MAN" % (st["overlay"], ani)))
        chunks.append(parade_ani_chunk(ani, nf, ground))
        print("%s <- %s: %d kl. (blok)" % (ani, clip, nf))
    ani = "S_%sATTACK" % w
    frames, ev, ground = style_combo({c: dump(c) for c in st["combo"]}, sk, grip, st, stance)
    nf = make_pair.write_man(vfs, sk, frames, ani, os.path.join(COMPILED, "%s-%s.MAN" % (st["overlay"], ani)))
    chunks.append(combo_ani_chunk(ani, nf, ev, ground))
    build_overlay.write_overlay(st["overlay"], COMPILED, chunks, vfs)
    print("%s (kombinacja): %d kl., trafienia %s, konce %s, okna %s" % (ani, nf, ev["opt"], ev["end"], ev["win"]))
    print("   kolejnosc: %s" % " -> ".join(c.replace("man_geralt_sword_attack_", "") for c in ev["clips"]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", choices=sorted(STYLES) + sorted(SIDE_ALTS), help="tylko jeden styl / zestaw ciec bocznych")
    a = ap.parse_args()
    vfs = build_overlay.game_vfs()
    sk = retarget.GothicSkeleton(vfs)
    grip = make_pair.grip_local(vfs, sk)
    os.makedirs(COMPILED, exist_ok=True)
    for old in os.listdir(COMPILED):   # stare nakladki (przed stylami) i atak w biegu z gorna polowa (teraz ruch pluginu)
        u = old.upper()
        if u.startswith(("HUMANS_ZDF_W3ATK", "HUMANS_ZDF_W3COMBO")) or (u.startswith("HUMANS_ZDF_W3_") and "ATTACKMOVE" in u):
            os.remove(os.path.join(COMPILED, old))
    for w, st in STYLES.items():
        if not a.only or a.only == w:
            build_style(vfs, sk, grip, w, st)
    for key in SIDE_ALTS:
        if not a.only or a.only == key:
            build_side_alt(vfs, sk, grip, key)


if __name__ == "__main__":
    main()
