@echo off
rem Build the 32-bit rfvk_inject.exe outside the main x64 tree.
rem Args: %1 source .cpp, %2 output dir (next to the launcher), %3 vcvarsall.bat.
rem Switches this cmd process to the x86 toolchain and compiles one
rem self-contained file. ASCII only on purpose: cmd reads .bat in the OEM
rem codepage, so non-ASCII comments would be misparsed as commands.
setlocal
set "SRC=%~1"
set "OUTDIR=%~2"
set "VCVARSALL=%~3"

if not exist "%VCVARSALL%" (
    echo [rfvk_inject] vcvarsall.bat not found: "%VCVARSALL%" 1>&2
    exit /b 1
)
call "%VCVARSALL%" x86 >nul || (echo [rfvk_inject] vcvarsall x86 failed 1>&2 & exit /b 1)

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

cl /nologo /O2 /MT /EHsc /W3 /utf-8 /DUNICODE /D_UNICODE ^
   "%SRC%" ^
   /Fe:"%OUTDIR%\rfvk_inject.exe" ^
   /Fo:"%OUTDIR%\rfvk_inject.obj" ^
   /link /SUBSYSTEM:CONSOLE kernel32.lib user32.lib advapi32.lib || exit /b 1

exit /b 0
