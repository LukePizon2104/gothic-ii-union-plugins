# Gothic II Union plugins

Source code of three [Union](https://worldofplayers.ru/threads/41415/) plugins for **Gothic II: Night of the Raven**
(Gothic2.exe 2.6, Steam / GOG) and the **New Balance** mod:

| Plugin | What it does | Package |
|---|---|---|
| [zQuickSign](zQuickSign/) | Witcher-style quick "Sign": cast the selected rune with one key while the sword stays drawn (also while running), rune selection without drawing, sign wheel. Plain Win32 DLL, no Union SDK needed. | `System\Autorun\zQuickSign.dll` (New Balance: `NB_UNION_PLUGIN_ALLOWED_01.DLL`) |
| [zDismember](zDismember/) | Dismemberment, finishers, ragdolls, parry sparks, dodges / rolls, traversal, Witcher signs, meditation, back-mounted swords and more. Union SDK v1.0m. | `Data\zDismember.vdf` |
| [zTimeRewind](zTimeRewind/) | "Sands of Time" rune: hold to rewind the world up to 30 s (Prince of Persia style), returns consumed items / mana. Union SDK v1.0m. | `Data\zTimeRewind.vdf` |

Code comments and the in-game texts are mostly Polish. Detailed user documentation (settings in `Gothic.ini` /
`zQuickSign.ini`, console commands) is in `zDismember/README.txt`, `zTimeRewind/README.txt` and in the header
comment of `zQuickSign/zQuickSign.cpp`.

## Not included

* **No Witcher 3 (CD PROJEKT RED) assets.** zDismember can optionally use animations ported from The Witcher 3
  (finishers, W3 combat style, traversal, drawing swords from the back). Those animations and everything generated
  from them (`data_w3`, `*.MAN`/`*.MSB`/`*.MDH`, `zDismember_W3.vdf`) are **not** part of this repository and must not
  be redistributed. The scripts in `zDismember/anims/pipeline` are kept as tools only. Without the W3 package the
  plugin runs with the game's own animations (features that need W3 clips stay off).
* No compiled game animations at all (also none derived from Gothic or Mixamo): `zDismember/data/_work` is produced
  locally by the pipeline (`build_daze.py`, `make_finisher.py`) and packed into the VDF only when present.
* No Union SDK files (see below) and no binaries (`.dll`, `.vdf`).

## Requirements

* Windows, **Visual Studio 2022** with "Desktop development with C++" (MSVC **v143**, x86 tools) and a
  **Windows 10/11 SDK**. The build scripts find Visual Studio with `vswhere`; set `VCVARS32` and `MSBUILD` to override.
* **Python 3** (tested with 3.12, the `py` launcher) for packaging: `build_vdf.py`, `gen_g2a_funcs.py` and `setup_zengin.py` use only
  the standard library. The animation pipeline additionally needs `numpy`, `zenkit` (Python 3.12) and Blender.
* **Union SDK v1.0m** - only for zDismember and zTimeRewind. Install it with the
  [Union Resource Manager](https://worldofplayers.ru/threads/41415/) (Union team). The installer puts:
  * the SDK into `%ALLUSERSPROFILE%\Union\SDK\` (`Union\v1.0m\Include`, `Union\v1.0m\Lib`, `Union\v1.0m\Build\beforebuild.exe`,
    `incver.exe`, `DirectX7\include`, `DirectX7\Lib`);
  * the Visual Studio project template `UnionPlugin_1_0m.zip` into
    `%USERPROFILE%\Documents\Visual Studio 2022\Templates\ProjectTemplates\`.

  A different SDK location can be given with the `UnionSdkDir` environment variable (or `/p:UnionSdkDir=...`).

### ZenGin headers

The projects include the ZenGin engine API headers (`<plugin>\<plugin>\ZenGin\`, (c) Union team) that come with
the plugin template. They are not stored here; recreate them once after cloning:

```bat
py -3 tools\setup_zengin.py
rem or: py -3 tools\setup_zengin.py --template "D:\path\to\UnionPlugin_1_0m.zip"
```

The script copies `ZENGIN\` from the template into `zDismember\zDismember\ZenGin` and `zTimeRewind\zTimeRewind\ZenGin`
and applies `<plugin>\sdk\ZenGin.diff` (MSVC 2022 / C++latest fixes and the plugins' own declarations in
`Gothic_UserAPI\*.inl`). The template path can also be set with `UNION_PLUGIN_TEMPLATE`.

## Building

All commands from a normal `cmd` prompt in the repository root. Without the game variables below the scripts only
build; with them they also install into the game (close the game first - it locks the DLL / VDF).

```bat
rem zQuickSign (no Union SDK)
zQuickSign\build.cmd              & rem -> zQuickSign\zQuickSign.dll
zQuickSign\build.cmd steam        & rem + copy to %GOTHIC2_DIR%\System\Autorun\zQuickSign.dll
zQuickSign\build.cmd nb           & rem + copy to %GOTHIC2_NB_DIR%\System\Autorun\NB_UNION_PLUGIN_ALLOWED_01.DLL

rem zDismember (G2A MT Release)
zDismember\build.cmd              & rem -> zDismember\Bin\zDismember.dll + zDismember\release\zDismember.vdf
zDismember\build.cmd nb           & rem -> zDismember\release\nb\zDismember.vdf (New Balance slot NB_UNION_PLUGIN_DIFF_01.DLL)
zDismember\build.cmd novdf        & rem DLL only

rem zTimeRewind (G2A MT Release)
zTimeRewind\build.cmd             & rem -> zTimeRewind\Bin\zTimeRewind.dll + zTimeRewind\release\zTimeRewind.vdf
zTimeRewind\build.cmd novdf       & rem DLL + loose scripts into %GOTHIC2_DIR%\System\Autorun (quick tests)
```

The `.sln` files can also be opened in Visual Studio (configuration `G2A MT Release`, platform `x86`). The Union
pre-build step (`incver.exe`) bumps the build number in `resource.rc` on every build.

| Variable | Meaning |
|---|---|
| `GOTHIC2_DIR` | Gothic II NotR folder (Steam/GOG) - install target of the Steam packages |
| `GOTHIC2_NB_DIR` | Gothic II folder with New Balance - target of the `nb` packages, source of `HUMANS.MSB` for `zQuickSign\build_anims.py` |
| `GOTHIC_KM_DIR` | The Chronicles of Myrtana folder (`build_vdf.py --km`) |
| `PY` | Python command, default `py -3` |
| `VCTOOLSVERSION` | pin an MSVC toolset version, e.g. `14.44.35207` (default: newest installed v143) |
| `UnionSdkDir`, `UNION_PLUGIN_TEMPLATE` | Union SDK root / plugin template (see above) |
| `VCVARS32`, `MSBUILD` | explicit Visual Studio paths |
| `BLENDER_EXE`, `BLENDER45_EXE`, `W3_DUMP_PY`, `GOTHIC_TOOLS_DIR`, `XBOW_SEED_DUMPS` | animation pipeline / script checker only |

## Tests

The engine-independent logic (`Core\`, `zQuickSignSelect.h`) has plain C++ test programs (no game, no Union SDK):

```bat
zQuickSign\build_tests.cmd
zDismember\tests\build_tests.cmd [outdir]
zTimeRewind\tests\build_tests.cmd
```

Exit code 0 means all checks passed. `zDismember\anims\pipeline\tests_pipeline.py` tests the animation pipeline: it
needs the game data (`GOTHIC2_DIR`), `zenkit` and `numpy`; tests that need W3 work files are skipped, and five tests
fail without a local Mixamo dump (`anims\work\Great Sword Slash.json`, not included).

## Installing the built files

* `Data\zDismember.vdf`, `Data\zTimeRewind.vdf` - copy into the game's `Data` folder (Union 1.0m or newer).
* `zQuickSign.dll` - into `System\Autorun` (New Balance: under the name `NB_UNION_PLUGIN_ALLOWED_01.DLL`, because
  New Balance only loads DLLs from its own name whitelist).
* Optional: `zQuickSign\build_anims.py` builds `Data\zQuickSign_Anims.vdf` (cast gesture) from the installed New Balance
  `HUMANS.MSB` - rebuild it after every New Balance update.

## Repository layout

```
_shared/        headers shared by zDismember and zTimeRewind (logging, profiler, spell slots, cut link)
tools/          setup_zengin.py (ZenGin headers from the Union template), find_vs.cmd (Visual Studio lookup)
zQuickSign/     zQuickSign.cpp + headers, tests, build_anims.py (+ manlib.py / msblib.py, MAN/MSB helpers)
zDismember/     zDismember/ (plugin, Core/ = engine-free logic), tests/, data/ (Daedalus scripts, finishers.ini),
                data_nb/ (New Balance only scripts), anims/pipeline/ (animation tools), tools/, sdk/ZenGin.diff
zTimeRewind/    zTimeRewind/ (plugin, Core/), tests/, scripts/ (rune + menu), build_vdf.py, sdk/ZenGin.diff
```

## Third-party notes

* Union, Union SDK, ZenGin API headers and the menu base script `Menu\zUnionMenu.d`: Union team (Gratt and contributors).
  The boilerplate files of the plugin template (`UnionAfx.h`, `DllMain.cpp`, `Interface.cpp`, `resource.*`,
  `.build_settings.inl`, `Plugin.patch`) come from the Union SDK template.
* Gothic II (c) Piranha Bytes / THQ Nordic. New Balance is a mod by its own authors; zDismember's path menu contains a
  table describing New Balance skills (costs, requirements, teachers), written for compatibility.
* The Witcher 3 (c) CD PROJEKT RED - no assets included (see above). Finisher animations of the public build: Mixamo (Adobe).

## License

MIT - see [LICENSE](LICENSE). The license covers the code in this repository, not the Union SDK, the game or any
third-party assets.

---

### PL - w skrócie

Kod źródłowy pluginów Union zQuickSign, zDismember i zTimeRewind do Gothic II NK (Steam/GOG) i moda New Balance.
Wymagane: Visual Studio 2022 (v143, x86, Windows SDK), Python 3, a dla zDismember / zTimeRewind Union SDK v1.0m
z Union Resource Manager. Po sklonowaniu: `py -3 tools\setup_zengin.py` (nagłówki ZenGin z szablonu Union SDK),
potem `zQuickSign\build.cmd`, `zDismember\build.cmd`, `zTimeRewind\build.cmd`; testy: `build_tests.cmd`.
Instalacja do gry tylko przy ustawionych `GOTHIC2_DIR` / `GOTHIC2_NB_DIR`. Animacji z Wiedźmina 3 (CD PROJEKT RED)
ani żadnych wygenerowanych z nich plików w repozytorium nie ma i nie wolno ich rozpowszechniać.
