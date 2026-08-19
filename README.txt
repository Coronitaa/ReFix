====================================================================
               ReFix Deployment Suite v3.2 (Standalone)
====================================================================

HERRAMIENTAS INCLUIDAS:

1. AutoDeploy.bat
-----------------
- Herramienta principal de despliegue automatico de emulacion Steam.
- Modos disponibles:
  [1] ReFix Online por Steam (Spacewar 480 / servidores de Steam).
  [2] Re:Goldberg LAN sin Steam (Emulacion 100% offline, broadcast local).
- Deteccion automatica de motores (Unity, Unreal, Godot, Native), rutas de ejecutables y AppID original.
- Generacion de asistente portable 'Configurar_Firewall_LAN.bat' para pendrive / USB.

2. DLC_Unlocker.bat (BLUESTAR Engine)
--------------------------------------
- Desbloqueador universal de DLCs basado en SmokeAPI / CreamAPI.
- Desbloqueo universal automatico (unlockall = true / unlock_all = true).
- Consulta automatica del catalogo oficial de DLCs via Steam Store API.
- Creacion y restauracion de respaldos originales (steam_api64_o.dll / steam_api_o.dll).
- Opciones de instalacion, desinstalacion y consulta de estado.

3. Uninstall_ReFix.bat
----------------------
- Desinstalador y restaurador universal.
- Restaura los archivos DLL originales (.orig, _valve, _o) y elimina todos los proxies, emuladores y configuraciones.
