# NexoEmu - Guia de Integracion de RaptorNetwork en Eden

## Que es esto?

Este proyecto porta toda la funcionalidad de **RaptorNetwork** (del emulador RaptorCitrus, basado en yuzu 2021) al emulador **Eden** (fork moderno de yuzu que se sigue actualizando).

RaptorNetwork permite jugar online con el emulador de Switch, conectandose a servidores propios en vez de los de Nintendo.

---

## Estructura del proyecto

```
NexoEmu/
├── src/
│   ├── common/
│   │   ├── hardware_id.h          # [NUEVO] Generador de Hardware ID
│   │   └── hardware_id.cpp        # [NUEVO] Implementacion (Windows + Linux)
│   │
│   ├── core/
│   │   ├── online_initiator.h     # [NUEVO] Clase principal de conexion online
│   │   ├── online_initiator.cpp   # [NUEVO] Autenticacion, tokens, URL rewriting
│   │   └── hle/service/bcat/backend/
│   │       └── boxcat_raptor.h    # [NUEVO] Constantes BCAT para Raptor
│   │
│   ├── yuzu/
│   │   ├── online/
│   │   │   ├── types.h            # [NUEVO] Enumeraciones de estado online
│   │   │   ├── monitor.h          # [NUEVO] Widget de estado de conexion
│   │   │   ├── monitor.cpp        # [NUEVO] UI del monitor online
│   │   │   ├── notification_queue.h   # [NUEVO] Cola de notificaciones
│   │   │   └── notification_queue.cpp # [NUEVO] WebSocket notifications
│   │   └── configuration/
│   │       ├── configure_raptor_online.h   # [NUEVO] Config UI header
│   │       └── configure_raptor_online.cpp # [NUEVO] Config UI implementation
│   │
│   └── web_service/
│       (modificaciones via patches)
│
├── patches/
│   ├── 01_settings_raptor_token.patch     # Agregar raptor_token a Settings
│   ├── 02_web_backend_raptor_header.patch # Agregar R-HardwareId a web requests
│   ├── 03_bcat_boxcat_raptor.patch        # Redirigir BCAT a Raptor
│   ├── 04_nsd_raptor_resolver.patch       # Integrar URL resolver en NSD
│   ├── 05_core_system_online_initiator.patch  # Agregar OnlineInitiator a System
│   └── 06_cmake_integration.patch         # Agregar archivos al build
│
└── GUIA_INTEGRACION.md  (este archivo)
```

---

## Paso a paso para integrar en Eden

### Paso 1: Preparar el codigo fuente de Eden

```bash
# Clonar Eden
git clone https://git.eden-emu.dev/eden-emu/eden.git
cd eden

# Crear una rama para los cambios
git checkout -b feature/raptor-network
```

### Paso 2: Copiar archivos nuevos

```bash
# Copiar hardware_id
cp NexoEmu/src/common/hardware_id.h   eden/src/common/
cp NexoEmu/src/common/hardware_id.cpp eden/src/common/

# Copiar online_initiator
cp NexoEmu/src/core/online_initiator.h   eden/src/core/
cp NexoEmu/src/core/online_initiator.cpp eden/src/core/

# Copiar boxcat raptor header
cp NexoEmu/src/core/hle/service/bcat/backend/boxcat_raptor.h eden/src/core/hle/service/bcat/backend/

# Crear directorio online en yuzu
mkdir -p eden/src/yuzu/online/

# Copiar archivos online UI
cp NexoEmu/src/yuzu/online/types.h              eden/src/yuzu/online/
cp NexoEmu/src/yuzu/online/monitor.h            eden/src/yuzu/online/
cp NexoEmu/src/yuzu/online/monitor.cpp          eden/src/yuzu/online/
cp NexoEmu/src/yuzu/online/notification_queue.h eden/src/yuzu/online/
cp NexoEmu/src/yuzu/online/notification_queue.cpp eden/src/yuzu/online/

# Copiar configuracion
cp NexoEmu/src/yuzu/configuration/configure_raptor_online.h   eden/src/yuzu/configuration/
cp NexoEmu/src/yuzu/configuration/configure_raptor_online.cpp eden/src/yuzu/configuration/
```

### Paso 3: Aplicar patches (modificar archivos existentes de Eden)

Sigue las instrucciones en cada archivo de la carpeta `patches/` en orden:

1. **Patch 01** - `src/common/settings.h`: Agregar `raptor_token` a Settings
2. **Patch 02** - `src/web_service/web_backend.cpp`: Agregar header R-HardwareId
3. **Patch 03** - `src/core/hle/service/bcat/backend/`: Redirigir BCAT a Raptor
4. **Patch 04** - `src/core/hle/service/sockets/nsd.cpp`: Integrar URL resolver
5. **Patch 05** - `src/core/core.h` y `core.cpp`: Agregar OnlineInitiator al System
6. **Patch 06** - CMakeLists.txt: Agregar archivos al build

### Paso 4: Agregar dependencia httplib

Eden necesita cpp-httplib para las peticiones HTTPS a Raptor Network:

```bash
# Opcion A: Copiar de RaptorCitrus
mkdir -p eden/externals/httplib
cp RaptorCitrus/externals/httplib/httplib.h eden/externals/httplib/
```

Luego agregar al CMakeLists.txt de externals:
```cmake
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/httplib)
```

### Paso 5: Compilar

```bash
cd eden
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

---

## Conceptos clave para entender el codigo

### Como funciona la autenticacion con Raptor Network

1. El usuario obtiene un **token** de la pagina web de Raptor Network
2. Lo pega en la configuracion del emulador
3. El emulador genera un **Hardware ID** unico para esa maquina
4. Al conectarse, envia ambos al servidor en los headers HTTP:
   - `Authorization: Bearer {token}`
   - `R-HardwareId: {hardware_id}`
   - `R-ClientId: nexoemu`
5. El servidor responde con tokens de sesion para cada juego

### Como funciona el URL Rewriting

Nintendo Switch usa URLs como `*.nintendo.net` para sus servicios online.
Raptor Network intercepta estas URLs y las redirige a sus propios servidores:

1. Al conectarse, el emulador descarga una lista de "rewrites" del servidor
2. Cuando un juego intenta conectarse a `algo.nintendo.net`, el emulador busca en la lista
3. Si hay un rewrite, usa la URL de Raptor Network en su lugar
4. Si la URL contiene "nintendo" y no hay rewrite, la bloquea (redirige a 127.0.0.1)

### Archivos mas importantes

| Archivo | Que hace |
|---------|----------|
| `hardware_id.cpp` | Genera un ID de 48 chars unico por maquina |
| `online_initiator.cpp` | Maneja TODA la conexion con Raptor Network |
| `monitor.cpp` | Muestra el estado de conexion en la UI |
| `configure_raptor_online.cpp` | Pantalla de configuracion del token |

---

## Diferencias principales entre RaptorCitrus (2021) y Eden (2026)

| Aspecto | RaptorCitrus | Eden |
|---------|-------------|------|
| C++ Standard | C++17 | C++20 |
| Settings | `Setting<Type>` simple | `Setting<Type>` con linkage y categorias |
| Frontend | Qt5 | Qt5/Qt6 |
| Network | Solo Raptor Network | Room-based multiplayer + web service |
| Build | CMake basico | CMake + CPM package manager |
| Servicios HLE | Basicos | Completos (SSL backends, etc.) |

### Adaptaciones realizadas

- Settings usa el nuevo formato de Eden con `linkage` y `Category::WebService`
- Se accede al token via `Settings::values.raptor_token.GetValue()` en vez de
  `Settings::values.raptor_token` directo
- Se usan `[[maybe_unused]]` y otros atributos C++20
- Se adaptan los includes a la estructura de Eden
- Se agrega `client.set_connection_timeout(10)` a todas las conexiones httplib

---

## TODO (funcionalidades pendientes)

- [ ] Implementar conexion WebSocket real para notificaciones (requiere IXWebSocket)
- [ ] Portar el sistema de amigos completo (`friends.cpp/h`)
- [ ] Portar los delegates de usuario (`user_delegate.cpp/h`)
- [ ] Integrar con el sistema de multiplayer existente de Eden (Room/RoomMember)
- [ ] Agregar soporte para Android (Eden tiene frontend Android)
- [ ] Testing con un servidor Raptor Network real

---

## Recursos

- Eden Emulator: https://eden-emu.dev/
- Eden GitHub: https://github.com/eden-emulator
- Raptor Network: https://raptor.network/
