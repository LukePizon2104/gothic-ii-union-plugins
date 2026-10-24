r"""Sprawdzenie skryptow Znakow Wiedzmina (zDismember data\System\Autorun\zDismember_Signs*.d).

Bez gry. Kolejno:
1. Plik: UTF-8 bez BOM, znaki do przedstawienia w cp1250 (tak jak tekst trafia do gry), blok META
   z wlasciwym Parser (Game / VFX / PFX / SFX), Engine = G2A, MergeMode.
2. Kompilacja kompilatorem Daedalus z warsztatu (bin\gdc.cmd) RAZEM ze zrodlami G2 Addon
   (gothic-2-addon-scripts): Gothic.src + zDismember_Signs.d, VisualFX.src + ..._SignsVfx.d,
   ParticleFX.src + ..._SignsPfx.d, SFX.src + ..._SignsSfx.d. Blok META (skladnia zParserExtendera,
   gdc jej nie zna) zamieniany na puste linie, wiec numery linii bledow sie zgadzaja. Wymagane: zero
   bledow i zero ostrzezen w naszych plikach (ostrzezenia vanilli sa pomijane). Kopie drzew vanilli
   ida do katalogu tymczasowego (gdc nie przyjmuje w .src sciezek z '..' ani bezwzglednych).
3. Symbole w skompilowanych .DAT (dat_symbols.py): stale SPL_Aard..SPL_Quen = 90..94, czary, funkcje
   Spell_Cast_*, runy ItRu_*, efekty SPELLFX_* z projektu (z runda 3).
4. Odwolania, ktorych kompilator nie widzi (to tylko napisy): visName -> PFX, emFXCreate_S / emCreateFXID
   -> VFX, sfxID -> SFX, lightPresetName -> LIGHTPRESETS.ZEN, klucze _KEY_* -> efekt bazowy, effect runy
   -> VFX, tekstury -> Textures*.vdf, pliki .wav -> Sounds*.vdf, modele run -> Meshes*.vdf,
   wartosci tekstowe PFX (shpType, FOR, dirMode, orientacja, alfa), brak kolizji i obrazen w VFX.

Uzycie: py -3.12 -I check_signs_scripts.py [--keep] [--autorun KATALOG]
  --keep     zostawia katalog tymczasowy z .src, logami gdc i .DAT
  --autorun  sprawdza pliki z innego katalogu (domyslnie data\System\Autorun)
Kod wyjscia 0 = wszystko w porzadku.
"""
import os, re, sys, shutil, subprocess, tempfile, struct
import zenkit

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))           # plugins\zDismember
AUTORUN = os.path.join(ROOT, "data", "System", "Autorun")
TOOLS = os.environ.get("GOTHIC_TOOLS_DIR", "")   # gdc (kompilator Daedalusa) + skrypty wanilii - spoza repozytorium
GDC = os.path.join(TOOLS, "bin", "gdc.cmd")
VANILLA = os.path.join(TOOLS, "gothic-2-addon-scripts", "_work", "Data", "Scripts")
GAME = os.environ.get("GOTHIC2_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Gothic II")
GAME_DATA = os.path.join(GAME, "Data")
LIGHT_PRESETS = os.path.join(GAME, "_work", "Data", "Presets", "LIGHTPRESETS.ZEN")

sys.path.insert(0, TOOLS)
import dat_symbols  # noqa: E402

# plik -> (Parser w META, .src vanilli wzgledem Scripts, nazwa DAT)
FILES = {
    "zDismember_Signs.d":    ("GAME", r"Content\Gothic.src", "GOTHIC.DAT"),
    "zDismember_SignsVfx.d": ("VFX", r"System\VisualFX.src", "VISUALFX.DAT"),
    "zDismember_SignsPfx.d": ("PFX", r"System\ParticleFX.src", "PARTICLEFX.DAT"),
    "zDismember_SignsSfx.d": ("SFX", r"System\SFX.src", "SFX.DAT"),
}
SIGNS = ["AARD", "IGNI", "AXII", "YRDEN", "QUEN"]
SPEC_VFX = ["SPELLFX_AARD", "SPELLFX_AARD_WAVE", "SPELLFX_AARD_HIT",
            "SPELLFX_IGNI", "SPELLFX_IGNI_CONE", "SPELLFX_IGNI_HIT",
            "SPELLFX_AXII", "SPELLFX_AXII_TARGET", "SPELLFX_AXII_FORGET",
            "SPELLFX_YRDEN", "SPELLFX_YRDEN_ZONE", "SPELLFX_YRDEN_SLOW",
            "SPELLFX_QUEN", "SPELLFX_QUEN_SHIELD", "SPELLFX_QUEN_BREAK",
            # runda 3: tryby alternatywne (+ kula-ladunek pulapki Yrdena, gaszona osobno)
            "SPELLFX_AARD_SWEEP", "SPELLFX_IGNI_STREAM", "SPELLFX_YRDEN_TRAP", "SPELLFX_YRDEN_TRAP_ORB",
            "SPELLFX_YRDEN_BOLT", "SPELLFX_QUEN_ACTIVE", "SPELLFX_QUEN_ABSORB", "SPELLFX_AXII_PUPPET"]
LOOPS = ["SPELLFX_AXII_TARGET", "SPELLFX_YRDEN_ZONE", "SPELLFX_YRDEN_SLOW", "SPELLFX_QUEN_SHIELD",
         "SPELLFX_IGNI_STREAM", "SPELLFX_YRDEN_TRAP", "SPELLFX_YRDEN_TRAP_ORB", "SPELLFX_QUEN_ACTIVE",
         "SPELLFX_AXII_PUPPET"]
# bezpieczniki dlugich petli rundy 3 (koordynator): korzen ma dokladnie tyle sekund
LOOP_SAFETY = {"SPELLFX_IGNI_STREAM": 20, "SPELLFX_QUEN_ACTIVE": 30, "SPELLFX_YRDEN_TRAP": 60,
               "SPELLFX_YRDEN_TRAP_ORB": 60, "SPELLFX_AXII_PUPPET": 30}
# runda 4: Axii zielony i bez petli dzwieku
AXII_BANNED_TEX = {"DEMONSIGN.TGA"}
AXII_BANNED_LIGHT = {"POISON", "AURA"}
AXII_MAX_SFX_SEC = 3.0
LOOP_SFX_ALLOWED = {"SPELLFX_IGNI_STREAM", "SPELLFX_QUEN_ACTIVE"}   # podtrzymywane tryby z szumem w petli
# runda 5: dzwieki rzutu i dzwiek przy kazdym spowolnionym wrogu
CAST_MAX_SFX_SEC = 3.0
SLOW_MAX_SFX_SEC = 1.5
SLOW_MAX_VOL = 60
# jedyny efekt "od-do": tor TARGET, ale bez kolizji i z HALT na koncu toru
TARGET_TRJ_PREFIX = "SPELLFX_YRDEN_BOLT"
KEY_RE = re.compile(r"^(.*)_KEY_(OPEN|INIT|CAST|STOP|COLLIDE|INVEST_\d+)$")
# efekty silnika, ktorych nazwa visName nie jest instancja PFX
SPECIAL_VIS = ("SCREENBLEND.SCX", "MORPH.FOV", "TIME.SLW", "EARTHQUAKE.EQK")

errors, notes = [], []


def err(msg):
    errors.append(msg)
    print("  BLAD:", msg)


def ok(msg, since=None):
    """Wypisuje 'ok' tylko wtedy, gdy od chwili since (liczba bledow) nie doszedl zaden blad."""
    if since is not None and len(errors) > since:
        return
    print("  ok  ", msg)


# ---------------------------------------------------------------- parsowanie Daedalusa (wystarczajace do odwolan)

def strip_comments(text):
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = text.find('"', i + 1)
            j = n - 1 if j < 0 else j
            out.append(text[i:j + 1]); i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            seg = text[i:(n if j < 0 else j + 2)]
            out.append("\n" * seg.count("\n")); i = n if j < 0 else j + 2
        else:
            out.append(c); i += 1
    return "".join(out)


INST_RE = re.compile(r"\b(instance|prototype)\s+([\w,\s]+?)\s*\(\s*(\w+)\s*\)\s*(\{|;)", re.I)
ASSIGN_STR = re.compile(r"(\w+)\s*(?:\[\s*\w+\s*\])?\s*=\s*\"([^\"]*)\"")
ASSIGN_NUM = re.compile(r"(\w+)\s*(?:\[\s*\w+\s*\])?\s*=\s*(-?[\d.]+(?:e-?\d+)?)\s*;", re.I)


def parse_instances(text):
    """{NAZWA: (klasa_lub_proto, {pole: wartosc})} - pola malymi literami, nazwy wielkimi."""
    text = strip_comments(text)
    res = {}
    for m in INST_RE.finditer(text):
        kind, names, parent, tail = m.groups()
        body = ""
        if tail == "{":
            depth, j = 1, m.end()
            while j < len(text) and depth:
                depth += {"{": 1, "}": -1}.get(text[j], 0)
                j += 1
            body = text[m.end():j - 1]
        fields = {}
        for a in ASSIGN_STR.finditer(body):
            fields[a.group(1).lower()] = a.group(2)
        for a in ASSIGN_NUM.finditer(body):
            fields.setdefault(a.group(1).lower(), float(a.group(2)))
        for nm in names.split(","):
            res[nm.strip().upper()] = (parent.upper(), fields, kind.lower())
    return res


def resolve(insts, name):
    """Pola instancji z dziedziczeniem po prototypach z tego samego zbioru."""
    cls, fields, _ = insts[name]
    if cls in insts and insts[cls][2] == "prototype":
        merged = dict(resolve(insts, cls))
        merged.update(fields)
        return merged
    return dict(fields)


def read_vanilla(rel_glob_list):
    text = ""
    for rel in rel_glob_list:
        p = os.path.join(VANILLA, rel)
        text += open(p, "rb").read().decode("cp1250", errors="replace") + "\n"
    return text


# ---------------------------------------------------------------- 1. pliki i META

META_RE = re.compile(r"\A\s*META\s*\{(.*?)\}\s*;?", re.S)


def check_file(fname, parser):
    e0 = len(errors)
    path = os.path.join(AUTORUN, fname)
    raw = open(path, "rb").read()
    if raw.startswith(b"\xef\xbb\xbf"):
        err(f"{fname}: BOM na poczatku (ma byc UTF-8 bez BOM)")
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as e:
        err(f"{fname}: nie jest UTF-8 ({e})")
        text = raw.decode("utf-8", errors="replace")
    for ln, line in enumerate(text.splitlines(), 1):
        try:
            line.encode("cp1250")
        except UnicodeEncodeError:
            err(f"{fname}:{ln}: znak spoza cp1250: {line.strip()!r}")
    m = META_RE.match(text)
    if not m:
        err(f"{fname}: brak bloku META na poczatku pliku")
        return text, text
    meta = dict((k.strip().upper(), v.strip()) for k, v in re.findall(r"(\w+)\s*=\s*([^;]+);", m.group(1)))
    if meta.get("PARSER", "").upper() != parser:
        err(f"{fname}: META Parser = {meta.get('PARSER')} (oczekiwane {parser})")
    if meta.get("ENGINE", "").upper() != "G2A":
        err(f"{fname}: META Engine = {meta.get('ENGINE')} (oczekiwane G2A)")
    if meta.get("MERGEMODE", "").lower() not in ("1", "true"):
        err(f"{fname}: META MergeMode = {meta.get('MERGEMODE')} (oczekiwane 1)")
    ok(f"{fname}: UTF-8 bez BOM, META Parser={meta.get('PARSER')} Engine={meta.get('ENGINE')} "
       f"MergeMode={meta.get('MERGEMODE')}", e0)
    stripped = "\n" * m.group(0).count("\n") + text[m.end():]
    return text, stripped


# ---------------------------------------------------------------- 2. kompilacja gdc

def compile_all(stripped, work):
    for sub in ("Content", "System"):
        shutil.copytree(os.path.join(VANILLA, sub), os.path.join(work, sub))
    os.makedirs(os.path.join(work, "signs"))
    dats = {}
    for fname, (parser, src_rel, dat) in FILES.items():
        open(os.path.join(work, "signs", fname), "wb").write(stripped[fname].encode("cp1250"))
        src = os.path.join(work, f"check_{parser.lower()}.src")
        open(src, "w", newline="\r\n").write(src_rel + "\n" + "signs\\" + fname + "\n")
        out_dat = os.path.join(work, "out", dat)
        os.makedirs(os.path.dirname(out_dat), exist_ok=True)
        r = subprocess.run(["cmd", "/c", GDC, src, "-o", out_dat], cwd=work, capture_output=True,
                           text=True, encoding="utf-8", errors="replace")
        log = r.stdout + r.stderr
        open(os.path.join(work, f"check_{parser.lower()}.log"), "w", encoding="utf-8").write(log)
        success = "Compilation completed successfully" in log and r.returncode == 0
        mine = [l for l in log.splitlines() if fname.lower() in l.lower() and
                re.search(r"\b(error|warning)\b", l, re.I)]
        n_err = len([l for l in log.splitlines() if re.search(r"\berror\b", l, re.I)])
        m_warn = re.search(r"(\d+) warnings? generated", log)
        if not success:
            tail = "\n".join(log.splitlines()[-25:])
            err(f"gdc {src_rel} + {fname}: KOMPILACJA NIEUDANA\n{tail}")
        elif mine:
            err(f"gdc {src_rel} + {fname}: komunikaty w naszym pliku:\n" + "\n".join(mine))
        else:
            ok(f"gdc {src_rel} + {fname}: skompilowane, bledow {n_err}, ostrzezen w naszym pliku 0 "
               f"(ostrzezenia vanilli: {m_warn.group(1) if m_warn else 0})")
        dats[parser] = out_dat if success else None
    return dats


def check_dat_symbols(dats, game_insts):
    e0 = len(errors)
    syms = {}
    for parser, path in dats.items():
        if path and os.path.exists(path):
            _, symbols, _ = dat_symbols.parse(path)
            syms[parser] = {s["name"].upper(): s for s in symbols}
    g = syms.get("GAME", {})
    for i, s in enumerate(SIGNS):
        nm = "SPL_" + s
        sym = g.get(nm)
        if not sym or sym.get("values") != [90 + i]:
            err(f"GOTHIC.DAT: {nm} = {sym.get('values') if sym else 'brak'} (oczekiwane {90 + i})")
        for need, typ in (("SPELL_" + s, dat_symbols.INST), ("SPELL_CAST_" + s, dat_symbols.FUNC),
                          ("ITRU_" + s, dat_symbols.INST)):
            if need not in g or g[need]["type"] != typ:
                err(f"GOTHIC.DAT: brak {need} ({dat_symbols.TYPE_NAME[typ]})")
    v = syms.get("VFX", {})
    for nm in SPEC_VFX:
        if nm not in v or v[nm]["type"] != dat_symbols.INST:
            err(f"VISUALFX.DAT: brak instancji {nm}")
    if g and v:
        ok(f"symbole w .DAT: SPL_Aard..SPL_Quen = 90..94, Spell_*, Spell_Cast_*, ItRu_*, "
           f"{len(SPEC_VFX)} x SPELLFX_* z projektu (z runda 3)", e0)


# ---------------------------------------------------------------- 4. zasoby gry

def vfs_names(archives):
    v = zenkit.Vfs()
    names = set()
    for a in archives:
        p = os.path.join(GAME_DATA, a)
        if os.path.exists(p):
            v.mount_disk(p)
    stack = [v.root]
    while stack:
        n = stack.pop()
        for c in n.children:
            if c.is_dir():
                stack.append(c)
            else:
                names.add(c.name.upper())
    return names


def wav_seconds(names):
    """{NAZWA.WAV: sekundy} z Sounds*.vdf (naglowek fmt: byteRate, dlugosc chunku data)."""
    v = zenkit.Vfs()
    for a in ("Sounds.vdf", "Sounds_Addon.vdf"):
        p = os.path.join(GAME_DATA, a)
        if os.path.exists(p):
            v.mount_disk(p)
    res = {}
    for n in names:
        node = v.find(n.upper())
        if node is None:
            continue
        d = bytes(node.data)
        pos, rate, size = 12, 0, 0
        while pos + 8 <= len(d):
            cid, sz = d[pos:pos + 4], struct.unpack_from("<I", d, pos + 4)[0]
            if cid == b"fmt ":
                rate = struct.unpack_from("<I", d, pos + 16)[0]
            elif cid == b"data":
                size = sz
            pos += 8 + sz + (sz & 1)
        res[n.upper()] = size / rate if rate else 0.0
    return res


def light_presets():
    t = open(LIGHT_PRESETS, "rb").read().decode("latin1")
    return set(x.strip().upper() for x in re.findall(r"presetName=string:([^\r\n]*)", t))


def tex_exists(texs, name):
    base = os.path.splitext(name.upper())[0]
    return (base + "-C.TEX") in texs


ENUMS = {
    "shptype_s": {"POINT", "LINE", "BOX", "CIRCLE", "SPHERE", "MESH"},
    "shpfor_s": {"OBJECT", "WORLD"},
    "dirfor_s": {"OBJECT", "WORLD"},
    "dirmode_s": {"DIR", "TARGET", "MESH", "RAND", "NONE"},
    "shpdistribtype_s": {"RAND", "RANDOM", "UNIFORM", "WALK"},
    "visorientation_s": {"NONE", "VELO", "VELO3D", "VOB"},
    "visalphafunc_s": {"ADD", "BLEND", "MUL"},
}


def check_refs(texts):
    my = {k: parse_instances(texts[f]) for k, f in
          (("GAME", "zDismember_Signs.d"), ("VFX", "zDismember_SignsVfx.d"),
           ("PFX", "zDismember_SignsPfx.d"), ("SFX", "zDismember_SignsSfx.d"))}
    van_vfx = parse_instances(read_vanilla([r"System\VisualFX\VisualFxInst.d"]))
    van_pfx = parse_instances(read_vanilla([r"System\PFX\PfxInstEngine.d", r"System\PFX\PfxInst.d",
                                            r"System\PFX\PfxInstMagic.d"]))
    van_sfx = parse_instances(read_vanilla([r"System\SFX\SfxInst.d", r"System\SFX\SfxInstSpeech.d"]))
    all_vfx = set(van_vfx) | set(my["VFX"])
    all_pfx = set(van_pfx) | set(my["PFX"])
    all_sfx = set(van_sfx) | set(my["SFX"])

    # nazwy nie moga nadpisac vanilli (MergeMode podmienilby instancje gry)
    for k, van in (("VFX", van_vfx), ("PFX", van_pfx), ("SFX", van_sfx)):
        clash = sorted(set(my[k]) & set(van))
        if clash:
            err(f"{k}: nasze nazwy nadpisuja instancje gry: {clash}")

    print("  ... montuje archiwa gry (tekstury, dzwieki, modele)")
    texs = vfs_names(["Textures.vdf", "Textures_Addon.vdf"])
    wavs = vfs_names(["Sounds.vdf", "Sounds_Addon.vdf"])
    meshes = vfs_names(["Meshes.vdf", "Meshes_Addon.vdf"])
    presets = light_presets()

    # --- VFX
    e0 = len(errors)
    n_ref = 0
    for name, (cls, f, kind) in my["VFX"].items():
        km = KEY_RE.match(name)
        if cls == "C_PARTICLEFXEMITKEY":
            if not km or km.group(1) not in all_vfx:
                err(f"VFX {name}: klucz bez efektu bazowego")
        elif km:
            err(f"VFX {name}: nazwa jak klucz, a klasa {cls}")
        vis = f.get("visname_s", "")
        if vis and vis.upper() not in SPECIAL_VIS and vis.upper() not in all_pfx:
            err(f"VFX {name}: visName_S {vis!r} - brak takiego PFX"); n_ref += 1
        for fld in ("emfxcreate_s", "emcreatefxid", "emfxinvestorigin_s", "emfxinvesttarget_s",
                    "emfxcollstat_s", "emfxcolldyn_s", "emfxcolldynperc_s"):
            val = f.get(fld, "")
            if val and val.upper() not in all_vfx:
                err(f"VFX {name}: {fld} {val!r} - brak takiego VFX")
        sfx = f.get("sfxid", "")
        if sfx and sfx.upper() not in all_sfx:
            err(f"VFX {name}: sfxID {sfx!r} - brak takiego SFX")
        lp = f.get("lightpresetname", "")
        if lp and lp.upper() not in presets:
            err(f"VFX {name}: lightPresetName {lp!r} - brak w LIGHTPRESETS.ZEN ({sorted(presets)})")
        # brak kolizji i obrazen
        if f.get("emcheckcollision", 0) != 0:
            err(f"VFX {name}: emCheckCollision != 0 (efekt nie moze kolidowac)")
        if f.get("sendassessmagic", 0) != 0:
            err(f"VFX {name}: sendAssessMagic != 0")
        for fld in ("emactioncolldyn_s", "emactioncollstat_s", "emfxcolldyn_s", "emfxcollstat_s",
                    "emfxcolldynperc_s"):
            if f.get(fld, ""):
                err(f"VFX {name}: {fld} ustawione - efekt nie moze kolidowac")
        mode = f.get("emtrjmode_s", "").upper()
        if mode == "TARGET" and name.startswith(TARGET_TRJ_PREFIX):
            if f.get("emtrjloopmode_s", "").upper() != "HALT":
                err(f"VFX {name}: tor TARGET bez emTrjLoopMode_S HALT (efekt polecialby dalej)")
            if f.get("emfxlifespan", -1) <= 0:
                err(f"VFX {name}: tor TARGET bez emFXLifeSpan")
        elif mode and mode not in ("FIXED", "NONE"):
            err(f"VFX {name}: emTrjMode_S {mode!r} - tylko FIXED/NONE (TARGET tylko {TARGET_TRJ_PREFIX}*)")
    # cykle w emFXCreate_S
    for name in my["VFX"]:
        seen, cur = set(), name
        while cur and cur in my["VFX"]:
            if cur in seen:
                err(f"VFX {name}: cykl emFXCreate_S"); break
            seen.add(cur)
            cur = my["VFX"][cur][1].get("emfxcreate_s", "").upper()
    # petle: kazda czesc ma bezpiecznik emFXLifeSpan > 0
    for root in LOOPS:
        cur = root
        while cur and cur in my["VFX"]:
            ls = my["VFX"][cur][1].get("emfxlifespan", -1)
            if ls <= 0:
                err(f"VFX {cur} (petla {root}): brak emFXLifeSpan > 0 jako bezpiecznika")
            cur = my["VFX"][cur][1].get("emfxcreate_s", "").upper()
    for root, sec in LOOP_SAFETY.items():
        if root in my["VFX"] and my["VFX"][root][1].get("emfxlifespan", -1) != sec:
            err(f"VFX {root}: emFXLifeSpan {my['VFX'][root][1].get('emfxlifespan')} (bezpiecznik ma byc {sec} s)")
    ok(f"VFX: {len(my['VFX'])} instancji, odwolania do PFX/VFX/SFX/presetow swiatla sprawdzone, "
       f"bez kolizji i obrazen, petle z bezpiecznikiem czasu", e0)

    # --- PFX
    e0 = len(errors)
    for name, (cls, _, kind) in my["PFX"].items():
        if kind == "prototype":
            continue
        f = resolve(my["PFX"], name)
        vis = f.get("visname_s", "")
        if not vis:
            err(f"PFX {name}: brak visName_S")
        elif vis.upper().endswith(".TGA") and not tex_exists(texs, vis):
            err(f"PFX {name}: tekstura {vis!r} - brak w Textures*.vdf")
        for fld, allowed in ENUMS.items():
            val = f.get(fld)
            if val is not None and val.upper() not in allowed:
                err(f"PFX {name}: {fld} = {val!r} (dozwolone {sorted(allowed)})")
        ce = f.get("ppscreateem_s", "")
        if ce and ce.upper() not in all_pfx:
            err(f"PFX {name}: ppsCreateEm_S {ce!r} - brak takiego PFX")
        if f.get("m_bisambientpfx", 0):
            err(f"PFX {name}: m_bIsAmbientPFX = 1 (gra moze go wylaczyc)")
        if "vissizeendscale" not in f:
            err(f"PFX {name}: brak visSizeEndScale (domyslne 0 = czastka znika od razu)")
    ok(f"PFX: {len([1 for v in my['PFX'].values() if v[2] != 'prototype'])} instancji, tekstury istnieja, "
       f"wartosci tekstowe poprawne", e0)

    # --- SFX
    e0 = len(errors)
    for name, (cls, f, kind) in my["SFX"].items():
        wav = f.get("file", "")
        if not wav or wav.upper() not in wavs:
            err(f"SFX {name}: plik {wav!r} - brak w Sounds*.vdf")
    ok(f"SFX: {len(my['SFX'])} instancji, pliki .wav istnieja", e0)

    # --- runda 4: Axii zielony (bez fioletu), dzwieki bez petli
    e0 = len(errors)
    def rgb(s):
        try:
            return [int(x) for x in s.split()]
        except ValueError:
            return None
    for name, (cls, _, kind) in my["PFX"].items():
        if kind == "prototype" or not (name.startswith("ZDF_SIGN_AXII") or name == "ZDF_SIGN_RUNE_AXII"):
            continue
        f = resolve(my["PFX"], name)
        if f.get("visname_s", "").upper() in AXII_BANNED_TEX:
            err(f"PFX {name}: tekstura {f['visname_s']} - purpurowa, po zabarwieniu na zielono wychodzi fiolet")
        for fld in ("vistexcolorstart_s", "vistexcolorend_s"):
            c = rgb(f.get(fld, ""))
            if c and len(c) == 3 and sum(c) > 0 and not (c[1] >= c[0] and c[1] >= c[2]):
                err(f"PFX {name}: {fld} = {f[fld]!r} - Axii ma byc zielony (G najwieksze), bez fioletu")
    for name, (cls, f, kind) in my["VFX"].items():
        if name.startswith("SPELLFX_AXII") and f.get("lightpresetname", "").upper() in AXII_BANNED_LIGHT:
            err(f"VFX {name}: swiatlo {f['lightpresetname']} - nie dla Axii (fiolet/niebieski Yrdena)")
    looping_sfx = {n for n, (c, f, k) in my["SFX"].items() if f.get("loop", 0)}
    for name, (cls, f, kind) in my["VFX"].items():
        sfx = f.get("sfxid", "").upper()
        if sfx in looping_sfx and name not in LOOP_SFX_ALLOWED:
            err(f"VFX {name}: dzwiek w petli {sfx} - dozwolony tylko w {sorted(LOOP_SFX_ALLOWED)}")
        if name.startswith("SPELLFX_AXII") and sfx in looping_sfx:
            err(f"VFX {name}: Axii z dzwiekiem w petli {sfx}")
    axii_sfx = {n: f for n, (c, f, k) in my["SFX"].items() if n.startswith("ZDF_SIGN_AXII")}
    secs = wav_seconds([f.get("file", "") for f in axii_sfx.values()])
    for n, f in axii_sfx.items():
        if f.get("loop", 0):
            err(f"SFX {n}: loop = 1 (Axii ma byc jednorazowy)")
        s = secs.get(f.get("file", "").upper(), 0)
        if s > AXII_MAX_SFX_SEC:
            err(f"SFX {n}: {f.get('file')} trwa {s:.1f} s (Axii: najwyzej {AXII_MAX_SFX_SEC} s)")
    # runda 5: dzwiek rzutu kazdego znaku w SPELLFX_<ZNAK>_FLASH (petla iskierek zyjaca dluzej niz dzwiek),
    # nie w kluczu dloni (efekt dloni znika ok. 0,8 s po rzucie i silnik ucina wtedy jego dzwiek)
    cast_sfx = {n: f for n, (c, f, k) in my["SFX"].items() if n.endswith("_CAST")}
    cast_secs = wav_seconds([f.get("file", "") for f in cast_sfx.values()])
    for s in SIGNS:
        key = my["VFX"].get(f"SPELLFX_{s}_KEY_CAST")
        if key and key[1].get("sfxid"):
            err(f"VFX SPELLFX_{s}_KEY_CAST: dzwiek w kluczu dloni (ucina go koniec efektu dloni) - ma byc w SPELLFX_{s}_FLASH")
        fl = my["VFX"].get(f"SPELLFX_{s}_FLASH")
        if not fl:
            err(f"VFX: brak SPELLFX_{s}_FLASH"); continue
        f = fl[1]
        snd = f.get("sfxid", "").upper()
        if snd != f"ZDF_SIGN_{s}_CAST":
            err(f"VFX SPELLFX_{s}_FLASH: sfxID {snd!r} (ma byc ZDF_SIGN_{s}_CAST)"); continue
        vis = f.get("visname_s", "").upper()
        if vis not in my["PFX"] or not resolve(my["PFX"], vis).get("ppsislooping", 0):
            err(f"VFX SPELLFX_{s}_FLASH: visName {vis} nie jest petla - efekt zgasnie przed koncem dzwieku")
        sf = cast_sfx.get(snd, {})
        sec = cast_secs.get(sf.get("file", "").upper(), 0)
        if sf.get("loop", 0):
            err(f"SFX {snd}: loop = 1 (dzwiek rzutu ma byc jednorazowy)")
        if sec > CAST_MAX_SFX_SEC:
            err(f"SFX {snd}: {sf.get('file')} trwa {sec:.1f} s (rzut: najwyzej {CAST_MAX_SFX_SEC} s)")
        if f.get("emfxlifespan", -1) < sec + 0.1:
            err(f"VFX SPELLFX_{s}_FLASH: emFXLifeSpan {f.get('emfxlifespan')} krotszy niz dzwiek {sec:.1f} s")
    # efekt przy kazdym spowolnionym wrogu: krotki, cichy, jednorazowy dzwiek (albo zaden)
    slow = my["VFX"].get("SPELLFX_YRDEN_SLOW")
    if slow and slow[1].get("sfxid"):
        sf = my["SFX"].get(slow[1]["sfxid"].upper(), (None, {}, None))[1]
        sec = wav_seconds([sf.get("file", "")]).get(sf.get("file", "").upper(), 0)
        if sf.get("loop", 0) or sec > SLOW_MAX_SFX_SEC or sf.get("vol", 127) > SLOW_MAX_VOL:
            err(f"SFX {slow[1]['sfxid']}: przy wrogu w kregu ma byc jednorazowy, do {SLOW_MAX_SFX_SEC} s i vol "
                f"do {SLOW_MAX_VOL} (jest {sec:.1f} s, vol {sf.get('vol')}, loop {sf.get('loop', 0)})")
    ok("Axii: zielone czastki bez fioletu i purpurowych tekstur, swiatlo bez POISON/AURA, dzwieki jednorazowe "
       f"(najdluzszy {max(secs.values()) if secs else 0:.1f} s), petle dzwieku tylko w {sorted(LOOP_SFX_ALLOWED)}", e0)
    ok("dzwieki rzutu 5 znakow w SPELLFX_<ZNAK>_FLASH (petla iskierek dluzsza niz dzwiek, bez dzwieku w kluczu dloni, "
       f"najdluzszy {max(cast_secs.values()) if cast_secs else 0:.1f} s); YRDEN_SLOW krotki i cichy", e0)

    # --- GAME: runy
    e0 = len(errors)
    for s in SIGNS:
        nm = "ITRU_" + s
        if nm not in my["GAME"]:
            err(f"Game: brak {nm}"); continue
        f = my["GAME"][nm][1]
        vis = f.get("visual", "")
        mrm = os.path.splitext(vis.upper())[0] + ".MRM"
        if mrm not in meshes:
            err(f"{nm}: model {vis!r} ({mrm}) - brak w Meshes*.vdf")
        eff = f.get("effect", "")
        if eff.upper() not in all_vfx:
            err(f"{nm}: effect {eff!r} - brak takiego VFX")
        if "mag_circle" in f:
            err(f"{nm}: ma mag_circle (runa ma dzialac bez kregu)")
    visuals = [my["GAME"]["ITRU_" + s][1].get("visual", "").upper() for s in SIGNS if "ITRU_" + s in my["GAME"]]
    if len(set(visuals)) != len(visuals):
        err(f"runy: modele sie powtarzaja {visuals}")
    ok(f"runy: modele istnieja i sa rozne ({', '.join(visuals)}), poswiaty WEAR_EFFECT istnieja, bez mag_circle", e0)


def main():
    global AUTORUN
    keep = "--keep" in sys.argv
    if "--autorun" in sys.argv:          # inny katalog z plikami (np. test z celowo wstawionymi bledami)
        AUTORUN = sys.argv[sys.argv.index("--autorun") + 1]
    print("Znaki Wiedzmina - sprawdzenie skryptow\n")
    print("[1] pliki i META")
    texts, stripped = {}, {}
    for fname, (parser, _, _) in FILES.items():
        texts[fname], stripped[fname] = check_file(fname, parser)

    print("\n[2] kompilacja gdc ze zrodlami G2 Addon")
    work = tempfile.mkdtemp(prefix="zdf_signs_")
    try:
        dats = compile_all(stripped, work)
        print("\n[3] symbole w .DAT")
        check_dat_symbols(dats, None)
    finally:
        if keep:
            print("  katalog roboczy:", work)
        else:
            shutil.rmtree(work, ignore_errors=True)

    print("\n[4] odwolania i zasoby gry")
    check_refs(texts)

    print()
    if errors:
        print(f"WYNIK: {len(errors)} bledow")
        return 1
    print("WYNIK: OK - zero bledow")
    return 0


if __name__ == "__main__":
    sys.exit(main())
