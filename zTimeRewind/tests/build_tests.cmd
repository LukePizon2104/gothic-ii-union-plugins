@echo off
rem Buduje i uruchamia testy logiki Core (bez silnika gry i bez Union SDK). Kod wyjscia 0 = wszystkie przeszly.
setlocal
call "%~dp0..\..\tools\find_vs.cmd" || exit /b 1
call "%VCVARS32%" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /std:c++20 /EHsc /W3 /utf-8 /I..\zTimeRewind test_core.cpp ..\zTimeRewind\Core\*.cpp /Fe:out\test_core.exe /Fo:out\ >out\build.log 2>&1 || (type out\build.log & exit /b 1)
out\test_core.exe
