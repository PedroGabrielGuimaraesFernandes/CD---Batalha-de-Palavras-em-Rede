/*
 * cliente.c — Batalha de Palavras em Rede
 *
 * Baseado no chat_cliente.c do professor: um select() só, olhando ao
 * mesmo tempo pro teclado (STDIN_FILENO) e pro socket do servidor.
 * Sem isso, eu teria que escolher entre ler o teclado ou ler o
 * socket — com select() dá pra fazer as duas coisas no mesmo laço.
 *
 * A diferença pro chat é que aqui o teclado só importa em dois
 * momentos (dizer o nome, digitar a palavra da rodada), e durante uma
 * rodada o select() precisa de um timeout — é isso que implementa o
 * limite de 10 segundos pra responder.
 *
 * Compilar: gcc -Wall -Wextra -pedantic -std=c11 -o cliente cliente.c jogo.c
 * Rodar:    ./cliente [ip] [porta]
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocolo.h"
#include "jogo.h"

/* Guardo em que ponto da conversa com o servidor eu estou, porque
 * isso decide se o teclado entra ou não no select() e se preciso de
 * timeout. No chat, o teclado está sempre "ativo"; aqui não — só faz
 * sentido ler o teclado quando o servidor está esperando algo de
 * mim (nome ou palavra). */
typedef enum {
    CLIENTE_AGUARDANDO_NOME,
    CLIENTE_OCIOSO,
    CLIENTE_RODADA_ATIVA
} EstadoCliente;

/* Trata uma linha "TIPO|dados" recebida do servidor. Devolve 1 se o
 * cliente deve encerrar (chegou FIM), 0 caso contrário. */
static int tratar_linha_do_servidor(const char *linha, EstadoCliente *estado, time_t *prazo_rodada) {
    char tipo[TAM_TIPO], dados[TAM_BUFFER];
    parse_mensagem(linha, tipo, dados);

    if (strcmp(tipo, MSG_NOME) == 0) {
        printf("  Digite seu nome: ");
        fflush(stdout);
        *estado = CLIENTE_AGUARDANDO_NOME;

    } else if (strcmp(tipo, MSG_AGUARDE) == 0) {
        printf("  %s\n", dados);

    } else if (strcmp(tipo, MSG_MSG) == 0) {
        printf("  %s\n\n", dados);

    } else if (strcmp(tipo, MSG_RODADA) == 0) {
        int numero, tempo;
        char letra;

        if (sscanf(dados, "%d|%c|%d", &numero, &letra, &tempo) == 3) {
            *prazo_rodada = time(NULL) + tempo;
            *estado = CLIENTE_RODADA_ATIVA;

            printf("\n  ╔══════════════════════════════════╗\n");
            printf(  "  ║        RODADA %d de %-2d            ║\n", numero, NUM_RODADAS);
            printf(  "  ║  Letra: [%c]   Tempo: %2d seg      ║\n", letra, tempo);
            printf(  "  ║  Mínimo: %d caracteres            ║\n", MIN_CARACTERES);
            printf(  "  ╚══════════════════════════════════╝\n");
            printf(  "  Sua palavra: ");
            fflush(stdout);
        }

    } else if (strcmp(tipo, MSG_RESULTADO) == 0) {
        printf("\n   %s\n", dados);

    } else if (strcmp(tipo, MSG_PLACAR) == 0) {
        char nome1[TAM_NOME], nome2[TAM_NOME];
        int pts1, pts2;

        if (sscanf(dados, "%63[^|]|%d|%63[^|]|%d", nome1, &pts1, nome2, &pts2) == 4) {
            printf("  ┌─────────────────────────────────────┐\n");
            printf("  │  PLACAR: %-10s %2d  x  %2d %-10s │\n", nome1, pts1, pts2, nome2);
            printf("  └─────────────────────────────────────┘\n");
        }

    } else if (strcmp(tipo, MSG_FIM) == 0) {
        printf("\n  🏁 %s\n\n", dados);
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    const char *ip = "127.0.0.1";
    int porta = PORTA_PADRAO;

    if (argc >= 2) ip = argv[1];
    if (argc >= 3) porta = atoi(argv[2]);

    signal(SIGPIPE, SIG_IGN);

    printf("========================================\n");
    printf("     BATALHA DE PALAVRAS — Cliente\n");
    printf("========================================\n");
    printf("Conectando a %s:%d...\n", ip, porta);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servidor_addr;
    memset(&servidor_addr, 0, sizeof(servidor_addr));
    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port = htons((uint16_t)porta);

    if (inet_pton(AF_INET, ip, &servidor_addr.sin_addr) <= 0) {
        fprintf(stderr, "Endereço IP inválido: %s\n", ip);
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (connect(fd, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) < 0) {
        perror("Erro ao conectar (o servidor está rodando?)");
        close(fd);
        exit(EXIT_FAILURE);
    }

    printf("Conectado!\n\n");

    /* Assim que conecto, o servidor já manda o NOME| — então começo
     * "ocioso" e deixo o próprio tratamento de mensagem mudar o
     * estado quando essa mensagem chegar. */
    EstadoCliente estado = CLIENTE_OCIOSO;
    time_t prazo_rodada = 0;

    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);
        int max_fd = fd;

        /* Só coloco o teclado no select() quando faz sentido ler
         * dele. Fora desses dois momentos, digitar algo não teria
         * pra onde ir mesmo. */
        if (estado == CLIENTE_AGUARDANDO_NOME || estado == CLIENTE_RODADA_ATIVA) {
            FD_SET(STDIN_FILENO, &read_fds);
            if (STDIN_FILENO > max_fd) max_fd = STDIN_FILENO;
        }

        /* Só uso timeout durante uma rodada — é isso que implementa
         * o limite de tempo pra digitar a palavra. Fora de rodada,
         * bloqueio sem limite, igual ao chat_cliente.c original. */
        struct timeval tv;
        struct timeval *ptv = NULL;
        if (estado == CLIENTE_RODADA_ATIVA) {
            time_t restante = prazo_rodada - time(NULL);
            if (restante < 0) restante = 0;
            tv.tv_sec = restante;
            tv.tv_usec = 0;
            ptv = &tv;
        }

        int atividade = select(max_fd + 1, &read_fds, NULL, NULL, ptv);
        if (atividade < 0) {
            if (errno == EINTR) continue;
            perror("Erro no select");
            break;
        }

        /* select() só retorna 0 quando demos um timeout — ou seja,
         * só pode acontecer durante uma rodada: o tempo acabou e eu
         * não digitei nada. */
        if (atividade == 0 && estado == CLIENTE_RODADA_ATIVA) {
            enviar_msg(fd, "%s|", MSG_TIMEOUT);
            printf("\n  ⏱ Tempo esgotado!\n");
            estado = CLIENTE_OCIOSO;
            continue;
        }

        /* -------- Chegou algo do SERVIDOR -------- */
        if (FD_ISSET(fd, &read_fds)) {
            char buffer[TAM_BUFFER * 2];
            ssize_t n = recv(fd, buffer, sizeof(buffer) - 1, 0);

            if (n <= 0) {
                printf("\n[!] Conexão com o servidor perdida.\n");
                break;
            }
            buffer[n] = '\0';

            /* Pode vir mais de uma mensagem grudada no mesmo recv() */
            int deve_sair = 0;
            char *salvar = NULL;
            char *linha = strtok_r(buffer, "\n", &salvar);
            while (linha != NULL && !deve_sair) {
                deve_sair = tratar_linha_do_servidor(linha, &estado, &prazo_rodada);
                linha = strtok_r(NULL, "\n", &salvar);
            }
            if (deve_sair) break;
        }

        /* -------- Chegou algo do TECLADO -------- */
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char input[TAM_PALAVRA];

            if (fgets(input, sizeof(input), stdin) == NULL) {
                break; /* Ctrl+D */
            }
            char *nl = strchr(input, '\n');
            if (nl) *nl = '\0';

            if (estado == CLIENTE_AGUARDANDO_NOME) {
                if (input[0] == '\0') strcpy(input, "Jogador");
                enviar_msg(fd, "%s|%s", MSG_NOME, input);
                printf("  Bem-vindo, %s!\n\n", input);
                estado = CLIENTE_OCIOSO;

            } else if (estado == CLIENTE_RODADA_ATIVA) {
                if (input[0] == '\0') {
                    printf("  Sua palavra: ");
                    fflush(stdout);
                } else {
                    enviar_msg(fd, "%s|%s", MSG_PALAVRA, input);
                    printf("  Enviado: \"%s\" — aguardando resultado...\n", input);
                    estado = CLIENTE_OCIOSO;
                }
            }
        }
    }

    close(fd);
    printf("Desconectado. Até mais!\n");
    return 0;
}
