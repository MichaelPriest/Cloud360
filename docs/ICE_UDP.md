# Cloud360 — UDP e verificação de integridade STUN/ICE

## Escopo

Esta etapa implementa **componentes transportáveis em C** para
Xbox 360 XSDK/XEX (Aurora), com testes no Linux. Não conecta
a Microsoft nem inicia sessões reais de xCloud.
Nenhum XEX foi gerado nesta etapa.

O padrão de referência é [RFC 8445 ICE](https://www.rfc-editor.org/rfc/rfc8445),
especialmente a formação de credenciais da seção 7.2.2, e
[RFC 8489 STUN](https://www.rfc-editor.org/rfc/rfc8489)
para MESSAGE-INTEGRITY (seção 14.5).

## Arquivos

- `src/ice.h` / `src/ice.c`: constrói requisição STUN Binding
  com USERNAME remote:local, PRIORITY, ICE-CONTROLLING ou
  ICE-CONTROLLED, tie-breaker, USE-CANDIDATE opcional e
  MESSAGE-INTEGRITY. Valida resposta de sucesso, identificador
  de transação, HMAC, XOR-MAPPED-ADDRESS e CRC opcional.
- `src/ice_crypto.c`: usa HMAC-SHA1 via mbedTLS se
  `CLOUD360_WITH_MBEDTLS` estiver habilitado; sem isso
  recusa a assinatura/verificação.
- `src/udp.h` / `src/udp.c`: socket IPv4/UDP não bloqueante,
  bind por interface/porta, porta efêmera, envio/recepção
  com timeout, origem explícita e descarte de datagramas
  com mais de 1500 bytes.
- `tests/test_ice.c`: OpenSSL somente no Linux para verificar
  requests/responses HMAC, alterações, falha na verificação,
  ID divergente, papel do agente e buffers.
- `tests/test_udp.c`: round-trip IPv4 em loopback Linux,
  timeouts, tamanho máximo e fechamento seguro.

## Segurança

O segredo compartilhado para um check ICE é a senha de ICE
**remota** fornecida na negociação SDP; o USERNAME é a
concatenação remote:local. O RFC determina o HMAC sobre
os bytes anteriores ao atributo MESSAGE-INTEGRITY, com
Length temporariamente ajustado até o fim do atributo.
O teste OpenSSL verifica este cálculo independentemente.

Tokens Xbox, credenciais OAuth e senhas ICE não devem entrar
em logs, arquivos de configuração ou na UI. Não armazenar
senhas ICE no catálogo. Os transaction IDs de 96 bits
precisam ser criados por um **CSPRNG adequado** fornecido
pelo futuro backend; o código atual recebe esses IDs como
argumento. Não usar `rand()` ou tempo de sistema para isso.

O FINGERPRINT STUN usa CRC-32, não é autenticação.
Uma resposta com HMAC válido significa apenas que
os bytes foram assinados usando a senha ICE informada;
**não prova eleição do par, consentimento de envio,
fingerprint DTLS ou que existe um stream ativo**.

O callback mbedTLS precisa de uma biblioteca portável e
testada no XSDK antes de habilitar checks reais.
O CMake define `CLOUD360_WITH_MBEDTLS` somente quando o
transporte HTTPS opcional é configurado com mbedtls-xenon.
Em modo padrão o módulo ICE criptográfico falha fechado.

## Próximos requisitos antes de aceitar um par

1. Integrar o backend seguro de sessões do serviço autorizado,
   sem presumir endpoints não públicos.
2. Implementar checklist e state machine ICE, com pares locais/
   remotos, nominação, controle de papel e retransmissões.
3. Verificar origem da resposta UDP contra o par esperado,
   além do transaction ID e HMAC do pacote STUN.
4. Implementar consent freshness contínuo, rate limit e TURN
   sob necessidade, validando recursos de NAT no console.
5. Implementar DTLS com certificado efetivamente comparado ao
   SHA-256 anunciado no SDP, SRTP e antirreplay.
6. Processar RTP, H.264, Opus e entrada de controle, com medição
   de latência, CPU e memória em Xbox 360 físico.

**A aba SESSÃO ainda exige um provedor real e não deve
transitar para STREAMING a partir desses helpers.**
