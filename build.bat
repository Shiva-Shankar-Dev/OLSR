@echo off
echo Building OLSR Protocol...

if "%1"=="clean" (
    echo Cleaning build artifacts...
    del /q *.o olsr.exe 2>nul
    echo Clean complete.
    goto :eof
)

if "%1"=="help" (
    echo Usage:
    echo   build.bat        - Build OLSR protocol
    echo   build.bat clean  - Clean build artifacts  
    echo   build.bat help   - Show this help
    goto :eof
)

echo Compiling with full OLSR functionality...
gcc -Wall -Wextra -std=c99 -Iinclude src/*.c -o olsr.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✅ Build successful! Created olsr.exe
    echo Features enabled:
    echo   • Neighbor discovery via HELLO messages
    echo   • MPR selection algorithm  
    echo   • TC message flooding
    echo   • Global topology database
    echo   • Multi-hop routing calculation
    echo   • Duplicate detection
    echo.
) else (
    echo ❌ Build failed!
    exit /b 1
)