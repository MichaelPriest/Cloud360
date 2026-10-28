# Cloud360 — execução experimental no Xenia Canary

O Cloud360 0.2.1 possui um executável específico para o emulador: 
`cloud360_xenia.xex`. A edição Xbox 360 (`cloud360.xex`) continua
separada. Nenhuma delas realiza streaming xCloud ainda.

## Por que dois arquivos?

O XSDK usa o controlador de vídeo Xenos físico para `xsdk_present_open`.
O Xenia não implementa esse controlador e retorna STATUS_NOT_SUPPORTED
(`0xC00000BB`). A edição Xenia evita o splash e o controlador de vídeo,
aloca dois frontbuffers e usa a implementação de `VdSwap` da GPU do XSDK
para enviar os comandos PM4_XE_SWAP ao emulador. Esse backend depende de uma
função interna do XSDK e ainda precisa de teste com o Xenia real.

## Como abrir no Windows

1. Obtenha a versão atual do **Xenia Canary** do projeto oficial.
2. Faça download de `Cloud360-Xenia-XEX` na execução GitHub Actions.
3. Extraia e localize `cloud360_xenia.xex`. Não abra `cloud360.xex` no Xenia.
4. Execute no PowerShell, ajustando os caminhos:

```powershell
& "C:\Xenia\xenia_canary.exe" "C:\Cloud360\cloud360_xenia.xex" --async_shader_compilation=false --readback_resolve=full
```

Também é possível usar `scripts/Iniciar-Cloud360-Xenia.bat` ao colocá-lo na
mesma pasta do XEX e do executável Xenia Canary (ou no PATH do Windows).
O parâmetro para compilação síncrona de shaders ajuda a evitar quadros
incompletos no primeiro carregamento; readback completo ajuda a depurar.

## Logs se não abrir / tela preta

- `cache\cloud360_xenia.log` na pasta usada pelo Xenia.
- `xenia.log` gerado pelo emulador.
- Criar um arquivo vazio `dbgprint.txt` ao lado do XEX pode encaminhar
  o log da biblioteca XSDK para o `xenia.log`.

Procure por `XENIA: display controller bypass enabled`, 
`XENIA: VdSwap backend ready` e `XENIA: submitted frame 1 via VdSwap`.
Esse último registro confirma apenas que o swap foi **enviado**, não que
a imagem foi renderizada corretamente.

## Estado real de compatibilidade

O CI compila XEX2 e valida buffers, sincronização e recursos da interface.
Ele **não executa o Xenia Canary com GPU**, logo não certifica resultado
gráfico, FPS ou estabilidade no emulador. Isso depende de um teste local.
A versão de console RGH/JTAG mantém seu próprio backend nativo.

Fontes: https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md
e https://github.com/xenia-project/xenia/blob/master/src/xenia/kernel/xboxkrnl/xboxkrnl_video.cc
