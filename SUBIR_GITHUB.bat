@echo off
chcp 437 >nul
title VRD ESP - Subir para GitHub

cd /d "%~dp0"

echo.
echo  VRD ESP - Compilar na nuvem (zero instalacao local)
echo  =====================================================
echo.
echo  Este script vai:
echo   1. Instalar o GitHub CLI (programa pequeno)
echo   2. Fazer login na sua conta GitHub (abre o navegador)
echo   3. Subir o codigo para um repositorio privado
echo   4. O GitHub compila tudo automaticamente em ~10 minutos
echo   5. Voce baixa o LAUNCH.zip pronto
echo.
echo  Precisa de uma conta GitHub gratuita: https://github.com/join
echo.
pause

:: ------
:: Instalar GitHub CLI se nao tiver
:: ------
where gh >nul 2>&1
if %errorLevel% neq 0 (
    echo [*] Instalando GitHub CLI...
    winget install --id GitHub.cli -e --silent --accept-package-agreements --accept-source-agreements
    if %errorLevel% neq 0 (
        echo [!] Falha no winget. Baixe manualmente: https://cli.github.com
        echo     Instale e rode este script novamente.
        pause & exit /b 1
    )
    :: Atualizar PATH sem reiniciar
    set "PATH=%PATH%;%LOCALAPPDATA%\Microsoft\WinGet\Links;C:\Program Files\GitHub CLI"
    echo [+] GitHub CLI instalado
) else (
    echo [+] GitHub CLI ja instalado
)

:: ------
:: Login GitHub (abre navegador uma unica vez)
:: ------
gh auth status >nul 2>&1
if %errorLevel% neq 0 (
    echo.
    echo [*] Fazendo login no GitHub...
    echo     (vai abrir o navegador - clique em "Authorize GitHub CLI")
    echo.
    gh auth login --hostname github.com --git-protocol https --web
    if %errorLevel% neq 0 (
        echo [!] Login falhou. Tente novamente.
        pause & exit /b 1
    )
    echo [+] Login OK
) else (
    echo [+] Ja logado no GitHub
)

:: ------
:: Inicializar git se necessario
:: ------
where git >nul 2>&1
if %errorLevel% neq 0 (
    echo [*] Instalando Git...
    winget install --id Git.Git -e --silent --accept-package-agreements --accept-source-agreements
    set "PATH=%PATH%;C:\Program Files\Git\cmd"
)

if not exist ".git" (
    git init
    git checkout -b main
)

:: ------
:: Criar repositorio privado e subir codigo
:: ------
echo.
echo [*] Criando repositorio privado no GitHub...

gh repo create vrd-esp-build --private --source=. --remote=origin --push
if %errorLevel% neq 0 (
    echo.
    echo [*] Repositorio ja existe, atualizando...
    git add .
    git commit -m "update source" 2>nul || echo [*] Nada novo para commitar
    git push origin main --force
)

echo.
echo [+] Codigo enviado!
echo.
echo =====================================================
echo  PROXIMO PASSO:
echo.
echo  O GitHub esta compilando agora (~10 minutos).
echo  Abrindo a pagina de Actions no navegador...
echo.
echo  Quando aparecer um circulo verde:
echo   1. Clique em "Build VRD ESP"
echo   2. Role para baixo ate "Artifacts"
echo   3. Clique em "LAUNCH-pronto" para baixar
echo   4. Extraia o zip
echo   5. Coloque rtcore64.sys dentro da pasta
echo   6. Clique duas vezes em RODAR.bat
echo =====================================================
echo.

timeout /t 3 >nul

:: Pegar URL do repo e abrir Actions
for /f "usebackq delims=" %%i in (`gh repo view --json url -q .url 2^>nul`) do set "REPO_URL=%%i"
if defined REPO_URL (
    start "" "%REPO_URL%/actions"
) else (
    start "" "https://github.com"
)

echo [+] Navegador aberto. Aguarde o build terminar.
echo.
pause
