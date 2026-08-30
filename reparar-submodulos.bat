@echo off
setlocal enabledelayedexpansion
title NeXo-Emu - Reparar submodulos

REM ================================================================
REM   NeXo-Emu : recrea los punteros (gitlinks) de los submodulos
REM   que se perdieron al rehacer el historial con rama huerfana,
REM   commitea, hace push (opcional) y dice que falta.
REM ================================================================

set "REPO=C:\Users\Jous\Documents\NeXo\NeXo-emu-build"
cd /d "%REPO%" 2>nul || (echo [ERROR] No existe la carpeta: %REPO% & pause & exit /b 1)

echo ==================================================
echo   NeXo-Emu : reparar submodulos
echo   Carpeta: %REPO%
echo ==================================================
echo.

REM ---- Comprobaciones previas ----
where git >nul 2>&1 || (echo [ERROR] git no esta en el PATH. Instala Git para Windows. & pause & exit /b 1)
if not exist ".gitmodules" (echo [ERROR] No hay .gitmodules aqui. Repo equivocado? & pause & exit /b 1)
if not exist ".git" (echo [ERROR] Esto no es un repo git. & pause & exit /b 1)

echo [1/5] Recreando el puntero de cada submodulo (git ls-remote + update-index)...
echo.
set /a OK=0
set /a FAIL=0
set "FALTAN="

for /f "tokens=1,2" %%A in ('git config -f .gitmodules --get-regexp url') do (
    set "key=%%A"
    set "url=%%B"
    set "name=!key:submodule.=!"
    set "name=!name:.url=!"
    set "sp="
    for /f "delims=" %%P in ('git config -f .gitmodules --get "submodule.!name!.path"') do set "sp=%%P"

    set "sha="
    for /f "tokens=1" %%S in ('git ls-remote "!url!" HEAD 2^>nul') do set "sha=%%S"

    if defined sha (
        git update-index --add --cacheinfo 160000,!sha!,"!sp!" >nul 2>&1
        if !errorlevel! equ 0 (
            echo    [OK]      !sp!
            set /a OK+=1
        ) else (
            echo    [FALLO]   !sp!  ^(no se pudo escribir el puntero^)
            set /a FAIL+=1
            set "FALTAN=!FALTAN! !sp!"
        )
    ) else (
        echo    [SIN RED] !sp!  ^(!url!^)
        set /a FAIL+=1
        set "FALTAN=!FALTAN! !sp!"
    )
)

echo.
echo [2/5] Resumen: !OK! punteros OK, !FAIL! con problemas.

set "COUNT=0"
for /f %%C in ('git ls-files --stage externals ^| find /c "160000"') do set "COUNT=%%C"
echo        Punteros de submodulo en el indice: !COUNT!  (se esperan 23)
echo.

REM ---- Commit ----
echo [3/5] Creando commit...
git diff --cached --quiet
if !errorlevel! equ 0 (
    echo        No hay cambios nuevos que commitear ^(ya estaba hecho^).
) else (
    git commit -m "Restaurar submodulos (gitlinks) que faltaban tras el fork huerfano"
)
echo.

REM ---- Push (opcional) ----
echo [4/5] Remotos configurados:
git remote -v
echo.
choice /c SN /m "Hacer git push ahora"
if !errorlevel! equ 1 (
    echo        Subiendo...
    git push
) else (
    echo        Push omitido. Cuando quieras:  git push
)
echo.

REM ---- Diagnostico final ----
echo [5/5]
echo ==================================================
echo   QUE FALTA - diagnostico final
echo ==================================================
if defined FALTAN (
    echo   Estos submodulos NO se resolvieron ^(revisa conexion/URL^):
    for %%X in (!FALTAN!) do echo      - %%X
) else (
    echo   Todos los submodulos tienen su puntero correcto.
)
echo.
echo   Las carpetas de externals\ estan VACIAS en local: es normal.
echo     - GitHub Actions las descarga solo con "submodules: recursive"
echo       (ya esta en tus 4 .yml), asi que el proximo build deberia pasar.
echo     - Para COMPILAR EN TU PC, ejecuta ademas (baja varios GB):
echo           git submodule update --init --recursive
echo.
echo   IMPORTANTE: tu "origin" es Forgejo (git.joustech.space).
echo   El build corre en el ESPEJO de GitHub (NeXoNetwork/NeXo-Emu).
echo   Asegurate de que este commit llega TAMBIEN a GitHub
echo   (si Forgejo no lo espeja solo, haz push al remoto de GitHub).
echo.
pause
endlocal
