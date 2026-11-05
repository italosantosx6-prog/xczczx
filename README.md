# VRD ESP

Projeto de overlay/ESP para Valorant, baseado em driver kernel manualmente mapeado e loader BYOVD. O repositório inclui:

- `driver/` — driver kernel (`vread.sys`)
- `loader/` — mapper do driver (`loader.exe`)
- `cheat/` — overlay da ESP (`esp.exe`)
- `external/` — ImGui
- `.github/workflows/build.yml` — build automatizado em GitHub Actions

## Requisitos

- Windows 10/11 x64
- Visual Studio 2022 + C++ desktop workload
- WDK 10.0.26100+
- CMake 3.20+
- Git

## Compilar localmente

```bat
cd external
get_imgui.bat

cd ..\driver
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

cd ..\loader
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

cd ..\cheat
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Compilar na nuvem

O workflow em `.github/workflows/build.yml` tenta montar os artefatos automaticamente em GitHub Actions e gerar uma pasta `LAUNCH` com os executáveis prontos.

## Observações

Este projeto depende de técnicas de kernel-mode e BYOVD. Use apenas em ambientes controlados e em conformidade com as leis e políticas aplicáveis. O uso do projeto pode violar termos de serviço de jogos ou plataformas; faça isso por sua conta e risco.
