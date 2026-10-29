@echo off
rem Build a 32-bit helper (rfvk_inject.exe or rfvk_autologin.dll) outside the
rem main x64 tree.
rem Args: %1 source .cpp, %2 output dir (next to the launcher), %3 vcvarsall.bat,
rem       %4 kind: exe (default) or dll, %5 output name without extension
rem       (default: rfvk_inject).
rem Switches this cmd process to the x86 toolchain and compiles one
rem self-contained file. ASCII only on purpose: cmd reads .bat in the OEM
rem codepage, so non-ASCII comments would be misparsed as commands.
setlocal
set "SRC=%~1"
set "OUTDIR=%~2"
set "VCVARSALL=%~3"
set "KIND=%~4"
set "NAME=%~5"
if "%KIND%"=="" set "KIND=exe"
if "%NAME%"=="" set "NAME=rfvk_inject"

if not exist "%VCVARSALL%" (
    echo [%NAME%] vcvarsall.bat not found: "%VCVARSALL%" 1>&2
    exit /b 1
)
call "%VCVARSALL%" x86 >nul || (echo [%NAME%] vcvarsall x86 failed 1>&2 & exit /b 1)

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

if /i "%KIND%"=="dll" (
    cl /nologo /O2 /MT /EHsc /W3 /utf-8 /std:c++17 /DUNICODE /D_UNICODE /LD ^
       "%SRC%" ^
       /Fe:"%OUTDIR%\%NAME%.dll" ^
       /Fo:"%OUTDIR%\%NAME%.obj" ^
       /link /DLL kernel32.lib user32.lib || exit /b 1
) else (
    cl /nologo /O2 /MT /EHsc /W3 /utf-8 /DUNICODE /D_UNICODE ^
       "%SRC%" ^
       /Fe:"%OUTDIR%\%NAME%.exe" ^
       /Fo:"%OUTDIR%\%NAME%.obj" ^
       /link /SUBSYSTEM:CONSOLE kernel32.lib user32.lib advapi32.lib || exit /b 1
)

exit /b 0
