@echo off
title Installing Visual Studio 2022 C++ Build Tools (cl.exe)

echo ====================================================
echo  Installing MSVC C++ Build Tools via winget...
echo  This will install cl.exe and 64-bit build tools.
echo ====================================================
echo.

winget install Microsoft.VisualStudio.2022.BuildTools --override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" --accept-source-agreements --accept-package-agreements

echo.
echo ====================================================
echo  [DONE] C++ Build Tools Installation Finished!
echo ====================================================
pause
