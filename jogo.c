/*
 * jogo.c — Implementação da lógica do jogo e das funções de comunicação
 *
 * Ver jogo.h para a documentação de cada função.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "jogo.h"

/* ============================================================
 * Comunicação (protocolo)
 * ============================================================ */

int enviar_msg(int fd, const char *tipo, const char *conteudo)
{
    char linha[BUFFER_SIZE];

    /* Monta "tipo|conteudo\n" — conteudo pode ser vazio (ex: "NOME|") */
    snprintf(linha, sizeof(linha), "%s%s%s\n",
             tipo, PROTO_SEP, conteudo ? conteudo : "");

    size_t total = strlen(linha);
    size_t enviados = 0;

    /* send() pode enviar menos bytes do que o pedido; garantimos o envio
     * completo em um laço, como boa prática de programação de sockets. */
    while (enviados < total) {
        ssize_t n = send(fd, linha + enviados, total - enviados, 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        enviados += (size_t)n;
    }
    return 0;
}

int enviar_msgf(int fd, const char *tipo, const char *fmt, ...)
{
    char conteudo[BUFFER_SIZE];
    va_list args;

    va_start(args, fmt);
    vsnprintf(conteudo, sizeof(conteudo), fmt, args);
    va_end(args);

    return enviar_msg(fd, tipo, conteudo);
}

int receber_linha(int fd, char *buffer, size_t tamanho)
{
    size_t i = 0;
    char   c;

    if (tamanho == 0) return -1;

    while (i < tamanho - 1) {
        ssize_t n = recv(fd, &c, 1, 0);

        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) {
            /* Conexão fechada. Se já tínhamos lido algo, devolvemos
             * o que foi lido; senão, sinalizamos EOF (0). */
            break;
        }
        if (c == '\n') break;
        if (c != '\r') buffer[i++] = c;
    }

    buffer[i] = '\0';
    return (int)i;
}

int receber_com_timeout(int fd, char *buffer, size_t tamanho, int timeout_seg)
{
    fd_set         leitura;
    struct timeval tv;

    FD_ZERO(&leitura);
    FD_SET(fd, &leitura);
    tv.tv_sec  = timeout_seg;
    tv.tv_usec = 0;

    int pronto = select(fd + 1, &leitura, NULL, NULL, &tv);

    if (pronto == -1) {
        if (errno == EINTR) return -1;
        return -1;
    }
    if (pronto == 0) {
        return 0; /* estourou o tempo, nada chegou */
    }

    int n = receber_linha(fd, buffer, tamanho);
    if (n <= 0) return -1; /* conexão encerrada durante a espera */
    return n;
}

int parse_mensagem(char *linha, char **tipo, char *campos[], int max_campos)
{
    int   n = 0;
    char *p = linha;
    char *sep;

    *tipo = p;
    sep = strchr(p, '|');
    if (sep) {
        *sep = '\0';
        p = sep + 1;
    } else {
        p = NULL; /* sem separador: mensagem só tem o tipo */
    }

    while (p != NULL && n < max_campos) {
        campos[n] = p;
        sep = strchr(p, '|');
        if (sep) {
            *sep = '\0';
            p = sep + 1;
        } else {
            p = NULL;
        }
        n++;
    }

    return n;
}

/* ============================================================
 * Lógica do jogo
 * ============================================================ */

char gerar_letra_aleatoria(void)
{
    /* Evitamos K, W e Y — raras como início de palavra em português.
     * O chamador (servidor) deve ter chamado srand() uma única vez. */
    static const char letras[] = "ABCDEFGHIJLMNOPQRSTUVXZ";
    int total = (int)(sizeof(letras) - 1);
    return letras[rand() % total];
}

int validar_palavra(const char *palavra, char letra)
{
    size_t len = strlen(palavra);

    if (len < MIN_CARACTERES) return 0;

    if (toupper((unsigned char)palavra[0]) != toupper((unsigned char)letra)) {
        return 0;
    }

    for (size_t i = 0; i < len; i++) {
        if (!isalpha((unsigned char)palavra[i])) return 0;
    }

    return 1;
}

int palavras_iguais(const char *a, const char *b)
{
    if (strlen(a) != strlen(b)) return 0;

    for (size_t i = 0; a[i] != '\0'; i++) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) {
            return 0;
        }
    }
    return 1;
}
