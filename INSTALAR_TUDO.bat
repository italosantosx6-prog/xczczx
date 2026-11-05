@echo off
chcp 437 >nul
title VRD ESP - Instalacao

cd /d "%~dp0"

net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

cls
echo.
echo  VRD ESP - Instalador Automatico
echo  ========================================
echo.

:: ==========================================
:: PASSO 1 - Git
:: ==========================================
echo [1/7] Verificando Git...
where git >nul 2>&1
if %errorLevel% equ 0 (
    echo [+] Git OK
) else (
    echo [*] Instalando Git...
    winget install --id Git.Git -e --silent --accept-package-agreements --accept-source-agreements
    if %errorLevel% neq 0 (
        echo [!] Falha no Git. Baixe em https://git-scm.com/download/win e instale manualmente.
        pause & exit /b 1
    )
    set "PATH=%PATH%;C:\Program Files\Git\cmd"
    echo [+] Git instalado
)

:: ==========================================
:: PASSO 2 - CMake
:: ==========================================
echo.
echo [2/7] Verificando CMake...
where cmake >nul 2>&1
if %errorLevel% equ 0 (
    echo [+] CMake OK
) else (
    echo [*] Instalando CMake...
    winget install --id Kitware.CMake -e --silent --accept-package-agreements --accept-source-agreements
    if %errorLevel% neq 0 (
        echo [!] Falha no CMake. Baixe em https://cmake.org/download/
        pause & exit /b 1
    )
    set "PATH=%PATH%;C:\Program Files\CMake\bin"
    echo [+] CMake instalado
)

:: ==========================================
:: PASSO 3 - Visual Studio Build Tools 2022
:: ==========================================
echo.
echo [3/7] Verificando Visual Studio Build Tools...
echo [*] Pode demorar 5-10 minutos...

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_PATH="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VS_PATH=%%i"
)

if defined VS_PATH (
    echo [+] Visual Studio encontrado
) else (
    echo [*] Instalando VS Build Tools 2022...
    winget install --id Microsoft.VisualStudio.2022.BuildTools -e --silent --accept-package-agreements --accept-source-agreements --override "--quiet --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --includeRecommended"
    if %errorLevel% neq 0 (
        echo [!] Falha no VS Build Tools.
        echo [!] Baixe manualmente: https://aka.ms/vs/17/release/vs_buildtools.exe
        echo [!] Marque: C++ build tools + Windows 11 SDK
        pause & exit /b 1
    )
    echo [+] VS Build Tools instalado
)

:: ==========================================
:: PASSO 4 - Windows Driver Kit
:: ==========================================
echo.
echo [4/7] Verificando WDK...
echo [*] Pode demorar mais 5 minutos...

set "WDK_CHECK=%ProgramFiles(x86)%\Windows Kits\10\Include"
if exist "%WDK_CHECK%" (
    echo [+] WDK encontrado
) else (
    echo [*] Instalando WDK...
    winget install --id Microsoft.WindowsWDK -e --silent --accept-package-agreements --accept-source-agreements
    if %errorLevel% neq 0 (
        echo [*] Tentando via download direto...
        powershell -NoProfile -Command "Invoke-WebRequest -Uri 'https://go.microsoft.com/fwlink/?linkid=2249371' -OutFile '%TEMP%\wdksetup.exe' -UseBasicParsing"
        start /wait "" "%TEMP%\wdksetup.exe" /quiet /norestart
    )
    echo [+] WDK instalado
)

:: ==========================================
:: PASSO 5 - ImGui
:: ==========================================
echo.
echo [5/7] Baixando ImGui...

set "IMGUI_DIR=%~dp0external\imgui"
if exist "%IMGUI_DIR%\imgui.h" (
    echo [+] ImGui ja existe
) else (
    set "PATH=%PATH%;C:\Program Files\Git\cmd"
    git clone https://github.com/ocornut/imgui.git --branch v1.91.0 --depth 1 "%IMGUI_DIR%"
    if %errorLevel% neq 0 (
        echo [!] Falha ao baixar ImGui. Verifique internet.
        pause & exit /b 1
    )
    echo [+] ImGui OK
)

:: ==========================================
:: PASSO 6 - Encontrar MSBuild
:: ==========================================
echo.
echo [6/7] Localizando compilador...

set "MSBUILD="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do set "MSBUILD=%%i"
)

if not defined MSBUILD (
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" (
        set "MSBUILD=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    )
)

if not defined MSBUILD (
    echo [!] MSBuild nao encontrado.
    echo [!] Reinicie o PC e rode este script novamente.
    pause & exit /b 1
)
echo [+] MSBuild encontrado

:: Configurar ambiente VC
set "VCVARS="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
)
if defined VCVARS if exist "%VCVARS%" call "%VCVARS%" >nul 2>&1

:: ==========================================
:: PASSO 7 - Compilar
:: ==========================================
echo.
echo [7/7] Compilando...

set "BASE=%~dp0"

:: Driver
echo [*] Compilando driver...
mkdir "%BASE%driver\build\Release" >nul 2>&1
mkdir "%BASE%driver\build\obj" >nul 2>&1
"%MSBUILD%" "%BASE%driver\vread.vcxproj" /p:Configuration=Release /p:Platform=x64 /verbosity:minimal /nologo

if %errorLevel% neq 0 (
    echo.
    echo [!] Falha no driver.
    echo [!] Certifique-se que o WDK esta instalado com extensao do Visual Studio.
    echo [!] Reinicie o PC apos instalar WDK e tente novamente.
    pause & exit /b 1
)
echo [+] vread.sys OK

:: Loader
echo [*] Compilando loader...
cmake -B "%BASE%loader\build" -G "Visual Studio 17 2022" -A x64 "%BASE%loader" -DCMAKE_BUILD_TYPE=Release >nul 2>&1
cmake --build "%BASE%loader\build" --config Release >nul 2>&1
if %errorLevel% neq 0 (
    echo [!] Falha no loader.
    pause & exit /b 1
)
echo [+] loader.exe OK

:: ESP
echo [*] Compilando ESP...
cmake -B "%BASE%cheat\build" -G "Visual Studio 17 2022" -A x64 "%BASE%cheat" -DCMAKE_BUILD_TYPE=Release >nul 2>&1
cmake --build "%BASE%cheat\build" --config Release >nul 2>&1
if %errorLevel% neq 0 (
    echo [!] Falha no ESP.
    pause & exit /b 1
)
echo [+] esp.exe OK

:: ==========================================
:: Montar pasta LAUNCH
:: ==========================================
echo.
echo [*] Montando pasta LAUNCH...

set "LAUNCH=%BASE%LAUNCH"
mkdir "%LAUNCH%" >nul 2>&1

copy /y "%BASE%driver\build\Release\vread.sys"       "%LAUNCH%\vread.sys"  >nul
copy /y "%BASE%loader\build\Release\loader.exe"      "%LAUNCH%\loader.exe" >nul
copy /y "%BASE%cheat\build\Release\esp.exe"          "%LAUNCH%\esp.exe"    >nul
copy /y "%BASE%RODAR.bat"                            "%LAUNCH%\RODAR.bat"  >nul

if exist "%BASE%rtcore64.sys"        copy /y "%BASE%rtcore64.sys"        "%LAUNCH%\rtcore64.sys" >nul
if exist "%BASE%loader\rtcore64.sys" copy /y "%BASE%loader\rtcore64.sys" "%LAUNCH%\rtcore64.sys" >nul

:: ==========================================
:: Aviso rtcore64
:: ==========================================
if not exist "%LAUNCH%\rtcore64.sys" (
    echo.
    echo ----------------------------------------
    echo  FALTA: rtcore64.sys
    echo.
    echo  1. Busque no Google: rtcore64.sys sha256 f6f11ad2
    echo  2. Baixe e coloque em:
    echo     %LAUNCH%\rtcore64.sys
    echo ----------------------------------------
)

:: Verificar Secure Boot
echo.
powershell -NoProfile -Command "try{$sb=Confirm-SecureBootUEFI 2>$null;if($sb){Write-Host '[AVISO] Secure Boot ATIVO - desative no BIOS antes de rodar' -ForegroundColor Yellow}else{Write-Host '[+] Secure Boot desativado - OK' -ForegroundColor Green}}catch{Write-Host '[?] Nao foi possivel verificar Secure Boot'}"

echo.
echo ========================================
echo  CONCLUIDO!
echo.
echo  1. Coloque rtcore64.sys na pasta LAUNCH\
echo  2. Clique duas vezes em LAUNCH\RODAR.bat
echo ========================================
echo.
explorer "%LAUNCH%"
pause
