# Orbita: referência técnica para Cloud360

Análise em 28/10/2026 de https://github.com/sergiolimasks/orbita.

## Diferença mais importante

- **Orbita**: libxenon, executável ELF iniciado pelo XeLL Reloaded,
  próprio gerenciamento de GPU, rede, áudio e controles em bare-metal.
- **Cloud360**: XSDK, executável XEX iniciado diretamente no kernel
  do Xbox 360 via Aurora, APIs XAM de rede, vídeo e controles.
  O usuário solicitou permanecer nesse caminho.

Não existe compatibilidade binária direta entre os dois runtimes.
Portar a lógica de sessão, áudio e vídeo requer reimplementar os
adaptadores para o XSDK. Não afirmar que o código do Orbita executa
dentro de um XEX.

## Arquitetura do Orbita observada no código-fonte

1. **Login**: exibe um código de autorização Microsoft na TV e o usuário
   autoriza em outro dispositivo; gerencia renovação e tokens Xbox/XSTS.
   Referência: app/h264play/source/login e LOGIN-PROGRESS.md.
2. **Catálogo**: integra dados de conta, recentes, populares e lançamentos;
   usa cache local de capas e banners JPEG com gerenciamento de memória.
   Referência: app/h264play/source/catalog e CATALOG-COVERS.md.
3. **Sessão**: máquina de estados de autenticação, fila, conexão,
   sinalização, renegociação e encerramento. Referência:
   app/h264play/source/session e SESSION-STAGE3.md.
4. **WebRTC**: negociação SDP/ICE, UDP, DTLS-SRTP, RTP,
   DataChannels, controle de perda e jitter. Referência:
   app/h264play/source/webrtc e WEBRTC-STAGE4.md.
5. **Áudio e vídeo**: decodificação H.264 e Opus,
   separando threads de rede, codec e GPU. Referência:
   app/h264play/source/native, source/audio e DECODE-PERF.md.
6. **Teste no hardware**: módulo com testes simulados no Linux,
   build cross-compiler e diagnósticos reais no Xbox 360.

A documentação do Orbita declara beta fechado e streaming 720p60.
Essas afirmações são do projeto upstream; **Cloud360 não testou
ou verificou esses resultados no hardware**.

## Licença

Orbita é **GPL-3.0**, Cloud360 é **MIT**.
Esta análise é referência de arquitetura e não inclui código
copiado. A combinação de código derivado de GPL em um aplicativo
MIT pode exigir relicenciamento e cumprimento das obrigações da
GPL. Analisar as licenças antes de qualquer eventual port.

## Próximos passos no Cloud360

1. Finalizar TLS verificado por CA no XSDK e testar horário
   e entropia disponíveis no console.
2. Implementar interface segura para OAuth 2.0 Device
   Authorization Grant: código, intervalo de polling,
   expiração, cancelamento e revogação.
3. Pesquisar caminhos de autenticação Xbox/XSTS autorizados.
   Nunca exigir senha na dashboard e nunca registrar tokens.
4. Substituir o catálogo de teste por uma fonte de dados autorizada,
   com capas JPEG em cache limitado.
5. Desenvolver a máquina de estados da sessão, com testes
   de fila, falhas, reconexão e encerramento.
6. Avaliar transporte WebRTC, DTLS-SRTP e decodificação
   H.264/Opus especificamente como XEX, não como ELF libxenon.
7. Medir consumo de CPU, memória e frame time no Xbox 360 físico.

## Fontes

- https://github.com/sergiolimasks/orbita/blob/main/README.md
- https://github.com/sergiolimasks/orbita/blob/main/app/h264play/LOGIN-PROGRESS.md
- https://github.com/sergiolimasks/orbita/blob/main/app/h264play/CATALOG-COVERS.md
- https://github.com/sergiolimasks/orbita/blob/main/app/h264play/SESSION-STAGE3.md
- https://github.com/sergiolimasks/orbita/blob/main/app/h264play/WEBRTC-STAGE4.md
- https://github.com/sergiolimasks/orbita/blob/main/app/h264play/STANDALONE-STAGE5.md

**Não gerar XEX nesta etapa**, conforme solicitado.
