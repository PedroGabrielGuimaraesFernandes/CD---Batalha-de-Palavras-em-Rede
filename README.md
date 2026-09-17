# Batalha de Palavras em Rede

Jogo multiplayer baseado em texto, implementado em C com Sockets TCP e pthreads.

## Descrição do Jogo

Dois jogadores se conectam ao servidor. A cada rodada:

1. O servidor envia uma letra aleatória
2. Os jogadores têm 10 segundos para digitar uma palavra que:
   - Comece com a letra indicada
   - Tenha no mínimo 5 caracteres
   - Contenha apenas letras (sem números, espaços ou símbolos)
3. O servidor valida e pontua:
   - Palavra válida → +1 ponto
   - Palavra inválida ou tempo esgotado → 0 pontos
   - Palavras repetidas (iguais entre os dois jogadores) → ninguém pontua

O jogo tem 5 rodadas. Ganha quem tiver mais pontos.

## Estrutura do Projeto

```
atividade/
├── protocolo.h      ← definições do protocolo de comunicação (compartilhado)
├── jogo.h           ← interface da lógica do jogo
├── jogo.c           ← implementação: validação, geração de letras, comunicação
├── servidor.c       ← servidor concorrente (pthreads)
├── cliente.c        ← cliente interativo com select()
├── Makefile         ← compilação automatizada
└── README.md        ← este arquivo
```

### Responsabilidade de cada arquivo

| Arquivo | Responsabilidade |
|---|---|
| `protocolo.h` | Constantes do jogo (porta, rodadas, tempo) e prefixos das mensagens do protocolo. Compartilhado entre cliente e servidor. |
| `jogo.h` / `jogo.c` | Lógica pura do jogo: validação de palavras, geração de letras, funções de envio/recebimento formatadas pelo protocolo. |
| `servidor.c` | Aceita conexões, forma pares de jogadores, cria threads por partida, controla rodadas e placar. |
| `cliente.c` | Conecta ao servidor, interpreta mensagens do protocolo, exibe interface formatada, lê input com timeout via `select()`. |

## Protocolo de Comunicação

Todas as mensagens são strings de texto com campos separados por `|` (pipe) e terminadas por `\n`.

### Servidor → Cliente

| Tipo | Formato | Descrição |
|---|---|---|
| `MSG` | `MSG\|texto` | Mensagem genérica para exibição |
| `NOME` | `NOME\|` | Solicita o nome do jogador |
| `AGUARDE` | `AGUARDE\|texto` | Pede para aguardar (ex: segundo jogador) |
| `RODADA` | `RODADA\|num\|letra\|tempo` | Início de rodada com número, letra e tempo em segundos |
| `RESULTADO` | `RESULTADO\|texto` | Resultado individual da rodada |
| `PLACAR` | `PLACAR\|nome1\|pts1\|nome2\|pts2` | Placar atualizado |
| `FIM` | `FIM\|texto` | Fim do jogo com resultado final |

### Cliente → Servidor

| Tipo | Formato | Descrição |
|---|---|---|
| `NOME` | `NOME\|nome_do_jogador` | Resposta com o nome |
| `PALAVRA` | `PALAVRA\|palavra_digitada` | Palavra da rodada |
| `TIMEOUT` | `TIMEOUT\|` | Tempo esgotado (não respondeu a tempo) |

## Compilação

```
make              # compila servidor e cliente
make clean        # remove binários
```

### Manualmente

```
gcc -Wall -Wextra -pedantic -std=c11 -o servidor servidor.c jogo.c
gcc -Wall -Wextra -pedantic -std=c11 -o cliente cliente.c jogo.c
```

> Nenhum dos dois precisa de `-lpthread`: o servidor é **single-threaded**,
> baseado em `select()` — o mesmo modelo de um servidor de chat
> multicliente, só que cuidando de várias partidas (pares de jogadores)
> ao mesmo tempo dentro de um único laço de eventos.

## Execução

### 1. Iniciar o servidor

```
./servidor              # porta padrão 7070
./servidor 9000         # porta customizada
```

### 2. Conectar os clientes (em terminais separados)

```
./cliente                       # conecta em 127.0.0.1:7070
./cliente 127.0.0.1 9000        # IP e porta customizados
./cliente 192.168.1.10 7070     # conectar em outra máquina da rede
```

### 3. Jogar!

- O servidor aguarda 2 jogadores antes de iniciar
- Cada jogador informa seu nome
- A cada rodada, ambos digitam uma palavra que começa com a letra indicada
- Após 5 rodadas, o resultado final é exibido

## Conceitos Técnicos Demonstrados

| Conceito | Onde |
|---|---|
| Socket TCP (`socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`) | `servidor.c`, `cliente.c` |
| Servidor concorrente **sem threads**, usando `select()` sobre um array de clientes | `servidor.c` (laço principal em `main`) |
| Múltiplas partidas simultâneas dentro do mesmo laço de eventos | `servidor.c` (`partidas[]`, `verificar_prazos`) |
| Protocolo de aplicação com mensagens estruturadas | `protocolo.h`, `jogo.c` |
| Timeout de rodada via timeout dinâmico do `select()` | `servidor.c` (`calcular_timeout`, `verificar_prazos`) |
| Timeout de digitação no cliente via `select()` duplo (teclado + socket) | `cliente.c` |
| Validação de entrada | `jogo.c` (`validar_palavra`) |
| Remoção segura de clientes de um array (evitando corromper índices em uso) | `servidor.c` (`limpar_clientes_removidos`) |
| Modularização em múltiplos arquivos `.c` e `.h` | todos os arquivos |
| Tratamento de erros | todos os arquivos |
| Tratamento de sinais (`SIGPIPE`, `SIGINT`) | `servidor.c`, `cliente.c` |

## Regras de Validação

Uma palavra é válida se:

- [x] Começa com a letra da rodada (case insensitive)
- [x] Tem no mínimo 5 caracteres
- [x] Contém apenas letras (a-z, A-Z)
- [x] Não foi a mesma palavra que o oponente enviou
