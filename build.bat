@echo off
setlocal

:: --- CONFIGURATION ---
set PROJECT_NAME=VolarusVanguard
set OUT_DIR=build\Release
set SRC_DIR=src
set INC_DIR=include
set LIB_DIR=lib

:: --- PREPARE DIRECTORIES ---
if not exist %OUT_DIR% mkdir %OUT_DIR%

:: --- COMPILER FLAGS ---
:: /O2: Optimization
:: /W3: Warning level 3
:: /MT: Static CRT (makes the EXE more standalone)
:: /std:c++20
set CXXFLAGS=/O2 /W3 /MT /EHsc /std:c++20 /DUNICODE /D_UNICODE /D_WINSOCK_DEPRECATED_NO_WARNINGS

:: --- INCLUDE PATHS ---
set INCLUDES=/I%INC_DIR% /I%INC_DIR%\imgui /Ideps\npcap\Include /Isrc

:: --- LIBRARIES ---
set LIBS=d3d11.lib d3dcompiler.lib dxgi.lib user32.lib gdi32.lib shell32.lib iphlpapi.lib ws2_32.lib wininet.lib wlanapi.lib %LIB_DIR%\WinDivert.lib %LIB_DIR%\wpcap.lib %LIB_DIR%\Packet.lib

:: --- SOURCE FILES ---
set SOURCES=%SRC_DIR%\main.cpp %SRC_DIR%\ArpEngine.cpp %SRC_DIR%\ShaperEngine.cpp %SRC_DIR%\SnifferEngine.cpp %SRC_DIR%\SpeedTestEngine.cpp %SRC_DIR%\SslStripEngine.cpp %SRC_DIR%\KarmaEngine.cpp %SRC_DIR%\RawDeauthEngine.cpp %SRC_DIR%\ProxyBridgeEngine.cpp %SRC_DIR%\imgui.cpp %SRC_DIR%\imgui_draw.cpp %SRC_DIR%\imgui_widgets.cpp %SRC_DIR%\imgui_tables.cpp %SRC_DIR%\imgui_impl_win32.cpp %SRC_DIR%\imgui_impl_dx11.cpp

:: --- FIND MSVC ---
:: Check for VS 2022
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo [ERROR] MSVC vcvars64.bat not found. Please ensure Visual Studio 2022 is installed.
    exit /b 1
)

:: --- BUILD GO PROXY ---
set "GO_BIN=go"
where go >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    if exist "C:\Program Files\Go\bin\go.exe" (
        set "GO_BIN=C:\Program Files\Go\bin\go.exe"
    )
)

echo [BUILD] Checking for Go compiler...
if not "%GO_BIN%"=="" (
    echo [BUILD] Compiling go-http-proxy-to-socks gohpts.exe...
    pushd deps\go-http-proxy-to-socks
    "%GO_BIN%" build -o ..\..\build\Release\gohpts.exe .\cmd\gohpts
    popd
) else (
    echo [WARNING] Go is not installed. gohpts.exe will not be built.
)

:: --- BUILD RESOURCES ---
echo [BUILD] Compiling Resources...
rc /nologo /fo %OUT_DIR%\resources.res src\resources.rc

:: --- BUILD ---
echo [BUILD] Compiling %PROJECT_NAME%...
cl %CXXFLAGS% %INCLUDES% %SOURCES% /Fe%OUT_DIR%\%PROJECT_NAME%.exe /link %LIBS% %OUT_DIR%\resources.res /SUBSYSTEM:WINDOWS

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed.
    exit /b %ERRORLEVEL%
)

echo [SUCCESS] Build complete: %OUT_DIR%\%PROJECT_NAME%.exe
copy %LIB_DIR%\WinDivert.dll %OUT_DIR%\WinDivert.dll
copy %LIB_DIR%\*.dll %OUT_DIR%\ 2>nul

endlocal
