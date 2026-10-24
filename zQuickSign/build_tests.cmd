@echo off
rem Buduje i uruchamia testy czystej logiki zQuickSign (tests_zqs.cpp) - bez gry. Kod wyjscia 0 = wszystkie przeszly.
setlocal
call "%~dp0..\tools\find_vs.cmd" || exit /b 1
call "%VCVARS32%" >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
cl /nologo /EHsc /W3 tests_zqs.cpp /Fo:out\ /Fe:out\tests_zqs.exe >out\build.log 2>&1 || (type out\build.log & exit /b 1)
out\tests_zqs.exe
