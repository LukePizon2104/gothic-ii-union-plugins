import os, sys, struct, zenkit
G = os.path.join(os.environ.get("GOTHIC2_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Gothic II"), "Data")
vfs = zenkit.Vfs()
for f in ("Anims.vdf", "Anims_Addon.vdf"):
    vfs.mount_disk(G + "\\" + f, clobber=zenkit.VfsOverwriteBehavior.NEWER)
def parse_ani(b):
    o = 0
    def line():
        nonlocal o
        p = b.index(b"\n", o); s = b[o:p].decode("latin1"); o = p + 1; return s
    name = line(); layer, = struct.unpack_from("<I", b, o); o += 4
    nxt = line(); bi, bo = struct.unpack_from("<ff", b, o); o += 8
    flags = line(); asc = line(); d = line()
    first, last, fps, speed, cvs = struct.unpack_from("<iifff", b, o)
    return dict(name=name, layer=layer, next=nxt, blend=(bi, bo), flags=flags, asc=asc, dir=d, frames=(first, last), fps=fps, speed=speed, cvs=cvs)
def dump(fname, full, want=()):
    d = bytes(vfs.find(fname).data); o = 0; print("==", fname, len(d))
    while o < len(d):
        t, l = struct.unpack_from("<HI", d, o); body = d[o+6:o+6+l]
        if t == 0xF520:
            a = parse_ani(body)
            if full or a["name"] in want: print(hex(t), l, a)
        elif full:
            print(hex(t), l, body[:80])
        o += 6 + l
dump("HUMANS_1HST1.MSB", True)
dump("HUMANS.MSB", False, ("T_1HSFINISH", "T_2HSFINISH"))
for ov in ("HUMANS_1HST1.MSB", "HUMANS_1HST2.MSB", "HUMANS_2HST1.MSB", "HUMANS_2HST2.MSB"):
    dump(ov, False, ("T_1HSFINISH", "T_2HSFINISH"))
