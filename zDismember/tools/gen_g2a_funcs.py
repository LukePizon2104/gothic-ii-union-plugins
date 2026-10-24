r"""Tabela funkcji Gothic2.exe (G2A 2.6) dla profilera probkujacego zDismember: adres -> krotka nazwa.

Zrodlo: Names_G2A.hpp z Union SDK (ZenGin\Gothic_II_Addon) - pelna lista symboli exe z adresami. Bierzemy tylko sekcje
.text (0x401000..0x82E000 w Gothic2.exe ze Steama), nazwe skracamy do "Klasa::Metoda" (bez dostepu, typu, konwencji
wywolania i parametrow). Wynik: data\System\zDismember\g2a_funcs.txt (build.cmd pakuje data\ do VDF), wczytywany
w Game_Init (Sampler.cpp). Format: "XXXXXXXX nazwa" na linie, posortowane po adresie, linie z '#' to komentarze.

Uzycie: py -3.12 -I tools\gen_g2a_funcs.py [--names Names_G2A.hpp] [--out plik]
"""
import argparse
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NAMES = os.path.join(ROOT, "zDismember", "ZenGin", "Gothic_II_Addon", "Names_G2A.hpp")
OUT = os.path.join(ROOT, "data", "System", "zDismember", "g2a_funcs.txt")
TEXT_LO, TEXT_HI = 0x00401000, 0x0082E000
CONVS = (" __thiscall ", " __cdecl ", " __stdcall ", " __fastcall ")


def cut_params(s):
    """Nazwa bez listy parametrow; operator() i operatory z '<' / '>' zostaja w calosci."""
    k = s.find("operator")
    if k >= 0:
        rest = s[k + 8:]
        if rest.startswith("()"):
            return s[:k + 10]
        p = rest.find("(")
        return s[:k + 8] + (rest[:p] if p >= 0 else rest)
    depth = 0
    quote = False
    for i, c in enumerate(s):
        if c == "`":
            quote = True
        elif c == "'" and quote:
            quote = False
        elif quote:
            continue
        elif c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == "(" and depth <= 0:
            return s[:i]
    return s


def short_name(full):
    s = full.strip()
    if s.startswith("[thunk]:"):
        s = s[8:].strip()
    for conv in CONVS:
        k = s.find(conv)
        if k >= 0:
            s = s[k + len(conv):]
            break
    s = cut_params(s).strip()
    s = s.replace(" :: ", "::").replace(" ::", "::").replace(":: ", "::")
    if "::" not in s and " " not in s:
        m = re.fullmatch(r"_?([A-Za-z_][A-Za-z0-9_]*)@\d+", s)   # __stdcall z C: _Nazwa@8
        if m:
            s = m.group(1)
    s = re.sub(r"\b(class|struct|enum|union) ", "", s)
    s = re.sub(r"\s+", " ", s)
    if len(s) > 80:   # std::basic_string<char,std::char_traits<char>,...> -> std::basic_string<>
        s = collapse_templates(s)
    return s or full.strip()


def collapse_templates(s):
    out, depth = [], 0
    for c in s:
        if c == "<":
            if depth == 0:
                out.append("<>")
            depth += 1
        elif c == ">" and depth > 0:
            depth -= 1
        elif depth == 0:
            out.append(c)
    return "".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--names", default=NAMES)
    ap.add_argument("--out", default=OUT)
    a = ap.parse_args()
    funcs = {}
    with open(a.names, encoding="latin1") as f:
        for line in f:
            m = re.match(r"0x([0-9A-Fa-f]{8}) (.*)$", line.rstrip("\r\n"))
            if not m:
                continue
            addr = int(m.group(1), 16)
            if not (TEXT_LO <= addr < TEXT_HI) or "<STRUCT" in m.group(2):
                continue
            name = short_name(m.group(2))
            if addr not in funcs or len(name) < len(funcs[addr]):
                funcs[addr] = name
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w", encoding="ascii", errors="replace", newline="\n") as f:
        f.write("# Funkcje Gothic2.exe (G2A) z Union SDK Names_G2A.hpp - tools\\gen_g2a_funcs.py; adres hex, nazwa\n")
        for addr in sorted(funcs):
            f.write("%08X %s\n" % (addr, funcs[addr]))
    print("OK: %d funkcji -> %s (%d B)" % (len(funcs), a.out, os.path.getsize(a.out)))


if __name__ == "__main__":
    sys.exit(main())
