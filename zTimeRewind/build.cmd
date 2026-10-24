@echo off
rem Buduje zTimeRewind (Union SDK v1.0m, konfiguracja G2A MT Release) i sklada paczke zTimeRewind.vdf (DLL, runa, menu Union).
rem   build.cmd        - kompilacja + release\zTimeRewind.vdf; z ustawionym GOTHIC2_DIR tez instalacja do %GOTHIC2_DIR%\Data
rem                      (+ usuniecie luznych plikow testowych z System\Autorun)
rem   build.cmd novdf  - kompilacja + luzne pliki w %GOTHIC2_DIR%\System\Autorun (szybkie testy), VDF z Data\ usuwany
rem Gra musi byc zamknieta (DLL i VDF sa zablokowane przez dzialajaca gre).
rem Przed pierwszym buildem: py -3 ..\tools\setup_zengin.py (naglowki ZenGin z szablonu Union SDK, README.md).
rem Opcjonalnie: PY (polecenie Pythona, domyslnie "py -3"), VCTOOLSVERSION, UnionSdkDir - jak w zDismember\build.cmd.
setlocal
call "%~dp0..\tools\find_vs.cmd" || exit /b 1
if not defined PY set "PY=py -3"
cd /d "%~dp0"
if not exist zTimeRewind\ZenGin\zGothicAPI.h (
	echo Brak zTimeRewind\ZenGin - uruchom najpierw: %PY% ..\tools\setup_zengin.py  ^(README.md^)
	exit /b 1
)
set _CL_=/std:c++latest /permissive /Zc:twoPhase- /source-charset:utf-8 /execution-charset:.1250
set "VCTV="
if defined VCTOOLSVERSION set "VCTV=-p:VCToolsVersion=%VCTOOLSVERSION%"
"%MSBUILD%" zTimeRewind.sln -nologo -v:minimal "-p:Configuration=G2A MT Release" -p:Platform=x86 %VCTV% || exit /b 1
if /i "%~1"=="novdf" goto loose
set "INST="
if defined GOTHIC2_DIR set "INST=--install"
%PY% -I build_vdf.py %INST% || exit /b 1
exit /b 0

:loose
if not defined GOTHIC2_DIR (echo novdf: ustaw GOTHIC2_DIR na katalog Gothic II & exit /b 1)
set "GAME=%GOTHIC2_DIR%"
if exist "%GAME%\Data\zTimeRewind.vdf" del /q "%GAME%\Data\zTimeRewind.vdf" || exit /b 1
copy /y Bin\zTimeRewind.dll "%GAME%\System\Autorun\zTimeRewind.dll" >nul || exit /b 1
if exist "%GAME%\System\Autorun\zTimeRewind.d" del /q "%GAME%\System\Autorun\zTimeRewind.d"
rem skrypt runy w UTF-8, parser Gothica czyta cp1250; menu jak w zDismember zostaje w UTF-8
powershell -NoProfile -Command "$t=[IO.File]::ReadAllText('scripts\zTimeRewind_Rune.d',[Text.Encoding]::UTF8); [IO.File]::WriteAllText('%GAME%\System\Autorun\zTimeRewind_Rune.d',$t,[Text.Encoding]::GetEncoding(1250))" || exit /b 1
copy /y scripts\zTimeRewind_Menu.d "%GAME%\System\Autorun\zTimeRewind_Menu.d" >nul || exit /b 1
if not exist "%GAME%\System\Autorun\Menu\zUnionMenu.d" (
	if not exist "%GAME%\System\Autorun\Menu" mkdir "%GAME%\System\Autorun\Menu"
	copy /y scripts\Menu\zUnionMenu.d "%GAME%\System\Autorun\Menu\zUnionMenu.d" >nul || exit /b 1
)
echo OK (luzne): %GAME%\System\Autorun\zTimeRewind.dll + zTimeRewind_Rune.d + zTimeRewind_Menu.d
