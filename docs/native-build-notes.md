# Build experimental de 19/11/2026

O NRO foi ligado e empacotado localmente com ferramentas Windows nativas.
O GitHub Actions não foi executado para este artefato.

- Base Moonlight: `841fc353aa5152df659aad4f6e1907d4ad3ba1d5`.
- Compilador: devkitA64 GCC 16.1.0; SDK libnx 4.12.0.
- Renderização: deko3d; decodificação H.264/HEVC: caminho NVTEGRA existente.
- GLM: versão 1.0.1, cabeçalhos de g-truc/glm.
- dav1d: versão 1.5.0, build estático AArch64 com assembly, via Meson/Ninja.
  O patch `dav1d-switch.patch` evita `sysconf`, indisponível no libnx, usando
  o fallback de contagem de threads que já existe no dav1d. Essa biblioteca
  atende o backend de AV1 por software; H.264/HEVC seguem seu backend original.

`scripts/build-from-generated-makefiles.py` reaproveita os comandos de uma
configuração CMake devkitPro já gerada em `build/switch-local`. Converte os
caminhos MSYS2 para Windows e executa GCC, ar, nacptool e elf2nro diretamente,
sem shell MSYS2. Não é um substituto geral para configurar o projeto do zero.
Ao alterar cabeçalhos ou opções de compilação, regenere a configuração e
recompile os objetos afetados; o reaproveitamento local verifica a data do
arquivo-fonte, não implementa todo o grafo de dependências do Make.

Validações concluídas: testes locais da fila; compilação e ligação ARM64;
assinatura NRO0; seção ASET; inclusão das opções em português e dos shaders.
Não houve execução nem medição no Switch. A versão continua experimental.
