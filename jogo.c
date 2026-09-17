#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "jogo.h"

char gerar_letra_aleatoria(void) {
    static const char letras[] = "ABCDEFGHIJLMNOPRSTUV";
    int n = (int)(sizeof(letras) - 1);
    return letras[rand() % n];
}

int validar_palavra(const char *palavra, char letra) {
    if (palavra == NULL) return 0;

    size_t len = strlen(palavra);
    if (len < MIN_CARACTERES) return 0;

    for (size_t i = 0; i < len; i++) {
        if (!isalpha((unsigned char)palavra[i])) return 0;
    }

    if (tolower((unsigned char)palavra[0]) != tolower((unsigned char)letra)) {
        return 0;
    }

    return 1;
}

int enviar_msg(int fd, const char *fmt, ...) {
    char buffer[TAM_BUFFER];
    va_list args;

    va_start(args, fmt);
    int n = vsnprintf(buffer, sizeof(buffer) - 2, fmt, args);
    va_end(args);

    if (n < 0) return -1;
    if (n == 0 || buffer[n - 1] != '\n') {
        buffer[n] = '\n';
        n++;
        buffer[n] = '\0';
    }

    size_t total_enviado = 0;
    while (total_enviado < (size_t)n) {
        ssize_t enviados = send(fd, buffer + total_enviado,
                                 (size_t)n - total_enviado,
#ifdef MSG_NOSIGNAL
                                 MSG_NOSIGNAL
#else
                                 0
#endif
        );
        if (enviados < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        total_enviado += (size_t)enviados;
    }
    return 0;
}

int receber_linha(int fd, char *buffer, size_t tam) {
    size_t pos = 0;

    while (pos < tam - 1) {
        char c;
        ssize_t r = recv(fd, &c, 1, 0);

        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) {
            if (pos == 0) return 0;
            break;
        }
        if (c == '\n') break;
        if (c != '\r') {
            buffer[pos++] = c;
        }
    }

    buffer[pos] = '\0';
    return (int)pos;
}

int receber_com_timeout(int fd, char *buffer, size_t tam, int segundos) {
    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    tv.tv_sec = segundos;
    tv.tv_usec = 0;

    int ret = select(fd + 1, &readfds, NULL, NULL, &tv);
    if (ret < 0) {
        return -1;
    }
    if (ret == 0) {
        return 0;
    }
    return receber_linha(fd, buffer, tam);
}

void parse_mensagem(const char *linha, char *tipo, char *dados) {
    const char *sep = strchr(linha, SEPARADOR);

    if (sep == NULL) {
        strncpy(tipo, linha, TAM_TIPO - 1);
        tipo[TAM_TIPO - 1] = '\0';
        dados[0] = '\0';
        return;
    }

    size_t tam_tipo = (size_t)(sep - linha);
    if (tam_tipo >= TAM_TIPO) tam_tipo = TAM_TIPO - 1;

    strncpy(tipo, linha, tam_tipo);
    tipo[tam_tipo] = '\0';

    strncpy(dados, sep + 1, TAM_BUFFER - 1);
    dados[TAM_BUFFER - 1] = '\0';
}
