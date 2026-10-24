@echo off
rem Ustawia VCVARS32 (vcvars32.bat) i MSBUILD (MSBuild.exe) z Visual Studio 2022 z narzedziami C++ (vswhere).
rem Wywolywany przez build.cmd / build_tests.cmd. Wlasna instalacja: ustaw VCVARS32 i MSBUILD przed wywolaniem.
if defined VCVARS32 if defined MSBUILD exit /b 0
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
	echo Nie znaleziono vswhere.exe - zainstaluj Visual Studio 2022 z "Desktop development with C++" albo ustaw VCVARS32 i MSBUILD.
	exit /b 1
)
set "VSDIR="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
	echo Nie znaleziono Visual Studio z narzedziami C++ x86/x64 ^(toolset v143^).
	exit /b 1
)
if not defined VCVARS32 set "VCVARS32=%VSDIR%\VC\Auxiliary\Build\vcvars32.bat"
if not defined MSBUILD set "MSBUILD=%VSDIR%\MSBuild\Current\Bin\MSBuild.exe"
exit /b 0
