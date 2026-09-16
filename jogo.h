
#ifndef JOGO_H
#define JOGO_H

#include <stddef.h> // Necessário para o tipo size_t

#define BUFFER_SIZE 256
#define PROTO_SEP "|"
#define MIN_CARACTERES 5

/**
 * Envia uma mensagem formatada "tipo|conteudo\n" de forma segura.
 * Retorna 0 em caso de sucesso ou -1 em caso de erro.
 */
int enviar_msg(int fd, const char *tipo, const char *conteudo);

/**
 * Envia uma mensagem utilizando formatação estilo printf (variadic arguments).
 * Retorna 0 em caso de sucesso ou -1 em caso de erro.
 */
int enviar_msgf(int fd, const char *tipo, const char *fmt, ...);

/**
 * Lê de um socket caractere por caractere até encontrar um '\n' ou o limite do buffer.
 * Retorna o número de bytes lidos, 0 se a conexão fechou, ou -1 em caso de erro.
 */
int receber_linha(int fd, char *buffer, size_t tamanho);

/**
 * Aguarda por dados no socket até o tempo limite (timeout) usando select().
 * Retorna o número de bytes lidos, 0 se ocorreu timeout, ou -1 em caso de erro.
 */
int receber_com_timeout(int fd, char *buffer, size_t tamanho, int timeout_seg);

/**
 * Divide uma linha lida no formato "TIPO|CAMPO1|CAMPO2" em ponteiros separados.
 * Modifica a string original trocando '|' por '\0'.
 * Retorna a quantidade de campos encontrados após o tipo.
 */
int parse_mensagem(char *linha, char **tipo, char *campos[], int max_campos);


/* ============================================================
 * Lógica do jogo
 * ============================================================ */

/**
 * Retorna uma letra aleatória válida para o jogo (exclui K, W, Y).
 */
char gerar_letra_aleatoria(void);

/**
 * Valida se a palavra começa com a letra informada, tem o tamanho mínimo
 * e contém apenas letras do alfabeto.
 * Retorna 1 se for válida ou 0 se for inválida.
 */
int validar_palavra(const char *palavra, char letra);

/**
 * Compara duas palavras de forma case-insensitive.
 * Retorna 1 se forem iguais ou 0 se forem diferentes.
 */
int palavras_iguais(const char *a, const char *b);

#endif /* JOGO_H */
