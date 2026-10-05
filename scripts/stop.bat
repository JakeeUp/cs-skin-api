@echo off
REM Stop both the API and frontend dev servers

echo [SkinAPI Dev] Stopping development services...
echo.

REM Kill the API server window by title
taskkill /FI "WINDOWTITLE eq SkinAPI API*" /T /F 2>nul
if errorlevel 1 (
    echo [SkinAPI Dev] API server not running or already stopped.
) else (
    echo [SkinAPI Dev] Stopped API server.
)

REM Kill the frontend server window by title
taskkill /FI "WINDOWTITLE eq SkinAPI WEB*" /T /F 2>nul
if errorlevel 1 (
    echo [SkinAPI Dev] Frontend server not running or already stopped.
) else (
    echo [SkinAPI Dev] Stopped frontend server.
)

echo.
echo [SkinAPI Dev] Services stopped.
echo.
