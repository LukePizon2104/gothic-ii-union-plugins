"""Buduje nakladke modelu ludzi (MSB + MDH) z gotowych .MAN.

Uzycie: py -3.12 -I build_overlay.py NAZWA_NAKLADKI katalog_z_MAN [--next S_1HRUN] [--install]
Pliki MAN w katalogu: NAZWA_NAKLADKI-<ANI>.MAN; animacje dostaja definicje jak T_1HSFINISH w HUMANS.MSB
(warstwa 1, blend 0.1/0.1, flagi 'M.'). Nakladka moze tylko PODMIENIAC istniejace nazwy animacji.
"""
import argparse, glob, os, shutil, struct, sys
import zenkit

import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import manlib, msblib

GAME = os.environ.get("GOTHIC2_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Gothic II")
COMPILED = os.path.join(GAME, r"_work\Data\Anims\_compiled")


def game_vfs():
    vfs = zenkit.Vfs()
    for f in ("Anims.vdf", "Anims_Addon.vdf"):
        vfs.mount_disk(os.path.join(GAME, "Data", f), clobber=zenkit.VfsOverwriteBehavior.NEWER)
    return vfs


def chunks(d):
    o = 0
    while o < len(d):
        t, l = struct.unpack_from("<HI", d, o)
        yield t, d[o:o + 6 + l], d[o + 6:o + 6 + l]
        o += 6 + l


def parse_ani(body):
    o = 0
    def line():
        nonlocal o
        p = body.index(b"\n", o); s = body[o:p].decode("latin1"); o = p + 1; return s
    name = line(); layer, = struct.unpack_from("<I", body, o); o += 4
    nxt = line(); bi, bo = struct.unpack_from("<ff", body, o); o += 8
    flags = line(); asc = line(); d = line()
    first, last, fps, speed, cvs = struct.unpack_from("<iifff", body, o)
    return dict(name=name, layer=layer, next=nxt, blend_in=bi, blend_out=bo, flags=flags, asc=asc, dir=d,
                first=first, last=last, fps=fps, speed=speed, cvs=cvs)


def humans_ani_defs(vfs):
    """Definicje animacji (chunk 0xF520) z bazowego HUMANS.MSB."""
    d = bytes(vfs.find("HUMANS.MSB").data)
    return {a["name"]: a for a in (parse_ani(b) for t, _, b in chunks(d) if t == 0xF520)}


def build_msb(name, ani_chunks, vfs):
    """Naglowek z nakladki gry (HUMANS_1HST1.MSB) + podane chunki ANI + zakonczenie."""
    tmpl = bytes(vfs.find("HUMANS_1HST1.MSB").data)
    head = b""
    for t, raw, body in chunks(tmpl):
        if t == 0xF100:   # data + sciezka zrodla MDS
            raw = msblib.chunk(0xF100, body[:16] + (r"\_WORK\DATA\ANIMS\MDS_OVERLAY" + "\\" + name + ".MDS\n").encode("latin1"))
        if t in (0xF000, 0xF100, 0xF200, 0xF300, 0xF500, 0xF590):
            head += raw
        if t == 0xF520:
            break
    return head + b"".join(ani_chunks) + msblib.chunk(0xF5FF, b"") + msblib.chunk(0xF2FF, b"") + msblib.chunk(0xFFFF, b"")


def write_overlay(name, src, ani_chunks, vfs):
    msb = build_msb(name, ani_chunks, vfs)
    out = os.path.join(src, name + ".MSB")
    open(out, "wb").write(msb)
    open(os.path.join(src, name + ".MDH"), "wb").write(bytes(vfs.find("HUMANS_1HST1.MDH").data))
    return out


def install_files(paths):
    os.makedirs(COMPILED, exist_ok=True)
    for f in paths:
        shutil.copy2(f, COMPILED)


def build(name, src, next_ani="", vfs=None, install=False):
    name = name.upper()
    vfs = vfs or game_vfs()
    mans = sorted(glob.glob(os.path.join(src, name + "-*.MAN")))
    anis = []
    for path in mans:
        m = manlib.Man(open(path, "rb").read())
        ani = os.path.basename(path)[len(name) + 1:-4].upper()
        anis.append(msblib.ani_chunk(ani, 1, next_ani, 0.1, 0.1, "M.", "ZDISM_%s.ASC" % ani, 1, m.frames))
    out = write_overlay(name, src, anis, vfs)
    if install:
        install_files([out, os.path.join(src, name + ".MDH")] + mans)
    return [out, os.path.join(src, name + ".MDH")] + mans


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("name"); ap.add_argument("src")
    ap.add_argument("--next", default="")
    ap.add_argument("--install", action="store_true")
    a = ap.parse_args()
    files = build(a.name, a.src, a.next, install=a.install)
    print(a.name.upper(), "->", len(files) - 2, "animacji", "(zainstalowano)" if a.install else "")


if __name__ == "__main__":
    main()
