# Ktora os ZS_RIGHTHAND to ostrze: w postawie S_1HRUN miecz trzymany przed soba (przod +Z, w gore +Y).
import sys, numpy as np, zenkit
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign")); sys.path.insert(0, ".")
import manlib
from retarget import GothicSkeleton, qmat, GAME_DATA
vfs = zenkit.Vfs()
for f in ("Anims.vdf", "Anims_Addon.vdf"):
    vfs.mount_disk(GAME_DATA + "\\" + f, clobber=zenkit.VfsOverwriteBehavior.NEWER)
sk = GothicSkeleton(vfs)
for ani in ("S_1HRUN", "S_2HRUN", "T_1HATTACKL", "T_2HATTACKL"):
    node = vfs.find(f"HUMANS-{ani}.MAN")
    if not node: print(ani, "brak"); continue
    m = manlib.Man(bytes(node.data))
    f = m.frames // 2
    local = [l[:3, :3].copy() for l in sk.local]; root = sk.root_pos
    for k, ni in enumerate(m.nodes):
        (x, y, z, w), p = m.decode(m.sample(f, k)); local[ni] = qmat(-x, -y, -z, w)
        if sk.parent[ni] < 0: root = np.array(p)
    W = sk.compose(local, root)
    R = W[sk.idx("ZS_RIGHTHAND")][:3, :3]
    print(f"{ani:12} kl.{f}: X={np.round(R[:,0],2)} Y={np.round(R[:,1],2)} Z={np.round(R[:,2],2)}  dlon={np.round(W[sk.idx('ZS_RIGHTHAND')][:3,3])}")
