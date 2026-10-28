# Cloud360 — protótipo nativo para Xbox 360

**Estado atual:** scaffold inicial em C para pesquisa e desenvolvimento de um aplicativo XEX para Xbox 360 RGH/JTAG. **Ainda não é um cliente funcional do Xbox Cloud Gaming (xCloud)** e não inclui um arquivo `.xex` compilado.

## Objetivo desta primeira etapa

Preparar a base de um aplicativo que possa ser iniciado pela Aurora, sem depender de iniciar o XeLL manualmente. O projeto usa a toolchain comunitária aberta [xsdk](https://github.com/xenosworks/xsdk) como ponto de partida.

O protótipo tenta inicializar a GPU e o vídeo em 720p, manter uma tela de fundo e encerrar ao pressionar **Back**. A compatibilidade das chamadas de API ainda precisa ser conferida e o projeto precisa ser compilado e testado em um Xbox 360 RGH/JTAG.

## O que está incluído

- Código-fonte em C.
- Configuração CMake para empacotar o aplicativo como XEX com xsdk.
- Instruções para preparar WSL/Debian no Windows.
- Referências técnicas para desenvolvimento homebrew.

## O que ainda não está implementado

- Interface de catálogo ou navegação.
- Login Microsoft/Xbox, OAuth ou XSTS.
- Integração com catálogo do Game Pass.
- Sessão xCloud, WebRTC, decodificação H.264/Opus ou streaming.
- Binário XEX pré-compilado e validado em hardware.

**Não coloque senhas, cookies, tokens ou credenciais da sua conta no repositório.**

## Requisitos de desenvolvimento

- Windows 10/11 com WSL 2 e Debian.
- VS Code com extensão WSL.
- Git, CMake, Ninja e ferramentas de compilação.
- Xbox 360 com RGH/JTAG e Aurora, para teste posterior.
- Cerca de 10 GB livres para a toolchain e dependências, conforme as necessidades do ambiente.

## 1. Preparar o WSL

Abra o PowerShell e execute:

```powershell
wsl --install -d Debian
```

Depois de abrir o Debian e criar seu usuário Linux:

```bash
sudo apt update
sudo apt install -y build-essential bison flex texinfo libgmp-dev libmpfr-dev libmpc-dev curl xz-utils patch python3 cmake ninja-build git unzip
```

## 2. Obter o Cloud360

Clone este repositório dentro do terminal Debian:

```bash
mkdir -p ~/projects
cd ~/projects
git clone https://github.com/MichaelPriest/Cloud360.git
cd Cloud360
code .
```

É recomendável compilar dentro do filesystem do Linux, em vez de `/mnt/c`, para evitar builds mais lentos.

## 3. Preparar a toolchain xsdk

Os comandos abaixo seguem a abordagem documentada pelo projeto xsdk. Eles baixam e compilam projetos externos; não incluem nem distribuem arquivos proprietários do Xbox 360 SDK.

```bash
git clone https://github.com/xenosworks/xsdk.git ~/xsdk
cd ~/xsdk
PREFIX=$HOME/xsdk-gcc WORK=$HOME/xsdk-gcc-work JOBS=$(nproc) tools/toolchain/build.sh
```

Compile o compilador de shaders compatível:

```bash
git clone --depth 1 --branch xenon https://github.com/xenosworks/glslang-xenon.git ~/glslang-xenon
cd ~/glslang-xenon
python3 update_glslang_sources.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_OPT=ON -DSPIRV_SKIP_EXECUTABLES=ON -DGLSLANG_TESTS=OFF
cmake --build build
cp build/StandAlone/glslang ~/xsdk-gcc/bin/
```

Compile o xsdk:

```bash
cd ~/xsdk
make -j"$(nproc)"
```

A compilação pode demorar e consumir vários gigabytes. Se ocorrer um erro, pare e investigue a primeira mensagem de falha antes de continuar.

## 4. Tentar compilar o protótipo

Na raiz do repositório:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$HOME/xsdk/cmake/xsdk.cmake" \
  -DXSDK_TOOLCHAIN="$HOME/xsdk-gcc"
cmake --build build
```

Se configuração e compilação forem bem-sucedidas, o alvo foi configurado para produzir `build/cloud360.xex`. Esse resultado é esperado, não garantido: **o código deste scaffold ainda não foi compilado nem validado** contra a versão atual da toolchain.

## 5. Teste no console

Somente depois de um build bem-sucedido:

1. Faça backup dos arquivos importantes do console.
2. Copie o XEX gerado para uma pasta própria no HDD ou USB.
3. Na Aurora, localize e execute o XEX.
4. Registre qualquer erro ou comportamento inesperado.

Um build bem-sucedido não garante compatibilidade com todos os consoles ou configurações. Testes finais precisam ser feitos no hardware RGH/JTAG.

## Referências técnicas

- [xsdk — toolchain e documentação](https://github.com/xenosworks/xsdk)
- [Guia de instalação do xsdk](https://github.com/xenosworks/xsdk/blob/main/docs/GETTING_STARTED.md)
- [Xenonlight — streaming de PC/Sunshine para Xbox 360](https://github.com/roderick-neuhoff/Xenonlight) — referência arquitetural, não é um cliente xCloud.
- [Orbita — projeto independente de xCloud para Xbox 360](https://github.com/sergiolimasks/orbita) — referência técnica; este repositório não incorpora o código do projeto.

## Próximas etapas

1. Confirmar as assinaturas das APIs xsdk usadas no código.
2. Compilar e corrigir erros de toolchain e código.
3. Validar a inicialização e entrada do controle na Aurora.
4. Criar uma interface navegável.
5. Investigar separadamente a viabilidade técnica e os requisitos de autenticação/rede para streaming xCloud.

Cloud360 é um projeto independente, não afiliado nem endossado pela Microsoft, Xbox ou pelos projetos de referência citados.
