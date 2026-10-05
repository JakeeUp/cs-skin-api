@echo off
REM Development server launcher for SkinAPI
REM Builds the C++ API and starts both API and frontend dev server

setlocal enabledelayedexpansion

REM Get the project root (parent of scripts directory)
for %%I in ("%~dp0..") do set "PROJECT_ROOT=%%~fI"

REM Set PATH to include MSYS2 ucrt64 bin directory for libcurl and build tools
set "PATH=C:\msys64\ucrt64\bin;!PATH!"

echo.
echo [SkinAPI Dev] Starting development environment...
echo [SkinAPI Dev] Project root: !PROJECT_ROOT!
echo.

REM Check if build directory exists; if not, build
if not exist "!PROJECT_ROOT!\build" (
    echo [SkinAPI Dev] Build directory not found, creating and building...
    mkdir "!PROJECT_ROOT!\build"
    pushd "!PROJECT_ROOT!\build"
    cmake .. -G "MinGW Makefiles"
    if errorlevel 1 (
        echo [SkinAPI Dev] CMake configuration failed.
        popd
        exit /b 1
    )
    cmake --build . -j 8
    if errorlevel 1 (
        echo [SkinAPI Dev] Build failed.
        popd
        exit /b 1
    )
    popd
) else (
    REM Build exists; rebuild to pick up changes
    echo [SkinAPI Dev] Building...
    pushd "!PROJECT_ROOT!\build"
    cmake --build . -j 8
    if errorlevel 1 (
        echo [SkinAPI Dev] Build failed.
        popd
        exit /b 1
    )
    popd
)

echo [SkinAPI Dev] Build complete. Starting services...
echo.

REM Start the API server in a new window with a specific title
start "SkinAPI API" /d "!PROJECT_ROOT!" cmd /k "!PROJECT_ROOT!\build\cs-skin-api.exe"

REM Give the API a moment to start
timeout /t 1 /nobreak

REM Start the Python HTTP server in a new window with a specific title
start "SkinAPI WEB" /d "!PROJECT_ROOT!" cmd /k "python -m http.server 5500"

echo.
echo [SkinAPI Dev] Services started:
echo   - API:       http://127.0.0.1:8080
echo   - Frontend:  http://127.0.0.1:5500
echo.
echo [SkinAPI Dev] Tip: To stop both services, run 'scripts\stop.bat' or close the windows.
echo.

endlocal
