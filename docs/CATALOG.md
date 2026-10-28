# Catálogo JSON experimental no Cloud360

A página https://www.xbox.com/pt-br/play é uma aplicação web.
**Não** fornece diretamente o formato JSON desta etapa.
O catálogo atual é próprio do Cloud360, para testar layout,
paginação e confiabilidade do processamento. Não inicia jogos.

## Formato local

Crie um arquivo UTF-8 game:/catalog.json, ao lado do XEX
quando chegar a fase de testes. Exemplo com títulos fictícios:

~~~json
{
  "games": [
    {
      "id": "demo-001",
      "title": "Jogo demonstrativo A",
      "category": "Aventura",
      "description": "Nao executavel, somente interface"
    },
    {
      "id": "demo-002",
      "title": "Jogo demonstrativo B",
      "category": "Corrida",
      "description": "Teste de navegacao do Cloud360"
    }
  ]
}
~~~

Aceita até 36 jogos e até 16.384 bytes por documento.
ID e título são obrigatórios; IDs duplicados, JSON incompleto e
strings longas são rejeitados. Falhas não apagam a lista anterior.

Na aba EXPLORAR:
- X recarrega a lista local.
- Y tenta uma atualização HTTPS configurada.
- A ou baixo foca os cards; esquerda/direita percorre os jogos.
- B retorna à navegação superior; LB/RB alternam abas.

## Fonte remota, opcional

O arquivo game:/catalog-source.txt contém dois termos
(host, caminho), como este exemplo não operacional:

~~~text
servidor-exemplo.invalid /catalog.json
~~~

O servidor escolhido pelo operador precisa devolver o mesmo
esquema JSON. A requisição exige curl-xenon + mbedtls-xenon,
certificados verificados em game:/cacert.pem e CA confiável.
Na ausência do backend, a atualização HTTPS falha claramente,
sem mudar o catálogo.

O site xbox.com/pt-br/play **não** deve ser colocado como
fonte: é uma página web, não esse serviço JSON.

## Próxima etapa

Usar OAuth consentido e fontes de catálogo autorizadas;
preservar os limites e o cache. A função de busca existe no
parser mas ainda não está conectada ao teclado virtual.
Sem login, sem streaming, sem promessas de disponibilidade.

O workflow automático testa apenas os fontes e **não gera XEX**.
