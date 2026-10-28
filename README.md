# Cloud360 — protótipo nativo para Xbox 360

**Estado atual:** aplicativo experimental para Xbox 360 RGH/JTAG com interface Azul Cloud360 **0.2**, inspirada na organização da prévia web do Xbox Cloud Gaming (2026). Inclui navegação superior, categorias, cards, ícones próprios, configurações reais e animações suaves. **Não é ainda um cliente funcional do xCloud:** não há login Microsoft, catálogo conectado, vídeo nem streaming. O **build XEX da versão 0.2 foi concluído com sucesso** no GitHub Actions em 28/10/2026; a execução em hardware ainda precisa ser testada.

## Objetivo desta etapa

Criar uma primeira aplicação nativa que possa ser iniciada pela Aurora sem iniciar o XeLL manualmente. O projeto usa a toolchain comunitária [xsdk](https://github.com/xenosworks/xsdk).

O código mostra o splash, inicializa a GPU e o vídeo em 720p, desenha a dashboard Azul Cloud360 com a API de texto do XSDK e permite navegação animada e configuração pelo controle. As opções de streaming exibem avisos de que ainda não estão implementadas. **O build XEX foi aprovado; o teste em console real ainda está pendente.**

## O que está incluído

- Código-fonte em C.
- CMake configurado para produzir um XEX.
- Testes automatizados da navegação, animações, preferências e ícones com compilador nativo no GitHub Actions.
- Renderizador GPU com fonte bitmap do XSDK para fundos, cards, ícones e destaques; sem texturas externas.
- Instruções para preparar o ambiente no Windows com WSL/Debian.
- Referências técnicas para as próximas etapas.

## Interface Azul Cloud360 v0.2

Inspirada na organização da prévia web do Xbox Cloud Gaming: barra superior, destaque editorial, cards de categorias, espaço para biblioteca e busca. **Não reproduz artes, logotipos ou código da Microsoft.** Utiliza ícones originais em pixel art (caracteres de bloco da fonte XSDK).

- **Tela:** 1280×720, tema azul-marinho/azul elétrico, indicadores de seleção e janelas de aviso.
- **Abas:** Início, Explorar, Buscar, Biblioteca, Ajustes, Rede e Sobre.
- **Controle:** esquerda/direita escolhem aba, baixo ou A entra nos cards, esquerda/direita troca card, cima ou B retorna. LB/RB mudam aba de qualquer lugar; Back encerra o aplicativo. O analógico esquerdo também navega.
- **Configurações disponíveis:** animações ligadas/desligadas, iluminação ambiente ligada/desligada, alto contraste, dicas na tela e restaurar padrões. Altere com A ou direita, e B retorna. As opções alteram a interface de verdade.
- **Persistência:** `game:/cloud360.cfg` na pasta do executável, sem salvar senha ou token. Se a mídia estiver protegida contra gravação, a interface mostra o aviso e os ajustes permanecem ativos apenas durante a execução.
- **Animação:** transições de aba e foco com easing cúbico, entrada de painel e modal, iluminação ambiente discreta; desligar animações remove as transições.
- **Catálogo/busca:** **são placeholders identificados**; não há dados reais de jogos ou sessão online ainda.

### Desenvolvimento e testes

```bash
gcc -std=c17 -Wall -Wextra -Werror -pedantic src/settings.c src/menu.c tests/test_menu.c -o /tmp/cloud360-menu && /tmp/cloud360-menu
gcc -std=c17 -Wall -Wextra -Werror -pedantic src/settings.c src/menu.c src/ui.c tests/test_ui.c -o /tmp/cloud360-ui && /tmp/cloud360-ui
gcc -std=c17 -Wall -Wextra -Werror -pedantic src/settings.c tests/test_settings.c -o /tmp/cloud360-settings && /tmp/cloud360-settings
```

O workflow de testes valida navegação, rede, configurações, ícones e sintaxe XSDK sem criar o .xex. O build de release será feito apenas quando solicitado.

## Desenvolvimento sem gerar XEX

**A partir desta etapa não há compilação automática de XEX.** Commits executam
apenas testes Linux e verificação de sintaxe; a construção do executável
para o Xbox 360 real fica disponível só via GitHub Actions manual, quando for
solicitada.

Foi integrada uma aba **REDE**, com leitura real do link e do IPv4 local
pela API XAM via XSDK, indicação de erros e atualização manual pelo botão A.
Esse diagnóstico **não comprova acesso à internet ou serviço xCloud**.
O botão **X** inicia, em segundo plano, um teste DNS de `www.xbox.com` e
de conexão TCP na porta 443 com timeout. A tela mostra sucesso ou falha.
**Há código-fonte HTTPS com verificação obrigatória, mas o backend TLS está
desativado por padrão até a integração das bibliotecas e testes no console.**
O botão **Y** solicita um GET HTTPS de `https://example.com/` quando
curl-xenon, mbedtls-xenon e `game:/cacert.pem` estiverem configurados.
Sem isso, mostra `TLS NAO INTEGRADO`. A camada OAuth Microsoft
por código foi implementada em fonte, mas ainda depende de integração TLS
validada no hardware e de um cliente Microsoft registrado pelo operador.

Veja o [plano de integração por módulos](docs/DEVELOPMENT.md), o
[guia de HTTPS/XSDK](docs/HTTPS.md), o
[guia de catálogo JSON](docs/CATALOG.md) e a
[análise do Orbita](docs/ORBITA_REFERENCE.md) e o
[guia de login Microsoft](docs/AUTH.md) e
[guia da integração Xbox/XSTS](docs/XSTS.md) e o
[guia do ciclo de sessões de jogo](docs/SESSION.md).

### Catálogo JSON (fonte independente, não é Game Pass)

A aba EXPLORAR carrega até 36 jogos de game:/catalog.json.
X recarrega a lista local; Y tenta baixar um catálogo HTTPS de uma
fonte explicitamente configurada em game:/catalog-source.txt,
somente se curl-xenon + mbedtls-xenon estiverem habilitados
e o arquivo de certificados estiver disponível.
Os cards exibem títulos importados, identificados como não jogáveis.
O parser possui busca C interna, ainda sem teclado virtual.
O módulo CONTA possui OAuth por código e a implementação em fonte do
handshake **Xbox U-token → XSTS** com retorno de estados e falhas à interface.
Após autorizar uma conta Microsoft, **A** tenta validar Xbox Live/XSTS;
**X** cancela e **Y** encerra a sessão, apagando tokens de memória.
Sem curl-xenon/mbedtls-xenon o fluxo falha claramente, sem login fictício.
Nenhum token Xbox Live/XSTS ou sessão xCloud foi validado no console real.
Ainda não há catálogo Game Pass nem streaming.

### Sessão de jogo: estados prontos para integrar

O módulo `src/session.c` adiciona um coordenador nativo para pedido
de sessão, provisionamento, fila com posição/ETA, sinalização,
conexão, primeiro vídeo, reconexão e parada. Os eventos externos
são identificados por geração, impedindo que respostas atrasadas
reativem uma sessão cancelada. Os prazos usam relógio monotônico.

Uma nova aba **SESSAO** mostra esses estados. O botão **A**
verifica XSTS e disponibilidade do provedor; como não existe
um provedor xCloud integrado neste momento, o aplicativo informa
`PROVEDOR XCLOUD NAO INTEGRADO` sem iniciar rede ou criar sessão
fictícia. **X** cancela/encerra o estado local.

Os testes usam eventos de um backend simulado; o console continua
sem WebRTC, vídeo H.264, áudio Opus ou catálogo de jogos autorizado.

## O que ainda não está implementado

- Validação de Xbox Live/XSTS com servidor real e conta autorizada no
  Xbox 360; o protocolo foi implementado somente em fonte.
- Acesso autorizado aos serviços específicos de xCloud e verificação
  de assinatura e elegibilidade da conta.
- Validação real de OAuth/TLS no Xbox 360 físico.
- Integração com o catálogo do Game Pass.
- Sessão xCloud, WebRTC, decodificação H.264/Opus ou streaming.
- Validação da execução real do XEX em Xbox 360 RGH/JTAG.

**Nunca publique senhas, cookies, tokens ou credenciais de conta no repositório.**

## Versão compilada e testes

- **Versão:** 0.2.0.0, XEX2 compilado para PowerPC com XSDK.
- **GitHub Actions:** https://github.com/MichaelPriest/Cloud360/actions/runs/40740740069
- **Artifact:** `Cloud360-XEX` na execução acima; o artefato do GitHub expira após 14 dias.
- **Testes CI:** navegação, transições, gravação de preferências, ícones, sintaxe e compilação XSDK aprovados.
- **Hardware:** ainda não validado em Xbox 360 real. Não instalar como dashboard padrão; testar pelo gerenciador da Aurora e manter possibilidade de retornar ao sistema.

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

1. Integrar rede segura, HTTP e catálogo progressivamente sem gerar o XEX.
2. Implementar e testar os módulos de autenticação e navegação.
3. Realizar o build XEX e testar na Aurora somente quando solicitado.
4. Pesquisar implementação e viabilidade de rede, WebRTC, vídeo H.264, áudio Opus e controle para streaming do xCloud.

Cloud360 é um projeto independente, não afiliado nem endossado pela Microsoft, Xbox ou pelos projetos de referência.
