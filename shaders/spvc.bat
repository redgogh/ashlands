@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
echo [spvc] script dirname: %SCRIPT_DIR%
cd /d "%SCRIPT_DIR%"

for %%f in (*.vert *.frag) do (
    if exist "%%f" (
        echo [spvc] compiling %%f ...
        glslangValidator -V "%%f" -o "%%f.spv"
    )
)

if not exist "..\cmake-build-debug" mkdir "..\cmake-build-debug"
move *.spv ..\cmake-build-debug
