@echo off
REM Build PDFTranView using the MSYS2/MinGW64 toolchain.
REM Prerequisites (installed automatically):
REM   MSYS2 at C:\msys64 with packages:
REM     mingw-w64-x86_64-qt5-base  qt5-tools  libmupdf  mujs  cmake  ninja

setlocal EnableDelayedExpansion

set MSYS2=C:\msys64
set MINGW64=%MSYS2%\mingw64
set PATH=%MINGW64%\bin;%MSYS2%\usr\bin;%PATH%

REM Verify tools
where cmake.exe >nul 2>&1 || (echo ERROR: cmake not found & exit /b 1)
where ninja.exe >nul 2>&1 || (echo ERROR: ninja not found & exit /b 1)
where gcc.exe   >nul 2>&1 || (echo ERROR: gcc not found  & exit /b 1)

echo.
echo ===  Configuring PDFTranView  ===
if not exist build_msys2 mkdir build_msys2
cmake -S PDFTranView -B build_msys2 ^
    -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_PREFIX_PATH=%MINGW64% ^
    -DCMAKE_C_COMPILER=%MINGW64%\bin\gcc.exe ^
    -DCMAKE_CXX_COMPILER=%MINGW64%\bin\g++.exe ^
    -DPKG_CONFIG_EXECUTABLE=%MINGW64%\bin\pkg-config.exe
if errorlevel 1 (echo CMake configure failed & exit /b 1)

echo.
echo ===  Building PDFTranView  ===
cmake --build build_msys2 --config Release -- -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 (echo Build failed & exit /b 1)

set EXE=build_msys2\PDFTranView.exe
if not exist "%EXE%" (echo ERROR: %EXE% not found & exit /b 1)

echo.
echo ===  Deploying Qt5 DLLs  ===
REM windeployqt collects the three Qt DLLs and the platform plugin
set WDQT=%MINGW64%\bin\windeployqt-qt5.exe
if exist "%WDQT%" (
    "%WDQT%" --release --dir build_msys2 "%EXE%"
) else (
    echo WARNING: windeployqt-qt5 not found; install mingw-w64-x86_64-qt5-tools
)

echo.
echo ===  Deploying runtime DLLs  ===
REM Copy all MinGW/MuPDF DLLs that the exe and its deps need.
REM Uses a Bash script via MSYS2 to walk the dependency tree.
%MSYS2%\usr\bin\bash.exe -lc "
BUILD='/d/dev_cch/KGH_works/pdf-translator/build_msys2'
MINGW='/c/msys64/mingw64/bin'
SKIP='kernel32|msvcrt|user32|gdi32|advapi32|shell32|ole32|ws2_32|ntdll|comctl32|comdlg32|winspool|imm32|winmm|opengl32|dwmapi|uxtheme|version|cfgmgr32|setupapi|wtsapi32|rpcrt4|dwrite|oleaut32|crypt32|secur32|bcrypt|usp10|uuid|shlwapi|mpr|netapi32|userenv|d3d11|dxgi'

declare -A seen
added=1
while [ \$added -ne 0 ]; do
  added=0
  for f in \"\$BUILD\"/*.dll \"\$BUILD/PDFTranView.exe\"; do
    [ -f \"\$f\" ] || continue
    while IFS= read -r dll; do
      dll_l=\$(echo \"\$dll\" | tr '[:upper:]' '[:lower:]')
      echo \"\$dll_l\" | grep -qiE \"\$SKIP\" && continue
      [ -n \"\${seen[\$dll_l]}\" ] && continue
      seen[\$dll_l]=1
      src=\"\$MINGW/\$dll\"
      dst=\"\$BUILD/\$dll\"
      if [ -f \"\$src\" ] && [ ! -f \"\$dst\" ]; then
        cp \"\$src\" \"\$dst\" && echo \"  Copied: \$dll\"
        added=1
      fi
    done < <(objdump -p \"\$f\" 2>/dev/null | grep 'DLL Name' | awk '{print \$3}')
  done
done
echo '  Runtime DLL deploy complete.'
"

REM Copy Qt5 platform plugin (windeployqt may have already done this)
if not exist build_msys2\platforms mkdir build_msys2\platforms
if not exist build_msys2\platforms\qwindows.dll (
    copy "%MINGW64%\share\qt5\plugins\platforms\qwindows.dll" "build_msys2\platforms\" >nul
    echo   Copied: platforms\qwindows.dll
)

echo.
echo ===  Done  ===
echo Output : %EXE%
echo Run via: build_msys2\PDFTranView.exe
endlocal
