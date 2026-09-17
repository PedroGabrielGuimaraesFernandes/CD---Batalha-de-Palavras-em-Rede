#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp — usada pra comparar palavras dos dois jogadores */
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


typedef struct {
    int  fd;
    char nome[TAM_NOME];
    char ip[INET_ADDRSTRLEN];
    int  porta;
    int  pontos;

    int  em_partida;   /* 0 = ainda esperando adversário, 1 = jogando */
    int  id_partida;   /* índice em 'partidas[]' — só vale se em_partida == 1 */

    int  remover;     
} Jogador;

static Jogador clientes[MAX_CLIENTES];
static int     num_clientes = 0;


// Uma partida = uma dupla de jogadores jogando entre si.
typedef struct {
    int    ativa;
    int    idx_a, idx_b;     /* índices dos dois jogadores dentro de clientes[] */
    int    numero_partida;
    int    rodada;
    char   letra;
    time_t prazo;            /* momento em que a rodada atual vence */

    int  respondeu_a, respondeu_b;
    int  enviou_a, enviou_b;
    char palavra_a[TAM_PALAVRA];
    char palavra_b[TAM_PALAVRA];
} Partida;

static Partida partidas[MAX_CLIENTES / 2 + 1];
static int     num_partidas = 0;

static int g_fd_escuta = -1;
static int g_contador_partidas = 0;


static void tratar_sigint(int sig) {
    (void)sig;
    printf("\n[!] Encerrando servidor...\n");
    if (g_fd_escuta >= 0) close(g_fd_escuta);
    exit(0);
}



static int adicionar_cliente(int fd, const char *nome, const char *ip, int porta) {
    if (num_clientes >= MAX_CLIENTES) {
        return -1;
    }

    clientes[num_clientes].fd = fd;
    strncpy(clientes[num_clientes].nome, nome, TAM_NOME - 1);
    clientes[num_clientes].nome[TAM_NOME - 1] = '\0';
    strncpy(clientes[num_clientes].ip, ip, sizeof(clientes[num_clientes].ip) - 1);
    clientes[num_clientes].porta = porta;
    clientes[num_clientes].pontos = 0;
    clientes[num_clientes].em_partida = 0;
    clientes[num_clientes].id_partida = -1;
    clientes[num_clientes].remover = 0;

    num_clientes++;
    return num_clientes - 1;
}

/*
 * Fecha o socket do cliente no índice 'i' e o tira da lista, colocando
 * o último cliente da lista no lugar dele (é o mesmo truque do "swap
 * com o último" que o chat_servidor.c usa em remover_cliente()).
 *
 * A parte que o chat NÃO precisa se preocupar, e que eu precisei
 * resolver, é a seguinte: como as partidas guardam o ÍNDICE dos dois
 * jogadores (idx_a e idx_b), se eu mover alguém de posição sem avisar
 * a partida dele, ela continua "acreditando" que o jogador está no
 * índice antigo — só que agora ali está outra pessoa. Descobri isso
 * testando com duas partidas rodando juntas: uma partida terminava,
 * eu removia os dois jogadores dela, e o placar da OUTRA partida
 * começava a sair errado.
 *
 * Por isso, depois do swap, eu confiro se quem foi movido está numa
 * partida em andamento e, se estiver, corrijo o índice guardado nela.
 */
static void remover_cliente_no_indice(int i) {
    close(clientes[i].fd);

    int ultimo = num_clientes - 1;
    if (i != ultimo) {
        clientes[i] = clientes[ultimo];

        if (clientes[i].em_partida) {
            Partida *p = &partidas[clientes[i].id_partida];
            if (p->idx_a == ultimo) p->idx_a = i;
            if (p->idx_b == ultimo) p->idx_b = i;
        }
    }
    num_clientes--;
}

/*
 * Aqui é a parte chata que também tive que descobrir sozinho:
 * eu NÃO posso simplesmente chamar remover_cliente_no_indice() na
 * hora em que percebo que preciso remover alguém (por exemplo, dentro
 * do laço que trata mensagem por mensagem, lá no main). O motivo é
 * que esse laço percorre os clientes por índice, e se eu removo
 * alguém no meio do percurso (o que troca as posições no array), o
 * laço pode acabar processando de novo, sem querer, um cliente que já
 * tinha processado antes — e nesse reprocessamento eu chamaria
 * recv() num socket que já não tem mais nada pra ler, o que trava o
 * servidor esperando um dado que nunca chega.
 *
 * A solução mais simples que encontrei foi: em vez de remover na
 * hora, eu só "marco" o cliente com remover = 1, e só depois que o
 * laço principal termina de passar por todo mundo é que eu chamo essa
 * função aqui pra remover de verdade quem ficou marcado.
 */
static void limpar_marcados_para_remover(void) {
    for (int i = num_clientes - 1; i >= 0; i--) {
        if (clientes[i].remover) {
            remover_cliente_no_indice(i);
        }
    }
}

/* ============================================================
 * Uma rodada nova: sorteia a letra, zera as respostas da dupla e
 * manda RODADA|num|letra|tempo pros dois.
 * ============================================================ */
static void iniciar_rodada(Partida *p) {
    Jogador *ja = &clientes[p->idx_a];
    Jogador *jb = &clientes[p->idx_b];

    p->letra = gerar_letra_aleatoria();
    p->respondeu_a = p->respondeu_b = 0;
    p->enviou_a = p->enviou_b = 0;
    p->palavra_a[0] = '\0';
    p->palavra_b[0] = '\0';
    p->prazo = time(NULL) + TEMPO_RODADA;

    enviar_msg(ja->fd, "%s|%d|%c|%d", MSG_RODADA, p->rodada, p->letra, TEMPO_RODADA);
    enviar_msg(jb->fd, "%s|%d|%c|%d", MSG_RODADA, p->rodada, p->letra, TEMPO_RODADA);

    printf("  [Partida #%d] Rodada %d — Letra: %c\n",
           p->numero_partida, p->rodada, p->letra);
}

/* Cria a struct Partida pra essa dupla e manda a primeira rodada.
 * Procuro primeiro por um "slot" livre de alguma partida que já
 * acabou, e só crio um novo se não achar nenhum — assim não preciso
 * de um array gigante, já que nem todas as partidas acontecem ao
 * mesmo tempo. */
static void iniciar_partida(int idx_a, int idx_b) {
    int slot = -1;
    for (int i = 0; i < num_partidas; i++) {
        if (!partidas[i].ativa) { slot = i; break; }
    }
    if (slot == -1) {
        if (num_partidas >= (int)(sizeof(partidas) / sizeof(partidas[0]))) {
            /* Não deveria acontecer — temos slots pra metade de
             * MAX_CLIENTES — mas por segurança evito estourar o array. */
            enviar_msg(clientes[idx_a].fd, "%s|Servidor sem vagas para nova partida.", MSG_MSG);
            enviar_msg(clientes[idx_b].fd, "%s|Servidor sem vagas para nova partida.", MSG_MSG);
            return;
        }
        slot = num_partidas++;
    }

    Partida *p = &partidas[slot];
    p->ativa = 1;
    p->idx_a = idx_a;
    p->idx_b = idx_b;
    p->numero_partida = ++g_contador_partidas;
    p->rodada = 1;

    clientes[idx_a].em_partida = 1;
    clientes[idx_b].em_partida = 1;
    clientes[idx_a].id_partida = slot;
    clientes[idx_b].id_partida = slot;
    clientes[idx_a].pontos = 0;
    clientes[idx_b].pontos = 0;

    printf("[Partida #%d] Jogadores: %s vs %s\n",
           p->numero_partida, clientes[idx_a].nome, clientes[idx_b].nome);

    char anuncio[TAM_BUFFER];
    snprintf(anuncio, sizeof(anuncio), "%s vs %s — %d rodadas. Boa sorte!",
             clientes[idx_a].nome, clientes[idx_b].nome, NUM_RODADAS);
    enviar_msg(clientes[idx_a].fd, "%s|%s", MSG_MSG, anuncio);
    enviar_msg(clientes[idx_b].fd, "%s|%s", MSG_MSG, anuncio);

    iniciar_rodada(p);
}

/* Manda FIM personalizado pros dois (quem ganhou, quem perdeu, ou
 * empate) e marca os dois pra serem removidos da lista. */
static void finalizar_partida(Partida *p) {
    Jogador *ja = &clientes[p->idx_a];
    Jogador *jb = &clientes[p->idx_b];

    char fim_a[TAM_BUFFER], fim_b[TAM_BUFFER];

    if (ja->pontos > jb->pontos) {
        snprintf(fim_a, sizeof(fim_a), "Você venceu! Placar final: %d x %d", ja->pontos, jb->pontos);
        snprintf(fim_b, sizeof(fim_b), "%s venceu. Placar final: %d x %d", ja->nome, jb->pontos, ja->pontos);
    } else if (jb->pontos > ja->pontos) {
        snprintf(fim_b, sizeof(fim_b), "Você venceu! Placar final: %d x %d", jb->pontos, ja->pontos);
        snprintf(fim_a, sizeof(fim_a), "%s venceu. Placar final: %d x %d", jb->nome, ja->pontos, jb->pontos);
    } else {
        snprintf(fim_a, sizeof(fim_a), "Empate! Placar final: %d x %d", ja->pontos, jb->pontos);
        snprintf(fim_b, sizeof(fim_b), "Empate! Placar final: %d x %d", jb->pontos, ja->pontos);
    }

    printf("[Partida #%d] Fim! Placar final: %s %d x %d %s\n",
           p->numero_partida, ja->nome, ja->pontos, jb->pontos, jb->nome);

    enviar_msg(ja->fd, "%s|%s", MSG_FIM, fim_a);
    enviar_msg(jb->fd, "%s|%s", MSG_FIM, fim_b);

    ja->em_partida = 0;
    jb->em_partida = 0;
    ja->id_partida = -1;
    jb->id_partida = -1;
    ja->remover = 1;
    jb->remover = 1;

    p->ativa = 0;
}

/* Confere as duas palavras da rodada, dá ponto pra quem acertou,
 * manda RESULTADO e PLACAR, e decide se começa a próxima rodada ou
 * se a partida já acabou (5 rodadas). */
static void finalizar_rodada(Partida *p) {
    Jogador *ja = &clientes[p->idx_a];
    Jogador *jb = &clientes[p->idx_b];

    int valida_a = p->enviou_a && validar_palavra(p->palavra_a, p->letra);
    int valida_b = p->enviou_b && validar_palavra(p->palavra_b, p->letra);
    int repetida = valida_a && valida_b && (strcasecmp(p->palavra_a, p->palavra_b) == 0);

    const char *texto_a, *texto_b;

    if (repetida) {
        texto_a = texto_b = "0 pontos — palavra repetida entre os dois jogadores";
    } else {
        if (valida_a)          { ja->pontos++; texto_a = "+1 ponto! Palavra válida."; }
        else if (!p->enviou_a) texto_a = "0 pontos — tempo esgotado.";
        else                   texto_a = "0 pontos — palavra inválida.";

        if (valida_b)          { jb->pontos++; texto_b = "+1 ponto! Palavra válida."; }
        else if (!p->enviou_b) texto_b = "0 pontos — tempo esgotado.";
        else                   texto_b = "0 pontos — palavra inválida.";
    }

    char msg_a[TAM_BUFFER], msg_b[TAM_BUFFER];
    snprintf(msg_a, sizeof(msg_a), "%s [%s enviou: \"%s\"]", texto_a,
             jb->nome, p->enviou_b ? p->palavra_b : "(nada)");
    snprintf(msg_b, sizeof(msg_b), "%s [%s enviou: \"%s\"]", texto_b,
             ja->nome, p->enviou_a ? p->palavra_a : "(nada)");

    enviar_msg(ja->fd, "%s|%s", MSG_RESULTADO, msg_a);
    enviar_msg(jb->fd, "%s|%s", MSG_RESULTADO, msg_b);

    enviar_msg(ja->fd, "%s|%s|%d|%s|%d", MSG_PLACAR, ja->nome, ja->pontos, jb->nome, jb->pontos);
    enviar_msg(jb->fd, "%s|%s|%d|%s|%d", MSG_PLACAR, ja->nome, ja->pontos, jb->nome, jb->pontos);

    printf("  [Partida #%d] Rodada %d — %s=\"%s\"(%s) | %s=\"%s\"(%s) | Placar: %d x %d\n",
           p->numero_partida, p->rodada,
           ja->nome, p->enviou_a ? p->palavra_a : "-", valida_a ? "ok" : "inv",
           jb->nome, p->enviou_b ? p->palavra_b : "-", valida_b ? "ok" : "inv",
           ja->pontos, jb->pontos);

    p->rodada++;
    if (p->rodada > NUM_RODADAS) {
        finalizar_partida(p);
    } else {
        iniciar_rodada(p);
    }
}

/* Guarda a resposta de um dos dois jogadores da partida. Quando os
 * dois já tiverem respondido, fecha a rodada na hora — não precisa
 * esperar o prazo vencer se os dois já mandaram alguma coisa. */
static void registrar_resposta(Partida *p, int idx_jogador, const char *tipo, const char *dados) {
    int sou_a = (p->idx_a == idx_jogador);

    int  *respondeu = sou_a ? &p->respondeu_a : &p->respondeu_b;
    int  *enviou    = sou_a ? &p->enviou_a    : &p->enviou_b;
    char *palavra   = sou_a ? p->palavra_a    : p->palavra_b;

    if (*respondeu) return; /* já recebemos a resposta dessa rodada, ignora */

    if (strcmp(tipo, MSG_PALAVRA) == 0 && dados[0] != '\0') {
        strncpy(palavra, dados, TAM_PALAVRA - 1);
        palavra[TAM_PALAVRA - 1] = '\0';
        *enviou = 1;
    } else {
        palavra[0] = '\0';
        *enviou = 0;
    }
    *respondeu = 1;

    if (p->respondeu_a && p->respondeu_b) {
        finalizar_rodada(p);
    }
}

/*
 * Chamada toda volta do laço principal pra ver se alguma rodada
 * estourou o prazo de 10 segundos sem os dois responderem. É por
 * causa dessa função que o select() precisa de um timeout (ver
 * calcular_timeout mais abaixo) — sem timeout, o select() ficaria
 * bloqueado esperando alguém mandar dado, e nunca ia "acordar"
 * sozinho pra aplicar o tempo esgotado em quem não respondeu.
 */
static void verificar_prazos(void) {
    time_t agora = time(NULL);

    for (int i = 0; i < num_partidas; i++) {
        Partida *p = &partidas[i];
        if (!p->ativa) continue;
        if (agora < p->prazo) continue;

        if (!p->respondeu_a) { p->palavra_a[0] = '\0'; p->enviou_a = 0; p->respondeu_a = 1; }
        if (!p->respondeu_b) { p->palavra_b[0] = '\0'; p->enviou_b = 0; p->respondeu_b = 1; }

        finalizar_rodada(p);
    }
}

/*
 * O chat_servidor.c chama select() com timeout NULL, porque ele só
 * precisa acordar quando alguém manda mensagem. Aqui eu preciso de
 * outra coisa também: acordar sozinho quando o prazo de uma rodada
 * vence, mesmo que ninguém tenha mandado nada. Por isso calculo,
 * entre todas as partidas em andamento, qual é o prazo mais próximo,
 * e uso isso como timeout do select().
 */
static struct timeval *calcular_timeout(struct timeval *tv) {
    time_t agora = time(NULL);
    time_t menor_prazo = -1;

    for (int i = 0; i < num_partidas; i++) {
        if (!partidas[i].ativa) continue;
        if (menor_prazo == -1 || partidas[i].prazo < menor_prazo) {
            menor_prazo = partidas[i].prazo;
        }
    }

    if (menor_prazo == -1) {
        return NULL; /* nenhuma partida rodando: pode bloquear sem limite */
    }

    time_t restante = menor_prazo - agora;
    if (restante < 0) restante = 0;

    tv->tv_sec = restante;
    tv->tv_usec = 0;
    return tv;
}

/*
 * Cliente 'i' caiu no meio de uma partida. Avisa o adversário que ele
 * ganhou por W.O. e marca os dois pra saírem da lista.
 */
static void tratar_desconexao(int i) {
    Jogador *j = &clientes[i];

    if (j->em_partida && j->id_partida >= 0) {
        Partida *p = &partidas[j->id_partida];
        int idx_oponente = (p->idx_a == i) ? p->idx_b : p->idx_a;
        Jogador *op = &clientes[idx_oponente];

        char msg[TAM_BUFFER];
        snprintf(msg, sizeof(msg), "%s desconectou. Você venceu por W.O.", j->nome);
        enviar_msg(op->fd, "%s|%s", MSG_FIM, msg);

        printf("[Partida #%d] Encerrada: %s desconectou.\n", p->numero_partida, j->nome);

        p->ativa = 0;
        op->em_partida = 0;
        op->id_partida = -1;
        op->remover = 1;
    }

    j->em_partida = 0;
    j->id_partida = -1;
    j->remover = 1;
}

/*
 * Lê o que o cliente 'i' mandou. Só interessa duas coisas vindas dele
 * nesse ponto do jogo: PALAVRA|... ou TIMEOUT| (o nome já foi pego lá
 * no accept, então não preciso mais tratar NOME aqui).
 */
static void tratar_mensagem_cliente(int i) {
    char buffer[TAM_BUFFER * 2];
    ssize_t n = recv(clientes[i].fd, buffer, sizeof(buffer) - 1, 0);

    if (n <= 0) {
        printf("[-] %s desconectou (fd=%d)\n", clientes[i].nome, clientes[i].fd);
        tratar_desconexao(i);
        return;
    }
    buffer[n] = '\0';

    /* Um recv() pode trazer mais de uma linha grudada (o TCP não
     * garante que cada send() do cliente vira um recv() separado
     * aqui do lado do servidor), então separo por '\n' e processo uma
     * linha de cada vez. */
    char *salvar = NULL;
    char *linha = strtok_r(buffer, "\n", &salvar);
    while (linha != NULL) {
        char tipo[TAM_TIPO], dados[TAM_BUFFER];
        parse_mensagem(linha, tipo, dados);

        if ((strcmp(tipo, MSG_PALAVRA) == 0 || strcmp(tipo, MSG_TIMEOUT) == 0)
            && clientes[i].em_partida) {
            registrar_resposta(&partidas[clientes[i].id_partida], i, tipo, dados);
        }

        linha = strtok_r(NULL, "\n", &salvar);
    }
}

/*
 * Trata uma nova conexão. Aqui é onde eu segui bem de perto o trecho
 * que vi no chat_servidor.c: assim que aceito a conexão, peço o nome
 * e chamo recv() DIRETO, sem passar pelo select(). Isso quer dizer
 * que o servidor fica esperando bloqueado só nesse recv() até o
 * jogador digitar o nome — igual acontece no exemplo do chat.
 *
 * Sei que isso trava o servidor inteiro (inclusive outras partidas em
 * andamento) enquanto esse jogador não manda o nome. Pra esse
 * trabalho, com poucos jogadores testando ao mesmo tempo, na prática
 * não chega a ser um problema, porque o cliente manda o nome assim
 * que conecta. Mas é um ponto que eu sei que existe.
 */
static void aceitar_conexao(int server_fd) {
    struct sockaddr_in cliente_addr;
    socklen_t tam = sizeof(cliente_addr);

    int novo_fd = accept(server_fd, (struct sockaddr *)&cliente_addr, &tam);
    if (novo_fd < 0) {
        perror("Erro no accept");
        return;
    }

    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cliente_addr.sin_addr, ip_str, sizeof(ip_str));
    int porta_cliente = ntohs(cliente_addr.sin_port);
    printf("[+] Nova conexão: %s:%d (fd=%d)\n", ip_str, porta_cliente, novo_fd);

    /* Pede o nome (mensagem NOME| do protocolo) e espera a resposta
     * de forma bloqueante, igual ao exemplo do chat. */
    enviar_msg(novo_fd, "%s|", MSG_NOME);

    char linha[TAM_BUFFER];
    ssize_t n = recv(novo_fd, linha, sizeof(linha) - 1, 0);
    if (n <= 0) {
        printf("[-] Cliente desconectou antes de informar o nome\n");
        close(novo_fd);
        return;
    }
    linha[n] = '\0';

    /* Tira o '\n' do final da linha, se tiver */
    char *quebra = strchr(linha, '\n');
    if (quebra) *quebra = '\0';

    char tipo[TAM_TIPO], nome[TAM_BUFFER];
    parse_mensagem(linha, tipo, nome);
    if (nome[0] == '\0') strcpy(nome, "Jogador");

    int idx = adicionar_cliente(novo_fd, nome, ip_str, porta_cliente);
    if (idx < 0) {
        enviar_msg(novo_fd, "%s|Servidor cheio. Tente mais tarde.", MSG_MSG);
        close(novo_fd);
        printf("[!] Servidor cheio, conexão de \"%s\" recusada\n", nome);
        return;
    }

    printf("[+] \"%s\" entrou (fd=%d)\n", clientes[idx].nome, novo_fd);

    /* Procuro alguém que já esteja esperando adversário. Se achar,
     * a partida já começa; se não achar, esse jogador vira quem
     * espera a vez. */
    int adversario = -1;
    for (int i = 0; i < num_clientes; i++) {
        if (i != idx && !clientes[i].em_partida) {
            adversario = i;
            break;
        }
    }

    if (adversario == -1) {
        enviar_msg(clientes[idx].fd, "%s|Aguardando outro jogador para iniciar...", MSG_AGUARDE);
    } else {
        iniciar_partida(adversario, idx);
    }
}

int main(int argc, char *argv[]) {
    int porta = PORTA_PADRAO;
    if (argc >= 2) porta = atoi(argv[1]);

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, tratar_sigint);
    srand((unsigned int)time(NULL));

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }
    g_fd_escuta = server_fd;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in servidor_addr;
    memset(&servidor_addr, 0, sizeof(servidor_addr));
    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_addr.s_addr = INADDR_ANY;
    servidor_addr.sin_port = htons((uint16_t)porta);

    if (bind(server_fd, (struct sockaddr *)&servidor_addr, sizeof(servidor_addr)) < 0) {
        perror("Erro no bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Erro no listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("========================================\n");
    printf("   BATALHA DE PALAVRAS — Servidor\n");
    printf("   Porta: %d\n", porta);
    printf("   Aguardando jogadores (pares de 2)...\n");
    printf("========================================\n\n");

    while (1) {
        fd_set read_fds;
        int    max_fd;

        /* Monto o fd_set de novo a cada volta, igual o chat faz —
         * select() só deixa marcado quem realmente teve atividade, e
         * a próxima chamada precisa começar do zero. */
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;

        for (int i = 0; i < num_clientes; i++) {
            FD_SET(clientes[i].fd, &read_fds);
            if (clientes[i].fd > max_fd) max_fd = clientes[i].fd;
        }

        struct timeval tv;
        struct timeval *ptv = calcular_timeout(&tv);

        int atividade = select(max_fd + 1, &read_fds, NULL, NULL, ptv);
        if (atividade < 0) {
            if (errno == EINTR) continue;
            perror("Erro no select");
            break;
        }

        /* Alguém tentando conectar */
        if (FD_ISSET(server_fd, &read_fds)) {
            aceitar_conexao(server_fd);
        }

        /* Algum jogador já conectado mandou alguma coisa. Percorro de
         * trás pra frente por hábito (é o que o chat faz), mas o
         * importante mesmo é que ninguém é removido AQUI dentro — só
         * mais tarde, em limpar_marcados_para_remover(). */
        for (int i = num_clientes - 1; i >= 0; i--) {
            if (FD_ISSET(clientes[i].fd, &read_fds)) {
                tratar_mensagem_cliente(i);
            }
        }
        limpar_marcados_para_remover();

        /* Alguma rodada pode ter estourado o prazo mesmo sem ninguém
         * mandar mensagem — é por isso que calculamos o timeout do
         * select() lá em cima. */
        verificar_prazos();
        limpar_marcados_para_remover();
    }

    for (int i = 0; i < num_clientes; i++) close(clientes[i].fd);
    close(server_fd);
    printf("\n[SERVIDOR] Encerrado.\n");
    return 0;
}
