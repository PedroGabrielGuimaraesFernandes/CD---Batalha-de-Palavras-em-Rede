#ifndef PROTOCOLO_H
#define PROTOCOLO_H

/* ===================== Parâmetros do jogo ===================== */
#define PORTA_PADRAO   7070
#define NUM_RODADAS    5
#define TEMPO_RODADA   10   /* segundos que cada jogador tem para responder */
#define MIN_CARACTERES 5
#define MAX_CLIENTES   20   /* conexões simultâneas suportadas pelo servidor */

/* ===================== Tamanhos de buffer ===================== */
#define TAM_BUFFER  512
#define TAM_NOME    64
#define TAM_PALAVRA 128
#define TAM_TIPO    32

/* ===================== Separador do protocolo =================== */
#define SEPARADOR '|'

/* ============ Prefixos: Servidor -> Cliente ============ */
#define MSG_MSG       "MSG"        /* MSG|texto                          */
#define MSG_NOME      "NOME"       /* NOME|  (solicita nome)              */
#define MSG_AGUARDE   "AGUARDE"    /* AGUARDE|texto                       */
#define MSG_RODADA    "RODADA"     /* RODADA|num|letra|tempo              */
#define MSG_RESULTADO "RESULTADO"  /* RESULTADO|texto                     */
#define MSG_PLACAR    "PLACAR"     /* PLACAR|nome1|pts1|nome2|pts2        */
#define MSG_FIM       "FIM"        /* FIM|texto                           */

/* ============ Prefixos: Cliente -> Servidor ============ */
#define MSG_PALAVRA   "PALAVRA"    /* PALAVRA|palavra_digitada            */
#define MSG_TIMEOUT   "TIMEOUT"    /* TIMEOUT|  (não respondeu a tempo)   */

#endif /* PROTOCOLO_H */
