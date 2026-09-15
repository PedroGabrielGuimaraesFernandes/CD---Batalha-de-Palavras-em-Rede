/* ============================================================================
 * ARQUIVO: cliente.c
 * OBJETIVO: Implementar o lado cliente que se conecta ao servidor.
 * ============================================================================
 * 
 * TODO LIST:
 * [ ] 1. Incluir bibliotecas padrão (stdio, stdlib, string, unistd, etc).
 * [ ] 2. Incluir headers de rede (sys/socket.h, netinet/in.h, arpa/inet.h).
 * [ ] 3. Incluir o header "protocolo.h" para uso das mensagens estruturadas.
 * [ ] 4. Ler o IP e a Porta do servidor (hardcoded, via argv, ou input do usuário).
 * [ ] 5. Criar o socket do cliente usando socket(AF_INET, SOCK_STREAM, 0).
 * [ ] 6. Verificar se o socket foi criado com sucesso (tratamento de erro).
 * [ ] 7. Configurar a estrutura sockaddr_in com os dados do servidor alvo.
 * [ ] 8. Estabelecer conexão com o servidor usando connect().
 * [ ] 9. Coletar os dados/inputs que precisam ser enviados.
 * [ ] 10. Montar a struct de requisição (baseada no protocolo.h) com os dados.
 * [ ] 11. Enviar a requisição para o servidor usando send() ou write().
 * [ ] 12. Aguardar a resposta do servidor usando recv() ou read() na struct apropriada.
 * [ ] 13. Processar/Exibir o resultado recebido na tela para o usuário.
 * [ ] 14. Fechar o socket do cliente com close() e encerrar o programa.
 */

#include <stdio.h>
#include <stdlib.h>
// Demais includes e código vêm aqui...
