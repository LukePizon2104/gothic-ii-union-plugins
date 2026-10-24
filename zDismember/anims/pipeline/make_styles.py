r"""Natywne style walki Gothica pod wlasnymi nazwami - do debugowego cyklu stylow w pluginie (zdism style).

zExtendedBattleAnimations przykrywa HUMANS_1HST1/1HST2/2HST1/2HST2 swoimi plikami o tych samych nazwach, wiec oryginaly
z Anims.vdf trafiaja do paczki prywatnej jako HUMANS_ZDF_NAT_<styl> (MSB z poprawiona sciezka zrodla + MAN-y + MDH).
Uzycie: py -3.12 -I make_styles.py
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import msblib
import build_overlay, make_moves

NATIVE = ("HUMANS_1HST1", "HUMANS_1HST2", "HUMANS_2HST1", "HUMANS_2HST2")


def vfs_files(vfs, prefix):
    out = []

    def walk(node):
        for c in node.children:
            if c.is_dir():
                walk(c)
            elif c.name.upper().startswith(prefix):
                out.append(c)
    walk(vfs.root)
    return out


def copy_native(vfs, src, dst, out_dir):
    """Kopia nakladki 'src' z gry jako 'dst' w out_dir; zwraca liste zapisanych plikow."""
    os.makedirs(out_dir, exist_ok=True)
    msb = b""
    for t, raw, body in build_overlay.chunks(bytes(vfs.find(src + ".MSB").data)):
        if t == 0xF100:   # data + sciezka zrodla MDS - jedyne miejsce z nazwa nakladki
            raw = msblib.chunk(0xF100, body[:16] + (r"\_WORK\DATA\ANIMS\MDS_OVERLAY" + "\\" + dst + ".MDS\n").encode("latin1"))
        msb += raw
    files = [os.path.join(out_dir, dst + ".MSB")]
    open(files[0], "wb").write(msb)
    mdh = vfs.find(src + ".MDH") or vfs.find("HUMANS_1HST1.MDH")
    files.append(os.path.join(out_dir, dst + ".MDH"))
    open(files[-1], "wb").write(bytes(mdh.data))
    for node in vfs_files(vfs, src.upper() + "-"):
        ani = node.name.upper()[len(src) + 1:]
        files.append(os.path.join(out_dir, "%s-%s" % (dst, ani)))
        open(files[-1], "wb").write(bytes(node.data))
    return files


def main():
    vfs = build_overlay.game_vfs()
    for src in NATIVE:
        dst = src.replace("HUMANS_", "HUMANS_ZDF_NAT_")
        files = copy_native(vfs, src, dst, make_moves.COMPILED)
        print("%s -> %s: %d plikow" % (src, dst, len(files)))


if __name__ == "__main__":
    main()
