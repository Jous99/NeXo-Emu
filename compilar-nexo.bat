@echo off
setlocal enabledelayedexpansion
title NeXo-Emu - Comprobar dependencias y compilar

REM ================================================================
REM  NeXo-Emu (fork de Citron) - build Windows clang-cl
REM  1) Comprueba dependencias y dice que falta
REM  2) Prepara los submodulos
REM  3) Configura y compila:
REM       build-clangtron-windows.sh setup --compiler clang-cl
REM       build-clangtron-windows.sh use  --compiler clang-cl --pgo none --lto none
REM ================================================================

set "REPO=C:\Users\Jous\Documents\NeXo\NeXo-emu-build"
cd /d "%REPO%" 2>nul || (echo [ERROR] No existe la carpeta: %REPO% & pause & exit /b 1)
if not exist "build-clangtron-windows.sh" (echo [ERROR] No encuentro build-clangtron-windows.sh. Repo equivocado? & pause & exit /b 1)

echo ==================================================
echo   NeXo-Emu  -  dependencias + build (clang-cl)
echo   Carpeta: %REPO%
echo ==================================================
echo.
echo [1/4] Comprobando dependencias...
echo.

set /a MISSING=0

REM ---------- git ----------
where git >nul 2>&1
if !errorlevel! equ 0 (
    echo    [OK]     git
) else (
    echo    [FALTA]  git  -^>  https://git-scm.com/download/win
    set /a MISSING+=1
)

REM ---------- MSYS2 ----------
set "MSYS2_PATH="
for %%P in ("C:\msys64" "C:\msys2" "%USERPROFILE%\msys64" "D:\msys64" "D:\msys2") do (
    if not defined MSYS2_PATH if exist "%%~P\usr\bin\bash.exe" set "MSYS2_PATH=%%~P"
)
if defined MSYS2_PATH (
    echo    [OK]     MSYS2  ^(!MSYS2_PATH!^)
) else (
    echo    [FALTA]  MSYS2  -^>  https://www.msys2.org/
    set /a MISSING+=1
)

REM ---------- Visual Studio 2022 + componente clang-cl ----------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_CLANG="
if exist "%VSWHERE%" (
    for /f "delims=" %%I in ('"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath 2^>nul') do set "VS_CLANG=%%I"
)
if defined VS_CLANG (
    echo    [OK]     Visual Studio 2022 con clang-cl
) else (
    echo    [FALTA]  Visual Studio 2022 + "C++ Clang tools for Windows"
    echo             En Visual Studio Installer: carga "Desktop development with C++"
    echo             y marca "C++ Clang Compiler for Windows".
    echo             https://visualstudio.microsoft.com/downloads/
    set /a MISSING+=1
)

REM ---------- Python nativo de Windows ----------
set "PY_OK="
for /f "delims=" %%I in ('where python 2^>nul') do (
    echo %%I | find /i "WindowsApps" >nul || set "PY_OK=%%I"
)
if defined PY_OK (
    echo    [OK]     Python  ^(!PY_OK!^)
) else (
    echo    [FALTA]  Python 3  -^>  https://www.python.org/downloads/  ^(marca "Add to PATH"^)
    set /a MISSING+=1
)

REM ---------- Strawberry Perl ----------
if exist "C:\Strawberry\perl\bin\perl.exe" (
    echo    [OK]     Strawberry Perl
) else (
    echo    [FALTA]  Strawberry Perl  -^>  https://strawberryperl.com/
    echo             ^(lo usa el build para compilar OpenSSL^)
    set /a MISSING+=1
)

REM ---------- Vulkan SDK (opcional) ----------
if defined VULKAN_SDK (
    echo    [OK]     Vulkan SDK  ^(!VULKAN_SDK!^)
) else (
    echo    [aviso]  Vulkan SDK no detectado ^(opcional; suele bajarse solo^).
    echo             Si el build se queja: https://vulkan.lunarg.com/sdk/home#windows
)

echo.
if !MISSING! gtr 0 (
    echo ==================================================
    echo   Faltan !MISSING! dependencia^(s^) OBLIGATORIA^(s^).
    echo   Instala lo marcado como [FALTA], reinicia la terminal
    echo   y vuelve a ejecutar este .bat.
    echo ==================================================
    echo.
    pause
    exit /b 1
)
echo    Todas las dependencias obligatorias estan presentes.
echo.

REM ---------- [2/4] Submodulos ----------
echo [2/4] Preparando submodulos...
set "SDL_EMPTY=1"
if exist "externals\SDL\CMakeLists.txt" set "SDL_EMPTY=0"
if "!SDL_EMPTY!"=="0" (
    echo    Submodulos ya presentes.
) else (
    echo    Descargando submodulos ^(git submodule update --init --recursive^)...
    echo    OJO: son varios GB, puede tardar un buen rato.
    git submodule update --init --recursive
    if !errorlevel! neq 0 (
        echo.
        echo    [ERROR] No se pudieron inicializar los submodulos.
        echo    Probablemente faltan los "gitlinks" en el commit.
        echo    Ejecuta primero  reparar-submodulos.bat  y vuelve aqui.
        echo.
        pause
        exit /b 1
    )
)
echo.

REM ---------- ruta POSIX del repo (comillas escapadas ^" para MSYS2) ----------
set "MSYS_SOURCE="
for /f "delims=" %%P in ('^""%MSYS2_PATH%\usr\bin\cygpath.exe" "%REPO%"^"') do set "MSYS_SOURCE=%%P"
if not defined MSYS_SOURCE (
    echo    [ERROR] cygpath no pudo convertir la ruta del repo.
    pause
    exit /b 1
)

REM ---------- [3/4] Confirmar ----------
echo [3/4] Todo listo para compilar.
echo    Se ejecutara ^(bajo MSYS2 CLANG64^):
echo      setup --compiler clang-cl
echo      use   --compiler clang-cl --pgo none --lto none
echo    La PRIMERA vez descarga Qt/FFmpeg/etc y compila: puede tardar 1-2 h
echo    y necesita ~30-40 GB libres en disco.
echo.
choice /c SN /m "Empezar la compilacion ahora"
if !errorlevel! neq 1 (
    echo    Cancelado. Cuando quieras, vuelve a ejecutar este .bat.
    pause
    exit /b 0
)

REM ---------- [4/4] Build ----------
echo.
echo [4/4] Compilando... ^(deja la ventana abierta^)
echo.
"%MSYS2_PATH%\usr\bin\env.exe" MSYSTEM=CLANG64 ^
  "%MSYS2_PATH%\usr\bin\bash.exe" --login -c ^
  "cd '%MSYS_SOURCE%' && chmod +x build-clangtron-windows.sh && ./build-clangtron-windows.sh setup --compiler clang-cl && ./build-clangtron-windows.sh use --compiler clang-cl --pgo none --lto none"
set "BUILD_RC=!errorlevel!"

echo.
echo ==================================================
if "!BUILD_RC!"=="0" (
    echo   [OK] Compilacion terminada.
    echo   Busca el ejecutable citron.exe / nexo.exe dentro de:
    echo       %REPO%\build\clang-cl
    echo   ^(carpeta bin\ o similar^)
) else (
    echo   [ERROR] La compilacion fallo ^(codigo !BUILD_RC!^).
    echo   Copia las ultimas lineas rojas [ERROR] de arriba y me las pasas.
)
echo ==================================================
echo.
pause
endlocal
