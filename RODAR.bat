@echo off
chcp 437 >nul
title VRD ESP

cd /d "%~dp0"

net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

cls
echo.
echo  VRD ESP v2.0
echo  ========================================
echo.

:: Verificar arquivos
if not exist "loader.exe"   echo [!] loader.exe nao encontrado - rode INSTALAR_TUDO.bat & pause & exit /b
if not exist "esp.exe"      echo [!] esp.exe nao encontrado - rode INSTALAR_TUDO.bat & pause & exit /b
if not exist "vread.sys"    echo [!] vread.sys nao encontrado - rode INSTALAR_TUDO.bat & pause & exit /b
if not exist "rtcore64.sys" echo [!] rtcore64.sys nao encontrado - coloque na pasta LAUNCH\ & pause & exit /b

:: Fechar Valorant se aberto
tasklist 2>nul | find /I "VALORANT-Win64" >nul
if %errorLevel% equ 0 (
    echo [*] Fechando Valorant...
    taskkill /F /IM "VALORANT-Win64-Shipping.exe" >nul 2>&1
    taskkill /F /IM "VALORANT.exe" >nul 2>&1
    timeout /t 3 >nul
)

:: Verificar se driver ja carregado
powershell -NoProfile -Command "try{$h=New-Object System.IO.FileStream('\\.\vrd9','Open');$h.Close();exit 0}catch{exit 1}" >nul 2>&1
if %errorLevel% equ 0 (
    echo [+] Driver ja carregado
    goto :abrir_valorant
)

:: Mapear driver
echo [*] Carregando driver no kernel...
loader.exe vread.sys

if %errorLevel% neq 0 (
    echo.
    echo [!] Loader falhou.
    echo.
    echo     Causas:
    echo     - Secure Boot ativo no BIOS (desative em UEFI Firmware)
    echo     - rtcore64.sys bloqueado (HVCI ativo)
    echo     - Nao esta como Administrador
    echo.
    pause
    exit /b 1
)
echo [+] Driver OK

:abrir_valorant
echo [*] Abrindo Valorant...

set "RIOT=%LOCALAPPDATA%\Riot Games\Riot Client\RiotClientServices.exe"
if exist "%RIOT%" (
    start "" "%RIOT%" --launch-product=valorant --launch-patchline=live
) else (
    start "" "valorant://"
)

echo [*] Esperando Valorant iniciar (max 2 min)...
set WAIT=0

:aguardar
timeout /t 5 >nul
tasklist 2>nul | find /I "VALORANT-Win64-Shipping" >nul
if %errorLevel% equ 0 goto :jogo_pronto
set /a WAIT=%WAIT%+5
if %WAIT% geq 120 goto :iniciar_esp
goto :aguardar

:jogo_pronto
echo [+] Valorant detectado!
echo [*] Aguardando 10 segundos (Vanguard)...
timeout /t 10 >nul

:iniciar_esp
echo.
echo [+] Iniciando ESP...
echo.
echo  Controles no jogo:
echo  INSERT  = Abrir / fechar menu
echo  F1      = Liga/desliga inimigos apenas
echo  F2      = Liga/desliga esqueleto
echo  END     = Fechar ESP
echo.

start "" "esp.exe"

echo [+] Pronto! Boa sorte.
timeout /t 5 >nul
exit /b
