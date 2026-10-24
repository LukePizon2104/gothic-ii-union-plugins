r"""Przejscie konca animacji w postawe walki Gothica: doklejone klatki z obrotami prowadzonymi (slerp, wygladzone)
od ostatniej pozy do pierwszej klatki S_1HRUN / S_2HRUN / S_FISTRUN z HUMANS - silnik przejmuje postac bez przeskoku."""
import math
import numpy as np
import manlib, retarget
from retarget import qmat


def stance_pose(vfs, sk, ani):
    """Pierwsza klatka animacji postawy z bazowego modelu: (lokalne obroty 3x3, pozycja korzenia)."""
    m = manlib.Man(bytes(vfs.find("HUMANS-%s.MAN" % ani).data))
    local = [l[:3, :3].copy() for l in sk.local]
    root = np.array(sk.root_pos, dtype=float)
    for k, ni in enumerate(m.nodes):
        (x, y, z, w), p = m.decode(m.sample(0, k))
        local[ni] = qmat(-x, -y, -z, w)
        if sk.parent[ni] < 0:
            root = np.array(p, dtype=float)
    return local, root


def append_recovery(frames, target, n):
    """frames + n klatek: obroty od ostatniej pozy do docelowej (smoothstep), wysokosc korzenia tak samo, x/z stoja."""
    t_local, t_root = target
    last_local, last_root = frames[-1]
    qa = [retarget.quat_from_mat(R) for R in last_local]
    qb = [retarget.quat_from_mat(R) for R in t_local]
    out = list(frames)
    for k in range(1, n + 1):
        u = k / n
        w = u * u * (3 - 2 * u)
        local = [qmat(*retarget.slerp(a, b, w)) for a, b in zip(qa, qb)]
        root = np.array([last_root[0], last_root[1] + (t_root[1] - last_root[1]) * w, last_root[2]])
        out.append((local, root))
    out[-1] = ([R.copy() for R in t_local], np.array([last_root[0], t_root[1], last_root[2]]))
    return out
