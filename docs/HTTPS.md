# Cloud360 — HTTPS verificável, sem criar XEX nesta fase

## Objetivo

A aba REDE possui um cliente HTTPS de teste, restrito a
`https://example.com/` — endereço público para verificar HTTPS, não uma
API Microsoft. O código foi criado para Xbox 360 com RGH/JTAG, usando XSDK.

**O backend seguro não está habilitado no projeto por padrão:** a
configuração depende das bibliotecas curl-xenon e mbedtls-xenon. A execução
no Xbox 360 ainda não foi validada. Sem backend, não há fallback HTTP.

## Código disponível

- `src/https.c`: validação de URL e estado assíncrono sem bloquear o controle.
- `src/https_curl.c`: libcurl com HTTPS-only e verificação de cadeia e
  hostname, TLS 1.2, redirects desabilitados e limites de tamanho/tempo.
- `src/https.h`: tipos, estados, limites e interface de transporte.
- `tests/test_https.c`: testes de sucessos/erros, transporte lento e limite.
- `tests/test_https_curl.c`: verifica que um arquivo CA ausente impede a
  tentativa e que não há fallback quando a biblioteca está desabilitada.

## Dependências para integrar quando for gerar executável

- [curl-xenon](https://github.com/xenosworks/curl-xenon),
  versão branch `xenon`, sobre mbedtls-xenon.
- [mbedtls-xenon](https://github.com/xenosworks/mbedtls-xenon),
  versão branch `xenon`, com TLS 1.2 para XSDK.
- `cacert.pem` de fonte confiável, mantido ao lado do aplicativo; consultar
  [curl CA extract](https://curl.se/docs/caextract.html).
- Configurar data/hora corretas no console para validação dos certificados.

Os fontes devem usar a mesma versão do XSDK no futuro build; não copiar
bibliotecas binárias do Windows ou usar certificados de teste em produção.

Para habilitar o código, o CMake possui as opções:
```bash
-DCLOUD360_ENABLE_SECURE_HTTP=ON
-DCLOUD360_MBEDTLS_XENON_DIR=/caminho/mbedtls-xenon
-DCLOUD360_CURL_XENON_DIR=/caminho/curl-xenon
```

**Nenhum comando de geração de XEX foi executado nesta etapa.**
O GitHub Actions de commits executa somente testes de C no Linux e
verificação de sintaxe. Quando chegarmos à fase de testes em hardware,
será necessário compilar as dependências de origem, testar HTTPS com
cadeias válidas e inválidas e medir memória/tempo de handshake.

## Segurança

- Nenhum token, cookie, senha ou identificador Xbox é enviado.
- `CURLOPT_SSL_VERIFYPEER=1` e `CURLOPT_SSL_VERIFYHOST=2` obrigatórios.
- Nenhum protocolo diferente de HTTPS, redirecionamento ou proxy.
- Tempo total máximo de 15 segundos, corpo máximo de 16 KiB.
- A falha de certificados ou a ausência de CA **impede** a conexão.
- Nenhuma funcionalidade de login ou streaming é anunciada como disponível.

Referências: [curl-xenon / XSDK](https://github.com/xenosworks/curl-xenon/blob/xenon/docs/README-xsdk.md)
e [mbedtls-xenon / XSDK](https://github.com/xenosworks/mbedtls-xenon/blob/xenon/docs/README-xsdk.md).
