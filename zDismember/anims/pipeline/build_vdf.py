r"""Sklada paczke Data\zDismember.vdf: DLL, finishers.ini i nakladki animacji.

Uzycie: py -3.12 -I build_vdf.py [--install] [--w3] [--nb | --km] [--dll PLIK] [--out KATALOG]
  --install  kopiuje VDF do Data\ gry i usuwa luzne pliki testowe (DLL w system\autorun, ini, nakladki w _work),
             zeby Union nie zaladowal pluginu dwa razy.
  --w3       prywatna paczka zDismember_W3.vdf z data_w3 (animacje CDPR - NIE do wydania).
  --nb       paczka dla New Balance (GOTHIC2_NB_DIR): DLL jako SYSTEM\AUTORUN\NB_UNION_PLUGIN_DIFF_01.DLL (biala lista DLL
             w UNION_ABI; ALLOWED_01..05 zajete), bez Menu\zUnionMenu.d (NB ma wlasna baze menu w Union.vdf), skrypt
             znakow ze stalymi SPL_Aard..SPL_Quen = 196..200 (NB zajmuje 0..195, zTimeRewind 201) i bez NAME_Dam_Fly
             (stalej nie ma w skryptach NB - wpisany tekst ze Steam G2) oraz pliki z data_nb (skrypty tylko dla NB, np.
             premia kieszonkowa Axii w RX_GetThieftChance). Z --w3: prywatna paczka W3 dla NB (te same dane).
             --install celuje wtedy w GOTHIC2_NB_DIR. Domyslny katalog wyjscia: release\nb\.
  --km       paczka dla Kronik Myrtany (TheChroniclesOfMyrtana): zawartosc jak Steam (DLL zDismember.dll - Kroniki
             nie maja bialej listy DLL, znaki 90..94 = SPL_RESERVED_9x, baza menu Union = ta sama co w Heartbeat.vdf),
             inny tylko komentarz naglowka, katalog wyjscia (release\km\) i cel --install (z --w3 tez). Roznice
             Kronik sa w Gothic.ini (AivarIndex=-1, klawisze) 
  --dll      DLL do paczki (domyslnie Bin\zDismember.dll); --out katalog VDF (domyslnie release\ albo release\nb\).
Bez --nb paczki sa dokladnie takie jak wczesniej (Steam, znaki 90..94) - poza znacznikiem czasu w naglowku.
Format VDF (PSVDSC_V2.00): komentarz 256 B, sygnatura 16 B, 6 x u32 (wpisy, pliki, czas DOS, rozmiar danych,
offset katalogu = 296, rozmiar wpisu = 80), wpisy (nazwa 64 B, offset, rozmiar, typ, atrybuty), dane.
Katalog: wpisy jednego folderu leza obok siebie, ostatni ma flage LAST, folder wskazuje indeks pierwszego dziecka.
"""
import argparse, datetime, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
GAME = os.environ.get("GOTHIC2_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Gothic II")
NB_GAME = os.environ.get("GOTHIC2_NB_DIR", "")   # katalog gry z New Balance
KM_GAME = os.environ.get("GOTHIC_KM_DIR", "")   # katalog Kronik Myrtany
NB_DLL_NAME = "NB_UNION_PLUGIN_DIFF_01.DLL"
NB_SIGN_IDS = {"Aard": 196, "Igni": 197, "Axii": 198, "Yrden": 199, "Quen": 200}
NB_NAME_DAM_FLY = '"Obr. wiatr"'   # wartosc NAME_Dam_Fly z GOTHIC.DAT Steam G2 (PL) - w NB stalej brak
SIGNS_SCRIPT = "SYSTEM\\AUTORUN\\ZDISMEMBER_SIGNS.D"
UNION_MENU_BASE = "SYSTEM\\AUTORUN\\MENU\\ZUNIONMENU.D"
NB_DATA_DIR = "data_nb"   # skrypty tylko dla New Balance (sciezki jak w data)
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

    entries = []   # [nazwa, typ('d'/'f'), wartosc, last]

    def add(node):
        items = sorted(node.items(), key=lambda kv: (not isinstance(kv[1], dict), kv[0]))
        start = len(entries)
        for i, (name, child) in enumerate(items):
            entries.append([name, "d" if isinstance(child, dict) else "f", child, i == len(items) - 1, 0])
        for i, (name, child) in enumerate(items):
            if isinstance(child, dict):
                entries[start + i][4] = len(entries)   # indeks pierwszego dziecka
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


def collect(data_dir="data", with_dll=True, dll=None, dll_name="SYSTEM\\AUTORUN\\ZDISMEMBER.DLL"):
    files = {}
    if with_dll:
        dll = dll or os.path.join(ROOT, "Bin", "zDismember.dll")
        files[dll_name] = open(dll, "rb").read()
    data = os.path.join(ROOT, data_dir)
    for base, _, names in os.walk(data):
        for n in names:
            full = os.path.join(base, n)
            rel = os.path.relpath(full, data).replace("/", "\\")
            files[rel.upper()] = open(full, "rb").read()
    return files


SPL_LINE = re.compile(r"^(const int SPL_(Aard|Igni|Axii|Yrden|Quen)\s*=\s*)\d+(\s*;)", re.M)
NAME_DAM_FLY = re.compile(r"\bNAME_Dam_Fly\b")


def signs_for_nb(text):
    """zDismember_Signs.d dla New Balance: stale slotow 196..200 i tekst zamiast NAME_Dam_Fly (w NB brak stalej)."""
    out, n = SPL_LINE.subn(lambda m: m.group(1) + str(NB_SIGN_IDS[m.group(2)]) + m.group(3), text)
    if n != 5:
        raise SystemExit("zDismember_Signs.d: oczekiwane 5 definicji SPL_Aard..SPL_Quen, jest %d" % n)
    out, k = NAME_DAM_FLY.subn(NB_NAME_DAM_FLY, out)
    if k != 1:
        raise SystemExit("zDismember_Signs.d: oczekiwane 1 uzycie NAME_Dam_Fly, jest %d" % k)
    out = out.replace("przejmuje sloty 90..94", "przejmuje sloty 196..200 (New Balance)")
    return out


def nb_files(files):
    """Pliki paczki Steam -> New Balance (nazwa DLL ustawiona w collect): bez bazy menu Union, znaki 196..200,
    skrypty tylko dla NB z data_nb (symbole NB - w Steam G2 by sie nie skompilowaly)."""
    files.pop(UNION_MENU_BASE, None)
    files[SIGNS_SCRIPT] = signs_for_nb(files[SIGNS_SCRIPT].decode("utf-8")).encode("utf-8")
    if os.path.isdir(os.path.join(ROOT, NB_DATA_DIR)):
        files.update(collect(NB_DATA_DIR, with_dll=False))
    return files


def install(vdf, name="zDismember.vdf", clean_loose=True, game=GAME, extra_loose=()):
    target = os.path.join(game, "Data", name)
    with open(vdf, "rb") as src, open(target, "wb") as dst:
        dst.write(src.read())
    removed = []
    if not clean_loose:
        return target, removed
    loose = [os.path.join(game, "System", "Autorun", "zDismember.dll"), os.path.join(game, "System", "zDismember", "finishers.ini")]
    loose += [os.path.join(game, "System", "Autorun", n) for n in extra_loose]
    comp = os.path.join(game, "_work", "Data", "Anims", "_compiled")
    if os.path.isdir(comp):
        loose += [os.path.join(comp, n) for n in os.listdir(comp) if n.upper().startswith(("HUMANS_ZDF_", "HUMANS_ZDISM"))]
    for f in loose:
        if os.path.exists(f):
            os.remove(f)
            removed.append(os.path.basename(f))
    return target, removed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--install", action="store_true")
    ap.add_argument("--w3", action="store_true", help="prywatna paczka zDismember_W3.vdf z data_w3 (animacje CDPR - nie do wydania)")
    ap.add_argument("--nb", action="store_true", help="paczka New Balance (GOTHIC2_NB_DIR): DLL NB_UNION_PLUGIN_DIFF_01, znaki 196..200")
    ap.add_argument("--km", action="store_true", help="paczka Kronik Myrtany (zawartosc jak Steam, --install do Kronik)")
    ap.add_argument("--dll", help="DLL do paczki (domyslnie Bin\\zDismember.dll)")
    ap.add_argument("--out", help="katalog VDF (domyslnie release\\, z --nb release\\nb\\)")
    a = ap.parse_args()
    if a.nb and a.km:
        raise SystemExit("--nb i --km naraz")
    out_dir = os.path.abspath(a.out) if a.out else os.path.join(ROOT, "release", "nb") if a.nb else \
        os.path.join(ROOT, "release", "km") if a.km else os.path.join(ROOT, "release")
    os.makedirs(out_dir, exist_ok=True)
    game = NB_GAME if a.nb else KM_GAME if a.km else GAME
    if a.install and not os.path.isdir(os.path.join(game, "Data")):
        raise SystemExit("--install: brak katalogu gry %r - ustaw GOTHIC2_DIR / GOTHIC2_NB_DIR (--nb) / GOTHIC_KM_DIR (--km)" % game)
    if a.w3:
        out = os.path.join(out_dir, "zDismember_W3.vdf")
        files = collect("data_w3", with_dll=False)
        comment = "zDismember W3 - PRYWATNE finishery z Wiedzmina 3 (CD Projekt RED), NIE DO WYDANIA"
        if a.nb:
            comment = "zDismember W3 (New Balance) - PRYWATNE animacje z Wiedzmina 3 (CD Projekt RED), NIE DO WYDANIA"
        if a.km:
            comment = "zDismember W3 (Kroniki Myrtany) - PRYWATNE animacje z Wiedzmina 3 (CD Projekt RED), NIE DO WYDANIA"
        write_vdf(out, files, comment)
        print("VDF W3:", out, len(files), "plikow,", os.path.getsize(out) // 1024, "KB")
        if a.install:
            print("zainstalowano:", install(out, "zDismember_W3.vdf", clean_loose=False, game=game)[0])
        return
    dll = a.dll and os.path.abspath(a.dll)
    if a.nb:
        files = nb_files(collect(dll=dll, dll_name="SYSTEM\\AUTORUN\\" + NB_DLL_NAME))
        comment = "zDismember - odcinanie konczyn, finishery i ruch W3 (New Balance, znaki 196-200, Union)"
    else:
        files = collect(dll=dll)
        comment = "zDismember - odcinanie konczyn i finishery (Gothic II NotR, Union)"
        if a.km:
            comment = "zDismember - odcinanie konczyn, finishery i ruch W3 (Kroniki Myrtany, znaki 90-94, Union)"
    out = os.path.join(out_dir, "zDismember.vdf")
    write_vdf(out, files, comment)
    print("VDF:", out, len(files), "plikow,", os.path.getsize(out) // 1024, "KB")
    if a.nb:
        for name in sorted(files):
            if name.startswith("SYSTEM\\AUTORUN\\"):
                print("  ", name, len(files[name]), "B")
    if a.install:
        target, removed = install(out, game=game, extra_loose=(NB_DLL_NAME,) if a.nb else ())
        print("zainstalowano:", target, "| usuniete luzne:", ", ".join(removed) or "-")


if __name__ == "__main__":
    main()
