@ECHO OFF
SETLOCAL EnableDelayedExpansion

ECHO ============================================================
ECHO  PackageHelper Native DLL Build Script
ECHO ============================================================
ECHO.

SET SCRIPT_DIR=%~dp0
SET SDK_PATH=%SCRIPT_DIR%..\..\OldUnreal-UT99-ACE-SDK
SET OUTPUT_DIR=%SCRIPT_DIR%Output
SET PH_DLL=PackageHelper_v15.dll

ECHO --- Checking prerequisites...
cmake --version >nul 2>&1
IF ERRORLEVEL 1 (
    ECHO [ERROR] CMake not found! Please install CMake and add it to PATH.
    EXIT /B 1
)

IF NOT EXIST "%SDK_PATH%" (
    ECHO [ERROR] SDK not found at: %SDK_PATH%
    EXIT /B 1
)

ECHO [OK] CMake found
ECHO [OK] SDK found at: %SDK_PATH%
ECHO.

REM ============================================================
REM Build x86 (32-bit) DLL
REM ============================================================
ECHO ============================================================
ECHO  Building x86 (32-bit) DLL
ECHO ============================================================

SET BUILD_DIR_X86=%SCRIPT_DIR%build-win-x86

ECHO --- Cleaning old build directory...
IF EXIST "%BUILD_DIR_X86%" rmdir /S /Q "%BUILD_DIR_X86%"
mkdir "%BUILD_DIR_X86%"

cd /d "%BUILD_DIR_X86%"

ECHO --- Configuring CMake (x86)...
cmake -A Win32 -DOLDUNREAL_SDK_PATH="%SDK_PATH%" ..
IF ERRORLEVEL 1 (
    ECHO [ERROR] CMake configuration failed for x86!
    GOTO BUILD_X64
)

ECHO --- Building (x86)...
cmake --build . --config Release
IF ERRORLEVEL 1 (
    ECHO [ERROR] Build failed for x86!
    GOTO BUILD_X64
)

IF NOT EXIST "Release\%PH_DLL%" (
    ECHO [ERROR] x86 DLL not found!
    GOTO BUILD_X64
)

ECHO --- Copying x86 DLL to output...
IF NOT EXIST "%OUTPUT_DIR%\System" mkdir "%OUTPUT_DIR%\System"
copy /Y "Release\%PH_DLL%" "%OUTPUT_DIR%\System\%PH_DLL%"
ECHO [OK] x86 DLL copied to %OUTPUT_DIR%\System\%PH_DLL%

SET X86_SUCCESS=1
ECHO.

:BUILD_X64
REM ============================================================
REM Build x64 (64-bit) DLL
REM ============================================================
ECHO ============================================================
ECHO  Building x64 (64-bit) DLL
ECHO ============================================================

SET BUILD_DIR_X64=%SCRIPT_DIR%build-win-x64

ECHO --- Cleaning old build directory...
IF EXIST "%BUILD_DIR_X64%" rmdir /S /Q "%BUILD_DIR_X64%"
mkdir "%BUILD_DIR_X64%"

cd /d "%BUILD_DIR_X64%"

ECHO --- Configuring CMake (x64)...
cmake -A x64 -DOLDUNREAL_SDK_PATH="%SDK_PATH%" ..
IF ERRORLEVEL 1 (
    ECHO [ERROR] CMake configuration failed for x64!
    GOTO SUMMARY
)

ECHO --- Building (x64)...
cmake --build . --config Release
IF ERRORLEVEL 1 (
    ECHO [ERROR] Build failed for x64!
    GOTO SUMMARY
)

IF NOT EXIST "Release\%PH_DLL%" (
    ECHO [ERROR] x64 DLL not found!
    GOTO SUMMARY
)

ECHO --- Copying x64 DLL to output...
IF NOT EXIST "%OUTPUT_DIR%\System64" mkdir "%OUTPUT_DIR%\System64"
copy /Y "Release\%PH_DLL%" "%OUTPUT_DIR%\System64\%PH_DLL%"
ECHO [OK] x64 DLL copied to %OUTPUT_DIR%\System64\%PH_DLL%

SET X64_SUCCESS=1
ECHO.

:SUMMARY
REM ============================================================
REM Summary
REM ============================================================
cd /d "%SCRIPT_DIR%"

ECHO ============================================================
ECHO  Build Summary
ECHO ============================================================
IF DEFINED X86_SUCCESS (
    ECHO   x86 ^(32-bit^): SUCCESS
) ELSE (
    ECHO   x86 ^(32-bit^): FAILED
)
IF DEFINED X64_SUCCESS (
    ECHO   x64 ^(64-bit^): SUCCESS
) ELSE (
    ECHO   x64 ^(64-bit^): FAILED
)
ECHO.

IF NOT DEFINED X86_SUCCESS GOTO SOME_FAILED
IF NOT DEFINED X64_SUCCESS GOTO SOME_FAILED

ECHO [OK] All builds completed successfully!
ECHO.
ECHO Output files:
IF EXIST "%OUTPUT_DIR%\System\%PH_DLL%" (
    FOR %%A IN ("%OUTPUT_DIR%\System\%PH_DLL%") DO ECHO   System\%PH_DLL% ^(%%~zA bytes^)
)
IF EXIST "%OUTPUT_DIR%\System64\%PH_DLL%" (
    FOR %%A IN ("%OUTPUT_DIR%\System64\%PH_DLL%") DO ECHO   System64\%PH_DLL% ^(%%~zA bytes^)
)
ECHO.
ECHO Output directory: %OUTPUT_DIR%
ECHO.
EXIT /B 0

:SOME_FAILED
ECHO [ERROR] Some builds failed!
EXIT /B 1
