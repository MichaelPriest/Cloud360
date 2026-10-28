# Cloud360 — infraestrutura WebRTC para Xbox 360 XSDK/XEX

**Estado: código-fonte e testes Linux; sem comunicação WebRTC real.**
Nenhum XEX foi gerado nesta fase.

## Objetivo técnico

Preparar parsing/validação segura de sinalização antes de estudar
ICE, STUN/TURN, DTLS-SRTP, RTP, decodificação H.264 e Opus para
Xbox 360 RGH/JTAG executado via Aurora.

Referências de padrão utilizadas:
- [RFC 8489 — STUN](https://www.rfc-editor.org/rfc/rfc8489)
- [RFC 8842 — SDP e negociação DTLS](https://www.rfc-editor.org/rfc/rfc8842)
- [RFC 8445 — ICE](https://www.rfc-editor.org/rfc/rfc8445)
- [RFC 8122 — DTLS fingerprints em SDP](https://www.rfc-editor.org/rfc/rfc8122)

O Orbita descreve uma arquitetura mais completa sob GPL-3.0
([WEBRTC-STAGE4.md](https://github.com/sergiolimasks/orbita/blob/main/app/h264play/WEBRTC-STAGE4.md)).
O Cloud360 usa código próprio e permanece licenciado sob MIT.

## Implementado — SDP/ICE (src/rtc.h e src/rtc.c)

`cloud360_rtc_sdp_parse()` lê um SDP remoto de até 16 KiB
e no máximo 256 linhas com tamanho máximo de 512 caracteres.
Para aceitar um SDP como **sintaticamente elegível**, exige:
- versão `v=0` e indicação de `a=group:BUNDLE`;
- ICE `a=ice-ufrag` e `a=ice-pwd` com caracteres/tamanhos limitados;
- `a=fingerprint:sha-256` com 32 bytes formatados;
- `a=setup:active` ou `a=setup:passive`;
- seção `m=video` com `a=rtpmap` H264/90000;
- `a=rtcp-mux` na seção de vídeo.

O parser extrai **bytes** da fingerprint para futura comparação
com a identidade do certificado remoto no handshake DTLS, mas
**não executa nem autentica DTLS**. A validação é deliberadamente
restrita e precisará de adaptação ao perfil de mídia autorizado
que um serviço real venha a negociar.

`cloud360_rtc_candidate_parse()` aceita apenas candidatos
`candidate:` com endereços **IPv4 e transporte UDP**, prioridade,
porta e tipo (host/srflx/prflx/relay), podendo incluir
`raddr` e `rport`. Candidatos TCP, IPv6 ou mDNS continuam
não suportados e são contados, mas não ativados. No máximo
128 linhas de candidatos no SDP.

## Implementado — STUN Binding (src/stun.h e src/stun.c)

`cloud360_stun_binding_request()` constrói o cabeçalho de
requisição Binding com magic cookie 0x2112A442 e ID de transação
de 96 bits **fornecido por um gerador criptográfico externo**.
Não existe fonte de aleatoriedade inventada no projeto.

`cloud360_stun_parse_binding_response()`:
- verifica classe/tipo Binding sucesso, tamanho, cookie e ID;
- interpreta XOR-MAPPED-ADDRESS IPv4, endereço e porta;
- rejeita TLVs malformados e atributos obrigatórios desconhecidos;
- verifica CRC-32 de FINGERPRINT se existir e exige que seja o
  último atributo, como especificado no padrão;
- não suporta autenticação MESSAGE-INTEGRITY nesta fase.

Uma resposta STUN válida **não comprova conectividade ICE**, nem
autenticação do servidor, DTLS ou conexão com o xCloud.
O FINGERPRINT STUN é um CRC de integridade acidental, **não uma
assinatura criptográfica**. Não há tráfego UDP na versão atual.

## Novidade: ICE autenticado e transporte UDP local

`src/ice.c` gera mensagens de verificação com RFC 8445 USERNAME
(remote:local), PRIORITY, ICE-CONTROLLING ou ICE-CONTROLLED,
USE-CANDIDATE quando aplicável e MESSAGE-INTEGRITY (HMAC-SHA1).
Compara o HMAC de respostas Binding em tempo independente dos
bytes de hash, respeitando o campo Length temporário exigido
pelo RFC 8489. Exige correspondência do transaction ID e
XOR-MAPPED-ADDRESS. Rejeita MESSAGE-INTEGRITY ausente, alterado,
duplicado ou atributos obrigatórios desconhecidos. O CRC
FINGERPRINT continua sendo uma checagem não criptográfica.

`src/ice_crypto.c` fornece o adaptador HMAC do mbedTLS
**somente quando o backend for habilitado**. Sem biblioteca
criptográfica, retorna erro em vez de aceitar pacotes.
O Linux usa OpenSSL apenas nos testes de referência.
`src/udp.c` abre/binda socket IPv4, usa nonblocking/poll,
envia e recebe datagramas com limites de tamanho e origem
visível. O teste de troca UDP acontece em localhost Linux,
não com servidores remotos ou Xbox 360.

**Ainda não existe cliente ICE funcional:** faltam verificação
de origem/candidate pair, retransmissões, controle de papéis,
conectividade bidirecional, consentimento, TURN, DTLS e SRTP.
A UI SESSAO continua com o provedor desativado.
Mais detalhes em [ICE_UDP.md](ICE_UDP.md).

## Integração ao coordenador de sessões

`cloud360_session_remote_sdp()` valida a resposta SDP e registra
somente metadados de diagnóstico (presença, quantidade de
candidatos IPv4/UDP e payload type H.264).
Agora a transição via evento `PEER_CONNECTED` é rejeitada
se o SDP não passou no parser; na reconexão, o estado SDP
é limpo e uma nova validação será necessária.

A UI da aba SESSAO informa se há SDP validado, porém não usa
respostas falsas nem produz uma conexão de demonstração.
O build de produção ainda passa `transport_available=false`.

## Para implementar streaming real

1. Integração de serviço de sessão autorizado e
   intercâmbio SDP/ICE (API estável e permitida).
2. Validar a implementação UDP IPv4 em hardware XSDK e adicionar
   CSPRNG confiável (sem `rand()` ou timestamp como ID de transação).
3. Conectar os helpers MESSAGE-INTEGRITY ao checklist ICE completo,
   com candidate pairing, retransmissões, consentimento e TURN quando
   necessário; IPv6/TCP opcional conforme redes suportadas.
4. DTLS com certificado verificado contra a fingerprint
   recebida; SRTP e anti-replay.
5. RTP/H.264, Opus e canais de dados de entrada do controle.
6. Medições no Xbox 360 físico de CPU/GPU, latência,
   jitter buffer e uso de memória.

A compatibilidade real com os serviços Xbox Cloud Gaming
ainda é desconhecida. Somente testes em Linux não bastam para
atestar reprodução de jogos no Xbox 360.
