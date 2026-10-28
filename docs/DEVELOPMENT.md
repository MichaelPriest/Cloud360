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

### Interface e preferências

Navegação com controle, abas, cards, animações suaves e opções persistidas em
`game:/cloud360.cfg` já estão conectadas. Ainda precisam de testes em hardware.

## Etapas técnicas restantes

| Prioridade | Subsistema | Resultado esperado |
|---|---|---|
| 1 | Diagnóstico de DNS/TCP/TLS | Estabelecer conectividade segura, validar certificados e timeouts no console |
| 2 | Cliente HTTP/JSON | Requisições seguras autenticadas, limites de memória e erros tratados |
| 3 | Autenticação OAuth Microsoft | Fluxos públicos/autorizados, consentimento do usuário, armazenamento seguro de tokens |
| 4 | Catálogo e busca | Dados reais carregados da rede, paginação, filtros, imagens otimizadas |
| 5 | Biblioteca e favoritos | Estado da conta sincronizado, cache local com integridade |
| 6 | Sessões na nuvem | Investigar APIs e protocolos autorizados; não presumir endpoints privados |
| 7 | WebRTC e transporte | Sinalização, ICE/STUN/TURN, DTLS/SRTP, baixa latência |
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
