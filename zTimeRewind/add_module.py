# Dopisuje modul pluginu do zTimeRewind.vcxproj oraz Headers.h/Sources.h (przed Plugin.*).
# Uzycie: py -3.12 -I add_module.py Name        -> Name.h + Name.cpp (kod silnika, ClInclude)
#         py -3.12 -I add_module.py --core Name -> Core\Name.cpp jako ClCompile (logika bez silnika)
#         py -3.12 -I add_module.py --remove Name -> usuwa wpisy modulu
import pathlib
import sys

BOM = b"\xef\xbb\xbf"
root = pathlib.Path(__file__).resolve().parent / "zTimeRewind"


def read(path):
    raw = path.read_bytes()
    bom = raw.startswith(BOM)
    return raw[3:].decode("utf-8") if bom else raw.decode("utf-8"), bom


def write(path, text, bom):
    data = text.encode("utf-8")
    path.write_bytes(BOM + data if bom else data)


args = sys.argv[1:]
mode = args[0] if args and args[0].startswith("--") else ""
name = args[-1]
proj = root / "zTimeRewind.vcxproj"
s, sbom = read(proj)

if mode == "--core":
    line = '    <ClCompile Include="Core\\%s.cpp" />' % name
    if line not in s:
        anchor = '    <ClCompile Include="Interface.cpp" />'
        s = s.replace(anchor, anchor + "\n" + line, 1)
else:
    entries = '    <ClInclude Include="%s.cpp" />\n    <ClInclude Include="%s.h" />\n' % (name, name)
    if mode == "--remove":
        s = s.replace(entries, "")
    elif ('Include="%s.cpp"' % name) not in s:
        anchor = '    <ClInclude Include="Plugin.cpp" />'
        s = s.replace(anchor, entries + anchor, 1)
    for fname, inc, anchor in (("Headers.h", '#include "%s.h"' % name, '#include "Plugin.h"'),
                               ("Sources.h", '#include "%s.cpp"' % name, '#include "Plugin.cpp"')):
        f = root / fname
        t, tbom = read(f)
        if mode == "--remove":
            t = t.replace(inc + "\n", "")
        elif inc not in t:
            t = t.replace(anchor, inc + "\n" + anchor, 1)
        write(f, t, tbom)

write(proj, s, sbom)
print("OK", mode or "add", name)
