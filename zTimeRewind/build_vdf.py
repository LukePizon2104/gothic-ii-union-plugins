r"""Sklada paczke Data\zTimeRewind.vdf: DLL, skrypt runy, menu Union i wspolna baza menu.

Uzycie: py -3.12 -I build_vdf.py [--install] [--nb | --km] [--dll PLIK] [--out KATALOG]
  --install  kopiuje VDF do Data\ gry i usuwa luzne pliki (DLL i skrypty w System\Autorun),
             zeby Union / zParserExtender nie zaladowaly ich dwa razy.
  --nb       paczka dla New Balance (GOTHIC2_NB_DIR): DLL jako SYSTEM\AUTORUN\NB_UNION_PLUGIN_DIFF_02.DLL (biala lista
             DLL w UNION_ABI), runa z SPL_TimeRewind = 201 (NB zajmuje ID 0..195, zDismember 196..200), bez
             Menu\zUnionMenu.d (NB ma wlasna baze menu w Union.vdf). --install celuje wtedy w GOTHIC2_NB_DIR.
  --km       paczka dla Kronik Myrtany (TheChroniclesOfMyrtana): zawartosc jak Steam (DLL zTimeRewind.dll, runa
             SPL_TimeRewind = 99 = SPL_RESERVED_99 w Kronikach, baza menu = ta z Heartbeat.vdf), inny tylko komentarz
             naglowka i cel --install. Domyslny katalog wyjscia: release\km\.
  --dll      DLL do paczki (domyslnie Bin\zTimeRewind.dll); --out katalog VDF (domyslnie release\).
Bez --nb paczka jest dokladnie taka jak wczesniej (Steam, ID 99).
Format VDF jak w zDismember (anims\pipeline\build_vdf.py): PSVDSC_V2.00, katalog wpisow po 80 B, dane.
Skrypt runy w paczce jest w cp1250 (teksty przedmiotu w grze), menu i baza menu w UTF-8 (jak zDismember_Menu.d).
"""
import argparse, datetime, os, re, struct

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.environ.get("GOTHIC2_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Gothic II")
NB_GAME = os.environ.get("GOTHIC2_NB_DIR", "")   # katalog gry z New Balance
KM_GAME = os.environ.get("GOTHIC_KM_DIR", "")   # katalog Kronik Myrtany
NB_DLL_NAME = "NB_UNION_PLUGIN_DIFF_02.DLL"
NB_SPELL_ID = 201
DIR, LAST = 0x80000000, 0x40000000


def dos_time(dt):
    return ((((dt.year - 1980) << 9) | (dt.month << 5) | dt.day) << 16) | (dt.hour << 11) | (dt.minute << 5) | (dt.second // 2)


def write_vdf(path, files, comment, when=None):
    """files: {'SYSTEM\\AUTORUN\\X.DLL': bytes}. Nazwy w VDF wielkimi literami."""
    tree = {}
    for name, data in files.items():
        parts = name.upper().split("\\")
        node = tree
        for p in parts[:-1]:
            node = node.setdefault(p, {})
        node[parts[-1]] = data

    entries = []   # [nazwa, typ('d'/'f'), wartosc, last, indeks pierwszego dziecka]

    def add(node):
        items = sorted(node.items(), key=lambda kv: (not isinstance(kv[1], dict), kv[0]))
        start = len(entries)
        for i, (name, child) in enumerate(items):
            entries.append([name, "d" if isinstance(child, dict) else "f", child, i == len(items) - 1, 0])
        for i, (name, child) in enumerate(items):
            if isinstance(child, dict):
                entries[start + i][4] = len(entries)
                add(child)

    add(tree)
    data_off = 296 + 80 * len(entries)
    blob = b""
    cat = b""
    nfiles = 0
    for name, kind, value, last, child in entries:
        flags = LAST if last else 0
        if kind == "d":
            cat += name.encode("latin1").ljust(64, b" ") + struct.pack("<IIII", child, 0, DIR | flags, 0)
        else:
            cat += name.encode("latin1").ljust(64, b" ") + struct.pack("<IIII", data_off + len(blob), len(value), flags, 0x20)
            blob += value
            nfiles += 1
    hdr = comment.encode("latin1")[:255].ljust(256, b"\x1a")
    hdr += b"PSVDSC_V2.00\n\r\n\r" + struct.pack("<IIIIII", len(entries), nfiles, dos_time(when or datetime.datetime.now()), len(blob), 296, 80)
    with open(path, "wb") as f:
        f.write(hdr + cat + blob)


def read(path):
    with open(os.path.join(HERE, path), "rb") as f:
        return f.read()


SPL_LINE = re.compile(r"^(const int SPL_TimeRewind\s*=\s*)\d+(\s*;)", re.M)


def rune_with_id(text, spell_id):
    """Skrypt runy z inna wartoscia SPL_TimeRewind (dokladnie jedna definicja stalej)."""
    out, n = SPL_LINE.subn(lambda m: m.group(1) + str(spell_id) + m.group(2), text)
    if n != 1:
        raise SystemExit("zTimeRewind_Rune.d: oczekiwana 1 definicja SPL_TimeRewind, jest %d" % n)
    return out


def collect(nb=False, dll=None):
    rune = read(r"scripts\zTimeRewind_Rune.d").decode("utf-8")
    dll_data = read(dll or r"Bin\zTimeRewind.dll")
    if not nb:
        return {
            "SYSTEM\\AUTORUN\\ZTIMEREWIND.DLL": dll_data,
            "SYSTEM\\AUTORUN\\ZTIMEREWIND_RUNE.D": rune.encode("cp1250"),
            "SYSTEM\\AUTORUN\\ZTIMEREWIND_MENU.D": read(r"scripts\zTimeRewind_Menu.d"),
            "SYSTEM\\AUTORUN\\MENU\\ZUNIONMENU.D": read(r"scripts\Menu\zUnionMenu.d"),
        }
    rune = rune_with_id(rune, NB_SPELL_ID).replace("domyslnie 99", "w New Balance %d" % NB_SPELL_ID)
    return {
        "SYSTEM\\AUTORUN\\" + NB_DLL_NAME: dll_data,
        "SYSTEM\\AUTORUN\\ZTIMEREWIND_RUNE.D": rune.encode("cp1250"),
        "SYSTEM\\AUTORUN\\ZTIMEREWIND_MENU.D": read(r"scripts\zTimeRewind_Menu.d"),
    }


def install(vdf, game=GAME, nb=False):
    target = os.path.join(game, "Data", "zTimeRewind.vdf")
    with open(vdf, "rb") as src, open(target, "wb") as dst:
        dst.write(src.read())
    autorun = os.path.join(game, "System", "Autorun")
    removed = []
    loose = ("zTimeRewind.dll", "zTimeRewind.d", "zTimeRewind_Rune.d", "zTimeRewind_Menu.d")
    if nb:
        loose += (NB_DLL_NAME,)
    for n in loose:
        f = os.path.join(autorun, n)
        if os.path.exists(f):
            os.remove(f)
            removed.append(n)
    return target, removed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--install", action="store_true")
    ap.add_argument("--nb", action="store_true", help="paczka New Balance (GOTHIC2_NB_DIR)")
    ap.add_argument("--km", action="store_true", help="paczka Kronik Myrtany (zawartosc jak Steam, --install do Kronik)")
    ap.add_argument("--dll", help="DLL do paczki (domyslnie Bin\\zTimeRewind.dll)")
    ap.add_argument("--out", help="katalog VDF (domyslnie release\\)")
    args = ap.parse_args()
    if args.nb and args.km:
        raise SystemExit("--nb i --km naraz")
    game = NB_GAME if args.nb else KM_GAME if args.km else GAME
    if args.install and not os.path.isdir(os.path.join(game, "Data")):
        raise SystemExit("--install: brak katalogu gry %r - ustaw GOTHIC2_DIR / GOTHIC2_NB_DIR (--nb) / GOTHIC_KM_DIR (--km)" % game)
    out_dir = os.path.abspath(args.out) if args.out else \
        os.path.join(HERE, "release", "km") if args.km else os.path.join(HERE, "release")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "zTimeRewind.vdf")
    files = collect(args.nb, args.dll and os.path.abspath(args.dll))
    comment = "zTimeRewind - runa Piaski Czasu, cofanie czasu (Gothic II NotR, Union)"
    if args.nb:
        comment = "zTimeRewind - runa Piaski Czasu, cofanie czasu (New Balance, ID %d)" % NB_SPELL_ID
    if args.km:
        comment = "zTimeRewind - runa Piaski Czasu, cofanie czasu (Kroniki Myrtany, ID 99)"
    write_vdf(out, files, comment)
    print("VDF:", out, len(files), "plikow,", os.path.getsize(out) // 1024, "KB")
    for name in sorted(files):
        print("  ", name, len(files[name]), "B")
    if args.install:
        target, removed = install(out, NB_GAME if args.nb else KM_GAME if args.km else GAME, args.nb)
        print("zainstalowano:", target, "| usuniete luzne:", ", ".join(removed) or "-")


if __name__ == "__main__":
    main()
