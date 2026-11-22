@echo off
call "%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0\.."
cl /nologo /O2 /W3 /EHsc /std:c++17 /Isrc tools\render_ui_test_standalone.cpp /link /out:tools\render_ui_test.exe
