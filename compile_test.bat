@echo off
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_DIR=%%i"
    )
)
if defined VS_DIR (
    call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat"
)
cl /EHsc C:\Users\Valen\.gemini\antigravity\brain\928d9474-79e0-43ab-bf66-85edea822704\scratch\test_sim.cpp /Fe:C:\Users\Valen\.gemini\antigravity\brain\928d9474-79e0-43ab-bf66-85edea822704\scratch\test_sim.exe
C:\Users\Valen\.gemini\antigravity\brain\928d9474-79e0-43ab-bf66-85edea822704\scratch\test_sim.exe
