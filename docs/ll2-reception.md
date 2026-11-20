# LL2 — investigação de engasgos após reconectar

Motivo: no teste do console chegaram apenas 2–13 FPS ao decoder, apesar de
decode em aproximadamente 2–3 ms e fila de apresentação vazia. Isso não
comprova qual componente perdeu os frames. A recepção e o estado da sessão
precisam ser investigados antes de alterar clocks.

Esta versão experimental:

- Reserva 3 MiB para recepção UDP na configuração libnx de Application e
  SystemApplication. Applet conserva a configuração padrão, por ter menos RAM.
  A configuração é aplicada pelo linker ao pedido de configuração existente;
  não modifica arquivos do SDK. O espaço absorve rajadas de pacotes comprimidos,
  sem criar uma reserva adicional de frames decodificados.
- Mostra LL2, Application/Applet e o maior SO_RCVBUF efetivamente observado.
  O tamanho reportado pode ser limitado pelo sistema; não é uma medição da
  quantidade de dados atualmente na fila.
- Zera contadores, janela de medição e sequência de frames quando o decoder
  inicia/termina uma sessão; protege o contador contra retorno da numeração.
- Preserva os dois modos de apresentação e a política anterior de clocks.

Os testes de host cobrem configuração por tipo de applet, manutenção dos outros
campos do SDK, passagem de erros BSD, leitura do buffer efetivo, reset de estado,
duplicatas, retorno da numeração e wrap de uint32. O build ARM64 passou.
Ainda falta validar a inicialização do BSD com a nova reserva e a recepção no
hardware real. Não há alegação de correção confirmada do engasgo.

Instalação: feche completamente o Moonlight, substitua o NRO e abra pelo
hbmenu com acesso completo à RAM. O buffer só muda ao iniciar o aplicativo,
não ao reconectar uma transmissão. Confira `Build: LL2 | launch: Application`
no overlay e compare a mesma cena e as mesmas opções da versão anterior.
