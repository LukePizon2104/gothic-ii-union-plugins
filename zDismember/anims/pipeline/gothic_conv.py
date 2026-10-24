import sys, zenkit, numpy as np
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import manlib
G = os.path.join(os.environ.get("GOTHIC2_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Gothic II"), "Data")
vfs = zenkit.Vfs()
for f in ("Anims.vdf", "Anims_Addon.vdf"):
    vfs.mount_disk(G + "\\" + f, clobber=zenkit.VfsOverwriteBehavior.NEWER)
h = zenkit.ModelHierarchy.load(vfs.find("HUMANS.MDH"))
names = [n.name for n in h.nodes]; par = [n.parent for n in h.nodes]
def mat(m):
    c = m.columns if not callable(m.columns) else m.columns()
    return np.array([[c[j][i] for j in range(4)] for i in range(4)], dtype=float)  # column-major -> row index i
M = [mat(n.transform) for n in h.nodes]
print("BIP01 local:\n", np.round(M[0], 3)); print("L UPPERARM local:\n", np.round(M[9], 3))
def world(local):
    W = [None]*len(local)
    for i in range(len(local)):
        W[i] = local[i] if par[i] < 0 else W[par[i]] @ local[i]
    return W
W = world(M)
for k in ("BIP01", "BIP01 HEAD", "BIP01 L HAND", "BIP01 R HAND", "BIP01 L FOOT", "BIP01 R FOOT"):
    print(f"rest {k:14} {np.round(W[names.index(k)][:3,3],1)}")

data = vfs.find("HUMANS-S_RUN.MAN").data
man = manlib.Man(bytes(data))
print("S_RUN frames", man.frames, "nodes", man.nnodes, "fps", man.fps, "checksum", hex(man.checksum), "layer", man.layer)
def qmat(q, conj):
    x, y, z, w = q
    if conj: x, y, z = -x, -y, -z
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                     [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                     [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])
for conj in (False, True):
    local = [m.copy() for m in M]
    for k, ni in enumerate(man.nodes):
        q, p = man.decode(man.sample(0, k))
        L = np.eye(4); L[:3,:3] = qmat(q, conj); L[:3,3] = p
        local[ni] = L
    Wa = world(local)
    print("conj" if conj else "asis", {k: tuple(np.round(Wa[names.index(k)][:3,3],0)) for k in ("BIP01 HEAD","BIP01 L HAND","BIP01 R HAND","BIP01 L FOOT","BIP01 R FOOT")})
    # porownanie rotacji klatki 0 z poza spoczynkowa (S_RUN ~ stoi)
    err = np.mean([np.abs(local[i][:3,:3] - M[i][:3,:3]).max() for i in man.nodes])
    print("  srednia roznica rotacji vs MDH:", round(err,3))
# pozycje lokalne z MAN vs MDH (dla wezlow innych niz korzen)
diffs = []
for k, ni in enumerate(man.nodes):
    q, p = man.decode(man.sample(0, k))
    diffs.append((names[ni], np.round(np.array(p) - M[ni][:3,3], 2)))
print(diffs[:6])
