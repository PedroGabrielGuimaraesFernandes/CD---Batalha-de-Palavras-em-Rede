#ifndef JOGO_H
#define JOGO_H

#include <stddef.h>
#include "protocolo.h"

/* ---------- Lógica pura do jogo ---------- */

/* Sorteia uma letra (maiúscula) dentre um conjunto pré-definido */
char gerar_letra_aleatoria(void);

/* Valida se 'palavra' começa com 'letra', tem tamanho mínimo e
 * contém apenas letras (a-z, A-Z). Não verifica repetição —
 * isso é feito pelo servidor, que conhece as duas respostas. */
int validar_palavra(const char *palavra, char letra);

/* ---------- Comunicação formatada pelo protocolo ---------- */

/* Envia uma mensagem formatada (estilo printf) terminada em '\n'.
 * Retorna 0 em sucesso, -1 em erro. */
int enviar_msg(int fd, const char *fmt, ...);

/* Lê uma linha (até '\n' ou até encher o buffer) de forma bloqueante.
 * Retorna o número de bytes lidos, 0 se a conexão foi fechada
 * pelo par, ou -1 em erro. */
int receber_linha(int fd, char *buffer, size_t tam);

/* Lê uma linha com timeout (segundos). Retorna:
 *   > 0  -> bytes lidos
 *   0    -> timeout (nada chegou a tempo) OU conexão fechada
 *  -1    -> erro
 */
int receber_com_timeout(int fd, char *buffer, size_t tam, int segundos);

/* Separa uma linha "TIPO|dados" em dois campos já alocados
 * pelo chamador (tipo deve ter espaço >= TAM_TIPO, dados >= TAM_BUFFER). */
void parse_mensagem(const char *linha, char *tipo, char *dados);

#endif /* JOGO_H */
