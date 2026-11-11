#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define n_aluno 8
#define n_processadore 3
#define n_disciplinas 2
#define tamanho_fila 4

// Disciplina com nome, quantidade de vagas e mutex para controle de acesso
typedef struct {
    char nome[40];
    int vagas;
    pthread_mutex_t mutex_disciplina;
} Disciplina;

// Solicitação de matricula com id do aluno, se é formando, IRA, timestamp e id do disciplina
typedef struct {
    int id_aluno;
    int formando;
    float ira;
    int timestamp;
    int id_disciplina;
} Solicitacao;

// Fila de solicitações com array de pedidos, quantidade, mutex e variaveis de condição
typedef struct {
    Solicitacao pedidos[tamanho_fila];
    int quantidade;

    pthread_mutex_t mutex_fila;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

    int encerrando;
} Fila;

// ArgsAluno struct para passar os dados do aluno para a thread
typedef struct {
    Solicitacao pedido;
} ArgsAluno;

// Variáveis globais originais
Disciplina disciplinas[n_disciplinas];
Fila fila;

// variaveis globais para controle da aula
int alunos_processados = 0;
int aula_encerrada = 0;
pthread_mutex_t mutex_aula;
pthread_cond_t cond_inicio_aula; // Professor espera aqui
pthread_cond_t cond_fim_aula;    // Alunos esperam aqui

// Comparação de prioridade entre duas solicitações
int tem_prioridade(Solicitacao a, Solicitacao b) {
    if (a.formando != b.formando)
        return a.formando > b.formando;
    if (a.ira != b.ira)
        return a.ira > b.ira;
    return a.timestamp < b.timestamp;
}

// Insere uma solicitação na fila
void inserir_fila(Solicitacao pedido) {
    pthread_mutex_lock(&fila.mutex_fila);

    while (fila.quantidade == tamanho_fila) {
        pthread_cond_wait(&fila.not_full, &fila.mutex_fila);
    }

    int i = fila.quantidade;
    while (i > 0 && tem_prioridade(pedido, fila.pedidos[i - 1])) {
        fila.pedidos[i] = fila.pedidos[i - 1];
        i--;
    }

    fila.pedidos[i] = pedido;
    fila.quantidade++;

    printf("[Aluno %d] Solicitação inserida na fila.\n", pedido.id_aluno);
    pthread_cond_signal(&fila.not_empty);
    pthread_mutex_unlock(&fila.mutex_fila);
}

// Retira uma solicitaçao da fila
int retirar_fila(Solicitacao *pedido) {
    pthread_mutex_lock(&fila.mutex_fila);

    while (fila.quantidade == 0 && !fila.encerrando) {
        pthread_cond_wait(&fila.not_empty, &fila.mutex_fila);
    }

    if (fila.quantidade == 0 && fila.encerrando) {
        pthread_mutex_unlock(&fila.mutex_fila);
        return 0;
    }

    *pedido = fila.pedidos[0];

    for (int i = 0; i < fila.quantidade - 1; i++) {
        fila.pedidos[i] = fila.pedidos[i + 1];
    }

    fila.quantidade--;

    pthread_cond_signal(&fila.not_full);
    pthread_mutex_unlock(&fila.mutex_fila);

    return 1;
}

void testar_prioridade() {
    printf("\nTeste de prioridade: \n");
    Solicitacao testes[4] = {
        {1, 0, 9.8, 4, 0},
        {2, 1, 8.0, 4, 0},
        {3, 0, 8.5, 3, 0},
        {4, 1, 8.0, 2, 0}
    };
    int ordem_esperada[4] = {4, 2, 1, 3};

    for (int i = 0; i < 4; i++) {
        inserir_fila(testes[i]);
    }

    for (int i = 0; i < 4; i++) {
        Solicitacao pedido;
        retirar_fila(&pedido);
        printf("Esperado: Aluno %d | Obtido: Aluno %d\n", ordem_esperada[i], pedido.id_aluno);

        if (pedido.id_aluno != ordem_esperada[i]) {
            printf("[Falha] Teste de prioridade!\n");
            return;
        }
    }
    printf("[Sucesso] Ordenacao por prioridade correta!\n\n");
}

// Thread do professor que aguarda todos os alunos serem processados antes de iniciar a aula
void* executar_professor(void *arg) {
    pthread_mutex_lock(&mutex_aula);
    
    // Professor aguarda até que todos os alunos passem pela secretaria
    while (alunos_processados < n_aluno) {
        pthread_cond_wait(&cond_inicio_aula, &mutex_aula);
    }
    
    printf("\n[Professor] Todos os alunos foram matriculados/recusados. Iniciando a aula...\n");
    sleep(3); // Tempo de duração da aula
    
    printf("[Professor] Aula encerrada! Alunos liberados para ir embora.\n\n");
    
    // Atualiza a flag e acorda todos os alunos
    aula_encerrada = 1;
    pthread_cond_broadcast(&cond_fim_aula); 
    
    pthread_mutex_unlock(&mutex_aula);
    return NULL;
}

// Thread do aluno que solicita a matricula e espera a aula do professor acabar
void* executar_aluno(void *arg) {
    ArgsAluno *dados = (ArgsAluno*) arg;

    printf("[Aluno %d] Solicitando matrícula...\n", dados->pedido.id_aluno);
    inserir_fila(dados->pedido);

    // O aluno agora espera a aula do professor acabar
    pthread_mutex_lock(&mutex_aula);
    while (!aula_encerrada) {
        pthread_cond_wait(&cond_fim_aula, &mutex_aula);
    }
    pthread_mutex_unlock(&mutex_aula);

    printf("[Aluno %d] Aula acabou. Indo para casa.\n", dados->pedido.id_aluno);

    return NULL;
}

// tread do processsador 
void* processar_matricula(void *arg) {
    int id_processador = *(int*)arg;  
    Solicitacao pedido;

    // Processa as solicitações que estão na fila até a fila ser encerrada
    while (retirar_fila(&pedido)) {
        Disciplina *disciplina = &disciplinas[pedido.id_disciplina];

        printf("[Processador %d] Processando aluno %d em %s.\n",
               id_processador, pedido.id_aluno, disciplina->nome);

        pthread_mutex_lock(&disciplina->mutex_disciplina);

        if (disciplina->vagas > 0) {
            sleep(1);
            disciplina->vagas--;
            printf("[Sucesso] Aluno %d matriculado em %s. Vagas restantes: %d\n",
                   pedido.id_aluno, disciplina->nome, disciplina->vagas);
        } else {
            printf("[Recusado] Aluno %d: disciplina %s sem vagas.\n",
                   pedido.id_aluno, disciplina->nome);
        }
        pthread_mutex_unlock(&disciplina->mutex_disciplina);

        // Dinâmica nova: Avisar que processou mais um aluno
        pthread_mutex_lock(&mutex_aula);
        alunos_processados++;
        
        // Se este foi o último aluno da fila a ser processado
        if (alunos_processados == n_aluno) {
            // Acorda o professor
            pthread_cond_signal(&cond_inicio_aula);
            
            // Encerra a fila para que os outros processadores parem o loop
            pthread_mutex_lock(&fila.mutex_fila);
            fila.encerrando = 1;
            pthread_cond_broadcast(&fila.not_empty);
            pthread_mutex_unlock(&fila.mutex_fila);
        }
        pthread_mutex_unlock(&mutex_aula);
    }
    return NULL;
}

int main() {
    pthread_t alunos[n_aluno];
    pthread_t processadores[n_processadore];
    pthread_t professor; // Nova Thread

    ArgsAluno args_alunos[n_aluno];
    int ids_processadores[n_processadore];

    // Inicializacao da fila
    fila.quantidade = 0;
    fila.encerrando = 0;
    pthread_mutex_init(&fila.mutex_fila, NULL);
    pthread_cond_init(&fila.not_empty, NULL);
    pthread_cond_init(&fila.not_full, NULL);

    // Inicialização do controle de aula
    pthread_mutex_init(&mutex_aula, NULL);
    pthread_cond_init(&cond_inicio_aula, NULL);
    pthread_cond_init(&cond_fim_aula, NULL);

    testar_prioridade();

    // Inicializacao das disciplinas
    sprintf(disciplinas[0].nome, "Calculo");
    sprintf(disciplinas[1].nome, "Algoritmos de Programação");
    disciplinas[0].vagas = 3;
    disciplinas[1].vagas = 2;

    for (int i = 0; i < n_disciplinas; i++) {
        pthread_mutex_init(&disciplinas[i].mutex_disciplina, NULL);
    }

    int formandos[n_aluno] = {0, 1, 0, 0, 1, 0, 0, 0};
    float iras[n_aluno] = {8.5, 7.2, 9.1, 6.8, 8.0, 9.5, 7.9, 8.5};
    int materias[n_aluno] = {0, 0, 1, 0, 1, 1, 0, 1};

    printf("Sistema de matriculas:\n");

    // Criação do Professor
    pthread_create(&professor, NULL, executar_professor, NULL);

    // Criacao dos Processadores
    for (int i = 0; i < n_processadore; i++) {
        ids_processadores[i] = i + 1;
        pthread_create(&processadores[i], NULL, processar_matricula, &ids_processadores[i]);
    }

    // Criacao dos Alunos
    for (int i = 0; i < n_aluno; i++) {
        args_alunos[i].pedido.id_aluno = i + 1;
        args_alunos[i].pedido.formando = formandos[i];
        args_alunos[i].pedido.ira = iras[i];
        args_alunos[i].pedido.timestamp = i;
        args_alunos[i].pedido.id_disciplina = materias[i];

        pthread_create(&alunos[i], NULL, executar_aluno, &args_alunos[i]);
    }

    // aguardamos todos os alunos serem processados antes de encerrar a fila 
    //Esperamos o professor dar a aula e encerrar
    pthread_join(professor, NULL);

    //Esperamos os alunos irem embora para casa
    for (int i = 0; i < n_aluno; i++) {
        pthread_join(alunos[i], NULL);
    }

    // Esperamos os processadores terminarem de processar as solicitações
    for (int i = 0; i < n_processadore; i++) {
        pthread_join(processadores[i], NULL);
    }

    printf("\nResultado final:\n");
    for (int i = 0; i < n_disciplinas; i++) {
        printf("%s: %d vagas restantes.\n",
               disciplinas[i].nome, disciplinas[i].vagas);
    }

    // Liberação dos recursos
    for (int i = 0; i < n_disciplinas; i++) {
        pthread_mutex_destroy(&disciplinas[i].mutex_disciplina);
    }
    pthread_mutex_destroy(&fila.mutex_fila);
    pthread_cond_destroy(&fila.not_empty);
    pthread_cond_destroy(&fila.not_full);
    pthread_mutex_destroy(&mutex_aula);
    pthread_cond_destroy(&cond_inicio_aula);
    pthread_cond_destroy(&cond_fim_aula);

    return 0;
}