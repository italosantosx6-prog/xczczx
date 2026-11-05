# COMO RODAR - VRD ESP
# Versao sem instalacao local

---

## METODO RAPIDO (recomendado) - GitHub compila pra voce

**Nenhum VS, nenhum WDK, nenhum CMake no seu PC.**
O GitHub tem servidores Windows na nuvem que compilam tudo.

### Passo 1 - Conta GitHub (gratis)
Crie em https://github.com/join se nao tiver

### Passo 2 - Rodar SUBIR_GITHUB.bat
Clique duas vezes. Ele vai:
- Instalar o GitHub CLI (programa pequeno ~50MB)
- Abrir o navegador pra login no GitHub (uma vez so)
- Subir o codigo pra um repositorio privado
- Abrir a pagina de Actions automaticamente

### Passo 3 - Esperar o build (~10 minutos)
Na pagina que abrir, aguarda o circulo ficar verde.

### Passo 4 - Baixar LAUNCH-pronto.zip
1. Clique em "Build VRD ESP" (o run que apareceu)
2. Role ate "Artifacts"
3. Clique em "LAUNCH-pronto"
4. Extraia o zip em qualquer pasta

### Passo 5 - rtcore64.sys
Coloque o rtcore64.sys dentro da pasta extraida.
Hash SHA256: f6f11ad2cd2b0cf95ed42324876bee1d83e01775d8dd92daaea85012e1245fd
Buscar: "rtcore64.sys kdmapper github"

### Passo 6 - Rodar
Clique duas vezes em RODAR.bat (ja esta dentro da pasta).

---

## CONFIGURACAO DO BIOS (fazer uma vez)

Vanguard usa HVCI que bloqueia drivers nao assinados. Precisa desativar:

1. Configuracoes -> Sistema -> Recuperacao -> Reinicializacao avancada -> Reiniciar agora
2. Solucionar problemas -> Opcoes avancadas -> Configuracoes de firmware UEFI
3. No BIOS procurar: Secure Boot -> Disabled
4. Salvar e sair

Se aparecer BSOD ao rodar o loader, Secure Boot ainda esta ativo.

---

## CONTROLES NO JOGO

| Tecla  | Funcao                      |
|--------|------------------------------|
| INSERT | Abrir / fechar o menu        |
| END    | Fechar o ESP completamente   |
| F1     | Liga/desliga somente inimigos|
| F2     | Liga/desliga esqueleto       |

---

## O MENU

3 abas:

### ESP
- Bounding box com cantos
- Esqueleto completo
- Ponto na cabeca
- Snap line (borda inferior ate o pe)
- Barra de vida e escudo
- Distancia em metros
- Nome do agente
- Distancia maxima (slider)
- Espessura das linhas

### COLORS
- Cor do inimigo, aliado, HP baixo
- Presets rapidos: Vermelho / Branco / Ciano / Laranja

### INFO
- Status do driver e do jogo
- Contagem de inimigos em tempo real
- Lista de controles

---

## ATUALIZAR OFFSETS (apos cada patch do Valorant)

Quando o Valorant atualizar o ESP para de funcionar.

1. Rodar o offset_scanner.py com um dump de memoria do jogo
2. Copiar os novos RVAs para cheat\offsets.hpp
3. Dar push novamente no GitHub (SUBIR_GITHUB.bat)
4. Baixar o novo LAUNCH-pronto.zip

---

## PROBLEMAS COMUNS

| Problema                          | Solucao                                    |
|-----------------------------------|--------------------------------------------|
| BSOD ao rodar loader.exe          | Secure Boot ativo - desativar no BIOS      |
| "Acesso negado" no loader         | Executar como Administrador                |
| "cannot open driver device"       | Loader nao terminou - ver saida do loader  |
| Overlay nao aparece               | Valorant em Janela sem bordas              |
| Nenhum inimigo aparece            | Offsets desatualizados                     |
| Build falhou no GitHub Actions    | Ver aba "build" no Actions pra ver o erro  |
| Vanguard barra o rtcore64.sys     | Usar driver alternativo (ver BUILD.md)     |
