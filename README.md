# Sistema de Matrículas com Threads em C

Este projeto implementa um sistema simulado de matrículas universitárias utilizando a biblioteca `pthread` da linguagem C. O código demonstra conceitos de programação concorrente por meio do padrão **Produtor-Consumidor**, com gerenciamento de prioridades e mecanismos de sincronização entre threads.

## Como Funciona a Dinâmica

O sistema é dividido em três agentes principais, executados simultaneamente em threads:

- **Alunos (Produtores):** Enviam solicitações de matrícula para uma fila compartilhada. Após o envio, aguardam o início e o término da aula.
- **Secretaria/Processadores (Consumidores):** Retiram os pedidos da fila respeitando uma ordem de prioridade (Formandos > Maior IRA > Tempo de chegada). Acessam as vagas das disciplinas de forma segura, utilizando mutexes, e contabilizam os pedidos processados.
- **Professor:** Aguarda a finalização de todas as matrículas. Quando o último processador termina, o professor é sinalizado para iniciar a aula. Ao final, utiliza um sinal de broadcast para liberar todos os alunos simultaneamente.

## Tecnologias e Conceitos Utilizados

- **Linguagem C**
- **POSIX Threads (`pthread`)**
- **Mutexes (`pthread_mutex_t`):** Protegem o acesso à fila compartilhada e às vagas das disciplinas, evitando condições de corrida.
- **Variáveis de Condição (`pthread_cond_t`):** Coordenam os estados de espera e sinalização das threads, controlando o fluxo da fila e a sincronização entre alunos, processadores e professor.
- **Produtor-Consumidor:** Organiza a comunicação entre os alunos, que produzem solicitações, e os processadores, que as consomem.

## Como Compilar e Executar

Para executar o projeto, é necessário possuir um compilador C, como o GCC, e suporte à biblioteca `pthread`.

**1. Compile o código no terminal Linux ou WSL:**

```bash
gcc concorrente.c -o projeto_concorrente -pthread
```

**2. Execute o programa:**

```bash
./projeto_concorrente
```