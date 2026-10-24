"""Nakladka oszolomienia HUMANS_ZDF_DAZE z animacji gry (tymczasowo, do czasu animacji z Mixamo).

Uzycie: py -3.12 -I build_daze.py katalog_wyjsciowy [--install]
Podmienia upadek i lezenie nieprzytomnego na klękanie z modlitwa (pokonany blaga o litosc),
a smierc z lezenia gra z dlugim przejsciem (cialo osuwa sie z kleczek).
"""
import os, sys
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "zQuickSign"))
import manlib, msblib
import build_overlay

NAME = "HUMANS_ZDF_DAZE"
# (podmieniana nazwa, zrodlowy MAN z gry, definicja (warstwa/next/blend/flagi) z HUMANS.MSB, wymuszony blend-in albo None)
# Smierc z kleczek: wlasna animacja smierci z lezenia + dlugie przejscie - bezglowe cialo osuwa sie z kleczek na ziemie.
# (T_DEADB zaczyna sie od stania - cialo najpierw wstawalo, test w grze 15.10.)
MAP = [
    ("T_STAND_2_WOUNDED", "T_STAND_2_PRAY", "T_STAND_2_WOUNDED", None),
    ("T_STAND_2_WOUNDEDB", "T_STAND_2_PRAY", "T_STAND_2_WOUNDEDB", None),
    ("S_WOUNDED", "S_PRAY", "S_WOUNDED", None),
    ("S_WOUNDEDB", "S_PRAY", "S_WOUNDEDB", None),
    ("T_WOUNDED_2_DEAD", "T_WOUNDED_2_DEAD", "T_WOUNDED_2_DEAD", 0.35),
    ("T_WOUNDEDB_2_DEADB", "T_WOUNDEDB_2_DEADB", "T_WOUNDEDB_2_DEADB", 0.35),
]


def build(out_dir, vfs=None, install=False):
    vfs = vfs or build_overlay.game_vfs()
    defs = build_overlay.humans_ani_defs(vfs)
    os.makedirs(out_dir, exist_ok=True)
    chunks, mans = [], []
    for target, src, defname, blend_in in MAP:
        m = manlib.Man(bytes(vfs.find("HUMANS-%s.MAN" % src).data))
        d = defs[defname]
        m.name = target
        m.next = d["next"]
        path = os.path.join(out_dir, "%s-%s.MAN" % (NAME, target))
        open(path, "wb").write(m.build())
        mans.append(path)
        # kierunek zawsze 'F' - tresc pochodzi z innej animacji niz definicja
        chunks.append(msblib.ani_chunk(target, d["layer"], d["next"], blend_in if blend_in is not None else d["blend_in"], d["blend_out"], d["flags"],
                                       "ZDISM_DAZE.ASC", 1, m.frames, "F", d["fps"] or 25.0, d["speed"], d["cvs"]))
    msb = build_overlay.write_overlay(NAME, out_dir, chunks, vfs)
    files = [msb, os.path.join(out_dir, NAME + ".MDH")] + mans
    if install:
        build_overlay.install_files(files)
    return files


if __name__ == "__main__":
    files = build(sys.argv[1], install="--install" in sys.argv)
    print(NAME, "->", len(files) - 2, "animacji")
