r"""Finisher parowany z Wiedzmina 3: animacja bohatera i ofiary zgrane jak w grze. TYLKO DO UZYTKU PRYWATNEGO
(animacje CD Projekt RED) - wynik trafia do data_w3 / zDismember_W3.vdf, nigdy do wydania.

Uzycie: py -3.12 -I make_pair.py W3_01 man_finisher_01_rp [--target neck|chest|belly] [--window 20,35] [--cut HEAD]
       [--weight 10] [--dazed-from 20 | --dazed-rise [--rise-speed 1.4]]   (wszystkie pary naraz: make_all_pairs.cmd)
Wynik (data_w3): nakladka bohatera HUMANS_ZDF_<ID> (T_1HSFINISH i T_2HSFINISH, w miejscu), nakladka ofiary
HUMANS_ZDF_<ID>_V (T_STAND_2_WOUNDED = czesc przed cieciem; T_DEAD/T_DEADB = czesc po cieciu; S_DEAD/S_DEADB =
ostatnia klatka), sekcje [<ID>_1H] i [<ID>_2H] w System\zDismember\finishers_w3.ini z torami ruchu.
Uklad: para zestawiona przez kosci Reference (jak w W3), bohater w klatce 0 stoi w (0,0) i patrzy na ofiare
wzdluz +Z; tory miednicy (x w prawo, z do przodu, cm) prowadzi plugin, a animacje sa w miejscu (ruch z animacji
gasi blokada sterowania). Animacja ofiary jest w ukladzie jej voba, obroconego twarza do bohatera.
"""
import argparse, json, math, os, re, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import manlib, msblib
import retarget, build_overlay, recovery, sfx_events
from retarget import qmat

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(ROOT, "anims", "work", "w3")
DATA = os.path.join(ROOT, "data_w3")
COMPILED = os.path.join(DATA, "_work", "Data", "Anims", "_compiled")
INI = os.path.join(DATA, "System", "zDismember", "finishers_w3.ini")
BLENDER45 = os.environ.get("BLENDER45_EXE", "blender")      # Blender 4.5 (importer W3 - narzedzie spoza repozytorium)
W3_DUMP = os.environ.get("W3_DUMP_PY", "")                 # skrypt zrzutu animacji W3 dla Blendera - spoza repozytorium
HERO_SET = r"animations\interaction\finishers\geralt_finishers.w2anims"
VICTIM_SET = r"animations\interaction\finishers\man_finishers.w2anims"
REC_FRAMES = 8   # przejscie bohatera do postawy Gothica po finiszerze (0,32 s)
BLADE_CM = (15.0, 95.0)   # ostrze wzdluz -X gniazda ZS_RIGHTHAND: od jelca do sztychu (cm)


def ry(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1.0, 0], [-s, 0, c]])


def body_yaw(root_rot, sk):
    """Kat obrotu ciala wokol Y (0 = przod +Z) z obrotu korzenia BIP01 wzgledem pozy spoczynkowej."""
    f = root_rot @ sk.local[0][:3, :3].T @ np.array([0.0, 0.0, 1.0])
    return math.atan2(f[0], f[2])


def grip_local(vfs, sk, ani="T_1HATTACKL"):
    m = manlib.Man(bytes(vfs.find("HUMANS-%s.MAN" % ani).data))
    gi = sk.idx("ZS_RIGHTHAND")
    (x, y, z, w), _ = m.decode(m.sample(0, m.nodes.index(gi)))
    return qmat(-x, -y, -z, w)


def bone_frame(dump, name, frame=0):
    i = [b["name"] for b in dump["bones"]].index(name)
    return np.array(dump["frames"][frame][i])


def flat(M):
    """Sam obrot wokol pionu (Z w Blenderze) i przesuniecie w poziomie - bez przechylow."""
    a = math.atan2(M[1][0], M[0][0])
    F = np.eye(4)
    F[:2, :2] = [[math.cos(a), -math.sin(a)], [math.sin(a), math.cos(a)]]
    F[:2, 3] = M[:2, 3]
    return F


def moved(dump, M):
    out = dict(dump)
    out["frames"] = [[(M @ np.array(m)).tolist() for m in fr] for fr in dump["frames"]]
    return out


def align_w3(dh, dv):
    """Uklad pary jak w W3 (syncManager.ws: useRefBone = 'Reference', SRT_TowardsOtherEntity): kosc Reference postaci
    to vob drugiej postaci w klatce 0, wiec Trajectory ofiary trafia na Reference bohatera. Potem caly uklad tak, zeby
    vob bohatera (Trajectory kl. 0) stal w (0,0) i patrzyl na ofiare wzdluz +Y Blendera (= +Z Gothica).
    Klipy *_remaster: Reference idzie za Trajectory, a obie postacie sa we wspolnej przestrzeni sceny."""
    th, rh, tv = bone_frame(dh, "Trajectory"), bone_frame(dh, "Reference"), bone_frame(dv, "Trajectory")
    if np.linalg.norm(rh[:2, 3] - th[:2, 3]) < 0.05:
        victim_to_hero, target = np.eye(4), tv
    else:
        victim_to_hero, target = flat(rh) @ np.linalg.inv(flat(tv)), rh
    fx, fy = target[0, 3] - th[0, 3], target[1, 3] - th[1, 3]
    a = math.pi / 2 - math.atan2(fy, fx)
    H = np.eye(4)
    H[:2, :2] = [[math.cos(a), -math.sin(a)], [math.sin(a), math.cos(a)]]
    H[:2, 3] = -(H[:2, :2] @ th[:2, 3])
    return moved(dh, H), moved(dv, H @ victim_to_hero)


def transform(frames, base):
    """Przesuwa korzen: bohater w klatce 0 w (0,0) (to samo dla ofiary - wspolny uklad)."""
    return [(local, np.array(root) - base) for local, root in frames]


def facing_hero(frames):
    """Uklad voba ofiary obroconego twarza do bohatera (o 180 stopni): obrot korzenia, pozycje bez zmian."""
    flip = ry(math.pi)
    return [([flip @ local[0]] + list(local[1:]), root) for local, root in frames]


def inplace(frames):
    return [(local, np.array([0.0, root[1], 0.0])) for local, root in frames]


def seg_dist(p, a, b):
    ab = b - a
    t = min(1.0, max(0.0, float(np.dot(p - a, ab) / np.dot(ab, ab))))
    return float(np.linalg.norm(p - (a + ab * t)))


def node_world(frames, sk, name):
    i = sk.idx(name)
    return [sk.compose(local, root)[i] for local, root in frames]


def blade_contact(blade, pts, lo, hi):
    """Najblizsze przejscie ostrza (odcinek na klatke) obok punktu ciala w klatkach [lo, hi): (cm, ulamek klatki).
    Szybki zamach przeskakuje kilkadziesiat cm miedzy klatkami - probkowanie takze miedzy nimi."""
    n = len(blade)
    best = (float("inf"), float(lo))
    for k in range(max(0, lo), min(hi, n)):
        k1 = min(k + 1, n - 1)
        for s in range(20):
            u = s / 20.0
            lerp = lambda x, y: x + (y - x) * u
            d = seg_dist(lerp(pts[k], pts[k1]), lerp(blade[k][0], blade[k1][0]), lerp(blade[k][1], blade[k1][1]))
            best = min(best, (d, k + u))
    return best


# cele ciosu: punkt miedzy dwoma wezlami ofiary; szyja = sciecie glowy, klatka/brzuch = pchniecie (cut=NONE)
TARGETS = {"neck": ("BIP01 NECK", "BIP01 HEAD"), "head": ("BIP01 HEAD", "BIP01 HEAD"), "chest": ("BIP01 SPINE1", "BIP01 SPINE2"), "belly": ("BIP01", "BIP01 SPINE"),
           "arm_l": ("BIP01 L UPPERARM", "BIP01 L FOREARM"), "arm_r": ("BIP01 R UPPERARM", "BIP01 R FOREARM"),
           "leg_l": ("BIP01 L THIGH", "BIP01 L CALF"), "leg_r": ("BIP01 R THIGH", "BIP01 R CALF")}
WOUND = {"chest": "BIP01 SPINE2", "belly": "BIP01 SPINE1"}
# czesc odcinana przy danym celu (reszta = pchniecie bez odciecia, krew z WOUND)
CUT_FOR = {"neck": "HEAD", "head": "HEAD", "arm_l": "UPPERARM_L", "arm_r": "UPPERARM_R", "leg_l": "THIGH_L", "leg_r": "THIGH_R"}
SETS = {"base": (HERO_SET, VICTIM_SET),
        "dlc16": (r"dlc\dlc16\data\animations\interaction\finishers\geralt_finishers.w2anims",
                  r"dlc\dlc16\data\animations\interaction\finishers\man_finishers.w2anims")}


def build_pair(dh, dv, sk, vfs, target="neck", window=None):
    grip = grip_local(vfs, sk)
    dh, dv = align_w3(dh, dv)
    hero = retarget.resample(retarget.retarget(dh, sk, grip, False, "w3", weapon="r_weapon", wrist_limit=75.0), dh["fps"])
    victim = retarget.resample(retarget.retarget(dv, sk, grip, False, "w3"), dv["fps"])
    base = np.array([hero[0][1][0], 0.0, hero[0][1][2]])
    return pair_from_frames(transform(hero, base), transform(victim, base), sk, target, window)


def blade_segments(hero, sk):
    """Ostrze w kazdej klatce: (jelec, sztych) w ukladzie pary."""
    return [(g[:3, 3] - g[:3, 0] * BLADE_CM[0], g[:3, 3] - g[:3, 0] * BLADE_CM[1]) for g in node_world(hero, sk, "ZS_RIGHTHAND")]


def pair_from_frames(hero, victim, sk, target="neck", window=None):
    """Para z klatek w ukladzie pary (bohater w kl. 0 w (0,0), przod +Z): klatka ciosu, tory, animacje w miejscu."""
    # ofiara moze padac dluzej niz trwa animacja bohatera (DLC: glowa, szyja) - bez przycinania; cios tylko w czasie bohatera
    n = min(len(hero), len(victim))

    # cios: klatka, w ktorej ostrze przechodzi najblizej celu (bez 3 pierwszych i ostatnich klatek albo w oknie)
    blade = blade_segments(hero, sk)
    lo, hi = window if window else (3, max(4, n - 3))
    hi = min(hi, n)
    contacts = {}
    for name, (a, b) in TARGETS.items():
        pts = [(x[:3, 3] + y[:3, 3]) * 0.5 for x, y in zip(node_world(victim[:n], sk, a), node_world(victim[:n], sk, b))]
        contacts[name] = blade_contact(blade[:n], pts, lo, hi)
    dist, frame = contacts[target]
    cut = int(round(frame))

    vin = inplace(facing_hero(victim))
    return {
        "hero": hero, "victim": victim,
        "heroInplace": inplace(hero), "victimInplace": vin,
        "heroTraj": [(float(r[0]), float(r[2])) for _, r in hero],
        "victimTraj": [(float(r[0]), float(r[2])) for _, r in victim],
        "cutFrame": cut, "cutDist": dist, "contacts": contacts,
        "victimA": vin[:cut + 1], "victimB": vin[cut:],
        "victimHold": [vin[-1], vin[-1]],
    }


# Wstawanie z kleczenia: w HUMANS.MSB T_PRAY_2_STAND to alias T_STAND_2_PRAY odtwarzanej od konca (z pozy S_PRAY,
# w ktorej kleczy oszolomiony - nakladka HUMANS_ZDF_DAZE z build_daze.py).
RISE_ANI = "T_STAND_2_PRAY"


def man_frames(vfs, sk, ani):
    """Wszystkie klatki animacji z HUMANS jako (lokalne obroty 3x3, pozycja korzenia); wezly spoza MAN = spoczynek."""
    m = manlib.Man(bytes(vfs.find("HUMANS-%s.MAN" % ani).data))
    out = []
    for f in range(m.frames):
        local = [l[:3, :3].copy() for l in sk.local]
        root = np.array(sk.root_pos, dtype=float)
        for k, ni in enumerate(m.nodes):
            (x, y, z, w), pos = m.decode(m.sample(f, k))
            local[ni] = qmat(-x, -y, -z, w)
            if sk.parent[ni] < 0:
                root = np.array(pos, dtype=float)
        out.append((local, root))
    return out, (m.fps or retarget.FPS_OUT)


def kneel_rise(vfs, sk, speed=1.0):
    """Wstawanie z kleczenia S_PRAY (T_PRAY_2_STAND z gry) w ukladzie voba ofiary (przod +Z), tempo x speed, 25 kl./s."""
    frames, fps = man_frames(vfs, sk, RISE_ANI)
    return retarget.resample(frames[::-1], fps * speed)


def crossfade(a, b, n):
    """Przejscie z sekwencji a w sekwencje b (obie trwaja dalej) przez n klatek: slerp obrotow, lerp korzenia
    z waga smoothstep; wynik ma len(a) == len(b) == n klatek."""
    out = []
    for k in range(n):
        u = (k + 1) / (n + 1)
        w = u * u * (3 - 2 * u)
        (la, ra), (lb, rb) = a[k], b[k]
        out.append(([qmat(*retarget.slerp(retarget.quat_from_mat(A), retarget.quat_from_mat(B), w)) for A, B in zip(la, lb)],
                    np.array(ra) * (1 - w) + np.array(rb) * w))
    return out


def windup_frame(hero, sk, cut):
    """Najglebszy zamach przed pchnieciem: klatka (od 3. do ciosu), w ktorej sztych jest najdalej z tylu (min z)."""
    blade = blade_segments(hero[:cut], sk)
    return min(range(3, cut), key=lambda k: blade[k][1][2])


def stance_loop(vfs, sk, ani, n):
    """n klatek petli postawy walki z gry (25 kl./s), korzen w miejscu (x = z = 0) - bohater stoi na swoim vobie."""
    frames, fps = man_frames(vfs, sk, ani)
    loop = retarget.resample(frames, fps)
    return [(loop[k % len(loop)][0], np.array([0.0, loop[k % len(loop)][1][1], 0.0])) for k in range(n)]


def rise_pair(p, vfs, sk, target, speed=1.4, hero_blend=5, victim_blend=5):
    """Egzekucja oszolomionego z pary, w ktorej ofiara nie kleczy (pchniecie W3_05/06 w stojacego).
    Ofiara: zaczyna w pozie kleczenia Gothica (S_PRAY - tak kleczy oszolomiony), wstaje (T_PRAY_2_STAND z gry, tempo
    x speed) i przez victim_blend klatek przechodzi w poze ofiary W3, ktora stoi jeszcze przed ciosem.
    Bohater: w czasie wstawania stoi w postawie walki z gry (S_1HRUN / S_2HRUN - w niej zaczyna egzekucje), bo w gardzie
    W3 (kl. 0-8) sztych jest przy gardle / ramieniu ofiary i wstajaca ofiara weszlaby na ostrze; potem przez
    hero_blend klatek przechodzi w zamach W3 i od najglebszego zamachu gra W3 bez zmian (wypad, pchniecie, chwyt za
    bark, wyrwanie ostrza). Od wejscia bohatera obie postacie sa w zgranej animacji W3 - geometria ostrza wzgledem
    klatki piersiowej jak w W3, tylko przesunieta w czasie."""
    rise = kneel_rise(vfs, sk, speed)
    n_rise, cut = len(rise), p["cutFrame"]
    entry = windup_frame(p["hero"], sk, cut)
    shift = n_rise + hero_blend - entry   # klatka nowa = klatka W3 + shift (bohater czeka n_rise klatek)
    j0 = n_rise - shift                    # klatka ofiary W3, w ktorej zaczyna sie jej przejscie z wstawania
    if j0 < 0 or entry - hero_blend < 0:
        raise ValueError("zamach za wczesnie na przejscie (kl. %d, przejscie %d kl.)" % (entry, hero_blend))
    w3h, w3v = p["hero"], p["victim"]

    def hero_for(stance):
        lead = stance_loop(vfs, sk, stance, n_rise + hero_blend)
        return lead[:n_rise] + crossfade(lead[n_rise:], w3h[entry - hero_blend:entry], hero_blend) + list(w3h[entry:])

    flip = ry(math.pi)   # vob ofiary patrzy na bohatera (-Z pary): obrot korzenia i przesuniecie o 180 stopni
    stand_root = rise[-1][1]
    # vob ofiary tak, zeby po wstaniu miednica trafila w miednice ofiary W3 z poczatku przejscia
    vob = np.array([w3v[j0][1][0] + stand_root[0], 0.0, w3v[j0][1][2] + stand_root[2]])
    victim = [([flip @ local[0]] + list(local[1:]), vob + flip @ root) for local, root in rise]
    victim += crossfade([victim[-1]] * victim_blend, w3v[j0:j0 + victim_blend], victim_blend) + list(w3v[j0 + victim_blend:])

    hero = hero_for("S_1HRUN")
    q = pair_from_frames(hero, victim, sk, target, (cut + shift - 2, cut + shift + 3))
    q.update(heroInplaceBy={"S_1HRUN": q["heroInplace"], "S_2HRUN": inplace(hero_for("S_2HRUN"))},
             riseFrames=n_rise, riseShift=shift, entryFrame=entry,
             note="ofiara wstaje z kleczenia (T_PRAY_2_STAND x%g, %d kl.), bohater z postawy wchodzi w zamach W3 kl. %d" % (
                 speed, n_rise, entry))
    return q


def slice_pair(p, start):
    """Para od klatki 'start' (np. ofiara juz kleczy - egzekucja oszolomionego): tory od nowa od bohatera."""
    hx, hz = p["heroTraj"][start]
    cut = p["cutFrame"] - start
    vin = p["victimInplace"][start:]
    return dict(p, hero=p["hero"][start:], victim=p["victim"][start:],
                heroInplace=p["heroInplace"][start:], victimInplace=vin,
                heroTraj=[(x - hx, z - hz) for x, z in p["heroTraj"][start:]],
                victimTraj=[(x - hx, z - hz) for x, z in p["victimTraj"][start:]],
                cutFrame=cut, victimA=vin[:cut + 1], victimB=vin[cut:])


def dump_clip(anim_set, clip, out):
    if not os.path.exists(out):
        subprocess.run([BLENDER45, "-b", "--factory-startup", "-P", W3_DUMP, "--", anim_set, clip, out],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return json.load(open(out))


def write_man(vfs, sk, frames, name, path, bbox_inplace=False):
    template = manlib.Man(bytes(vfs.find("HUMANS-T_1HSFINISH.MAN").data))
    nodes = [i for i, nm in enumerate(sk.names) if not nm.startswith("ZS_") or nm == "ZS_RIGHTHAND"]
    retarget.write_man(template, frames, sk, name, nodes, path, bbox_inplace=bbox_inplace)
    return len(frames)


def write_ini_sections(sections):
    os.makedirs(os.path.dirname(INI), exist_ok=True)
    text = open(INI, encoding="ascii").read() if os.path.exists(INI) else (
        "; Finishery parowane z Wiedzmina 3 - TYLKO UZYTEK PRYWATNY (animacje CD Projekt RED), nie do wydania.\n"
        "; Generowane przez anims\\pipeline\\make_pair.py. heroTraj/victimTraj: pary x,z (cm) co 1/trajFps s.\n")
    for sid, lines in sections:
        section = "[%s]\n%s\n" % (sid, "\n".join(lines))
        pattern = re.compile(r"^\[%s\]\n(?:(?!\[).*\n?)*" % re.escape(sid), re.M)
        text = pattern.sub(section + "\n", text) if pattern.search(text) else text.rstrip("\n") + "\n\n" + section
    open(INI, "w", encoding="ascii", newline="\n").write(text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("id"); ap.add_argument("clip")
    ap.add_argument("--target", default="neck", choices=sorted(TARGETS), help="cel ciosu: neck = sciecie glowy, chest/belly = pchniecie")
    ap.add_argument("--window", help="okno klatek (25 kl./s) z ciosem, np. 20,35")
    ap.add_argument("--cut", help="czesc do odciecia (domyslnie wg celu: HEAD, UPPERARM_L/R, THIGH_L/R; pchniecia NONE)")
    ap.add_argument("--wound", help="wezel rany przy cut=NONE (domyslnie wg celu: chest BIP01 SPINE2, belly BIP01 SPINE1) - "
                                    "pchniecie z oknem ciosu wg innego celu (W3_03/04: wyciagniecie ostrza z klatki obok szyi)")
    ap.add_argument("--set", default="base", choices=sorted(SETS), help="zestaw animacji W3 (base / dlc16 - finishery z odcinaniem)")
    ap.add_argument("--slowmo", default="0.35,-0.25,0.35")
    ap.add_argument("--weight", default="10")
    ap.add_argument("--dazed-from", type=int, help="dodatkowy wariant egzekucji oszolomionego od tej klatki (@25, ofiara kleczy)")
    ap.add_argument("--dazed-rise", action="store_true",
                    help="dodatkowy wariant egzekucji oszolomionego dla par bez kleczenia (pchniecia W3_05/06): ofiara "
                         "wstaje z kleczenia S_PRAY, bohater z postawy walki wchodzi w zamach W3")
    ap.add_argument("--rise-speed", type=float, default=1.4, help="tempo wstawania ofiary (T_PRAY_2_STAND = 1,4 s przy 1,0)")
    a = ap.parse_args()
    if a.dazed_rise and a.dazed_from is not None:
        sys.exit("--dazed-from i --dazed-rise wykluczaja sie (jeden wariant _D na pare)")
    fid = a.id.upper()
    cut = (a.cut or CUT_FOR.get(a.target, "NONE")).upper()
    window = tuple(int(x) for x in a.window.split(",")) if a.window else None
    vfs = build_overlay.game_vfs()
    sk = retarget.GothicSkeleton(vfs)
    os.makedirs(WORK, exist_ok=True)
    hero_set, victim_set = SETS[a.set]
    tag = "" if a.set == "base" else a.set + "_"
    dh = dump_clip(hero_set, a.clip, os.path.join(WORK, "hero_%s%s.json" % (tag, a.clip)))
    dv = dump_clip(victim_set, a.clip, os.path.join(WORK, "victim_%s%s.json" % (tag, a.clip)))
    p = build_pair(dh, dv, sk, vfs, a.target, window)
    os.makedirs(COMPILED, exist_ok=True)

    sections = write_pair(vfs, sk, fid, p, cut, a, "STANDING")
    if a.dazed_from is not None:
        if not 0 < a.dazed_from < p["cutFrame"]:
            sys.exit("--dazed-from musi byc przed klatka ciosu (%d)" % p["cutFrame"])
        sections += write_pair(vfs, sk, fid + "_D", slice_pair(p, a.dazed_from), cut, a, "DAZED")
    if a.dazed_rise:
        sections += write_pair(vfs, sk, fid + "_D", rise_pair(p, vfs, sk, a.target, a.rise_speed), cut, a, "DAZED")
    write_ini_sections(sections)


def write_pair(vfs, sk, fid, p, cut, a, pose):
    """Nakladki bohatera i ofiary + sekcje ini <ID>_1H/_2H."""
    hero_name, victim_name = "HUMANS_ZDF_%s" % fid, "HUMANS_ZDF_%s_V" % fid
    # dzwieki: kroki i swisty ostrza bohatera, dobicie w klatce ciosu; ofiara - kroki i upadek ciala
    sfx, ground = sfx_events.motion_sounds(p["hero"], sk, whoosh=True)
    sfx.append((p["cutFrame"] + 1, "FIG_SWORDFINAL"))
    chunks = []
    for ani, stance in (("T_1HSFINISH", "S_1HRUN"), ("T_2HSFINISH", "S_2HRUN")):
        # koniec animacji W3 przechodzi w postawe Gothica (bez przeskoku po finiszerze); poczatek moze byc wlasny dla
        # broni (rise_pair: bohater czeka w postawie walki 1H / 2H)
        hero = p.get("heroInplaceBy", {}).get(stance, p["heroInplace"])
        rec = recovery.append_recovery(hero, recovery.stance_pose(vfs, sk, stance), REC_FRAMES)
        nf = write_man(vfs, sk, rec, ani, os.path.join(COMPILED, "%s-%s.MAN" % (hero_name, ani)))
        chunks.append(msblib.ani_chunk(ani, 1, stance, 0.1, 0.1, "M.", "ZDISM_W3.ASC", 1, nf)
                      + sfx_events.events_block(sfx=sfx, ground=ground))
    build_overlay.write_overlay(hero_name, COMPILED, chunks, vfs)
    ck = p["cutFrame"]
    sfx_a, ground_a = sfx_events.motion_sounds(p["victim"][:ck + 1], sk)
    sfx_b, ground_b = sfx_events.motion_sounds(p["victim"][ck:], sk, fall=True)

    # z kleczenia ofiara wchodzi w animacje wolniej (0,3 s), zeby nie podrywala sie skokiem
    victim_blend = 0.3 if pose == "DAZED" else 0.1
    chunks = []
    for ani, frames, nxt, blend, ev in (("T_STAND_2_WOUNDED", p["victimA"], "", victim_blend, (sfx_a, ground_a)),
                                        ("T_DEAD", p["victimB"], "S_DEAD", 0.05, (sfx_b, ground_b)),
                                        ("T_DEADB", p["victimB"], "S_DEADB", 0.05, (sfx_b, ground_b)),
                                        ("S_DEAD", p["victimHold"], "S_DEAD", 0.0, ([], [])),
                                        ("S_DEADB", p["victimHold"], "S_DEADB", 0.0, ([], []))):
        nf = write_man(vfs, sk, frames, ani, os.path.join(COMPILED, "%s-%s.MAN" % (victim_name, ani)))
        chunks.append(msblib.ani_chunk(ani, 1, nxt, blend, 0.0, "M.", "ZDISM_W3.ASC", 1, nf)
                      + sfx_events.events_block(sfx=ev[0], ground=ev[1]))
    build_overlay.write_overlay(victim_name, COMPILED, chunks, vfs)

    fps = retarget.FPS_OUT
    traj = lambda t: " ".join("%.1f,%.1f" % xz for xz in t)
    vx, vz = p["victimTraj"][0]
    sections = []
    for weapon in ("1H", "2H"):
        sections.append(("%s_%s" % (fid, weapon), [
            "weapon=%s" % weapon, "victim=HUMAN", "pose=%s" % pose,
            "overlay=%s" % hero_name, "heroAni=T_%sSFINISH" % weapon, "cut=%s" % cut,
        ] + (["wound=%s" % (a.wound or WOUND[a.target])] if cut == "NONE" and (a.wound or a.target in WOUND) else []) + [
            "cutTime=%g" % round(p["cutFrame"] / fps, 3), "victimOffset=%.1f,0,%.1f" % (vx, vz),
            "victimOverlay=%s" % victim_name, "victimAni=T_STAND_2_WOUNDED",
            "slowmo=%s" % a.slowmo, "weight=%s" % a.weight, "trajFps=%g" % fps,
            "heroTraj=%s" % traj(list(p["heroTraj"]) + [p["heroTraj"][-1]] * REC_FRAMES), "victimTraj=%s" % traj(p["victimTraj"]),
            "; zrodlo W3: %s, %d kl. @%g, cios (%s) kl. %d, ostrze %.0f cm od celu" % (
                a.clip, len(p["hero"]), fps, a.target, p["cutFrame"], p["cutDist"]),
        ] + (["; " + p["note"]] if p.get("note") else [])))
    print("%s (%s): %d kl., cios %s kl. %d (%.2f s, ostrze %.0f cm od celu), cut=%s, ofiara na starcie (%.0f, %.0f)" % (
        fid, pose, len(p["hero"]), a.target, p["cutFrame"], p["cutFrame"] / fps, p["cutDist"], cut, vx, vz))
    return sections


if __name__ == "__main__":
    main()
