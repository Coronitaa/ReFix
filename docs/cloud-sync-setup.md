# Sincronizar perfiles con Google Drive y OneDrive: cómo conseguir los IDs de ReFix

ReFix puede guardar el archivo `.rfxp` de cada perfil directamente en la cuenta de Google Drive u OneDrive del
jugador, sin que tenga instalada la app de escritorio. Para eso ReFix necesita estar registrado como "app" en
Google y en Microsoft. Haces el registro **una sola vez**, es gratis y sirve para todos los usuarios: cada uno
usa su propio almacenamiento (Google: 15 GB gratis, OneDrive: 5 GB gratis), no el tuyo.

Lo que obtienes no son contraseñas de nadie: son los **IDs de cliente** que identifican a ReFix. Cada jugador
sigue teniendo que iniciar sesión y aceptar en su navegador.

## Permisos mínimos (no pidas más)

| Servicio | Permiso | Qué puede hacer ReFix |
|---|---|---|
| Google Drive | `https://www.googleapis.com/auth/drive.appdata` | Solo su carpeta oculta de datos de app. No ve ni toca los archivos del usuario. |
| OneDrive | `Files.ReadWrite.AppFolder` + `offline_access` | Solo la carpeta `Aplicaciones\ReFix`. `offline_access` sirve para no pedir el inicio de sesión cada vez. |

No se pide email, nombre, contactos ni foto de perfil.

## 1. Google Drive

1. Entra en <https://console.cloud.google.com/> con tu cuenta de Google y crea un proyecto llamado `ReFix`.
2. **APIs y servicios → Biblioteca**: busca **Google Drive API** y pulsa **Habilitar**.
3. **Google Auth Platform** (antes "Pantalla de consentimiento de OAuth"):
   - **Branding / Información de la app**: nombre `ReFix` y tu email de asistencia.
   - **Público / Audience**: tipo **Externo**.
   - **Acceso a datos / Data access → Añadir permisos**: marca solo `.../auth/drive.appdata`. Guarda.
4. **Clientes / Clients → Crear cliente**: tipo de aplicación **App de escritorio** (Desktop app), nombre `ReFix`.
   Copia el **ID de cliente** y el **Secreto del cliente** (o descarga el JSON).
5. **Público / Audience → Publicar app** (pasa de "Prueba" a "En producción").
   Mientras esté en "Prueba", solo pueden entrar los usuarios de prueba que añadas (hasta 100) y Google les
   caduca la sesión a los 7 días. `drive.appdata` es un permiso no sensible: Google no pide la auditoría de
   seguridad que exige para los permisos amplios de Drive.

En las apps de escritorio, Google no trata el "secreto del cliente" como secreto: va dentro del programa. La
seguridad la dan PKCE, el permiso mínimo y que cada usuario acepte en su navegador. Aun así, no lo publiques.

## 2. OneDrive (Microsoft)

1. Entra en <https://entra.microsoft.com/> → **Aplicaciones → Registros de aplicaciones → Nuevo registro**.
   Si te pide un directorio (inquilino), crea una cuenta gratuita de Azure: registrar apps no tiene coste.
2. Nombre `ReFix`. En **Tipos de cuenta compatibles** elige **Cuentas de cualquier directorio organizativo y
   cuentas Microsoft personales**.
3. **URI de redirección**: plataforma **Cliente público/nativo (móvil y escritorio)**, valor `http://localhost`.
   Registra. (Microsoft acepta cualquier puerto en `localhost`; ReFix usa uno al azar en cada inicio de sesión.)
4. Copia el **Id. de aplicación (cliente)**.
5. **Permisos de API**: quita `User.Read` y añade **Microsoft Graph → Permisos delegados →
   `Files.ReadWrite.AppFolder`** y **`offline_access`**. No hace falta consentimiento de administrador.
6. **No crees** secretos ni certificados: ReFix es un cliente público y no los usa.

## 3. Guardar los IDs de forma segura

1. En `src/overlay/`, copia `cloud_keys.local.h.example` como `cloud_keys.local.h` y pega tus valores.
   Git ignora ese archivo (`.gitignore`), así que no se sube nunca.
   El secreto de Google va como hex de sus bytes XOR 0x5A (el ejemplo trae el comando de Python), para que
   los escáneres de secretos de GitHub no lo encuentren en la DLL publicada y Google no lo revoque solo.
2. Guarda también una copia en tu gestor de contraseñas. No los pegues en issues, chats ni capturas.
3. Compila con `build.bat`. Si el archivo no existe, la build funciona igual y los botones de Google Drive y
   OneDrive no aparecen (siguen las carpetas, el USB y la exportación).
4. Si alguna vez se filtran:
   - Google: en **Clientes**, rota el secreto o borra el cliente y crea uno nuevo, y recompila.
   - Microsoft: el ID solo no da acceso a nada. Si quieres, borra el registro y crea otro.

## 4. Cómo se protegen los datos del usuario

- **Inicio de sesión** en el navegador del jugador, con OAuth 2.0 + PKCE y un parámetro `state` aleatorio. ReFix
  escucha la respuesta solo en `127.0.0.1` / `::1` (nada de la red puede llegar) y como mucho 3 minutos.
- **Tokens**:
  - El token de renovación se guarda cifrado con DPAPI de Windows para ese usuario de Windows, en
    `%APPDATA%\ReFix\Cloud\google.bin` u `onedrive.bin`. Otro usuario del PC, u otro PC, no puede descifrarlo.
  - El token de acceso (válido 1 hora) solo vive en memoria.
  - Los tokens nunca se escriben en `ReFix.log` y se borran de la memoria al terminar de usarlos.
- **Red**: solo HTTPS. Las redirecciones no se siguen con el token puesto, así que este nunca sale hacia otro
  servidor. Las descargas y subidas de OneDrive usan enlaces firmados, sin token.
- **Partidas**:
  - Lo que se sube es el mismo `.rfxp` ofuscado y con checksum.
  - La copia local se mantiene siempre.
  - Si al empezar la partida no se pudo leer la nube, ReFix no sube nada que pueda pisar una partida más nueva
    de otro PC.
- **Cerrar sesión** (Ajustes → Sincronización de perfiles):
  - Borra el token del PC.
  - En Google además revoca el permiso. En Microsoft, el usuario lo quita en
    <https://account.live.com/consent/Manage>.
  - Los datos en Google Drive se borran en Drive → Configuración → Administrar aplicaciones → ReFix → Eliminar
    datos ocultos de la aplicación. En OneDrive están en la carpeta `Aplicaciones\ReFix`.
