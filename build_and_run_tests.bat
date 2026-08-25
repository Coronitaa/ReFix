@echo off
setlocal enabledelayedexpansion

if not exist "%~dp0build\tests" mkdir "%~dp0build\tests"
cd /d "%~dp0"

where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    set "VCVARS="
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    if not defined VCVARS if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    if defined VCVARS call "!VCVARS!" >nul 2>&1
)

set CFLAGS=/nologo /EHsc /std:c++17 /O2 /Isrc
set LIBS=ws2_32.lib advapi32.lib user32.lib ole32.lib

echo ====================================================================
echo [*] Building EOS Online V2 Unit Tests...
echo ====================================================================

echo [*] Compiling test_abi_asserts...
cl %CFLAGS% tests\test_abi_asserts.cpp /Febuild\tests\test_abi_asserts.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_abi_asserts & exit /b 1 )

echo [*] Compiling test_identity...
cl %CFLAGS% tests\test_identity.cpp src\identity\online_identity_provider.cpp src\eos\eos_identity.cpp /Febuild\tests\test_identity.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_identity & exit /b 1 )

echo [*] Compiling test_callbacks...
cl %CFLAGS% tests\test_callbacks.cpp src\identity\online_identity_provider.cpp src\eos\eos_callbacks.cpp src\eos\eos_connect.cpp src\eos\eos_identity.cpp /Febuild\tests\test_callbacks.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_callbacks & exit /b 1 )

echo [*] Compiling test_connect_contract...
cl %CFLAGS% tests\test_connect_contract.cpp src\identity\online_identity_provider.cpp src\eos\eos_connect.cpp src\eos\eos_identity.cpp src\eos\eos_callbacks.cpp /Febuild\tests\test_connect_contract.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_connect_contract & exit /b 1 )

echo [*] Compiling test_connect_login...
cl %CFLAGS% tests\test_connect_login.cpp src\identity\online_identity_provider.cpp src\eos\eos_connect.cpp src\eos\eos_identity.cpp src\eos\eos_callbacks.cpp /Febuild\tests\test_connect_login.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_connect_login & exit /b 1 )

echo [*] Compiling test_connect_deviceid...
cl %CFLAGS% tests\test_connect_deviceid.cpp src\identity\online_identity_provider.cpp src\eos\eos_connect.cpp src\eos\eos_identity.cpp src\eos\eos_callbacks.cpp /Febuild\tests\test_connect_deviceid.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_connect_deviceid & exit /b 1 )

echo [*] Compiling test_connect_external_account...
cl %CFLAGS% tests\test_connect_external_account.cpp src\identity\online_identity_provider.cpp src\eos\eos_connect.cpp src\eos\eos_identity.cpp src\eos\eos_callbacks.cpp /Febuild\tests\test_connect_external_account.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_connect_external_account & exit /b 1 )

echo [*] Compiling test_two_machine_identity...
cl %CFLAGS% tests\test_two_machine_identity.cpp src\identity\online_identity_provider.cpp src\eos\eos_connect.cpp src\eos\eos_identity.cpp src\eos\eos_callbacks.cpp /Febuild\tests\test_two_machine_identity.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_two_machine_identity & exit /b 1 )

echo [*] Compiling test_backend_protocol...
cl %CFLAGS% tests\test_backend_protocol.cpp src\refix_online\refix_wire.cpp src\refix_online\refix_backend_protocol.cpp /Febuild\tests\test_backend_protocol.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_backend_protocol & exit /b 1 )

echo [*] Compiling test_backend_state...
cl %CFLAGS% tests\test_backend_state.cpp src\refix_online\refix_wire.cpp src\refix_online\refix_backend_protocol.cpp src\refix_online\refix_backend_state.cpp /Febuild\tests\test_backend_state.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_backend_state & exit /b 1 )

echo [*] Compiling test_backend_reconnect...
cl %CFLAGS% tests\test_backend_reconnect.cpp src\identity\online_identity_provider.cpp src\refix_online\refix_wire.cpp src\refix_online\refix_backend_protocol.cpp src\refix_online\refix_backend_state.cpp src\refix_online\refix_backend_client.cpp src\eos\eos_connect.cpp src\eos\eos_identity.cpp src\eos\eos_callbacks.cpp /Febuild\tests\test_backend_reconnect.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_backend_reconnect & exit /b 1 )

echo [*] Compiling test_lobby_create...
cl %CFLAGS% tests\test_lobby_create.cpp src\identity\online_identity_provider.cpp src\eos\eos_lobby.cpp src\eos\eos_connect.cpp src\eos\eos_identity.cpp src\eos\eos_callbacks.cpp src\refix_online\refix_wire.cpp src\refix_online\refix_backend_protocol.cpp src\refix_online\refix_backend_state.cpp src\refix_online\refix_backend_client.cpp src\eos_core\eos_room_manager.cpp /Febuild\tests\test_lobby_create.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_lobby_create & exit /b 1 )

echo [*] Compiling test_lobby_search...
cl %CFLAGS% tests\test_lobby_search.cpp src\identity\online_identity_provider.cpp src\eos\eos_lobby.cpp src\eos\eos_connect.cpp src\eos\eos_identity.cpp src\eos\eos_callbacks.cpp src\refix_online\refix_wire.cpp src\refix_online\refix_backend_protocol.cpp src\refix_online\refix_backend_state.cpp src\refix_online\refix_backend_client.cpp src\eos_core\eos_room_manager.cpp /Febuild\tests\test_lobby_search.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_lobby_search & exit /b 1 )

echo [*] Compiling test_lobby_join_leave...
cl %CFLAGS% tests\test_lobby_join_leave.cpp src\identity\online_identity_provider.cpp src\eos\eos_lobby.cpp src\eos\eos_connect.cpp src\eos\eos_identity.cpp src\eos\eos_callbacks.cpp src\refix_online\refix_wire.cpp src\refix_online\refix_backend_protocol.cpp src\refix_online\refix_backend_state.cpp src\refix_online\refix_backend_client.cpp src\eos_core\eos_room_manager.cpp /Febuild\tests\test_lobby_join_leave.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_lobby_join_leave & exit /b 1 )

echo [*] Compiling test_runtime_auth_harness...
cl %CFLAGS% tests\test_runtime_auth_harness.cpp /Febuild\tests\test_runtime_auth_harness.exe %LIBS%
if %ERRORLEVEL% neq 0 ( echo [!] Failed building test_runtime_auth_harness & exit /b 1 )

echo.
echo ====================================================================
echo [*] Executing EOS Online V2 Unit Tests...
echo ====================================================================

build\tests\test_abi_asserts.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_abi_asserts FAILED & exit /b 1 )

build\tests\test_identity.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_identity FAILED & exit /b 1 )

build\tests\test_callbacks.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_callbacks FAILED & exit /b 1 )

build\tests\test_connect_contract.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_connect_contract FAILED & exit /b 1 )

build\tests\test_connect_login.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_connect_login FAILED & exit /b 1 )

build\tests\test_connect_deviceid.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_connect_deviceid FAILED & exit /b 1 )

build\tests\test_connect_external_account.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_connect_external_account FAILED & exit /b 1 )

build\tests\test_two_machine_identity.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_two_machine_identity FAILED & exit /b 1 )

build\tests\test_backend_protocol.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_backend_protocol FAILED & exit /b 1 )

build\tests\test_backend_state.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_backend_state FAILED & exit /b 1 )

build\tests\test_backend_reconnect.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_backend_reconnect FAILED & exit /b 1 )

build\tests\test_lobby_create.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_lobby_create FAILED & exit /b 1 )

build\tests\test_lobby_search.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_lobby_search FAILED & exit /b 1 )

build\tests\test_lobby_join_leave.exe
if %ERRORLEVEL% neq 0 ( echo [!] test_lobby_join_leave FAILED & exit /b 1 )

echo.
echo ====================================================================
echo [ALL TESTS PASSED SUCCESSFULLY]
echo ====================================================================
