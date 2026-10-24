@echo off
rem Buduje zQuickSign.dll (x86, statyczny CRT). Bez Union SDK - zwykly DLL ladowany przez Union z System\Autorun.
rem   build.cmd        - tylko kompilacja: zQuickSign.dll obok zrodel
rem   build.cmd nb     - + kopia do %GOTHIC2_NB_DIR%\System\Autorun\NB_UNION_PLUGIN_ALLOWED_01.DLL
rem                      (New Balance: UNION_ABI.DLL wpuszcza tylko DLL z bialej listy nazw - wolny slot dla obcych
rem                      pluginow to NB_UNION_PLUGIN_ALLOWED_01..05)
rem   build.cmd steam  - + kopia do %GOTHIC2_DIR%\System\Autorun\zQuickSign.dll (czysty Gothic II NotR z Unionem)
setlocal
call "%~dp0..\tools\find_vs.cmd" || exit /b 1
call "%VCVARS32%" >nul || exit /b 1
cd /d "%~dp0"
cl /nologo /O2 /MT /LD /W3 /EHsc zQuickSign.cpp user32.lib /link /OUT:zQuickSign.dll || exit /b 1
echo OK: %~dp0zQuickSign.dll
if "%~1"=="" exit /b 0
if /i "%~1"=="nb" goto nb
if /i "%~1"=="steam" goto steam
echo Nieznany cel: %~1 (nb / steam)
exit /b 1

:nb
if not defined GOTHIC2_NB_DIR (echo Ustaw GOTHIC2_NB_DIR na katalog gry z New Balance & exit /b 1)
set "GAME=%GOTHIC2_NB_DIR%"
set SLOT=NB_UNION_PLUGIN_ALLOWED_01.DLL
goto install

:steam
if not defined GOTHIC2_DIR (echo Ustaw GOTHIC2_DIR na katalog Gothic II & exit /b 1)
set "GAME=%GOTHIC2_DIR%"
set SLOT=zQuickSign.dll

:install
copy /y zQuickSign.dll "%GAME%\System\Autorun\%SLOT%" >nul || exit /b 1
if /i not "%SLOT%"=="zQuickSign.dll" if exist "%GAME%\System\Autorun\zQuickSign.dll" del "%GAME%\System\Autorun\zQuickSign.dll"
echo OK: %GAME%\System\Autorun\%SLOT%
