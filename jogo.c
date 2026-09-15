/* ============================================================================
 * ARQUIVO: jogo.c
 * OBJETIVO: Implementar a lógica de negócio e as regras do jogo de forma 
 *           totalmente isolada da infraestrutura de rede (sockets).
 * ============================================================================
 * 
 * TODO LIST:
 * [ ] 1. Incluir bibliotecas padrão (stdio, stdlib, stdbool, etc).
 * [ ] 2. Incluir o header "jogo.h" (que conterá as assinaturas das funções e as 
 *        estruturas de dados do estado atual do jogo, como o tabuleiro).
 * [ ] 3. Implementar função de INICIALIZAÇÃO: Configurar o estado inicial do 
 *        jogo (ex: zerar o tabuleiro, definir quem começa, resetar pontuação).
 * [ ] 4. Implementar função de VALIDAÇÃO DE JOGADA: Receber uma tentativa de 
 *        movimento e retornar se é válida (ex: verificar se a casa já está 
 *        ocupada ou se as coordenadas estão fora do limite).
 * [ ] 5. Implementar função de EXECUÇÃO DE JOGADA: Atualizar o estado do jogo 
 *        com o movimento validado.
 * [ ] 6. Implementar função de VERIFICAÇÃO DE FIM DE PARTIDA: Checar as regras 
 *        de vitória, derrota ou empate (ex: trinca formada, limite de turnos, etc),
 *        retornando o status da partida.
 * [ ] 7. Implementar função para ALTERNAR O TURNO: Mudar qual é o jogador ativo.
 * [ ] 8. Implementar função de FORMATAÇÃO (Opcional, mas recomendada): Criar 
 *        uma função que transforma o estado atual do jogo (o tabuleiro) em uma 
 *        string, facilitando o envio dessa visão geral pelo servidor de volta 
 *        ao cliente.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
// #include "jogo.h"

// Implementação das funções de inicialização, validação, jogada e fim de jogo...
