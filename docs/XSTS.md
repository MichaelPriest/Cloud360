# Cloud360 — Xbox services U-token e XSTS

## Objetivo

Implementar o passo seguinte ao OAuth autorizado pelo dono da conta, usando
apenas os endpoints Xbox services documentados pela Microsoft:

1. `POST https://user.auth.xboxlive.com/user/authenticate`
   - Cabeçalho `x-xbl-contract-version: 1`
   - JSON com `RelyingParty=http://auth.xboxlive.com`, `TokenType=JWT`,
     `Properties.AuthMethod=RPS`, `SiteName=user.auth.xboxlive.com` e
     `RpsTicket=d=[access_token Microsoft]`.
   - Resposta: U-token e `DisplayClaims.xui[0].uhs`.
2. `POST https://xsts.auth.xboxlive.com/xsts/authorize`
   - `Properties.UserTokens=[U-token]`, `SandboxId=RETAIL`,
     `RelyingParty=http://xboxlive.com`, `TokenType=JWT`.
   - Resposta: token XSTS opaco ou `XErr` de autorização.

Esses endereços são para **Xbox services genéricos**, não são endpoints
privados de streaming. Um XSTS bem-sucedido **não autoriza automaticamente
Game Pass ou xCloud**.

## Implementação no Cloud360

- `src/xbl.h` / `src/xbl.c`: composição JSON limitada, validação de
  caracteres, resposta aninhada, códigos de falha, limpeza de memória.
- `src/xbl_runtime.h` / `src/xbl_runtime.c`: handshake assíncrono de
  duas etapas, uma tentativa por vez, validade da sessão OAuth e descarte
  das respostas após cancelamento ou logout.
- `src/https_curl.c`: método POST JSON com `Content-Type: application/json`,
  `x-xbl-contract-version: 1`, CA e hostname TLS obrigatórios.
- Aba **CONTA**: após OAuth autorizado, pressione A para solicitar o XSTS.
  O estado e erros aparecem na tela sem revelar tokens.
- Botão X cancela OAuth/XSTS; botão Y sai e apaga os tokens em memória.
- Testes de GCC/Linux não contatam Microsoft: simulam HTTPS, autorização,
  erros de conta, regiões, falhas TLS e cancelamento.

**Não publicar** `access_token`, `refresh_token`, `U-token`, `XSTS`,
`device_code` ou `uhs` em logs, arquivos ou GitHub.

## Erros reconhecidos

| XErr | Interpretação aproximada |
|---|---|
| 2148916233 | Conta sem perfil Xbox Live |
| 2148916235 | Possível restrição regional |
| 2148916238 | Conta infantil/familiar sem autorização |

As mensagens são diagnósticos e não substituem o motivo oficial retornado
pelo servidor. Outros erros são apresentados genericamente.

## Limitações atuais

O XSDK e o Xbox 360 real ainda não foram testados com o backend
curl-xenon/mbedtls-xenon ativado. O acesso a `xboxlive.signin` depende das
permissões do aplicativo Microsoft registrado. Essa etapa não implementa
renovação de token, catálogo Game Pass, sessão xCloud, WebRTC, áudio ou vídeo.
O build XEX permanece manual, e **nenhum executável deve ser gerado nesta
fase de desenvolvimento**.

## Referências oficiais

- [Microsoft GDK — Xbox services sign-in for title websites](https://learn.microsoft.com/en-us/xbox/gdk/docs/services/fundamentals/s2s-auth-calls/service-authentication/live-website-authentication)
- [Microsoft GDK — Title service calls](https://learn.microsoft.com/en-us/gaming/gdk/docs/services/fundamentals/s2s-auth-calls/s2s-calls/live-title-service-calls-xbox-live)
