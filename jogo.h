#ifndef JOGO_H
#define JOGO_H

#include <stddef.h>
#include "protocolo.h"

char gerar_letra_aleatoria(void);

int validar_palavra(const char *palavra, char letra);

int enviar_msg(int fd, const char *fmt, ...);

int receber_linha(int fd, char *buffer, size_t tam);

int receber_com_timeout(int fd, char *buffer, size_t tam, int segundos);

void parse_mensagem(const char *linha, char *tipo, char *dados);

#endif
