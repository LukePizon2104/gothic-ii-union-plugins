"""Wylicza z animacji bohatera klatke ciecia i miejsce ofiary dla finishers.ini.

Uzycie: py -3.12 -I analyze_anim.py plik.MAN
Ostrze = os -X gniazda ZS_RIGHTHAND (sprawdzone na T_1HSFINISH gry: przy pchnieciu w lezacego -X wskazuje w dol).
Ciecie = klatka najwiekszej poziomej predkosci punktu w polowie ostrza; ofiara stoi tam, gdzie ten punkt
jest w tej klatce (x w prawo, z do przodu wzgledem bohatera na starcie; ruch korzenia z animacji wliczony).
"""
import sys
import numpy as np

import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
sys.path.insert(0, __import__("os").path.dirname(__import__("os").path.abspath(__file__)))
import manlib
from retarget import qmat

BLADE_AXIS, BLADE_SIGN, CUT_POINT_CM = 0, -1.0, 45.0


def blade_points(m, sk):
    gi = sk.idx("ZS_RIGHTHAND")
    pts = []
    for f in range(m.frames):
        local = [l[:3, :3].copy() for l in sk.local]
        root = sk.root_pos
        for k, ni in enumerate(m.nodes):
            (x, y, z, w), p = m.decode(m.sample(f, k))
            local[ni] = qmat(-x, -y, -z, w)
            if sk.parent[ni] < 0:
                root = np.array(p)
        g = sk.compose(local, root)[gi]
        pts.append(g[:3, 3] + g[:3, BLADE_AXIS] * BLADE_SIGN * CUT_POINT_CM)
    return np.array(pts)


def analyze(man_path, sk, frame=None):
    """frame: wymuszona klatka ciecia (wariant w miejscu musi uzyc klatki z wariantu z ruchem)."""
    m = manlib.Man(open(man_path, "rb").read())
    pts = blade_points(m, sk)
    fps = m.fps or 25.0
    vel = np.zeros(len(pts))
    vel[1:] = np.linalg.norm((pts[1:] - pts[:-1])[:, [0, 2]], axis=1) * fps
    lo, hi = min(2, len(pts) - 1), max(len(pts) - 2, 1)
    f = lo + int(np.argmax(vel[lo:hi])) if hi > lo else int(np.argmax(vel))
    if frame is not None:
        f = max(0, min(int(frame), len(pts) - 1))
    p = pts[f]
    return {"cutFrame": f, "cutTime": round(f / fps, 3), "offset": [round(float(p[0]), 1), 0.0, round(float(p[2]), 1)],
            "height": round(float(p[1]), 1), "frames": m.frames, "duration": round((m.frames - 1) / fps, 3)}


if __name__ == "__main__":
    import zenkit
    from retarget import GothicSkeleton, GAME_DATA
    vfs = zenkit.Vfs()
    vfs.mount_disk(GAME_DATA + r"\Anims.vdf", clobber=zenkit.VfsOverwriteBehavior.NEWER)
    print(analyze(sys.argv[1], GothicSkeleton(vfs)))
