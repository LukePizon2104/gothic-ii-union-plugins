import sys, zenkit
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
G = os.path.join(os.environ.get("GOTHIC2_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Gothic II"), "Data")
vfs = zenkit.Vfs()
for f in ("Anims.vdf", "Anims_Addon.vdf"):
    vfs.mount_disk(G + "\\" + f, clobber=zenkit.VfsOverwriteBehavior.NEWER)
h = zenkit.ModelHierarchy.load(vfs.find("HUMANS.MDH"))
print("checksum", hex(h.checksum), "nodes", len(h.nodes), "root_translation", h.root_translation)
n0 = h.nodes[0]
print([a for a in dir(n0) if not a.startswith("_")])
print(type(n0.transform), [a for a in dir(n0.transform) if not a.startswith("_")][:30])
for i, n in enumerate(h.nodes):
    print(i, n.name, n.parent)
