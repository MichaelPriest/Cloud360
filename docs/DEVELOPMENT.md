# Cloud360 — desenvolvimento primeiro, executável depois

## Status do projeto

Nesta fase, o projeto é **100% dedicado ao Xbox 360 RGH/JTAG**. O trabalho
segue em módulos com código C, testes Linux de comportamento e verificação de
sintaxe com os cabeçalhos do XSDK. A compilação cruzada XEX não é disparada por
push. Ela só está disponível por acionamento manual e não deve ser executada
até a integração estar madura e o usuário solicitar.

## O que já está ligado na interface

### Rede local real — aba REDE

O arquivo `src/network.c` contém uma máquina de estados de conectividade
local. `src/main.c` conecta essa lógica à API XAM via as funções nativas
`xsdk_net_link` e `xsdk_net_address` fornecidas pelo XSDK.

- Ao abrir REDE, consulta o status do link da interface do console.
- Se houver link, consulta o IPv4 local do console.
- Exibe `SEM LINK DE REDE`, `AGUARDANDO IP`, `IP LOCAL DISPONIVEL`
  ou `ERRO DE REDE` no painel Azul Cloud360.
- Botão A força uma nova verificação.
- Evita executar chamadas de rede em todos os frames; quando disponível, o IP
  é atualizado no máximo uma vez a cada 10 segundos, e erros ou espera de IP
  são reavaliados após 2,5 segundos.
- O modelo é testável sem console usando callbacks simulados e testes de
  transição de status, erro e rollover do relógio.

**Limitação importante:** possuir um IPv4 local não significa ter internet.
Essa etapa não testa acesso ao Xbox Live, autenticação Microsoft ou xCloud.

### Diagnóstico DNS/TCP em segundo plano — aba REDE

Ao pressionar **X** na aba REDE (quando o console já possui IPv4 local), a
dashboard inicia uma thread POSIX separada. Ela resolve `www.xbox.com`
usando `getaddrinfo` do XSDK e tenta abrir uma conexão TCP na porta 443.
A abertura do socket usa `O_NONBLOCK`, `poll` com timeout de 2,5 s e
`getpeername` para verificar a conclusão da conexão. XAM não implementa
adequadamente `SO_ERROR`, portanto ele não é utilizado.

- O usuário continua navegando durante DNS e TCP; o resultado aparece na aba.
- Uma nova tentativa é bloqueada enquanto a anterior está em andamento.
- Erros DNS/TCP são separados; sockets são fechados mesmo quando falham.
- Nenhuma requisição HTTP, credencial, token ou dado pessoal é enviado.
- **Limite:** uma chamada DNS do XAM pode bloquear a thread mais que o
  timeout TCP; o fechamento do título encerra a thread destacada. A thread
  utiliza armazenamento estático pelo ciclo de vida do título.
- **Importante:** conexão TCP 443 bem-sucedida não prova TLS válido, acesso
  à conta Xbox ou suporte ao serviço xCloud.

### HTTPS/TLS com verificação estrita — aba REDE

O botão **Y** solicita uma requisição GET de teste a `https://example.com/`,
em uma thread independente. O resultado aparece na tela sem salvar cookies,
respostas ou credenciais.

Arquivos: `src/https.h`, `src/https.c` (validação e estado assíncrono),
`src/https_curl.c` (adaptador curl-xenon).

**Política de segurança:**

- A URL é criada exclusivamente com `https://` a partir de um host DNS e
  caminho previamente validados; controles e esquemas alternativos são rejeitados.
- `CURLOPT_SSL_VERIFYPEER=1` e `CURLOPT_SSL_VERIFYHOST=2` são obrigatórios.
- O bundle de CA `game:/cacert.pem` é indispensável e o relógio do console
  precisa estar correto para verificar validade dos certificados.
- TLS 1.2 / HTTP 1.1, sem redirecionamentos e sem proxy.
- Timeout de 8 segundos na conexão e 15 segundos na transferência.
- Resposta limitada a 16 KiB, descartada e nunca tratada como catálogo.
- Erros DNS/TCP, certificado, timeout e HTTP são representados separadamente.
- **Sem backend habilitado**, a função retorna `TLS NAO INTEGRADO`,
  sem qualquer conexão de rede. Não existe fallback HTTP.

**Integração das dependências para o futuro build:**

As bibliotecas open source `xenosworks/curl-xenon` e
`xenosworks/mbedtls-xenon`, no branch `xenon`, são as opções estudadas.
O CMake aceita `CLOUD360_ENABLE_SECURE_HTTP=ON`, mais
`CLOUD360_CURL_XENON_DIR` e `CLOUD360_MBEDTLS_XENON_DIR`, para vincular
essas dependências. Por padrão o recurso está desativado até verificarmos
compatibilidade no Xbox 360 físico.

Os testes Linux validam a máquina de estados, tamanhos e falhas de TLS
simuladas, além da compilação e do comportamento sem CA usando libcurl
do PC. **Isso não demonstra que o TLS funciona no Xbox 360.**

### Interface e preferências

Navegação com controle, abas, cards, animações suaves e opções persistidas em
`game:/cloud360.cfg` já estão conectadas. Ainda precisam de testes em hardware.

## Catálogo JSON e estudo do Orbita

Foram implementados parsing de JSON próprio do Cloud360, armazenamento
local e download HTTPS opcional (src/catalog.c e src/catalog_runtime.c).
O app exibe até três cards por página na aba EXPLORAR. O catálogo
não está ligado às APIs Microsoft ou Xbox Live; somente dados
explicitamente fornecidos são mostrados. Ver CATALOG.md.

Orbita (github.com/sergiolimasks/orbita) é uma referência técnica
para a arquitetura completa, mas usa libxenon e ELF via XeLL,
enquanto Cloud360 usa XSDK e XEX via Aurora. Não é uma
implementação diretamente transplantável. Orbita é GPL-3.0 e
Cloud360 é MIT; não copiar código sem resolver licenciamento.
Ver ORBITA_REFERENCE.md.

## OAuth Microsoft por código — implementado em fonte

A aba CONTA possui OAuth 2.0 Device Authorization Grant (RFC 8628),
com código e endereço de verificação na TV, intervalo de consulta,
slow_down (+5 segundos), expiração, cancelamento e limpeza em memória.
O backend usa POST HTTPS com TLS verificado, mas curl-xenon/mbedtls-xenon
continuam desabilitados por padrão até testes no console.
O ID público do aplicativo precisa ser registrado pelo operador e
lido de game:/oauth-client.txt: não utilizamos IDs/segredos de
outros aplicativos. Tokens ficam somente em memória até o logout;
nenhuma persistência segura ou renovação foi implementada.
O resultado OAuth não comprova acesso ao Xbox Live/XSTS nem xCloud.
Leia [AUTH.md](AUTH.md).

## Xbox Live e XSTS — handshake implementado em fonte

O Xbox usa um fluxo em duas etapas após OAuth, conforme documentação GDK
da Microsoft. O módulo `src/xbl.c` constrói solicitações JSON de
`user.auth.xboxlive.com/user/authenticate` para trocar o access token
Microsoft por um **U-token**. Depois utiliza
`xsts.auth.xboxlive.com/xsts/authorize` para obter o X-token/XSTS genérico
de Xbox services (`RelyingParty=http://xboxlive.com`, sandbox RETAIL).

O módulo `src/xbl_runtime.c` isola a transação em thread, só aceita OAuth
autorizado, preserva os tokens somente em memória e descarta respostas
tardias após logout. A tela CONTA mostra o estado Xbox e os códigos de
erro XSTS relevantes, sem exibir segredos.

**Limites:** TLS XSDK não está habilitado/validado; Xbox XSTS ainda não foi
testado num Xbox 360; uma autorização XSTS genérica não prova direito de uso
do Game Pass, nem garante acesso às APIs específicas do xCloud.
Ver [XSTS.md](XSTS.md).

## Sessões de jogo — coordenador independente do transporte

O módulo `src/session.h`/`src/session.c` trata solicitação de
sessão, provisionamento, fila, servidor disponível, sinalização,
conexão, espera do primeiro quadro, reconexão e encerramento.
Uma sessão só passa a `STREAMING` após evento `FIRST_VIDEO`
gerado por um futuro receptor/decodificador real.

Cada tentativa tem identificador de geração, rejeitando respostas
atrasadas após cancelamento. Há limites de tempo separados para
solicitação, provisionamento, fila, sinalização, vídeo e parada,
além de máximo de duas reconexões. Consultar [SESSION.md](SESSION.md).

A aba **SESSAO** expõe os estados. O executável ainda não possui
um provedor de sessões conectado; pressionar A verifica apenas
os pré-requisitos locais XSTS/transporte e retorna erro explícito.
Não usa endpoints privados nem simula uma partida como funcional.

## WebRTC: infraestrutura SDP / ICE / STUN

Foi implementado o parser limitado `src/rtc.c`, que valida um SDP
de resposta, a presença de SHA-256 DTLS fingerprint, ICE ufrag/pwd,
BUNDLE, RTCP mux, codec H.264/90000 e candidatos IPv4/UDP.
A máquina de sessão recusa `PEER_CONNECTED` antes de receber
um SDP sintaticamente válido; isso **não substitui autenticação
DTLS nem teste real de conectividade ICE**.

`src/stun.c` constrói mensagens Binding com ID de transação
fornecido por CSPRNG do futuro backend e valida respostas STUN
IPv4/XOR-MAPPED-ADDRESS. O parser confere tamanho, cookie, ID,
atributos obrigatórios e CRC FINGERPRINT quando presente.
Não estabelece conectividade ICE (faltam autenticação HMAC,
consentimento, nomination e sockets UDP).

Há testes de pacotes/candidatos/SDP falsos em Linux e verificação
de sintaxe dos fontes com XSDK; nenhum endpoint Microsoft foi
contatado e nenhum XEX foi compilado. Ver [WEBRTC.md](WEBRTC.md).

## Transporte IPv4/UDP e MESSAGE-INTEGRITY para ICE

Adicionamos `src/udp.c` com socket IPv4 não bloqueante, envio
e recebimento com limite de 1500 bytes, controle de origem e
timeout por `poll()`. Um teste Linux faz um round-trip UDP apenas
pela interface loopback; isso **não valida sockets UDP no Xbox 360**.

O módulo `src/ice.c` monta verificações ICE com credenciais
username fragment (remote:local), priority, papel controlling/
controlled, tiebreaker e atributo HMAC-SHA1 MESSAGE-INTEGRITY.
Verifica respostas STUN com transaction ID, XOR-MAPPED-ADDRESS,
HMAC e FINGERPRINT quando presente. A operação exige uma
implementação HMAC externa; o adaptador `src/ice_crypto.c`
usa mbedTLS somente se explicitamente configurado junto
das bibliotecas TLS de Xbox. Sem suporte criptográfico, o
código recusa os checks, sem fallback inseguro.

**Isso ainda não é ICE estabelecido nem conexão xCloud.**
Faltam checklist de candidatos, timer de retransmissão,
papéis, TURN, consentimento, DTLS-SRTP e um serviço de sessão
autorizado. A produção não abre sockets nem declara
`PEER_CONNECTED` com base na simples verificação HMAC.
Ver [ICE_UDP.md](ICE_UDP.md).

## Agendador de checks ICE e priorização de candidatos

`src/ice_agent.c` recebe até oito candidatos IPv4/UDP,
prioriza pela prioridade anunciada, cria IDs de transação via
CSPRNG injetado, gera STUN Binding autenticado e mantém
intervalos de retransmissão controlados. Cada candidato recebe
até cinco envios. Respostas recebidas são aceitas apenas com
endereço/porta de origem idênticos ao par atual, transaction ID
correto, HMAC válido, prazo em vigor e mesma geração da tentativa.
Falha um par, testa o próximo e encerra quando esgotados.

Um check aprovado marca somente `VERIFIED_NOT_NOMINATED`;
a máquina `session.c` **não é avançada** com base nesse estado.
O agendador não é iniciado pelo `main.c` e o aplicativo continua
sem conexão xCloud. Testes Linux utilizam um CSPRNG simulado apenas
no próprio teste, OpenSSL para HMAC e callback de envio fictício.
Faltam nomeação, controle de papéis ICE, consent freshness,
DTLS-SRTP e decodificação de mídia no console.
Ver [ICE_UDP.md](ICE_UDP.md).

## Etapas técnicas restantes

| Prioridade | Subsistema | Resultado esperado |
|---|---|---|
| 1 | HTTPS/TLS validado no hardware | Cliente HTTPS implementado em fonte; falta build com curl-xenon/mbedtls-xenon e teste real de CA/relógio |
| 2 | Cliente HTTP/JSON | Parser e catálogo próprios implementados; integração TLS em hardware e fonte oficialmente autorizada continuam pendentes |
| 3 | Autenticação OAuth Microsoft | Máquina de estados e POST em fonte; falta validar TLS no Xbox, tokens Xbox/XSTS e renovação/revogação |
| 4 | Xbox Live/XSTS | Handshake e erros implementados em fonte; falta validação em hardware e permissões de serviço |
| 5 | Catálogo e busca | Dados reais autorizados carregados da rede, paginação, filtros, imagens otimizadas |
| 5 | Biblioteca e favoritos | Estado da conta sincronizado, cache local com integridade |
| 6 | Sessões na nuvem | Máquina de estados com fila, timeouts e reconexão; faltam provedor autorizado e testes físicos |
| 7 | WebRTC e transporte | SDP, checks HMAC, UDP e agendador de até 8 pares com failover; faltam ICE completo, DTLS-SRTP e hardware |
| 8 | Vídeo e áudio | Viabilidade de decodificar H.264 e áudio no hardware Xenon |
| 9 | Controle e desempenho | Envio de input, latência, reconexão, métricas e robustez |
| 10 | Testes finais do XEX | Testes e diagnóstico na Aurora e no console Xbox 360 físico |

A compatibilidade do xCloud depende de viabilidade técnica e das interfaces
permitidas pela Microsoft. Não há garantia de que uma sessão de jogos será
viável no hardware do Xbox 360.

## Política de compilação

- Em um commit ou PR: workflow **Cloud360 Source Tests (No XEX)** — somente
  testes C compilados para Linux e verificação de sintaxe dos fontes.
- O workflow **Build Cloud360 XEX** só aceita
  `workflow_dispatch` (execução manual).
- Nenhum token OAuth, senha, cookie ou dado privado entra no GitHub, em
  repositórios públicos ou nos logs.

## Referências

- [XSDK — Network](https://github.com/xenosworks/xsdk/blob/main/include/xsdk/net.h)
- [XSDK — Guide: Sockets](https://github.com/xenosworks/xsdk/blob/main/docs/GUIDE.md)
