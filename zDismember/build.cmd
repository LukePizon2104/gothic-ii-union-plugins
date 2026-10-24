@echo off
rem Buduje zDismember (Union SDK v1.0m, konfiguracja G2A MT Release) i sklada paczke zDismember.vdf
rem (DLL + skrypty z data\ + finishers.ini [+ nakladki animacji z data\_work, jesli zbudowane pipeline'em]).
rem   build.cmd        - kompilacja + release\zDismember.vdf; z ustawionym GOTHIC2_DIR tez instalacja do %GOTHIC2_DIR%\Data
rem   build.cmd nb     - kompilacja + release\nb\zDismember.vdf (New Balance); z GOTHIC2_NB_DIR instalacja do gry
rem   build.cmd novdf  - tylko kompilacja (Bin\zDismember.dll); z GOTHIC2_DIR kopia luznego DLL do System\Autorun
rem Przed pierwszym buildem: py -3 ..\tools\setup_zengin.py (naglowki ZenGin z szablonu Union SDK, README.md).
rem Opcjonalnie: PY (polecenie Pythona, domyslnie "py -3"), VCTOOLSVERSION (wersja MSVC, np. 14.44.35207),
rem UnionSdkDir (katalog Union SDK, domyslnie %ALLUSERSPROFILE%\Union\SDK).
setlocal
call "%~dp0..\tools\find_vs.cmd" || exit /b 1
if not defined PY set "PY=py -3"
cd /d "%~dp0"
if not exist zDismember\ZenGin\zGothicAPI.h (
	echo Brak zDismember\ZenGin - uruchom najpierw: %PY% ..\tools\setup_zengin.py  ^(README.md^)
	exit /b 1
)
set _CL_=/std:c++latest /permissive /Zc:twoPhase- /source-charset:utf-8 /execution-charset:.1250
set "VCTV="
if defined VCTOOLSVERSION set "VCTV=-p:VCToolsVersion=%VCTOOLSVERSION%"
"%MSBUILD%" zDismember.sln -nologo -v:minimal "-p:Configuration=G2A MT Release" -p:Platform=x86 %VCTV% || exit /b 1
if /i "%~1"=="novdf" (
	if defined GOTHIC2_DIR (
		copy /y Bin\zDismember.dll "%GOTHIC2_DIR%\System\Autorun\zDismember.dll" >nul || exit /b 1
		echo OK: %GOTHIC2_DIR%\System\Autorun\zDismember.dll
	) else echo OK: %~dp0Bin\zDismember.dll
	exit /b 0
)
rem tabela funkcji Gothic2.exe dla profilera (generowana z Names_G2A.hpp z Union SDK, nie jest w repozytorium)
if not exist data\System\zDismember\g2a_funcs.txt (%PY% -I tools\gen_g2a_funcs.py || exit /b 1)
set "MODE="
set "INST="
if /i "%~1"=="nb" (
	set "MODE=--nb"
	if defined GOTHIC2_NB_DIR set "INST=--install"
) else (
	if defined GOTHIC2_DIR set "INST=--install"
)
%PY% -I anims\pipeline\build_vdf.py %MODE% %INST% || exit /b 1
rem Prywatna paczka animacji z Wiedzmina 3 (data_w3) - NIE jest czescia repozytorium i nie wolno jej publikowac.
if exist data_w3 %PY% -I anims\pipeline\build_vdf.py --w3 %MODE% %INST% || exit /b 1
