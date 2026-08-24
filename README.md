<div align="center">

# NeXo-emu

**A Nintendo Switch emulator with the NeXo Network online layer built in.**
Play supported titles online against your own NeXo server — no hosts-file edits, no external DNS, no manual certificate tricks.

[![License](https://img.shields.io/badge/license-GPL--2.0--or--later-blue)]()
[![Status](https://img.shields.io/badge/status-work%20in%20progress-yellow)]()
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-informational)]()

[English](#english) · [Español](#español)

</div>

---

<a name="english"></a>
## English

### What is NeXo-emu?

NeXo-emu is a build of the Citron emulator (in the yuzu / Citra lineage) with one thing added on top: a first-class client for **[NeXo Network](https://nexonetwork.space)**, the open-source, community-run alternative to the console's official online services. Where a normal setup would need you to edit a hosts file, run a DNS proxy and disable certificate checks by hand, NeXo-emu does all of that internally — you enter a server address, sign in, and play.

It is meant for **learning, preservation and experimentation**. It ships no games, no firmware and no keys; you bring your own dumped content.

### Online, at a glance

| Area | What works |
| --- | --- |
| **Sign-in** | Browser-based OAuth on loopback — the emulator never handles your password |
| **Redirection** | Official online hostnames are resolved to your NeXo server, internally |
| **TLS** | Handshake completed against the NeXo certificate, no manual bypass needed |
| **Authentication** | Device/account tickets and secure-server entry |
| **Matchmaking** | Session join and NAT traversal between peers |
| **Presence** | Online status published on sign-in and when a game starts/stops |
| **Profile** | Avatar and nickname synced from your NeXo account |

> **Heads-up:** online support is young and only lightly tested. Expect rough edges. Bug reports with a log attached are worth ten without one.

### Quick start

1. Grab a release build, or compile it yourself (see *Building*).
2. Open **Settings → Network** and turn on the NeXo network layer.
3. In the same panel, set the **server address** to your NeXo server (leave the default empty field pointing wherever your server lives).
4. Sign in through the browser window that opens.
5. Launch a supported game and enter its online mode.

### Building

NeXo-emu builds exactly like upstream Citron; nothing in the online layer changes the toolchain. In short:

- **Windows:** Visual Studio 2022 (or newer) with the C++ workload, CMake and Ninja. The CI workflow under `.github/workflows/` is the reference build.
- **Linux:** a recent Clang or GCC, CMake, Ninja and the Qt 6 / Vulkan development packages.

Dependencies are fetched automatically at configure time. The full, always-up-to-date steps live in [`docs/`](docs/).

### Configuration

The server the emulator talks to is a plain setting — you never recompile to change it:

- **Settings → Network** holds the NeXo toggle, the **server address** and the **NAT server address**.
- For scripting or CI, the equivalent environment overrides are read at startup if present.

Because those requests carry your account token, the API override only accepts a loopback address or an HTTPS address on your own NeXo domain.

### When something breaks

Attach a log. On Linux the log lives at `~/.local/share/citron/log/`, on Windows under the emulator's `log` folder. For network problems specifically, raise the log detail on the networking and SSL modules before you reproduce the issue, and include the on-screen error code if the game showed one.

### Credits & license

NeXo-emu is a **modified version** of the Citron emulator, itself derived from yuzu and Citra. All original copyright headers are kept intact. NeXo Network's changes are limited to the online client and its integration. See [`NOTICE.md`](NOTICE.md) for attribution and [`LICENSE`](LICENSE) for the full terms.

Released under **GPL-2.0-or-later**.

### Legal

NeXo-emu is an **educational, non-profit** project. It is not affiliated with, endorsed by, or associated with Nintendo or any other company, and it distributes no copyrighted material. You are responsible for owning the content you use. Use of this software is at your own risk.

---

<a name="español"></a>
## Español

### ¿Qué es NeXo-emu?

NeXo-emu es una compilación del emulador Citron (de la familia yuzu / Citra) con una cosa añadida encima: un cliente completo para **[NeXo Network](https://nexonetwork.space)**, la alternativa de código abierto y comunitaria a los servicios online oficiales de la consola. Donde una configuración normal te obligaría a editar un fichero hosts, montar un proxy de DNS y desactivar a mano la verificación de certificados, NeXo-emu hace todo eso por dentro: pones una dirección de servidor, inicias sesión y juegas.

Está pensado para **aprender, conservar y experimentar**. No incluye juegos, ni firmware, ni claves; el contenido lo aportas tú desde tu propia consola.

### El online, de un vistazo

| Área | Qué funciona |
| --- | --- |
| **Inicio de sesión** | OAuth en el navegador sobre loopback — el emulador nunca ve tu contraseña |
| **Redirección** | Los dominios online oficiales se resuelven a tu servidor NeXo, por dentro |
| **TLS** | Handshake completo contra el certificado de NeXo, sin trucos manuales |
| **Autenticación** | Tickets de dispositivo/cuenta y entrada al servidor seguro |
| **Emparejamiento** | Unión a sesión y travesía de NAT entre jugadores |
| **Presencia** | Estado en línea al iniciar sesión y al arrancar/cerrar un juego |
| **Perfil** | Avatar y apodo sincronizados desde tu cuenta NeXo |

> **Aviso:** el soporte online es reciente y está poco probado. Habrá fallos. Un reporte de bug con un log adjunto vale por diez sin él.

### Empezar rápido

1. Descarga una *release*, o compílala tú mismo (ver *Compilar*).
2. Abre **Settings → Network** y activa la capa de red de NeXo.
3. En ese mismo panel, pon la **dirección del servidor** apuntando a tu servidor NeXo (el campo viene vacío por defecto).
4. Inicia sesión en la ventana del navegador que se abre.
5. Arranca un juego compatible y entra en su modo online.

### Compilar

NeXo-emu se compila igual que el Citron original; la capa online no cambia nada del *toolchain*. En resumen:

- **Windows:** Visual Studio 2022 (o superior) con el paquete de C++, CMake y Ninja. El *workflow* de CI en `.github/workflows/` es la referencia de compilación.
- **Linux:** un Clang o GCC reciente, CMake, Ninja y los paquetes de desarrollo de Qt 6 / Vulkan.

Las dependencias se descargan solas al configurar. Los pasos completos y siempre actualizados están en [`docs/`](docs/).

### Configuración

El servidor con el que habla el emulador es un simple ajuste — no recompilas para cambiarlo:

- **Settings → Network** tiene el interruptor de NeXo, la **dirección del servidor** y la **dirección del servidor de NAT**.
- Para scripts o CI, se leen al arrancar las variables de entorno equivalentes si están definidas.

Como esas peticiones llevan el token de tu cuenta, la sobreescritura de la API solo acepta una dirección de loopback o una dirección HTTPS de tu propio dominio NeXo.

### Cuando algo falla

Adjunta un log. En Linux está en `~/.local/share/citron/log/`, en Windows en la carpeta `log` del emulador. Para problemas de red, sube el nivel de detalle del log en los módulos de red y SSL antes de reproducir el fallo, e incluye el código de error que muestre el juego si lo hay.

### Créditos y licencia

NeXo-emu es una **versión modificada** del emulador Citron, que a su vez deriva de yuzu y Citra. Se conservan intactas todas las cabeceras de copyright originales. Los cambios de NeXo Network se limitan al cliente online y su integración. Consulta [`NOTICE.md`](NOTICE.md) para la atribución y [`LICENSE`](LICENSE) para los términos completos.

Publicado bajo **GPL-2.0-or-later**.

### Aviso legal

NeXo-emu es un proyecto **educativo y sin ánimo de lucro**. No está afiliado, respaldado ni asociado con Nintendo ni ninguna otra empresa, y no distribuye ningún material con derechos de autor. Eres responsable de poseer el contenido que utilices. El uso de este software es responsabilidad tuya.

---

<div align="center">
<sub>NeXo Network · Open Source · GPL-2.0-or-later</sub>
</div>
