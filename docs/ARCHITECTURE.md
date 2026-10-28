# Arquitetura proposta — Cloud360 e xCloud

## Visão geral

Cloud360 0.2 é apenas uma **interface nativa XEX**, inspirada na experiência de navegação
do Xbox Cloud Gaming. A Microsoft não oferece oficialmente um cliente xCloud para
Xbox 360. O projeto não promete compatibilidade antes de testes de engenharia.

## Referência nativa no Xbox 360

Orbita: https://github.com/sergiolimasks/orbita — documentação e fontes
demonstram uma arquitetura com OAuth Microsoft, catálogo,
negociação de sessão xCloud, WebRTC, H.264, Opus e controles.
Utiliza libxenon/ELF iniciado pelo XeLL, não XSDK/XEX.
Há diferenças de GPU, I/O e threading; não copiar o código GPL-3.0
para o projeto MIT sem avaliar consequências de licenciamento.
Detalhes em ORBITA_REFERENCE.md.

## Etapas técnicas (não implementadas)

1. Autenticação: explorar um fluxo autorizado Microsoft OAuth (por exemplo, device
   authorization se suportado), OAuth/XSTS e renovação de sessões. Nunca incluir
   senhas ou tokens no GitHub ou nos logs.
2. Catálogo real: consumir APIs permitidas e gerenciar autenticação, filtros e
   metadados. Sem dados, a interface deve mostrar placeholders honestos.
3. Rede e sessão: investigar descoberta e negociação de sessões Xbox Cloud Gaming,
   TLS, WebSocket, ICE/STUN/TURN e WebRTC. Evitar suposições sobre endpoints
   privados e revisar compatibilidade com termos de uso.
4. Vídeo e áudio: validar decodificação H.264 e Opus no hardware Xenon em taxas
   adequadas; separação entre threads de rede, áudio, vídeo e entrada.
5. Controle: captura de botões e sticks com latência, reconexão e segurança.
6. Testes em Aurora/console: boot, 720p, 4:3, áudio, rede, encerramento limpo,
   FPS e uso de memória. O sucesso do build não equivale a testes em hardware.

## Separação de módulos atual

- `menu.c`: foco, abas, cards e ações.
- `ui.c`: easing, tempos e transições, sem dependência de hardware.
- `render.c`: renderização de painéis, categorias e configurações com XSDK.
- `icons.c`: ícones próprios de 6×6 pixels usando U+2580/U+2584/U+2588.
- `settings.c`: preferências visuais em `game:/cloud360.cfg`, sem credenciais.
- `main.c`: inicialização da GPU/controle/XEX e loop de renderização.
- `session.c`: estados de sessão, fila, sinalização, quadro de mídia,
  reconexão, timeouts e cancelamento por geração. Nenhum servidor conectado.
- `rtc.c`: parsing SDP e candidatos ICE com validação e limites estritos.
- `stun.c`: codec de mensagens Binding e análise XOR-MAPPED-ADDRESS
  (parser público não aceita MESSAGE-INTEGRITY sem verificação).
- `ice.c`: checks com HMAC-SHA1 validado por biblioteca criptográfica
  configurável; ainda não implementa um agente ICE completo.
- `ice_agent.c`: agenda até oito destinos, checa respostas autenticadas
  por origem/porta/geração e limita reenvios; não faz nomination,
  consent freshness nem declara o par conectado.
- `ice_crypto.c`: adaptador mbedTLS opcional, com falha segura sem backend.
- `udp.c`: envio/recebimento IPv4 com socket não bloqueante e limite
  de datagramas; nenhum servidor remoto é acionado em produção.

## Referência de interface

A organização de navegação e cards foi estudada a partir da prévia da experiência
web de Xbox Cloud Gaming publicada pela Microsoft em janeiro de 2026:
https://news.xbox.com/en-us/2026/01/26/try-the-new-xbox-cloud-gaming-web-experience-now-in-public-preview/

Esse projeto independente não é afiliado à Microsoft ou à marca Xbox.
