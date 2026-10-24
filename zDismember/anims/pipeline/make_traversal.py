r"""Przeskoki i wspinanie w biegu z Wiedzmina 3 (parkour) dla zDismember. TYLKO DO UZYTKU PRYWATNEGO (animacje
CD Projekt RED) - wynik trafia do data_w3 / zDismember_W3.vdf, nigdy do wydania.

Uzycie: py -3.12 -I make_traversal.py [--only CLIMB_100_LF,VAULT_150_RF,CLIMB_IDLE_250] [--report]
Wynik (data_w3): nakladki HUMANS_ZDF_MV_TRAV_<ID> i System\zDismember\traversal_w3.ini (osobny plik - make_moves.py
przepisuje caly moves_w3.ini).

Zrodlo: zestaw man_geralt_exploration.w2anims - climb_run_<h>_<lf|rf> (wspiecie z biegu na krawedz <h> cm) i
vault_run_<h>_<lf|rf> (przeskok w biegu) z ladowaniem vault_run_<h>_<f>_land_to_run (30 cm: klip konczy sie w biegu,
250 cm: brak wlasnego ladowania - ogolne land_run_<f>_to_run). lf/rf = noga w powietrzu na starcie klipu (lewa = prawa
podparta), plugin wybiera wariant wg nogi podpartej w biegu. Z miejsca (bohater stoi / idzie): climb_idle_<h> i
vault_idle_<h> (30-300 cm, bez wariantow nogi) - sekcje CLIMB_IDLE_<h> / VAULT_IDLE_<h> z start=IDLE.

Tor 3D (ini traj=x,y,z): x w prawo, y = wysokosc voba nad ziemia startu, z do przodu (cm, co 1/25 s). Animacja jest
w miejscu z korzeniem na STALEJ wysokosci postawy (P0 = BIP01 z S_RUN): cale pionowe przemieszczenie miednicy niesie
vob (plugin go podnosi), wiec silnik nie ma czego przenosic z ruchu korzenia (flagi "MF": F = lot - CheckPhysics nie
startuje spadania, gdy vob wisi nad ziemia). Na poczatku (ENTRY_FADE) korzen schodzi z wysokosci klipu do P0, a vob
startuje z ziemi - bez skoku voba i kamery.
Punkty klipu dla pluginu (s): takeoff (miednica rusza w gore) .. contact (pierwsze podparcie na gorze - dlon albo stopa)
= okno, w ktorym plugin dopasowuje wysokosc do prawdziwej przeszkody; przeskok: release (dlonie puszczaja gore) ..
land (stopa na ziemi za przeszkoda) = okno dopasowania do ziemi za przeszkoda; exit = od tej chwili z W prosto w bieg;
edge = odleglosc sciany od voba na starcie (plugin skaluje rozbieg do prawdziwej odleglosci); depth = najmniejsza
glebokosc gory (wspiecie), thick = najwieksza grubosc gory (przeskok).
Nazwy animacji: bez broni T_CANNOTTAKE (w bazowym HUMANS, silnik gra ja tylko przy nieudanym podniesieniu
przedmiotu - w trakcie ruchu bohater ma blokade), piesci / 1H / 2H - T_xPARADEJUMPB jak uniki (ruch chwilowy
podmienia odskok parady).
"""
import argparse, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import msblib
import retarget, build_overlay, make_pair, make_moves, recovery, sfx_events

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(ROOT, "anims", "work", "w3trav")
COMPILED = make_moves.COMPILED
INI = os.path.join(make_moves.DATA, "System", "zDismember", "traversal_w3.ini")
EXPLO_SET = chr(92).join(["animations", "man", "movement", "man_geralt_exploration.w2anims"])
BUCKETS = (30, 100, 150, 200, 250)
IDLE_BUCKETS = (30, 100, 150, 200, 250, 300)   # z miejsca: climb_idle_<h> / vault_idle_<h> (jest tez 300)
FEET = ("lf", "rf")
IDLE = "idle"           # "noga" sekcji z miejsca (klipy idle nie maja wariantow lf / rf)
KINDS = ("CLIMB", "VAULT")
# animacja na tryb broni (bez broni, piesci, 1H, 2H) i postawa, do ktorej wraca
ANIS = (("T_CANNOTTAKE", "S_RUN"), ("T_FISTPARADEJUMPB", "S_FISTRUN"), ("T_1HPARADEJUMPB", "S_1HRUN"),
        ("T_2HPARADEJUMPB", "S_2HRUN"))
FPS = retarget.FPS_OUT
REC_FRAMES = 6          # przejscie do postawy Gothica na koncu (0,24 s)
ENTRY_FADE = 0.2        # s - korzen schodzi z wysokosci klipu do P0, vob przejmuje ruch pionowy
FLAGS = "MF"            # M: ruch (korzen staly - bez przemieszczenia), F: lot (bez spadania i wyrownania do ziemi)
SETTLE_SPEED = 25.0     # cm/s - wolniej porusza sie miednica = postac stoi (koniec wspiecia, ogon klipu W3 uciety)
SETTLE_KEEP = 4         # klatek po ustaniu ruchu (dalej przejscie do postawy)
LOW_NODES = ("BIP01 L FOOT", "BIP01 R FOOT", "BIP01 L TOE0", "BIP01 R TOE0", "BIP01 L CALF", "BIP01 R CALF")
FEET_NODES = ("BIP01 L FOOT", "BIP01 R FOOT")
HAND_NODES = ("BIP01 L HAND", "BIP01 R HAND")
PLANT_SPEED = 80.0      # cm/s - dlon / stopa wolniej (poziomo i pionowo) = podparcie (oparta na gorze)
LOW_GAP = 15.0          # cm - punkt nog nizej niz gora o tyle = na pewno przed sciana (retarget przesuwa stopy)
FACE_MARGIN = 6.0       # cm - sciana tyle za najdalszym takim punktem
EDGE_SHARE = 0.3        # sciana w tym miejscu przedzialu [za nogami, przed podparciem]
BODY_RADIUS = 20.0      # cm - glebokosc gory potrzebna za koncowa pozycja (wspiecie; plugin daje jeszcze 20 cm zapasu)
VAULT_THICK_MAX = 120.0 # cm - przeskok: gora najwyzej tyle (dalej to juz wspiecie)


def section_id(kind, h, f):
    if f == IDLE:
        return "%s_IDLE_%d" % (kind, h)
    return "%s_%d_%s" % (kind, h, f.upper())


def section_clips(kind, h, f, names):
    """Klipy W3 sekcji (z listy dostepnych 'names'): wspiecie - jeden klip; przeskok - klip + ladowanie w bieg;
    z miejsca (f = IDLE) - climb_idle_<h> (konczy sie w postawie na gorze) / vault_idle_<h> (konczy sie w chwili
    dotkniecia ziemi; land_idle_to_idle ma miednice przesunieta o ok. 20 cm w bok i 27 w dol - skok toru na sklejeniu,
    wiec koniec to przejscie inercyjne do postawy Gothica jak w kazdej sekcji)."""
    if f == IDLE:
        main = "%s_idle_%d" % (kind.lower(), h)
        return [main] if main in names else None
    if kind == "CLIMB":
        main = "climb_run_%d_%s" % (h, f)
        return [main] if main in names else None
    main = "vault_run_%d_%s" % (h, f)
    if main not in names:
        return None
    if h == 30:
        return [main]   # konczy sie w biegu
    land = "vault_run_%d_%s_land_to_run" % (h, f)
    return [main, land] if land in names else [main, None]   # None = ogolne ladowanie wg stopy (land_clip)


def dump(clip):
    return make_pair.dump_clip(EXPLO_SET, clip, os.path.join(WORK, clip + ".json"))


def land_clip(d):
    """Ogolne ladowanie w bieg po klipie d: land_run_lf_to_run zaczyna sie z prawa stopa na ziemi (lewa w powietrzu),
    rf odwrotnie - wg nizszej stopy w ostatniej klatce."""
    names = [b["name"] for b in d["bones"]]
    last = d["frames"][-1]
    lf, rf = last[names.index("l_foot")][2][3], last[names.index("r_foot")][2][3]
    return "land_run_lf_to_run" if rf <= lf else "land_run_rf_to_run"


def scale_w3(d, sk):
    """Skala W3 -> Gothic jak w retarget: wysokosc korzenia Gothica / wysokosc miednicy W3 w spoczynku."""
    mrest, _ = retarget.mixamo_frames(d, retarget.SOURCES["w3"][0])
    return float(sk.root_pos[1] / mrest["pelvis"][1][1])


def load_motion(clips, sk, grip):
    """-> (klatki @25 (lokalne obroty, korzen) w ukladzie Gothica: vob kl. 0 w (0,0), przod +Z, ziemia y=0;
    skala W3->Gothic; zrodlo (dump) do wysokosci Trajectory)."""
    dumps = []
    for c in clips:
        dumps.append(dump(c))
    d = make_moves.to_entity_start(make_moves.concat(dumps) if len(dumps) > 1 else dumps[0])
    k = scale_w3(d, sk)
    frames = retarget.resample(retarget.retarget(d, sk, grip, False, "w3"), d["fps"])
    base = np.array([frames[0][1][0], 0.0, frames[0][1][2]])
    frames = [(l, np.array(r, dtype=float) - base) for l, r in frames]
    return frames, k, d


def node_track(frames, sk, names):
    """{wezel: tablica (n, 3) pozycji w ukladzie modelu} dla klatek (lokalne, korzen)."""
    W = [sk.compose(l, r) for l, r in frames]
    return {n: np.array([w[sk.idx(n)][:3, 3] for w in W]) for n in names}


def speeds(p):
    """Predkosc punktu (cm/s) w kolejnych klatkach (pierwsza = druga)."""
    v = [float(np.linalg.norm(p[k] - p[k - 1])) * FPS for k in range(1, len(p))]
    return [v[0]] + v


def planted(p, k, hmax=PLANT_SPEED, vmax=PLANT_SPEED):
    """Punkt p (n, 3) prawie nieruchomy w klatce k (do nastepnej): poziomo i pionowo wolniej niz hmax / vmax cm/s."""
    j = min(k + 1, len(p) - 1)
    i = j - 1 if j == k else k
    d = (p[j] - p[i]) * FPS
    return float(np.hypot(d[0], d[2])) < hmax and abs(float(d[1])) < vmax


def analyze(frames, sk, kind, top):
    """Punkty klipu (indeksy klatek @25) i geometria przeszkody w ukladzie Gothica (y w gore, z do przodu, ziemia 0).
    top = wysokosc gory (cm). -> dict: rest (wezel stopy podpartej nad ziemia), takeoff, contact (+ support - czym),
    edge (sciana), release / land / apex / thick (przeskok), depth (wspiecie), settle (koniec ruchu - ogon W3 uciety),
    exit.
    Sciana: za najdalszym punktem nog ponizej gory (minus LOW_GAP - retarget na nogach Gothica przesuwa stopy o kilka
    cm) przed podparciem, ale przed punktem podparcia; podparcie = dlon na wysokosci gory albo stopa na gorze,
    prawie nieruchome (dlon przesuwana po gorze albo stopa wslizgujaca sie na nia jeszcze nie podpiera)."""
    nodes = node_track(frames, sk, LOW_NODES + HAND_NODES + ("BIP01",))
    n = len(frames)
    feet_y = np.minimum(nodes["BIP01 L FOOT"][:, 1], nodes["BIP01 R FOOT"][:, 1])
    rest = float(min(feet_y[:3]))
    root_y = nodes["BIP01"][:, 1]
    root_z = nodes["BIP01"][:, 2]
    rise = root_y - root_y[0]
    rsp = speeds(nodes["BIP01"])
    # odbicie: miednica rusza w gore (ponad 6% wysokosci przeszkody), 2 klatki wczesniej
    up = next((k for k in range(n) if rise[k] > 0.06 * top + 2.0), n // 3)
    takeoff = max(0, up - 2)
    contact, support = None, None
    for k in range(takeoff + 1, n):
        low_z = [float(nodes[nm][j][2]) for j in range(k + 1) for nm in LOW_NODES if nodes[nm][j][1] < top - LOW_GAP]
        far = max(low_z) if low_z else 0.0
        for name in HAND_NODES + FEET_NODES:
            p = nodes[name]
            if name in HAND_NODES:
                ok = top - 6.0 <= p[k][1] <= top + 22.0
            else:
                ok = top + rest - 8.0 <= p[k][1] <= top + rest + 14.0 and p[k][2] > far + 5.0
            if ok and planted(p, k):
                contact, support = k, name
                break
        if contact is not None:
            break
    if contact is None:
        contact, support = min(n - 1, takeoff + 6), None
    low_z = [float(nodes[nm][k][2]) for k in range(contact + 1) for nm in LOW_NODES if nodes[nm][k][1] < top - LOW_GAP]
    face_lo = max(low_z) + FACE_MARGIN if low_z else 20.0
    face_hi = float(nodes[support][contact][2]) - 5.0 if support else face_lo + 40.0
    edge = face_lo + EDGE_SHARE * (face_hi - face_lo) if face_lo <= face_hi else 0.5 * (face_lo + face_hi)
    out = dict(rest=rest, takeoff=takeoff, contact=contact, support=support, edge=edge, face_lo=face_lo, face_hi=face_hi)
    if kind == "VAULT":
        apex = contact + int(np.argmax(root_y[contact:]))
        # ladowanie: stopa na ziemi za przeszkoda (po szczycie); puszczenie: ostatnie podparcie dlonia przed nim
        land = next((k for k in range(apex, n) for f in FEET_NODES
                     if nodes[f][k][1] < rest + 8.0 and nodes[f][k][2] > edge + 30.0), n - 1)
        release = apex
        for k in range(contact, land):
            if any(top - 6.0 <= nodes[h][k][1] <= top + 22.0 and planted(nodes[h], k) for h in HAND_NODES):
                release = k
        # dopasowanie ziemi za przeszkoda dopiero po szczycie (cialo nad gora) - dlon puszcza ja czasem wczesniej
        release = min(max(release, apex), land - 1)
        behind = [float(nodes[nm][k][2]) for k in range(apex, n) for nm in LOW_NODES
                  if nodes[nm][k][1] < top - LOW_GAP and nodes[nm][k][2] > edge + 20.0]
        thick = min(behind) - edge - 10.0 if behind else VAULT_THICK_MAX
        out.update(apex=apex, release=release, land=land, thick=float(min(VAULT_THICK_MAX, max(15.0, thick))),
                   settle=n - 1, exit=min(n - 1, land + 2))
    else:
        end_rise = root_y[-1] - root_y[0]
        moving = rsp[-1] > 150.0   # klip konczy sie w biegu (30 cm)
        settle = n - 1
        if not moving:
            # koniec ruchu: miednica stoi na gorze (ogon W3 = przestepowanie i rozgladanie sie w miejscu)
            for k in range(contact, n - 3):
                if all(rsp[j] < SETTLE_SPEED * 2.0 for j in range(k, k + 3)) and rise[k] > 0.9 * end_rise:
                    settle = min(n - 1, k + SETTLE_KEEP)
                    break
        stand = next((k for k in range(contact, n) if rise[k] >= 0.85 * end_rise), settle)
        exit_ = max(contact + 2, n - 4) if moving else min(settle, max(stand, contact + 2))
        # klip konczacy sie w biegu (30 cm): od chwili exit bieg prowadzi gra - gora potrzebna tylko do tego miejsca
        last = exit_ if moving else settle
        reach = max(float(root_z[contact:last + 1].max()), max(float(nodes[f][last][2]) for f in FEET_NODES))
        out.update(settle=settle, exit=exit_, depth=reach - edge + BODY_RADIUS)
    return out


def report(sid, clips, frames, sk, k, kind, top, a):
    nodes = node_track(frames, sk, ("BIP01",) + FEET_NODES + HAND_NODES)
    f = lambda i: "%.2f" % (i / FPS)
    print("%-14s %-48s top %.1f (k %.3f) rest %.1f takeoff %s contact %s (%s) edge %.1f [%.1f..%.1f]" % (
        sid, "+".join(c or "?" for c in clips), top, k, a["rest"], f(a["takeoff"]), f(a["contact"]), a["support"],
        a["edge"], a["face_lo"], a["face_hi"]), end="")
    if kind == "VAULT":
        print(" apex %s release %s land %s thick %.0f exit %s / %s" % (f(a["apex"]), f(a["release"]), f(a["land"]),
                                                                      a["thick"], f(a["exit"]), f(len(frames) - 1)))
    else:
        print(" settle %s exit %s depth %.0f / %s" % (f(a["settle"]), f(a["exit"]), a["depth"], f(len(frames) - 1)))
    r = nodes["BIP01"]
    print("    root y: start %.1f max %.1f end %.1f; z end %.1f" % (r[0][1], r[:, 1].max(), r[-1][1], r[-1][2]))


def smooth01(u):
    u = min(max(u, 0.0), 1.0)
    return u * u * (3.0 - 2.0 * u)


def support_level(z, kind, top, edge, thick):
    """Wysokosc podloza pod punktem z (cm do przodu) w klipie: przed sciana ziemia, nad przeszkoda jej gora,
    za przeskoczona przeszkoda znow ziemia."""
    if z <= edge + 5.0:
        return 0.0
    if kind == "CLIMB" or z < edge + thick:
        return top
    return 0.0


def footsteps(frames, sk, kind, top, a):
    """Kroki (klatki od 1, "RUN") - postawienie stopy wzgledem podloza pod nia (ziemia, gora przeszkody)."""
    nodes = node_track(frames, sk, FEET_NODES)
    out = []
    for f in FEET_NODES:
        ys = [float(p[1]) - support_level(float(p[2]), kind, top, a["edge"], a.get("thick", 0.0)) for p in nodes[f]]
        out += [(k + 1, "RUN") for k in sfx_events.landings(ys, lift=5.0, touch=3.0)]
    return sorted(set(out))


def vob_path(frames, p0):
    """Tor voba (x, y, z) z ruchu miednicy: x, z jak korzen (wzgledem kl. 0), y = korzen - P0 (cale pionowe
    przemieszczenie niesie vob), z wygasajacym przesunieciem startu (vob startuje z ziemi, korzen schodzi do P0)."""
    v0 = float(frames[0][1][1]) - p0
    out = []
    for k, (_, r) in enumerate(frames):
        fade = 1.0 - smooth01((k / FPS) / ENTRY_FADE)
        out.append((float(r[0]), float(r[1]) - p0 - v0 * fade, float(r[2])))
    return out


def build_section(vfs, sk, grip, kind, h, f, clips):
    """-> (id, linie ini, opis) i zapis nakladki HUMANS_ZDF_MV_TRAV_<ID> (4 animacje: bez broni, piesci, 1H, 2H)."""
    sid = section_id(kind, h, f)
    frames, k, d = load_motion(clips, sk, grip)
    top = clip_top(d, kind, h, k)
    a = analyze(frames, sk, kind, top)
    frames = frames[:a["settle"] + 1]
    level = top if kind == "CLIMB" else 0.0
    stances = {st: recovery.stance_pose(vfs, sk, st) for _, st in ANIS}
    p0 = float(stances["S_RUN"][1][1])
    per_ani = {}
    for ani, st in ANIS:
        t_local, t_root = stances[st]
        per_ani[ani] = make_moves.inertial_recovery(frames, (t_local, np.array([0.0, level + float(t_root[1]), 0.0])),
                                                     REC_FRAMES)
    path = vob_path(per_ani["T_CANNOTTAKE"], p0)
    name = "HUMANS_ZDF_MV_TRAV_%s" % sid
    steps = footsteps(frames, sk, kind, top, a)
    chunks = []
    for ani, st in ANIS:
        inplace = [(l, np.array([0.0, float(r[1]) - y, 0.0])) for (l, r), (_, y, _) in zip(per_ani[ani], path)]
        nf = make_pair.write_man(vfs, sk, inplace, ani, os.path.join(COMPILED, "%s-%s.MAN" % (name, ani)))
        chunks.append(msblib.ani_chunk(ani, 1, st, 0.1, 0.1, FLAGS, "ZDISM_W3.ASC", 1, nf)
                      + sfx_events.events_block(ground=steps))
    build_overlay.write_overlay(name, COMPILED, chunks, vfs)
    t = lambda i: "%g" % round(i / FPS, 2)
    lines = ["overlay=%s" % name, "kind=%s" % kind, "bucket=%d" % h, "height=%.1f" % top,
             "foot=%s" % ("L" if f == IDLE else f[0].upper())] + (["start=IDLE"] if f == IDLE else []) + [
             "trajFps=%g" % FPS, "traj=%s" % " ".join("%.1f,%.1f,%.1f" % p for p in path),
             "edge=%.1f" % a["edge"], "takeoff=%s" % t(a["takeoff"]), "contact=%s" % t(a["contact"])]
    if kind == "VAULT":
        lines += ["release=%s" % t(a["release"]), "land=%s" % t(a["land"]), "thick=%.0f" % a["thick"]]
    else:
        lines += ["depth=%.0f" % a["depth"]]
    lines += ["exit=%s" % t(a["exit"]), "cancel=%s" % t(len(path))]
    lines.append("; zrodlo W3: %s, %d kl. @25 (+%d przejscia), gora %.0f cm, sciana %.0f cm [%.0f..%.0f], podparcie %s" % (
        " + ".join(clips), len(frames), REC_FRAMES, top, a["edge"], a["face_lo"], a["face_hi"], a["support"]))
    return sid, lines


INI_HEADER = ("; Przeskoki i wspinanie w biegu z Wiedzmina 3 - TYLKO UZYTEK PRYWATNY (animacje CD Projekt RED), nie do wydania.\n"
              "; Generowane przez anims\\pipeline\\make_traversal.py. traj: trojki x,y,z (cm: x w prawo, y wysokosc voba nad ziemia\n"
              "; startu, z do przodu) co 1/trajFps s; edge: sciana od voba na starcie; takeoff..contact: okno dopasowania wysokosci;\n"
              "; release..land (przeskok): okno dopasowania ziemi za przeszkoda; exit: od tej chwili z W w bieg; cancel: koniec (s).\n"
              "; depth: najmniejsza glebokosc gory (wspiecie), thick: najwieksza grubosc gory (przeskok); foot: noga w powietrzu na\n"
              "; starcie (L = prawa podparta); start=IDLE: z miejsca (bohater stoi / idzie, sekcje *_IDLE_<h>).\n")


def write_ini(sections):
    """Sekcje [ID] do traversal_w3.ini - podmiana istniejacych (przy --only reszta zostaje)."""
    import re
    os.makedirs(os.path.dirname(INI), exist_ok=True)
    text = open(INI, encoding="ascii").read() if os.path.exists(INI) else INI_HEADER
    for sid, lines in sections:
        section = "[%s]\n%s\n" % (sid, "\n".join(lines))
        pattern = re.compile(r"^\[%s\]\n(?:(?!\[).*\n?)*" % re.escape(sid), re.M)
        text = pattern.sub(section + "\n", text) if pattern.search(text) else text.rstrip("\n") + "\n\n" + section
    open(INI, "w", encoding="ascii", newline="\n").write(text)


def all_sections(names):
    """[(kind, h, f, clips)] dla dostepnych klipow (names - zrzuty w WORK albo lista zestawu)."""
    out = []
    for kind in KINDS:
        for h in BUCKETS:
            for f in FEET:
                clips = section_clips(kind, h, f, names)
                if clips:
                    out.append((kind, h, f, clips))
    for kind in KINDS:
        for h in IDLE_BUCKETS:
            clips = section_clips(kind, h, IDLE, names)
            if clips:
                out.append((kind, h, IDLE, clips))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="", help="lista sekcji (np. CLIMB_100_LF,VAULT_150_RF)")
    ap.add_argument("--report", action="store_true", help="tylko analiza klipow (bez zapisu)")
    a = ap.parse_args()
    only = set(s.strip().upper() for s in a.only.split(",") if s.strip())
    vfs = build_overlay.game_vfs()
    sk = retarget.GothicSkeleton(vfs)
    grip = make_pair.grip_local(vfs, sk)
    os.makedirs(WORK, exist_ok=True)
    os.makedirs(COMPILED, exist_ok=True)
    names = set(os.path.splitext(f)[0] for f in os.listdir(WORK) if f.endswith(".json")) | set(CLIPS)
    sections = []
    for kind, h, f, clips in all_sections(names):
        sid = section_id(kind, h, f)
        if only and sid not in only:
            continue
        if clips[-1] is None:
            clips = clips[:-1] + [land_clip(dump(clips[0]))]
        if a.report:
            frames, k, d = load_motion(clips, sk, grip)
            top = clip_top(d, kind, h, k)
            report(sid, clips, frames, sk, k, kind, top, analyze(frames, sk, kind, top))
            continue
        sections.append(build_section(vfs, sk, grip, kind, h, f, clips))
        print(sid, " ".join(x for x in sections[-1][1] if x.split("=")[0] in ("edge", "contact", "exit", "cancel", "depth", "thick")))
    if sections:
        write_ini(sections)
        print("zapisano %d sekcji: %s" % (len(sections), INI))


# Klipy zestawu man_geralt_exploration.w2anims uzywane przez sekcje (lista z zestawu - do zrzutu bez wczesniejszego
# zrzutu w WORK). Brak vault_run_30/250_*_land_to_run - 30 cm konczy sie w biegu, 250 cm bierze land_run_*_to_run.
CLIPS = (["climb_run_%d_%s" % (h, f) for h in BUCKETS for f in FEET] +
         ["vault_run_%d_%s" % (h, f) for h in BUCKETS for f in FEET] +
         ["vault_run_%d_%s_land_to_run" % (h, f) for h in (100, 150, 200) for f in FEET] +
         ["land_run_lf_to_run", "land_run_rf_to_run"] +
         ["climb_idle_%d" % h for h in IDLE_BUCKETS] + ["vault_idle_%d" % h for h in IDLE_BUCKETS])


def clip_top(d, kind, h, k):
    """Wysokosc gory w ukladzie Gothica: wspiecie - koncowa wysokosc Trajectory W3 (stoi na gorze), przeskok -
    nominalna wysokosc z nazwy klipu (Trajectory wraca na ziemie)."""
    if kind == "CLIMB":
        return float(make_pair.bone_frame(d, "Trajectory", len(d["frames"]) - 1)[2][3]) * 100.0 * k
    return h * k


if __name__ == "__main__":
    main()
