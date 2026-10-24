@echo off
rem Buduje i uruchamia testy logiki Core (bez silnika gry i bez Union SDK). Kod wyjscia 0 = wszystkie przeszly.
rem   build_tests.cmd [katalog]  - katalog wyjsciowy (domyslnie out); osobne katalogi pozwalaja budowac rownolegle
setlocal
call "%~dp0..\..\tools\find_vs.cmd" || exit /b 1
call "%VCVARS32%" >nul || exit /b 1
cd /d "%~dp0"
set "OUT=%~1"
if "%OUT%"=="" set OUT=out
if not exist %OUT% mkdir %OUT%
cl /nologo /std:c++20 /EHsc /W3 /utf-8 /I..\zDismember test_core.cpp ..\zDismember\Core\*.cpp /Fe:%OUT%\test_core.exe /Fo:%OUT%\ >%OUT%\build.log 2>&1 || (type %OUT%\build.log & exit /b 1)
%OUT%\test_core.exe
