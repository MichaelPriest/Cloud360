# Cloud360 — protótipo nativo para Xbox 360

**Estado atual:** scaffold inicial em C para explorar um aplicativo XEX no Xbox 360 RGH/JTAG. **Ainda não é um cliente funcional do Xbox Cloud Gaming (xCloud)** e este repositório não contém um XEX compilado.

## Objetivo desta etapa

Criar uma primeira aplicação nativa que possa ser iniciada pela Aurora sem iniciar o XeLL manualmente. O projeto usa a toolchain comunitária [xsdk](https://github.com/xenosworks/xsdk).

O código tenta mostrar a tela de splash, inicializar a GPU e o vídeo em 720p, limpar a tela em cada frame e encerrar com o botão **Back**. As chamadas usadas foram comparadas com os cabeçalhos e o exemplo oficial atual do xsdk, mas **ainda não compilamos nem testamos este projeto**.

## O que está incluído

- Código-fonte em C.
- CMake configurado para produzir um XEX.
- Instruções para preparar o ambiente no Windows com WSL/Debian.
- Referências técnicas para as próximas etapas.

## O que ainda não está implementado

- Menu, catálogo ou interface completa.
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

\`\`\`powershell
wsl --install -d Debian
\`\`\`

Abra o Debian, crie seu usuário Linux e instale as dependências:

\`\`\`bash
sudo apt update
sudo apt install -y build-essential bison flex texinfo libgmp-dev libmpfr-dev libmpc-dev curl xz-utils patch python3 cmake ninja-build git
\`\`\`

A documentação atual do xsdk exige CMake 3.25 ou mais recente e Python 3.11 ou mais recente.

## 2. Baixar e compilar o xsdk

Execute dentro do Debian. É recomendável trabalhar dentro do filesystem Linux (`~/projects`), não em `/mnt/c`, para que a compilação seja mais rápida.

\`\`\`bash
git clone https://github.com/xenosworks/xsdk.git ~/xsdk
cd ~/xsdk
PREFIX=$HOME/xsdk-gcc WORK=$HOME/xsdk-gcc-work JOBS=$(nproc) tools/toolchain/build.sh
make -j"$(nproc)"
\`\`\`

O primeiro comando de build prepara o compilador cruzado. O comando `make` compila a biblioteca xsdk e as ferramentas usadas pelos títulos. Pode levar alguns minutos e usar vários gigabytes. Se ocorrer uma falha, pare e examine a primeira mensagem de erro e os logs antes de repetir.

## 3. Baixar e compilar o Cloud360

\`\`\`bash
mkdir -p ~/projects
cd ~/projects
git clone https://github.com/MichaelPriest/Cloud360.git
cd Cloud360
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/xsdk/cmake/xsdk.cmake" \
  -DXSDK_TOOLCHAIN="$HOME/xsdk-gcc" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
\`\`\`

Se tudo compilar corretamente, o alvo está configurado para gerar:

\`\`\`text
build/cloud360.xex
\`\`\`

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

1. Compilar o scaffold e corrigir qualquer erro real.
2. Testar a inicialização na Aurora.
3. Criar a interface de navegação.
4. Pesquisar separadamente a viabilidade de autenticação, rede e streaming do xCloud.

Cloud360 é um projeto independente, não afiliado nem endossado pela Microsoft, Xbox ou pelos projetos de referência.
