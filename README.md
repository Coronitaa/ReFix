# ReFix — Universal Game DLL Proxy & Steam/EOS Integration Framework

ReFix is a light-weight, modular C++ DLL proxy and runtime patcher for Windows games (Unity and Unreal Engine).

## Features
- **winmm.dll Proxy:** Transparent C++ proxy loader that hooks into game startup.
- **Steam Networking Proxy (`steam_api64.dll`):** Provides offline and online P2P network glue.
- **EOS Authentication & Session Emulator (`EOSSDK-Win64-Shipping.dll`):** Resolves Epic Online Services login requirement using local persistent DeviceID auth and emulates EOS session/lobby callbacks.
- **UPnP & Windows Firewall Automation:** Automatically configures local UDP ports (default 7777) for P2P multiplayer.
- **Steam Non-Steam Game Shortcut Installer:** Auto-generates binary VDF shortcuts for Steam client integration.

## Project Structure
```
ReFix_src/
├── src/
│   ├── winmm_proxy.cpp
│   ├── eos_proxy.cpp
│   ├── steam_proxy.cpp
│   ├── upnp_firewall.cpp
│   ├── upnp_firewall.h
│   └── minhook_repo/
├── build.bat
├── ReFix.ini
└── README.md
```

## Building
To build ReFix DLLs, run `build.bat` using MSVC (Developer Command Prompt for VS) or GCC / MinGW:
```cmd
build.bat
```
The compiled binaries will be output to `build/`.

## License
MIT License.
