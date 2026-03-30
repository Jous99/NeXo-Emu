# Guía de Compilación — NeXo Emulator (Basado en yuzu 2021)
> Windows 10/11 + Visual Studio

---

## ¿Por qué falla CMake configure?

El proyecto necesita varias dependencias externas. Si CMake no las encuentra, falla. Los problemas más comunes son:

1. **Conan no instalado** (gestor de paquetes que descarga fmt, OpenSSL, MbedTLS, etc.)
2. **Qt5 no encontrado** (la interfaz gráfica del emulador)
3. **Visual Studio 2022** (las Qt precompiladas del proyecto solo soportan VS2017/VS2019)

---

## PASO 1 — Requisitos previos

### 1.1 Visual Studio
Necesitas **Visual Studio 2019** (recomendado) o **2022**.
Instala con los siguientes componentes:
- `Desarrollo para el escritorio con C++`
- `Herramientas de compilación de C++ de Windows`
- SDK de Windows 10/11

Descarga: https://visualstudio.microsoft.com/es/vs/community/

### 1.2 CMake 3.15 o superior
Descarga e instala: https://cmake.org/download/
Durante la instalación, marca **"Add CMake to the PATH"**

### 1.3 Python 3 + Conan (MUY IMPORTANTE)
Conan descarga automáticamente las librerías que faltan.

```powershell
# Instala Python desde https://www.python.org/downloads/
# Luego abre PowerShell como Administrador y ejecuta:
pip install conan==1.65.0
```

> ⚠️ IMPORTANTE: Usa **Conan versión 1.x** (no la versión 2). El proyecto fue escrito para Conan 1.

### 1.4 Qt5 (solo si usas Visual Studio 2022)
Si tienes VS2022, las Qt precompiladas del proyecto no funcionan. Instala Qt5 manualmente:
1. Ve a: https://www.qt.io/download-qt-installer
2. Crea una cuenta gratuita y descarga el instalador
3. Instala **Qt 5.15.2** → selecciona `MSVC 2019 64-bit`

---

## PASO 2 — Fix del proyecto (necesario)

El proyecto tiene un pequeño error en `externals/CMakeLists.txt` que hace referencia a una carpeta de MbedTLS que no existe. Hay que corregirlo.

Abre el archivo `externals/CMakeLists.txt` y busca la línea:
```cmake
target_include_directories(ixwebsocket PRIVATE ./mbedtls/include)
```
**Elimina o comenta esa línea** (pon `#` delante):
```cmake
# target_include_directories(ixwebsocket PRIVATE ./mbedtls/include)
```

---

## PASO 3 — Configurar CMake

Abre **"Símbolo del sistema para desarrolladores de VS 2019/2022"** (búscalo en el menú inicio).

### Si tienes Visual Studio 2019:
```cmd
cd C:\ruta\a\NeXo1-emu

cmake -B build -S . ^
  -DCMAKE_BUILD_TYPE=Release ^
  -A x64 ^
  -DENABLE_QT=ON ^
  -DENABLE_SDL2=ON ^
  -DYUZU_USE_BUNDLED_QT=ON ^
  -DYUZU_USE_BUNDLED_SDL2=ON ^
  -DENABLE_WEB_SERVICE=ON ^
  -DYUZU_ENABLE_BOXCAT=ON
```

### Si tienes Visual Studio 2022:
```cmd
cd C:\ruta\a\NeXo1-emu

cmake -B build -S . ^
  -DCMAKE_BUILD_TYPE=Release ^
  -A x64 ^
  -DENABLE_QT=ON ^
  -DENABLE_SDL2=ON ^
  -DYUZU_USE_BUNDLED_QT=OFF ^
  -DYUZU_USE_BUNDLED_SDL2=OFF ^
  -DQt5_DIR="C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5" ^
  -DENABLE_WEB_SERVICE=ON ^
  -DYUZU_ENABLE_BOXCAT=ON
```
> Ajusta la ruta de Qt5 según donde lo instalaste.

---

## PASO 4 — Compilar

Una vez que CMake configure sin errores:

```cmd
cmake --build build --config Release --parallel
```

Esto puede tardar **entre 30 minutos y 2 horas** dependiendo de tu CPU.

El ejecutable final estará en:
```
build\bin\Release\yuzu.exe
```

---

## PASO 5 — Errores comunes y soluciones

| Error | Solución |
|-------|----------|
| `Could not find Conan` | Instala Conan 1.x con `pip install conan==1.65.0` |
| `No bundled Qt binaries for your toolchain` | Estás en VS2022, usa `-DYUZU_USE_BUNDLED_QT=OFF` y pasa la ruta de Qt |
| `Could NOT find OpenSSL` | Conan lo instalará automáticamente. Si falla, instala con `choco install openssl` |
| `Could NOT find MbedTLS` | Conan lo instalará. Si aún falla, asegúrate de estar en Conan 1.x |
| `cmake: mbedtls/include not found` | Aplica el fix del PASO 2 |
| `fatal error C1083: Cannot open include file` | Alguna dependencia de Conan no se instaló. Borra la carpeta `build/` y repite desde el PASO 3 |
| Error de submodulos git | El proyecto incluye los externales directamente (no git submodules). Ignora este mensaje |

---

## Sobre el futuro: portar a Eden

Eden es un fork activo de yuzu. Para portar las modificaciones de red de RaptorNetwork a Eden necesitarás:

1. **Clonar Eden**: `git clone https://git.eden-emu.dev/eden-emu/eden`
2. **Identificar los archivos modificados** en tu NeXo:
   - `src/core/online_initiator.cpp` (dominios de red)
   - `src/core/hle/service/bcat/backend/boxcat.cpp` (BCAT hostname)
   - `src/common/hardware_id.cpp` (Raptor hardware ID)
   - `src/common/settings.h` (raptor_token)
   - `src/yuzu/configuration/config.cpp` (URL de API y token)
   - `src/web_service/` (backend HTTP completo)
3. **Hacer cherry-pick o patch** de esos archivos sobre el código de Eden
4. Eden es más moderno (C++20, mejor Vulkan, soporte de Home Menu) pero la arquitectura de red es similar

---

## Resumen rápido

```
1. Instalar: Visual Studio 2019/2022, CMake, Python, Conan 1.x
2. Fix: comentar línea "mbedtls/include" en externals/CMakeLists.txt
3. Abrir: Developer Command Prompt de Visual Studio
4. Ejecutar: cmake -B build -S . -A x64 [opciones según tu VS]
5. Compilar: cmake --build build --config Release --parallel
```
