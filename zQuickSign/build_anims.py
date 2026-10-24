# -*- coding: utf-8 -*-
"""Nakladka Data/zQuickSign_Anims.vdf: animacja szybkiego Znaku T_QSCAST dla zQuickSign.

T_QSCAST = T_FIBCAST_2_FIBSHOOT (rzut kuli ognia, 8 klatek @25) obciety do 14 kosci tulowia i rak
(jak T_1HATTACKMOVE - cios w biegu), na warstwie 6, bez nastepnej animacji. Wyzsza warstwa
nadpisuje tylko swoje kosci, wiec nogi dalej biegna, a miecz zostaje w rece (ZS_RIGHTHAND poza
zestawem). Animacja nie ma zdarzen - czar rzuca plugin, nie animacja.

Nakladka overlay MDS nie moze dodac nowej nazwy animacji (silnik odrzuca caly overlay,
0x5894A0), wiec wpis ANI idzie do kopii bazowego HUMANS.MSB z New Balance.

UWAGA: nakladka zawiera CALE HUMANS.MSB z dnia budowania - po aktualizacji New Balance
uruchom skrypt ponownie (--check to wykrywa).

Uzycie:  py -3.12 build_anims.py          (buduje / przebudowuje; gra musi byc wylaczona)
         py -3.12 build_anims.py --check  (tylko sprawdza aktualnosc)
Cofniecie: usun Data/zQuickSign_Anims.vdf (i .txt) - plugin bez niej rzuca bez animacji.
"""
import datetime
import hashlib
import os
import sys
from pathlib import Path

from zenkit import GameVersion, ModelAnimation, ModelScript, Vfs, VfsNode, VfsOverwriteBehavior

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from manlib import Man                                   # noqa: E402
from msblib import ani_chunk, insert_before_enum_end      # noqa: E402

DATA = Path(os.environ.get("GOTHIC2_NB_DIR", ".")) / "Data"   # katalog gry z New Balance (GOTHIC2_NB_DIR)
OUT = DATA / "zQuickSign_Anims.vdf"
STAMP = OUT.with_suffix(".txt")
VPATH = "_WORK/DATA/ANIMS/_COMPILED"
MSB = "HUMANS.MSB"
SRC_ANI = "T_FIBCAST_2_FIBSHOOT"
NAME, LAYER = "T_QSCAST", 6
BLEND_IN, BLEND_OUT = 0.1, 0.3
# indeksy kosci HUMANS.MDH (checksum 0xC63488C2): SPINE1 SPINE2 NECK HEAD, L CLAVICLE..FINGER0,
# R CLAVICLE..FINGER0 - ten sam zestaw co T_1HATTACKMOVE i T_DIALOGGESTURE_*
KEEP = {3, 4, 5, 6, 8, 9, 10, 11, 12, 15, 16, 17, 18, 19}
# nowszy niz AB_Scripts.vdf (2033-12-05), z ktorego pochodzi HUMANS.MSB New Balance
BUILD_TIME = datetime.datetime(2041, 6, 3, 12, 0, 0)


def game_vfs(include_out):
    """Wszystkie VDF z Data, nowsze pliki wygrywaja - jak w grze."""
    vfs = Vfs()
    for p in sorted(DATA.glob("*.vdf")):
        if p == OUT and not include_out:
            continue
        vfs.mount_disk(str(p), clobber=VfsOverwriteBehavior.OLDER)
    return vfs


def read(vfs, name):
    node = vfs.find(name)
    assert node is not None, f"brak {name} w Data"
    return bytes(node.data)


def main():
    vfs = game_vfs(include_out=False)
    msb = read(vfs, MSB)
    src_md5 = hashlib.md5(msb).hexdigest()

    if "--check" in sys.argv:
        if not OUT.exists():
            print("Nakladka nie istnieje.")
            return 0
        old = STAMP.read_text(encoding="utf-8").split()[0] if STAMP.exists() else "?"
        if old == src_md5:
            print("OK: nakladka zbudowana z aktualnego HUMANS.MSB.")
            return 0
        print("UWAGA: New Balance ma nowe HUMANS.MSB - przebuduj nakladke (uruchom bez --check) "
              "albo ja usun!")
        return 1

    man = Man(read(vfs, f"HUMANS-{SRC_ANI}.MAN"))
    assert man.events() == 0, "zrodlowa animacja ma zdarzenia"
    script = f'\t\tANI        ("{NAME}" {LAYER} "" {BLEND_IN} {BLEND_OUT} M. "HUMANS_{NAME}.ASC" F 0 {man.frames - 1})'
    man.subset(KEEP, name=NAME, layer=LAYER, nxt="", source_script=script)
    assert man.nnodes == len(KEEP), (man.nnodes, len(KEEP))
    man_bytes = man.build()
    msb2 = insert_before_enum_end(msb, ani_chunk(NAME, LAYER, "", BLEND_IN, BLEND_OUT, "M.",
                                                 f"HUMANS_{NAME}.ASC", 0, man.frames - 1))

    out = Vfs()
    d = out.mkdir(VPATH)
    d.create(VfsNode.new_file(MSB, msb2, timestamp=BUILD_TIME))
    d.create(VfsNode.new_file(f"HUMANS-{NAME}.MAN", man_bytes, timestamp=BUILD_TIME))
    out.save(str(OUT), GameVersion.GOTHIC2, BUILD_TIME)
    STAMP.write_text(f"{src_md5}  zrodlowe HUMANS.MSB (najnowsze w Data)\n"
                     f"zbudowano {datetime.datetime.now():%Y-%m-%d %H:%M}; {NAME} z {SRC_ANI}, "
                     f"warstwa {LAYER}, {man.frames} klatek, {man.nnodes} kosci\n", encoding="utf-8")
    print(f"Zapisano {OUT} ({OUT.stat().st_size:,} B)")

    # weryfikacja: w grze wygrywa nasze MSB, a zenkit czyta oba pliki
    vfs2 = game_vfs(include_out=True)
    assert read(vfs2, MSB) == msb2, "nakladka przegrywa z innym HUMANS.MSB"
    tmp = Path(os.environ.get("TEMP", ".")) / "zqs_verify"
    tmp.mkdir(exist_ok=True)
    (tmp / MSB).write_bytes(msb2)
    (tmp / f"HUMANS-{NAME}.MAN").write_bytes(man_bytes)
    script_ok = ModelScript.load(str(tmp / MSB))
    ani = [a for a in script_ok.animations if a.name.upper() == NAME]
    assert ani and ani[0].layer == LAYER, "brak T_QSCAST w MSB"
    anim = ModelAnimation.load(str(tmp / f"HUMANS-{NAME}.MAN"))
    assert anim.frame_count == man.frames and anim.node_count == len(KEEP)
    print(f"Weryfikacja: {MSB} z nakladki wygrywa; {NAME}: warstwa {ani[0].layer}, "
          f"{anim.frame_count} klatek, {anim.node_count} kosci, animacji w MSB {len(script_ok.animations)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
