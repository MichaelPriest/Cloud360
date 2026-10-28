# Cloud360 — protótipo nativo para Xbox 360

**Estado atual:** protótipo com identidade visual **Azul Cloud360**, dashboard 720p, cards, painel de informações, seleção e transições animadas em C; navegação pelo direcional e botões A, B e Back, preparado para compilar como XEX para Xbox 360 RGH/JTAG. **Ainda não é um cliente funcional do Xbox Cloud Gaming (xCloud)** e o XEX ainda precisa ser compilado e validado.

## Objetivo desta etapa

Criar uma primeira aplicação nativa que possa ser iniciada pela Aurora sem iniciar o XeLL manualmente. O projeto usa a toolchain comunitária [xsdk](https://github.com/xenosworks/xsdk).

O código foi escrito para mostrar a tela de splash, inicializar GPU e vídeo em 720p, desenhar a dashboard Azul Cloud360 com a API de texto do XSDK e permitir navegação animada pelo controle. As opções de streaming exibem avisos de que ainda não estão implementadas. **A compilação e o teste em hardware ainda estão pendentes.**

## O que está incluído

- Código-fonte em C.
- CMake configurado para produzir um XEX.
- Testes automatizados da navegação e das animações com compilador nativo no GitHub Actions.
- Renderizador GPU com fonte bitmap do XSDK para fundos, cards e destaques; sem texturas externas.
- Instruções para preparar o ambiente no Windows com WSL/Debian.
- Referências técnicas para as próximas etapas.

## Interface Azul Cloud360 (protótipo)

- **Identidade:** fundo azul-marinho, azul elétrico de destaque, texto claro e painéis escuros.
- **Layout:** 1280×720, navegação vertical, hero, três cards de recursos e modal de detalhes.
- **Controles:** direcional para escolher, A para abrir detalhes, B para fechar e Back para sair.
- **Animações:** cursor com easing cúbico (~240 ms), entrada do conteúdo, abertura de modal (~220 ms) e iluminação ambiente discreta.
- **Desempenho:** animações baseadas em tempo decorrido, limitando saltos após pausa; fonte GPU do XSDK, sem imagens externas.
- **Estado:** nenhuma opção inicia streaming. As telas informam que esses serviços ainda não foram implementados.

## O que ainda não está implementado

- Login Microsoft/Xbox, OAuth ou XSTS.
- Integração com o catálogo do Game Pass.
- Sessão xCloud, WebRTC, decodificação H.264/Opus ou streaming.
- Binário XEX compilado e validado em hardware.

**Nunca publique senhas, cookies, tokens ou credenciais de conta no repositório.**

## Requisitos

- Windows 10/11 com WSL 2 e Debian, ou Linux.
- Cerca de 10 GB livres para compilar a toolchain.
- VS Code com extensão WSL (opcional, mas recomendado).
- Xbox 360 com RGH/JTAG e Aurora para testes posteriores.

## 1. Instalar WSL/Debian

No PowerShell do Windows, execute:

```powershell
wsl --install -d Debian
```

Abra o Debian, crie seu usuário Linux e instale as dependências:

```bash
sudo apt update
sudo apt install -y build-essential bison flex texinfo libgmp-dev libmpfr-dev libmpc-dev curl xz-utils patch python3 cmake ninja-build git
```

A documentação atual do xsdk exige CMake 3.25 ou mais recente e Python 3.11 ou mais recente.

## 2. Baixar e compilar o xsdk

Execute dentro do Debian. É recomendável trabalhar dentro do filesystem Linux (`~/projects`), não em `/mnt/c`, para que a compilação seja mais rápida.

```bash
git clone https://github.com/xenosworks/xsdk.git ~/xsdk
cd ~/xsdk
PREFIX=$HOME/xsdk-gcc WORK=$HOME/xsdk-gcc-work JOBS=$(nproc) bash tools/toolchain/build.sh
make -j"$(nproc)"
```

O primeiro comando de build prepara o compilador cruzado. O comando `make` compila a biblioteca xsdk e as ferramentas usadas pelos títulos. Pode levar bastante tempo e usar vários gigabytes. Se ocorrer uma falha, pare e examine a primeira mensagem de erro e os logs antes de repetir.

## 3. Baixar e compilar o Cloud360

```bash
mkdir -p ~/projects
cd ~/projects
git clone https://github.com/MichaelPriest/Cloud360.git
cd Cloud360
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/xsdk/cmake/xsdk.cmake" \
  -DXSDK_TOOLCHAIN="$HOME/xsdk-gcc" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Se tudo compilar corretamente, o alvo está configurado para gerar:

```text
build/cloud360.xex
```

Este é o resultado esperado, não uma garantia. Se o build falhar, copie aqui o erro completo desde a primeira mensagem de falha; podemos corrigir o código com base no erro real.

## 4. Testar no Xbox 360

Somente depois de gerar o XEX com sucesso:

1. Faça backup dos arquivos importantes do console.
2. Copie `cloud360.xex` para uma pasta própria no HDD ou USB.
3. Na Aurora, adicione ou localize o executável e tente iniciá-lo.
4. Confira se a tela aparece e se o botão Back encerra o aplicativo.
5. Se não abrir, anote o erro e, se disponível, o log ao lado do XEX.

Um build bem-sucedido não garante que o programa funcione em todas as configurações. O teste final precisa ocorrer em um Xbox 360 RGH/JTAG ou em um ambiente de emulação compatível.

## Referências técnicas

- [xsdk — toolchain e exemplos](https://github.com/xenosworks/xsdk)
- [Guia de instalação do xsdk](https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md)
- [Xenonlight](https://github.com/roderick-neuhoff/Xenonlight) — referência para streaming de PC/Sunshine no Xbox 360, não é cliente xCloud.
- [Orbita](https://github.com/sergiolimasks/orbita) — projeto independente relacionado a xCloud no Xbox 360; referência técnica separada, não incorporada neste repositório.

## Próximas etapas

1. Compilar o protótipo e corrigir qualquer erro real.
2. Testar a inicialização e o menu na Aurora.
3. Implementar e testar navegação adicional, estados e configurações.
4. Pesquisar separadamente a viabilidade de autenticação, rede e streaming do xCloud.

Cloud360 é um projeto independente, não afiliado nem endossado pela Microsoft, Xbox ou pelos projetos de referência.
