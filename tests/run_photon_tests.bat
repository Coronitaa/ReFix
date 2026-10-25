@echo off
setlocal
cd /d "%~dp0.."
where cl >nul 2>&1 || call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
where cl >nul 2>&1 || call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not exist build\tests mkdir build\tests
cl /nologo /EHsc /std:c++17 /W3 /Fobuild\tests\ /Febuild\tests\photon_test.exe tests\photon_test.cpp src\photon\pn_proto18.cpp src\photon\pn_enet.cpp src\photon\pn_server.cpp src\photon\pn_crypto.cpp src\photon\pn_fusion.cpp
if errorlevel 1 exit /b 1
build\tests\photon_test.exe
