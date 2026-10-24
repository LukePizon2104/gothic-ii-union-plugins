r"""Z jednego FBX (Mixamo) robi kompletny finisher: animacje bohatera z ruchem i w miejscu, nakladki, wpis w finishers.ini.

Uzycie:
  py -3.12 -I make_finisher.py FIN_2H_SLASH "Great Sword Slash.fbx" --weapon 2H --cut HEAD
       [--victims HUMAN,ORC,MONSTER] [--poses STANDING,DAZED] [--slowmo 0.35,-0.25,0.35] [--weight 10] [--install]
Wynik: zDismember\data\_work\Data\Anims\_compiled\HUMANS_ZDF_<ID>[_IP].* i sekcja [<ID>] w zDismember\data\System\zDismember\finishers.ini.
"""
import argparse, json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import retarget, analyze_anim, build_overlay

BLENDER = os.environ.get("BLENDER_EXE", r"C:\Program Files\Blender Foundation\Blender 4.4\blender.exe")
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORK = os.path.join(ROOT, "anims", "work")
DATA = os.path.join(ROOT, "data")
COMPILED = os.path.join(DATA, "_work", "Data", "Anims", "_compiled")
INI = os.path.join(DATA, "System", "zDismember", "finishers.ini")


def dump_fbx(fbx):
    out = os.path.join(WORK, os.path.splitext(os.path.basename(fbx))[0] + ".json")
    if not os.path.exists(out) or os.path.getmtime(out) < os.path.getmtime(fbx):
        subprocess.run([BLENDER, "-b", "--factory-startup", "-P", os.path.join(HERE, "fbx_dump.py"), "--", fbx, out],
                       check=True, stdout=subprocess.DEVNULL)
    return out


def run_retarget(dump, out, ani, grip, inplace):
    argv = ["retarget.py", dump, out, "--name", ani, "--grip", grip] + (["--inplace"] if inplace else [])
    saved, sys.argv = sys.argv, argv
    try:
        retarget.main()
    finally:
        sys.argv = saved


def write_ini_section(fid, lines):
    os.makedirs(os.path.dirname(INI), exist_ok=True)
    text = open(INI, encoding="ascii").read() if os.path.exists(INI) else (
        "; Katalog finisherow zDismember - generowany przez anims\\pipeline\\make_finisher.py\n"
        "; Pola: weapon 1H/2H, victim HUMAN/ORC/ZOMBIE/MONSTER, pose STANDING/DAZED/LYING, cut HEAD/ARM_L/..., cutTime s,\n"
        "; victimOffset cm (x w prawo, y, z do przodu wzgledem bohatera), slowmo mnoznik,od,do (s wzgledem ciecia)\n")
    section = "[%s]\n%s\n" % (fid, "\n".join(lines))
    pattern = re.compile(r"^\[%s\]\n(?:(?!\[).*\n?)*" % re.escape(fid), re.M)
    text = pattern.sub(section + "\n", text) if pattern.search(text) else text.rstrip("\n") + "\n\n" + section
    open(INI, "w", encoding="ascii", newline="\n").write(text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("id"); ap.add_argument("fbx")
    ap.add_argument("--weapon", required=True, choices=["1H", "2H"])
    ap.add_argument("--cut", default="HEAD")
    ap.add_argument("--victims", default="HUMAN,ORC")
    ap.add_argument("--poses", default="STANDING,DAZED")
    ap.add_argument("--slowmo", default="0.35,-0.25,0.35")
    ap.add_argument("--weight", default="10")
    ap.add_argument("--install", action="store_true", help="kopiuj nakladki i ini do gry (_work, System) do testow")
    a = ap.parse_args()

    fid = a.id.upper()
    ani = "T_%sSFINISH" % a.weapon
    grip = "T_%sATTACKL" % a.weapon
    stance = "S_%sRUN" % a.weapon
    dump = dump_fbx(a.fbx)
    vfs = build_overlay.game_vfs()
    sk = retarget.GothicSkeleton(vfs)

    results = {}
    for suffix, inplace in (("", False), ("_IP", True)):
        name = "HUMANS_ZDF_%s%s" % (fid, suffix)
        tmp = os.path.join(WORK, "fin", name)
        os.makedirs(tmp, exist_ok=True)
        man = os.path.join(tmp, "%s-%s.MAN" % (name, ani))
        run_retarget(dump, man, ani, grip, inplace)
        # wariant w miejscu w tej samej klatce ciecia co wariant z ruchem (w ini jest jeden cutTime)
        results[suffix] = analyze_anim.analyze(man, sk, frame=results[""]["cutFrame"] if suffix else None)
        files = build_overlay.build(name, tmp, next_ani=stance, vfs=vfs)
        os.makedirs(COMPILED, exist_ok=True)
        for f in files:
            with open(f, "rb") as src, open(os.path.join(COMPILED, os.path.basename(f)), "wb") as dst:
                dst.write(src.read())
        if a.install:
            build_overlay.install_files(files)

    r, ri = results[""], results["_IP"]
    fmt = lambda o: "%g,%g,%g" % tuple(o)
    write_ini_section(fid, [
        "weapon=%s" % a.weapon, "victim=%s" % a.victims, "pose=%s" % a.poses,
        "overlay=HUMANS_ZDF_%s" % fid, "heroAni=%s" % ani, "cut=%s" % a.cut.upper(),
        "cutTime=%g" % r["cutTime"], "victimOffset=%s" % fmt(r["offset"]),
        "inplace=HUMANS_ZDF_%s_IP" % fid, "inplaceOffset=%s" % fmt(ri["offset"]),
        "slowmo=%s" % a.slowmo, "weight=%s" % a.weight,
        "; zrodlo: %s, dlugosc %.2f s, ostrze w chwili ciecia na wysokosci %.0f cm" % (os.path.basename(a.fbx), r["duration"], r["height"]),
    ])
    if a.install:
        os.makedirs(os.path.join(build_overlay.GAME, "System", "zDismember"), exist_ok=True)
        with open(INI, "rb") as src, open(os.path.join(build_overlay.GAME, "System", "zDismember", "finishers.ini"), "wb") as dst:
            dst.write(src.read())
    print("%s: ciecie %.2f s, ofiara %s (w miejscu %s), ostrze na %.0f cm" % (fid, r["cutTime"], r["offset"], ri["offset"], r["height"]))


if __name__ == "__main__":
    main()
