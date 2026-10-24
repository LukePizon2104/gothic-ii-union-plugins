r"""Recreates the ZenGin headers of zDismember / zTimeRewind from the Union SDK v1.0m project template.

The ZenGin folder (Gothic engine API headers, (c) Union team) is not part of this repository. It ships with the
Union SDK as the Visual Studio project template UnionPlugin_1_0m.zip. This script copies ZENGIN\ from that
template into <plugin>\<plugin>\ZenGin and applies the plugin's small patch (<plugin>\sdk\ZenGin.diff):
MSVC 2022 / C++latest fixes and the plugin's own declarations in Gothic_UserAPI\*.inl.

Usage:
  py -3 tools\setup_zengin.py [--template PATH] [--plugin zDismember|zTimeRewind|all] [--force]
  py -3 tools\setup_zengin.py make --plugin NAME --from DIR [--template PATH]   (maintainers: rebuild the .diff)

--template: UnionPlugin_1_0m.zip or an extracted template folder (with ZENGIN\ inside). Default: the
UNION_PLUGIN_TEMPLATE environment variable, else
%USERPROFILE%\Documents\Visual Studio 2022\Templates\ProjectTemplates\UnionPlugin_1_0m.zip
(where the Union SDK installer puts it).
"""
import argparse
import difflib
import io
import os
import shutil
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLUGINS = ("zDismember", "zTimeRewind")
TOP_DIRS = {  # template (upper case) -> folder names used by the projects
    "GOTHIC_II_ADDON": "Gothic_II_Addon",
    "GOTHIC_II_CLASSIC": "Gothic_II_Classic",
    "GOTHIC_I_ADDON": "Gothic_I_Addon",
    "GOTHIC_I_CLASSIC": "Gothic_I_Classic",
    "GOTHIC_USERAPI": "Gothic_UserAPI",
}
NO_EOL = b"\\ No newline at end of file\n"


def default_template():
    env = os.environ.get("UNION_PLUGIN_TEMPLATE")
    if env:
        return env
    rel = os.path.join("Visual Studio 2022", "Templates", "ProjectTemplates", "UnionPlugin_1_0m.zip")
    for docs in (os.path.join(os.path.expanduser("~"), "Documents"),
                 os.path.join(os.environ.get("OneDrive", ""), "Documents")):
        p = os.path.join(docs, rel)
        if os.path.exists(p):
            return p
    return os.path.join(os.path.expanduser("~"), "Documents", rel)


def norm_rel(rel):
    """Template path below ZENGIN/ -> project path (top folder renamed)."""
    parts = rel.replace("\\", "/").split("/")
    parts[0] = TOP_DIRS.get(parts[0].upper(), parts[0])
    return "/".join(parts)


def template_files(template):
    """{project-relative path: bytes} of ZENGIN/ in the template (zip or folder)."""
    out = {}
    if os.path.isdir(template):
        root = os.path.join(template, "ZENGIN")
        if not os.path.isdir(root):
            root = os.path.join(template, "ZenGin")
        for d, _, files in os.walk(root):
            for f in files:
                p = os.path.join(d, f)
                out[norm_rel(os.path.relpath(p, root))] = open(p, "rb").read()
    else:
        with zipfile.ZipFile(template) as z:
            for info in z.infolist():
                name = info.filename.replace("\\", "/")
                if info.is_dir() or not name.upper().startswith("ZENGIN/"):
                    continue
                out[norm_rel(name[len("ZENGIN/"):])] = z.read(info)
    if "zGothicAPI.h" not in out:
        raise SystemExit("%s: no ZENGIN/zGothicAPI.h - is this the Union SDK v1.0m plugin template?" % template)
    return out


def split_lines(data):
    return io.BytesIO(data).readlines()


def make_diff(old, new, path):
    """Unified diff of two byte strings (line endings kept, latin-1 text); b"" when equal."""
    a = [l.decode("latin-1") for l in split_lines(old)]
    b = [l.decode("latin-1") for l in split_lines(new)]
    out = []
    for line in difflib.unified_diff(a, b, "a/" + path, "b/" + path, n=3):
        raw = line.encode("latin-1")
        out.append(raw if raw.endswith(b"\n") else raw + b"\n" + NO_EOL)
    return b"".join(out)


def parse_diff(data):
    """-> {path: [(old_start, [(op, line_bytes)])]}."""
    files, cur, hunk = {}, None, None
    lines = split_lines(data)
    i = 0
    while i < len(lines):
        l = lines[i]
        if l.startswith(b"--- a/") and i + 1 < len(lines) and lines[i + 1].startswith(b"+++ b/"):
            cur = lines[i + 1][6:].rstrip(b"\r\n").decode("latin-1")
            files[cur] = []
            i += 2
            continue
        if l.startswith(b"@@ "):
            old = l.split(b" ")[1]            # -start,count
            start = int(old[1:].split(b",")[0])
            hunk = (start, [])
            files[cur].append(hunk)
        elif l == NO_EOL:
            op, prev = hunk[1][-1]
            hunk[1][-1] = (op, prev[:-1])      # drop the newline added by make_diff
        elif l[:1] in (b" ", b"-", b"+"):
            hunk[1].append((l[:1], l[1:]))
        i += 1
    return files


def apply_hunks(old, hunks, path):
    src = split_lines(old)
    out, pos = [], 0
    for start, body in hunks:
        want = [t for op, t in body if op in (b" ", b"-")]
        guess = max(start - 1, 0)
        at = None
        for delta in range(0, 200):
            for cand in (guess + delta, guess - delta):
                if pos <= cand <= len(src) - len(want) and src[cand:cand + len(want)] == want:
                    at = cand
                    break
            if at is not None:
                break
        if at is None:
            raise SystemExit("%s: patch hunk @%d does not match the template (different Union SDK version?)" % (path, start))
        out += src[pos:at]
        out += [t for op, t in body if op in (b" ", b"+")]
        pos = at + len(want)
    out += src[pos:]
    return b"".join(out)


def setup(plugin, tpl, force):
    target = os.path.join(REPO, plugin, plugin, "ZenGin")
    diff_path = os.path.join(REPO, plugin, "sdk", "ZenGin.diff")
    if os.path.exists(os.path.join(target, "zGothicAPI.h")) and not force:
        print("%s: %s already exists (use --force to recreate)" % (plugin, target))
        return
    if os.path.isdir(target):
        shutil.rmtree(target)
    patches = parse_diff(open(diff_path, "rb").read()) if os.path.exists(diff_path) else {}
    lower = {k.lower(): k for k in tpl}
    for path in patches:
        if path.lower() not in lower:
            raise SystemExit("%s: %s missing in the template" % (plugin, path))
    patched = {lower[p.lower()]: h for p, h in patches.items()}
    for rel, data in tpl.items():
        if rel in patched:
            data = apply_hunks(data, patched[rel], rel)
        dst = os.path.join(target, *rel.split("/"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as f:
            f.write(data)
    print("%s: %d files -> %s (%d patched)" % (plugin, len(tpl), target, len(patched)))


def keep_template_whitespace(old, new):
    """Lines of `new` that differ from the template only in whitespace (IDE re-formatting) are taken from the
    template, so the diff carries only real changes. Compiles the same; the self-check compares against this."""
    a, b = split_lines(old), split_lines(new)
    key = lambda l: b"".join(l.split())
    sm = difflib.SequenceMatcher(None, [key(l) for l in a], [key(l) for l in b], autojunk=False)
    out = []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        out += a[i1:i2] if tag == "equal" else b[j1:j2]
    return b"".join(out)


def make(plugin, tpl, from_dir):
    have = {}
    for d, _, files in os.walk(from_dir):
        for f in files:
            p = os.path.join(d, f)
            have[os.path.relpath(p, from_dir).replace("\\", "/").lower()] = open(p, "rb").read()
    extra = sorted(set(have) - {k.lower() for k in tpl})
    if extra:
        raise SystemExit("files not in the template (not supported by the diff): %s" % ", ".join(extra[:10]))
    chunks = []
    for rel in sorted(tpl, key=str.lower):
        new = have.get(rel.lower())
        if new is None:
            raise SystemExit("%s missing in %s" % (rel, from_dir))
        if new != tpl[rel]:
            new = keep_template_whitespace(tpl[rel], new)
            have[rel.lower()] = new
            if new != tpl[rel]:
                chunks.append(make_diff(tpl[rel], new, rel))
    out = os.path.join(REPO, plugin, "sdk", "ZenGin.diff")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(b"".join(chunks))
    # self-check: the diff recreates from_dir exactly
    patches = parse_diff(b"".join(chunks))
    lower = {k.lower(): k for k in tpl}
    for p, h in patches.items():
        rel = lower[p.lower()]
        if apply_hunks(tpl[rel], h, rel) != have[rel.lower()]:
            raise SystemExit("self-check failed for " + rel)
    print("%s: %d changed files -> %s (self-check OK, whitespace-only changes dropped)" % (plugin, len(chunks), out))


def main():
    argv = sys.argv[1:]
    mode = "make" if argv[:1] == ["make"] else "apply"
    if mode == "make":
        argv = argv[1:]
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--template", default=default_template())
    ap.add_argument("--plugin", default="all", choices=PLUGINS + ("all",))
    ap.add_argument("--force", action="store_true")
    ap.add_argument("--from", dest="from_dir")
    a = ap.parse_args(argv)
    if not os.path.exists(a.template):
        raise SystemExit("Union SDK plugin template not found: %s\nInstall the Union SDK v1.0m (see README.md) or pass "
                         "--template / set UNION_PLUGIN_TEMPLATE." % a.template)
    tpl = template_files(a.template)
    plugins = PLUGINS if a.plugin == "all" else (a.plugin,)
    for p in plugins:
        if mode == "make":
            if not a.from_dir or a.plugin == "all":
                raise SystemExit("make needs --plugin NAME and --from DIR")
            make(p, tpl, a.from_dir)
        else:
            setup(p, tpl, a.force)


if __name__ == "__main__":
    main()
