# Cloud360 — Ciclo de vida das sessões xCloud (infraestrutura)

**Status:** coordenador de estados implementado e testado em C. Não há
provedor xCloud configurado, transporte WebRTC ou vídeo conectado.
O alvo continua sendo Xbox 360 XSDK/XEX pela Aurora; **nenhum XEX
foi gerado nesta etapa**.

## Referência de arquitetura

O Orbita documenta autenticação, região, sessão ativa, play,
provisionamento, fila, connect, SDP/ICE, WebRTC, vídeo e áudio:
https://github.com/sergiolimasks/orbita/blob/main/app/h264play/SESSION-STAGE3.md

O Cloud360 implementa lógica própria, não copia código GPL
e não trata URLs ou formatos não públicos como contratos estáveis.

## Máquina de estados

| Estado | Evento aceito | Próximo estado |
|---|---|---|
| IDLE/finalizada/falha | start com XSTS e transporte | REQUESTING |
| REQUESTING | sessão criada / preparando | PROVISIONING |
| REQUESTING/PROVISIONING | na fila | QUEUED |
| QUEUED | posição e estimativa | QUEUED, atualizadas |
| QUEUED | preparação | PROVISIONING |
| PROVISIONING/QUEUED | servidor pronto | READY_TO_CONNECT |
| READY_TO_CONNECT | sinalização aceita | SIGNALING |
| SIGNALING | par conectado | CONNECTING |
| CONNECTING | configuração aceita | WAITING_VIDEO |
| WAITING_VIDEO | primeiro quadro real | STREAMING |
| STREAMING | quadro real | atualiza atividade |
| STREAMING/WAITING_VIDEO | perda de conexão | RECONNECTING |
| RECONNECTING | sinalização recuperada | SIGNALING |
| qualquer ativo | erro remoto / timeout / credencial expirada | FAILED |
| qualquer ativo | cancelar | STOPPING |
| STOPPING | encerramento confirmado | STOPPED |

A transição para STREAMING depende de um evento de
primeiro vídeo vindo do futuro decodificador real. O
aplicativo não produz esse evento em produção hoje.

Agora, para aceitar PEER_CONNECTED durante SIGNALING, o coordenador
exige antes que `cloud360_session_remote_sdp()` tenha validado uma
resposta SDP com ICE/DTLS/H.264/RTCP mux. Uma resposta SDP válida
**não comprova** negociação ICE nem autenticação DTLS: o evento
PEER_CONNECTED deverá vir do futuro transportador real.
A flag de SDP é reinicializada em cada reconexão.
Ver [WEBRTC.md](WEBRTC.md).

## Limites implementados

- Poll de estado: no máximo uma solicitação por segundo.
- Solicitação inicial: 30 segundos.
- Preparação de servidor sem fila: 210 segundos.
- Fila total: 30 minutos.
- Sinalização: 60 segundos.
- Conexão e reconexão: 30 segundos.
- Primeiro quadro de vídeo ou ausência de quadros: 12 segundos.
- Reconexões: até 2 tentativas.
- Confirmação de encerramento: 10 segundos.

Prazos usam aritmética monotônica uint32, compatível com rollover.
Cada evento traz uma geração de sessão, para rejeitar respostas
de tentativas antigas. A interface não recebe tokens nem
identificadores secretos.

## Interface

A aba SESSAO fica entre CONTA e SOBRE. Ela mostra fase,
requisito de XSTS e, quando existirem eventos reais, fila,
tentativas e erros.

- **A:** verificar pré-requisitos e preparar uma sessão.
- **X:** cancelar/encerrar.
- **LB/RB:** alternar abas.

A produção mantém `transport_available=false`: falta um
adaptador autorizado e testado para sessão real. Portanto,
o botão A não faz HTTP nem inicia jogos; exibe
`PROVEDOR XCLOUD NAO INTEGRADO` se XSTS estiver autorizado.
Os títulos do JSON local **não são considerados jogáveis**.

## Próximas integrações

1. Verificar acesso autorizado às APIs do serviço, sem presumir
   que XSTS genérico signifique direito de uso do xCloud.
2. Criar adaptador assíncrono para descobrir região, solicitar
   sessão, consultar estado e encerrar com segurança.
3. Implementar sinalização SDP/ICE/STUN e DTLS-SRTP.
4. Integrar H.264/Opus diretamente ao XSDK.
5. Emitir `FIRST_VIDEO` apenas após quadro válido realmente
   decodificado e apresentado no hardware do Xbox 360.

Os testes automatizados Linux não validam funcionamento do
xCloud ou desempenho na GPU/CPU do console.
