# Cloud360 — Autorização Microsoft por código na TV

**Status:** código-fonte e testes automatizados; **não validado em Xbox 360 real**.
O projeto continua usando XSDK/XEX para Aurora. Nenhum XEX deve ser gerado
antes de o usuário solicitar. Esta fase não contém streaming nem integração
Xbox Live/XSTS.

## Fluxo

O Cloud360 implementa o protocolo padronizado **OAuth 2.0 Device
Authorization Grant (RFC 8628)**, documentado oficialmente pela Microsoft.

1. Na aba **CONTA**, pressione **A** para solicitar um código.
2. Se as bibliotecas de TLS estiverem habilitadas e o cliente estiver
   registrado, o aplicativo faz um POST HTTPS de autorização.
3. O console mostra apenas `verification_uri` e `user_code`. O usuário
   visita o endereço no celular/computador e autoriza a própria conta.
4. O cliente consulta o endpoint de token respeitando o intervalo
   `interval`. Respostas `authorization_pending` aguardam; `slow_down`
   soma cinco segundos a todos os intervalos futuros.
5. Em caso de autorização, somente o estado OAuth é marcado como
   autorizado. Os tokens de acesso e de renovação permanecem
   temporariamente na memória do processo — não em arquivo.
6. **X** cancela; **Y** sai da conta e apaga os tokens da memória.
   Códigos expirados ou recusados também interrompem as consultas.

O botão **B** apenas retorna ao menu principal. As consultas continuam
em segundo plano até sua conclusão, expiração ou cancelamento explícito.

## Configuração do aplicativo Microsoft

Registre um aplicativo **seu** na plataforma de identidade Microsoft
com suporte a clientes públicos e fluxo de dispositivo, conforme
permissões e condições aplicáveis. Em particular, as permissões Xbox
`xboxlive.signin` dependem da configuração e das políticas Microsoft:
**não há garantia de que serão concedidas a qualquer aplicativo novo**.

Crie `game:/oauth-client.txt` contendo somente o identificador de
aplicativo (GUID público, 36 caracteres), uma linha. Exemplo de forma
**não operacional**:

```text
00000000-0000-0000-0000-000000000000
```

Não use o identificador de outro aplicativo (Orbita, Green-NX, navegador,
etc.) e não inclua `client_secret`. O arquivo acima é apenas um
exemplo de formato e não autoriza um login.

A conexão exige também `game:/cacert.pem` confiável e data/hora
correta do console. O suporte curl-xenon/mbedtls-xenon continua
desativado na configuração padrão do CMake, portanto uma versão
sem as dependências deve falhar com `HTTPS/LOGIN INDISPONIVEL`.

## Implementação

- `src/oauth.h`, `src/oauth.c`: estados, prazos e cancelamento,
  sem acesso à rede.
- `src/oauth_json.h`, `src/oauth_json.c`: parsing limitado de respostas
  e erros de OAuth, sem exibir segredos.
- `src/oauth_runtime.h`, `src/oauth_runtime.c`: thread de requisição
  em segundo plano, códigos de geração para descartar respostas depois
  do cancelamento, armazenamento volátil dos tokens.
- `src/https_curl.c`: POST de formulário com validação TLS e hostname,
  redirect desabilitado, corpo limitado a 16 KiB e timeout.
  Respostas OAuth HTTP 400 podem conter erros JSON; elas são mantidas
  **somente se o TLS foi validado e o transporte concluiu**.

Os testes usam IDs fictícios e servidor simulado. Nenhuma credencial
real é enviada pelo GitHub Actions.

## Limitações e segurança

- **O handshake Xbox Live/XSTS genérico agora está implementado em outro
  módulo, mas não foi validado no hardware. Não há acesso aos serviços
  específicos do xCloud, confirmação de assinatura Game Pass, sessões de
  jogo ou WebRTC.**
- Este módulo não implementa ainda renovação automática, revogação
  remota de tokens ou persistência criptografada; sair limpa a memória.
- O cliente não solicita nem armazena senha. Não registre
  `device_code`, `access_token` ou `refresh_token` nos logs.
- Uma consulta HTTPS em andamento pode terminar depois de pressionar
  cancelar; sua resposta é descartada usando o número de geração.
- A compatibilidade das bibliotecas curl-xenon, do relógio/certificados
  e da OAuth Microsoft em hardware real precisa ser validada.

## Referências

- Microsoft, fluxo Device Authorization:
  https://learn.microsoft.com/en-us/entra/identity-platform/v2-oauth2-device-code
- RFC 8628: https://www.rfc-editor.org/rfc/rfc8628.html
- Orbita (referência arquitetural GPL-3.0, código não copiado):
  https://github.com/sergiolimasks/orbita

**Foco atual:** desenvolvimento de fontes e testes, sem XEX.
