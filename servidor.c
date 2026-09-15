/* ============================================================================
 * ARQUIVO: servidor.c
 * OBJETIVO: Implementar o lado servidor da aplicação de Sockets.
 * ============================================================================
 * 
 * TODO LIST:
 * [ ] 1. Incluir bibliotecas padrão (stdio, stdlib, string, unistd, etc).
 * [ ] 2. Incluir headers de rede (sys/socket.h, netinet/in.h, arpa/inet.h).
 * [ ] 3. Incluir o header "protocolo.h" para acessar as estruturas de dados.
 * [ ] 4. Definir a porta na qual o servidor vai rodar (via macro ou argv).
 * [ ] 5. Criar o socket do servidor usando socket(AF_INET, SOCK_STREAM, 0).
 * [ ] 6. Verificar se o socket foi criado com sucesso (tratamento de erro).
 * [ ] 7. Configurar a estrutura sockaddr_in do servidor (Family, ADDR_ANY, Port).
 * [ ] 8. Fazer o bind() associando o socket do servidor ao IP e Porta.
 * [ ] 9. Colocar o socket em modo de escuta usando listen().
 * [ ] 10. Iniciar o loop principal do servidor (ex: while(1)).
 * [ ] 11. Dentro do loop: Aceitar a conexão do cliente usando accept().
 * [ ] 12. Dentro do loop: Ler a requisição do cliente com recv() ou read(), 
 *         armazenando os dados na struct definida no protocolo.h.
 * [ ] 13. Dentro do loop: Processar os dados recebidos de acordo com a regra de negócio.
 * [ ] 14. Dentro do loop: Montar a struct de resposta baseada no protocolo.h.
 * [ ] 15. Dentro do loop: Enviar a resposta de volta ao cliente com send() ou write().
 * [ ] 16. Dentro do loop: Fechar o descritor de arquivo do cliente conectado (close).
 * [ ] 17. Fechar o socket principal do servidor no encerramento do programa.
 */

#include <stdio.h>
#include <stdlib.h>
// Demais includes e código vêm aqui...
