# Baixa latência no Switch OLED — versão experimental

Base: XITRIX/Moonlight-Switch, revisão
`841fc353aa5152df659aad4f6e1907d4ad3ba1d5`. Os submódulos conservam as
revisões dessa base. Esta alteração destina-se ao Switch original/OLED,
executando homebrew com acesso à memória completa.

## Escolha nas configurações

Em **Configurações → Qualidade → Apresentação dos quadros (próxima sessão)**:

- **Preferir baixa latência**: padrão desta versão no Switch original/OLED.
  Mantém no máximo um frame decodificado aguardando o renderizador. Um frame
  mais novo substitui o pendente; o primeiro frame não espera uma reserva de
  jitter e não existe uma segunda espera pelo relógio adaptativo da fila.
- **Preferir vídeo suave**: conserva a política adaptativa original e sua
  reserva de frames. Pode absorver melhor variações da rede, com mais atraso.

A escolha é salva na chave booleana `low_latency_video` do `settings.json`.
Abra uma nova sessão após mudar a opção. Não é preciso apagar o pareamento
nem outras configurações. Nas outras plataformas o padrão anterior continua.

Os frames descartados já foram decodificados. Não descartamos pacotes H.264/HEVC
de referência, nem eliminamos as sincronizações que protegem os recursos da GPU.
O frame atualmente exibido continua válido enquanto novos frames chegam.

## O que este modo não promete

Não elimina o tempo de rede, NVDEC, GPU ou atualização do painel. Em baixa
latência, uma rede instável pode produzir repetições de imagem, em vez de
esconder a instabilidade numa fila. A contagem de frames recebidos pelo cliente
pode ultrapassar 60 mesmo com apresentação em painel de 60 Hz.

A fila de saída do decodificador pode mostrar **1** antes do consumo e **0**
depois dele. Isso não mede outras filas do sistema nem a latência até o painel.
Não há alegação de desempenho equivalente ao Android sem testes no console.

## Verificação local

```sh
python3 scripts/test_frame_queue.py
```

O teste compila e executa a classe de fila de produção com fixtures simples de
AVFrame. Cobre seleção do mais recente, propriedade/reutilização, rajadas,
pausa, limpeza/reconexão, modo suave e produtor/consumidor simultâneos.
Não exercita o NVDEC ou as sincronizações da GPU real.

## Compilação e validação no console

No PowerShell normal do Windows, com devkitPro instalado em `C:\devkitPro`:

Antes da primeira compilação, atualize o ambiente MSYS2 inteiro para evitar
incompatibilidade de DLLs entre o instalador e os pacotes atuais:

```powershell
& 'C:\devkitPro\msys2\usr\bin\bash.exe' --login -c 'pacman -Syu'
```

Se o gerenciador pedir para fechar o terminal após atualizar o runtime, feche-o,
abra novamente e repita a atualização até não haver atualizações pendentes.
Em seguida, execute a compilação:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-switch-windows.ps1
```

O script instala as bibliotecas que não vêm no grupo Switch Development,
inicializa os submódulos e compila no MSYS2 exigido pelo SDK. Saída:
`build/dist/Moonlight-LowLatency.nro`. Log: `build/switch-local.log`.
O parâmetro ExecutionPolicy vale apenas para essa execução do script local;
ele não altera a política do Windows permanentemente.

O SDK devkitPro/devkitA64 com as dependências Switch do projeto é necessário.
Inicialize os submódulos antes da configuração:

```sh
git submodule update --init --recursive
cmake -B build/switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON
cmake -B build/switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON
cmake --build build/switch --target Moonlight.nro -j4
```

O segundo configure segue o fluxo de CI já adotado pelo projeto. O workflow
`Switch` existente também compila o NRO num ambiente devkitPro no GitHub Actions.
Nenhum workflow foi publicado nem executado remotamente por esta modificação.

Guarde o NRO estável antes de instalar o experimental. Compare os dois modos
na mesma cena a 720p60 e com a mesma conexão/bitrate: fila, frames descartados,
fluidez e latência controle→imagem (idealmente filmada em alta velocidade).
Teste também suspensão/retomada e pelo menos 15 minutos de sessão. A aprovação
no console exige confirmar ausência de corrupção gráfica e travamentos.
