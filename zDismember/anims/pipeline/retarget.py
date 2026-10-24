"""Przenosi animacje Mixamo (zrzut fbx_dump.py) na szkielet ludzi Gothica i zapisuje .MAN.

Uzycie: py -3.12 -I retarget.py zrzut.json wyjscie.MAN --name T_1HSFINISH --grip T_1HATTACKL [--preview obraz.png]

Konwencje ustalone na danych gry (15.10.2026):
  - MDH: macierze kolumnowe, przesuniecie w 4. kolumnie, swiat = rodzic @ lokalna;
  - MAN: kwaternion zapisany jako sprzezenie obrotu w tej konwencji, pozycja korzenia = pozycja w modelu,
    pozostale wezly = przesuniecie z MDH;
  - Blender (zrzut): metry, Z w gore, postac patrzy w -Y, lewa reka +X;
    Gothic: cm, Y w gore, przod +Z, lewa strona -X  =>  g = (-x, z, -y) * 100.
Metoda: kierunki kosci Gothica w pozie spoczynkowej wyrownane do T-pose Mixamo, potem kazda kosc dostaje
ten sam obrot swiatowy wzgledem pozy spoczynkowej co odpowiadajaca kosc Mixamo.
"""
import argparse, json, math, sys
import numpy as np
import zenkit

import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import manlib

GAME_DATA = os.path.join(os.environ.get("GOTHIC2_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Gothic II"), "Data")
P = np.array([[-1.0, 0, 0], [0, 0, 1.0], [0, -1.0, 0]])
FPS_OUT = 25.0

# wezel Gothica -> (kosc Mixamo, dziecko Gothica do kierunku, dziecko Mixamo do kierunku)
# kierunek None = os X wezla (konwencja bipeda 3ds Max) / brak wyrownania dla korzenia
MAP = {
    "BIP01":            ("Hips", None, None),
    "BIP01 SPINE":      ("Spine", "BIP01 SPINE1", "Spine1"),
    "BIP01 SPINE1":     ("Spine1", "BIP01 SPINE2", "Spine2"),
    "BIP01 SPINE2":     ("Spine2", "BIP01 NECK", "Neck"),
    "BIP01 NECK":       ("Neck", "BIP01 HEAD", "Head"),
    "BIP01 HEAD":       ("Head", None, "HeadTop_End"),
}
for side, mx in (("L", "Left"), ("R", "Right")):
    MAP.update({
        f"BIP01 {side} CLAVICLE": (f"{mx}Shoulder", f"BIP01 {side} UPPERARM", f"{mx}Arm"),
        f"BIP01 {side} UPPERARM": (f"{mx}Arm", f"BIP01 {side} FOREARM", f"{mx}ForeArm"),
        f"BIP01 {side} FOREARM":  (f"{mx}ForeArm", f"BIP01 {side} HAND", f"{mx}Hand"),
        f"BIP01 {side} HAND":     (f"{mx}Hand", f"BIP01 {side} FINGER0", f"{mx}HandMiddle1"),
        f"BIP01 {side} FINGER0":  (f"{mx}HandMiddle1", None, f"{mx}HandMiddle2"),
        f"BIP01 {side} THIGH":    (f"{mx}UpLeg", f"BIP01 {side} CALF", f"{mx}Leg"),
        f"BIP01 {side} CALF":     (f"{mx}Leg", f"BIP01 {side} FOOT", f"{mx}Foot"),
        f"BIP01 {side} FOOT":     (f"{mx}Foot", f"BIP01 {side} TOE0", f"{mx}ToeBase"),
        f"BIP01 {side} TOE0":     (f"{mx}ToeBase", None, f"{mx}Toe_End"),
    })

# Wiedzmin 3 (man_base.w2rig z Witcher3-Blender-Tools): metry, Z w gore, postac patrzy w +Y, lewa reka -X
#  =>  g = (x, z, y) * 100.  Tylko do uzytku prywatnego (animacje CD Projekt RED).
P_W3 = np.array([[1.0, 0, 0], [0, 0, 1.0], [0, 1.0, 0]])
MAP_W3 = {
    "BIP01":            ("pelvis", None, None),
    "BIP01 SPINE":      ("torso", "BIP01 SPINE1", "torso2"),
    "BIP01 SPINE1":     ("torso2", "BIP01 SPINE2", "torso3"),
    "BIP01 SPINE2":     ("torso3", "BIP01 NECK", "neck"),
    "BIP01 NECK":       ("neck", "BIP01 HEAD", "head"),
    "BIP01 HEAD":       ("head", None, None),
}
for side, w in (("L", "l"), ("R", "r")):
    MAP_W3.update({
        f"BIP01 {side} CLAVICLE": (f"{w}_shoulder", f"BIP01 {side} UPPERARM", f"{w}_bicep"),
        f"BIP01 {side} UPPERARM": (f"{w}_bicep", f"BIP01 {side} FOREARM", f"{w}_forearm"),
        f"BIP01 {side} FOREARM":  (f"{w}_forearm", f"BIP01 {side} HAND", f"{w}_hand"),
        f"BIP01 {side} HAND":     (f"{w}_hand", f"BIP01 {side} FINGER0", f"{w}_middle1"),
        f"BIP01 {side} FINGER0":  (f"{w}_middle1", None, f"{w}_middle2"),
        f"BIP01 {side} THIGH":    (f"{w}_thigh", f"BIP01 {side} CALF", f"{w}_shin"),
        f"BIP01 {side} CALF":     (f"{w}_shin", f"BIP01 {side} FOOT", f"{w}_foot"),
        f"BIP01 {side} FOOT":     (f"{w}_foot", f"BIP01 {side} TOE0", f"{w}_toe"),
        f"BIP01 {side} TOE0":     (f"{w}_toe", None, None),
    })

SOURCES = {"mixamo": (P, MAP, "Hips"), "w3": (P_W3, MAP_W3, "pelvis")}
# os ostrza w lokalnym ukladzie kosci broni zrodla (Blender); W3: r_weapon, ostrze wzdluz +Z (linia knykci maly->wskazujacy)
BLADE_AXIS = {"w3": np.array([0.0, 0.0, 1.0])}


def mat_from_zenkit(m):
    c = m.columns if not callable(m.columns) else m.columns()
    return np.array([[c[j][i] for j in range(4)] for i in range(4)], dtype=float)


def qmat(x, y, z, w):
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def quat_from_mat(R):
    t = np.trace(R)
    if t > 0:
        s = np.sqrt(t + 1.0) * 2
        w, x, y, z = 0.25 * s, (R[2, 1] - R[1, 2]) / s, (R[0, 2] - R[2, 0]) / s, (R[1, 0] - R[0, 1]) / s
    elif R[0, 0] > R[1, 1] and R[0, 0] > R[2, 2]:
        s = np.sqrt(1.0 + R[0, 0] - R[1, 1] - R[2, 2]) * 2
        w, x, y, z = (R[2, 1] - R[1, 2]) / s, 0.25 * s, (R[0, 1] + R[1, 0]) / s, (R[0, 2] + R[2, 0]) / s
    elif R[1, 1] > R[2, 2]:
        s = np.sqrt(1.0 + R[1, 1] - R[0, 0] - R[2, 2]) * 2
        w, x, y, z = (R[0, 2] - R[2, 0]) / s, (R[0, 1] + R[1, 0]) / s, 0.25 * s, (R[1, 2] + R[2, 1]) / s
    else:
        s = np.sqrt(1.0 + R[2, 2] - R[0, 0] - R[1, 1]) * 2
        w, x, y, z = (R[1, 0] - R[0, 1]) / s, (R[0, 2] + R[2, 0]) / s, (R[1, 2] + R[2, 1]) / s, 0.25 * s
    q = np.array([x, y, z, w])
    return q / np.linalg.norm(q)


def slerp(a, b, t):
    d = np.dot(a, b)
    if d < 0:
        b, d = -b, -d
    if d > 0.9995:
        r = a + t * (b - a)
        return r / np.linalg.norm(r)
    th = np.arccos(d)
    return (np.sin((1 - t) * th) * a + np.sin(t * th) * b) / np.sin(th)


def rot_between(a, b):
    a = a / np.linalg.norm(a); b = b / np.linalg.norm(b)
    v = np.cross(a, b); c = np.dot(a, b)
    if c < -0.9999:
        axis = np.cross(a, [1, 0, 0]) if abs(a[0]) < 0.9 else np.cross(a, [0, 1, 0])
        axis /= np.linalg.norm(axis)
        return 2 * np.outer(axis, axis) - np.eye(3)
    vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + vx + vx @ vx / (1 + c)


def ortho(M3):
    return M3 / np.linalg.norm(M3, axis=0)


class GothicSkeleton:
    def __init__(self, vfs):
        h = zenkit.ModelHierarchy.load(vfs.find("HUMANS.MDH"))
        self.checksum = h.checksum
        self.names = [n.name for n in h.nodes]
        self.parent = [n.parent for n in h.nodes]
        self.local = [mat_from_zenkit(n.transform) for n in h.nodes]
        rt = h.root_translation
        self.root_pos = np.array([rt.x, rt.y, rt.z]) if hasattr(rt, "x") else np.array(list(rt))
        self.world = self.compose([m[:3, :3] for m in self.local], self.root_pos)

    def idx(self, name):
        return self.names.index(name)

    def compose(self, local_rots, root_pos):
        """Zwraca macierze 4x4 w ukladzie modelu dla lokalnych obrotow i pozycji korzenia."""
        W = []
        for i, R in enumerate(local_rots):
            L = np.eye(4); L[:3, :3] = R
            L[:3, 3] = root_pos if self.parent[i] < 0 else self.local[i][:3, 3]
            W.append(L if self.parent[i] < 0 else W[self.parent[i]] @ L)
        return W


def mixamo_frames(dump, P=P):
    names = [b["name"].split(":")[-1] for b in dump["bones"]]
    def conv(m):
        m = np.array(m)
        return P @ ortho(m[:3, :3]) @ P.T, P @ m[:3, 3] * 100.0
    rest = {n: conv(b["rest"]) for n, b in zip(names, dump["bones"])}
    frames = [{n: conv(m) for n, m in zip(names, fr)} for fr in dump["frames"]]
    return rest, frames


def _descends(sk, i, root):
    while i >= 0:
        if i == root:
            return True
        i = sk.parent[i]
    return False


def retarget(dump, sk, grip_local, inplace=False, source="mixamo", weapon=None, wrist_limit=None):
    """weapon: nazwa kosci broni zrodla - wtedy dlon (z palcami i ZS_RIGHTHAND) obraca sie najkrotsza droga tak, zeby
    ostrze (-X ZS_RIGHTHAND) szlo jak w oryginale. Chwyt (lokalny ZS_RIGHTHAND) zostaje staly jak w animacjach Gothica -
    przerwana animacja nie zostawia przekreconego miecza w animacjach, ktore tym wezlem nie steruja (np. zEBA).
    wrist_limit (st.): wiekszy obrot nadgarstka niz limit (zrodlo przeklada bron w dloni) przejmuje slot - tylko dla
    animacji konczacych sie przejsciem do postawy (finisher, unik), ktore przywraca chwyt."""
    P_src, MAP, hips = SOURCES[source]
    blade_local = P_src @ BLADE_AXIS[source] if weapon else None
    mrest, mframes = mixamo_frames(dump, P_src)
    nn = len(sk.names)
    Wrest = sk.world

    def gdir(i, child):
        if child is None:
            return Wrest[i][:3, 0]
        return Wrest[sk.idx(child)][:3, 3] - Wrest[i][:3, 3]

    offset = {}
    for gname, (mname, gchild, mchild) in MAP.items():
        i = sk.idx(gname)
        Rg = Wrest[i][:3, :3]
        Rb = mrest[mname][0]
        if mchild is None or gname == "BIP01":
            Gal = Rg
        else:
            db = mrest[mchild][1] - mrest[mname][1]
            Gal = rot_between(gdir(i, gchild), db) @ Rg
        offset[i] = Rb.T @ Gal

    hip_rest = mrest[hips][1]
    k = sk.root_pos[1] / hip_rest[1]
    grip_idx = sk.idx("ZS_RIGHTHAND")
    hand_idx = sk.parent[grip_idx]
    hand_tree = [i for i in range(nn) if _descends(sk, i, hand_idx)]

    out = []  # na klatke: lista lokalnych obrotow (3x3) + pozycja korzenia
    weapon_rows = []
    for fr in mframes:
        Wrot = [None] * nn
        local = [None] * nn
        for i in range(nn):
            p = sk.parent[i]
            if i in offset:
                Wrot[i] = fr[MAP[sk.names[i]][0]][0] @ offset[i]
            elif i == grip_idx and grip_local is not None:
                Wrot[i] = Wrot[p] @ grip_local
            else:
                Wrot[i] = Wrot[p] @ sk.local[i][:3, :3]
        if weapon and grip_local is not None:
            weapon_rows.append((Wrot, fr[weapon][0]))
            delta = (fr[hips][1] - hip_rest) * k
            if inplace:
                d0 = (mframes[0][hips][1] - hip_rest) * k
                delta = np.array([d0[0], delta[1], d0[2]])
            out.append((Wrot, sk.root_pos + delta))
            continue
        for i in range(nn):
            p = sk.parent[i]
            local[i] = Wrot[i] if p < 0 else Wrot[p].T @ Wrot[i]
        delta = (fr[hips][1] - hip_rest) * k
        if inplace:   # bez przemieszczenia w poziomie wzgledem pierwszej klatki
            d0 = (mframes[0][hips][1] - hip_rest) * k
            delta = np.array([d0[0], delta[1], d0[2]])
        root = sk.root_pos + delta
        out.append((local, root))
    if weapon_rows:
        out = _hand_from_weapon(sk, out, weapon_rows, grip_idx, hand_idx, hand_tree, grip_local, blade_local, wrist_limit)
    return out


def _axis_rot(axis, ang):
    a = axis / np.linalg.norm(axis)
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + math.sin(ang) * K + (1 - math.cos(ang)) * K @ K


def _hand_from_weapon(sk, out, rows, grip_idx, hand_idx, hand_tree, grip_local, blade_local, wrist_limit=None):
    """Prawa dlon trzyma bron chwytem Gothica: staly skret dloni wokol osi palcow (przeniesienie dloni ustala os palcow,
    ale skret bierze ze spoczynku Gothica - inny niz w zrodle) dobrany tak, by ostrze szlo jak bron zrodla; reszte
    wyrownuje najmniejszy obrot nadgarstka w kazdej klatce. Palce i slot ida za dlonia."""
    fi = [i for i in range(len(sk.names)) if sk.parent[i] == hand_idx and i != grip_idx]
    axis = sk.local[fi[0]][:3, 3] if fi else np.array([1.0, 0.0, 0.0])
    axis = axis / np.linalg.norm(axis)
    minus_x = np.array([-1.0, 0.0, 0.0])
    angle = lambda a, b: math.acos(max(-1.0, min(1.0, float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b))))))
    best = None
    for step in range(72):
        roll = _axis_rot(axis, math.radians(step * 5))
        err = sum(angle(Wrot[hand_idx] @ roll @ grip_local @ minus_x, Ww @ blade_local) for Wrot, Ww in rows[::2])
        if best is None or err < best[0]:
            best = (err, roll)
    roll = best[1]
    res = []
    for (Wrot, Ww), (_, root) in zip(rows, out):
        Wrot = list(Wrot)
        H = Wrot[hand_idx] @ roll
        wrist = rot_between(H @ grip_local @ minus_x, Ww @ blade_local)
        if wrist_limit is not None:
            ang = math.degrees(math.acos(max(-1.0, min(1.0, (np.trace(wrist) - 1) / 2))))
            if ang > wrist_limit:
                q = quat_from_mat(wrist)
                wrist = qmat(*slerp(np.array([0.0, 0.0, 0.0, 1.0]), q, wrist_limit / ang))
        H = wrist @ H
        R = H @ Wrot[hand_idx].T
        for i in hand_tree:
            Wrot[i] = R @ Wrot[i]
        if wrist_limit is not None:   # reszta (ponad limit nadgarstka) w slocie
            Wrot[grip_idx] = rot_between(Wrot[grip_idx] @ minus_x, Ww @ blade_local) @ Wrot[grip_idx]
        local = [Wrot[i] if sk.parent[i] < 0 else Wrot[sk.parent[i]].T @ Wrot[i] for i in range(len(Wrot))]
        res.append((local, root))
    return res


def resample(frames, fps_in):
    n_in = len(frames)
    dur = (n_in - 1) / fps_in
    n_out = int(round(dur * FPS_OUT)) + 1
    nn = len(frames[0][0])
    quats = [[quat_from_mat(R) for R in f[0]] for f in frames]
    res = []
    for j in range(n_out):
        t = min(j / FPS_OUT * fps_in, n_in - 1)
        a = int(np.floor(t)); b = min(a + 1, n_in - 1); u = t - a
        local = [qmat(*slerp(quats[a][i], quats[b][i], u)) for i in range(nn)]
        root = frames[a][1] * (1 - u) + frames[b][1] * u
        res.append((local, root))
    return res


def write_man(template, frames, sk, name, nodes, out_path, bbox_inplace=False):
    """bbox_inplace (24.10, petle ruchu make_loco): ramka bez drogi korzenia - punkty wzgledem korzenia TEJ klatki w
    poziomie i dol na stopach (jak HUMANS-S_RUNL gry). Bez tego petla W3 miala ramke z cala droga cyklu (sprint +466 cm
    przed postacia), a kolizja postaci liczona od nowa wypychala bohatera o metry do tylu."""
    S = 2.1 / 65535.0
    m = template
    pos = []
    rots = []
    for local, root in frames:
        for i in nodes:
            q = quat_from_mat(local[i])
            q = np.array([-q[0], -q[1], -q[2], q[3]])      # MAN = sprzezenie
            if q[3] < 0:
                q = -q
            rots.append(q)
            pos.append(root if sk.parent[i] < 0 else sk.local[i][:3, 3])
    pos = np.array(pos)
    lo, hi = pos.min(), pos.max()
    scale = (hi - lo) / 65535.0 or 1e-6
    samples = []
    for q, p in zip(rots, pos):
        r = tuple(int(max(0, min(65535, round(v / S + 32767)))) for v in q[:3])
        pp = tuple(int(max(0, min(65535, round((v - lo) / scale)))) for v in p)
        samples.append(r + pp)
    # bbox: jak w MAN gry - wzgledem korzenia z pierwszej klatki, z zapasem
    pts = []
    for local, root in frames:
        W = sk.compose(local, root)
        ref = np.array([root[0], frames[0][1][1], root[2]]) if bbox_inplace else frames[0][1]
        pts += [w[:3, 3] - ref for w in W]
    pts = np.array(pts)
    blo = pts.min(axis=0) - 25.0   # osobna nazwa: 'lo' (min. pozycji probek) idzie nizej do m.pmin
    if bbox_inplace:
        blo[1] += 25.0
    bb = list(blo) + list(pts.max(axis=0) + 25.0)

    m.name = name
    m.frames = len(frames)
    m.nodes = list(nodes)
    m.nnodes = len(nodes)
    m.samples = samples
    m.pmin, m.pscale = float(lo), float(scale)
    m.fps = m.fps_src = FPS_OUT
    m.bbox = [float(v) for v in bb]
    m.next = ""
    if manlib.CH_EVENTS in m.raw:
        m.order = [t for t in m.order if t != manlib.CH_EVENTS]
    data = m.build()
    open(out_path, "wb").write(data)
    return data


def check_roundtrip(data, frames, sk):
    m = manlib.Man(data)
    worst = 0.0
    for f in range(m.frames):
        local = [sk.local[i][:3, :3] for i in range(len(sk.names))]
        root = sk.root_pos
        for k, ni in enumerate(m.nodes):
            (x, y, z, w), p = m.decode(m.sample(f, k))
            local[ni] = qmat(-x, -y, -z, w)
            if sk.parent[ni] < 0:
                root = np.array(p)
        A = sk.compose(local, root)
        B = sk.compose(frames[f][0], frames[f][1])
        worst = max(worst, max(np.linalg.norm(a[:3, 3] - b[:3, 3]) for a, b in zip(A, B)))
    return worst


def preview(frames, sk, mframes_g, path, title):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    picks = np.linspace(0, len(frames) - 1, 6).astype(int)
    fig, axes = plt.subplots(2, len(picks), figsize=(3 * len(picks), 7))
    for col, f in enumerate(picks):
        W = sk.compose(frames[f][0], frames[f][1])
        P3 = np.array([w[:3, 3] for w in W])
        for row, (ax_i, label) in enumerate(((0, "przod (X-Y)"), (2, "bok (Z-Y)"))):
            ax = axes[row][col]
            for i, p in enumerate(sk.parent):
                if p < 0 or sk.names[i].startswith("ZS_"):
                    continue
                c = "tab:red" if " L " in sk.names[i] else "tab:blue" if " R " in sk.names[i] else "k"
                ax.plot([P3[p, ax_i], P3[i, ax_i]], [P3[p, 1], P3[i, 1]], c=c, lw=2)
            g = P3[sk.idx("ZS_RIGHTHAND")]
            R = W[sk.idx("ZS_RIGHTHAND")][:3, :3]
            for axis, cc in ((0, "orange"), (1, "green"), (2, "purple")):
                e = g + R[:, axis] * 40
                ax.plot([g[ax_i], e[ax_i]], [g[1], e[1]], c=cc, lw=1)
            ax.set_aspect("equal"); ax.set_xlim(-120, 120); ax.set_ylim(-10, 230)
            ax.set_title(f"kl. {f} {label}", fontsize=8)
    fig.suptitle(title + "  (czerwony = lewa, niebieski = prawa; ZS_RIGHTHAND: X pomaranczowy, Y zielony, Z fiolet)", fontsize=9)
    fig.tight_layout()
    fig.savefig(path, dpi=70)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dump"); ap.add_argument("out")
    ap.add_argument("--name", required=True)
    ap.add_argument("--grip", default=None, help="animacja gry, z ktorej brac obrot ZS_RIGHTHAND (chwyt broni)")
    ap.add_argument("--preview")
    ap.add_argument("--inplace", action="store_true", help="usun przemieszczenie korzenia w poziomie")
    ap.add_argument("--source", default="mixamo", choices=sorted(SOURCES), help="szkielet zrodla (mixamo / w3)")
    a = ap.parse_args()

    vfs = zenkit.Vfs()
    for f in ("Anims.vdf", "Anims_Addon.vdf"):
        vfs.mount_disk(GAME_DATA + "\\" + f, clobber=zenkit.VfsOverwriteBehavior.NEWER)
    sk = GothicSkeleton(vfs)

    grip_local = None
    if a.grip:
        node = vfs.find(f"HUMANS-{a.grip}.MAN")
        gm = manlib.Man(bytes(node.data))
        gi = sk.idx("ZS_RIGHTHAND")
        if gi in gm.nodes:
            (x, y, z, w), _ = gm.decode(gm.sample(0, gm.nodes.index(gi)))
            grip_local = qmat(-x, -y, -z, w)
            print("chwyt z", a.grip)
        else:
            print("UWAGA:", a.grip, "nie animuje ZS_RIGHTHAND - chwyt z MDH")

    dump = json.load(open(a.dump))
    frames = retarget(dump, sk, grip_local, a.inplace, a.source)
    frames = resample(frames, dump["fps"])
    nodes = [i for i, n in enumerate(sk.names) if not n.startswith("ZS_") or n == "ZS_RIGHTHAND"]
    template = manlib.Man(bytes(vfs.find("HUMANS-T_1HSFINISH.MAN").data))
    data = write_man(template, frames, sk, a.name, nodes, a.out)
    err = check_roundtrip(data, frames, sk)
    W0 = sk.compose(frames[0][0], frames[0][1])
    print(f"{a.out}: {len(frames)} kl. @25, wezly {len(nodes)}, blad zapisu max {err:.2f} cm,"
          f" glowa kl.0 y={W0[sk.idx('BIP01 HEAD')][1,3]:.0f} stopy y={W0[sk.idx('BIP01 L FOOT')][1,3]:.0f}/{W0[sk.idx('BIP01 R FOOT')][1,3]:.0f}")
    if a.preview:
        preview(frames, sk, None, a.preview, a.name + " <- " + dump["source"])


if __name__ == "__main__":
    main()
