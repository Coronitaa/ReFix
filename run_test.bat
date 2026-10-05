@echo off
set "VCVARS="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul
cl /nologo /LD /EHsc dummy.cpp /Febin\steam_api64_valve.dll
cd bin
test_provider.exe
