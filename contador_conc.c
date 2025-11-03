#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <ctype.h>
#include "timer.h" 

#define BUFFER 64
#define TAMANHO_LEITURA 1024*10 // 10KB

char *Buffer[BUFFER]; // O buffer compartilhado (fila de tarefas)
const char *separadores = " \t\n\r¹²³£¢¬§ªº°\"!@#$%¨&*()_+`{}^<>:?'=´[]~,.;/";
sem_t cheio, vazio; // Semáforos de contagem
sem_t mutex; // Mutex para proteger o acesso ao buffer

// TABELA DE SEPARADORES (substitui strchr) 
char tabela_separadores[256] = {0};

void iniciar_tabela() {
    for (int i = 0; i < (int)strlen(separadores); i++) {
        tabela_separadores[(unsigned char)separadores[i]] = 1;
    }
}

int contar_palavras(char *texto)
{
    int contador = 0;
    char *ptr = texto; // Ponteiro para percorrer o texto

    while (*ptr != '\0')
    {
        // Pula os separadores
        while (*ptr != '\0' && tabela_separadores[(unsigned char)*ptr])
            ptr++;

        if (*ptr == '\0') break;

        // Conta palavra e avança até o próximo separador
        while (*ptr != '\0' && !tabela_separadores[(unsigned char)*ptr])
            ptr++;

        contador++;
    }
    return contador;
}

char *Retira(void) {
    static int out = 0;
    char *frag;

    sem_wait(&cheio); // Espera por um slot cheio
    sem_wait(&mutex); // Trava o mutex
    
    frag = Buffer[out]; 
    out = (out + 1) % BUFFER; 
    
    sem_post(&mutex); // Libera o mutex
    sem_post(&vazio); // Sinaliza que há um novo slot vazio

    return frag;
}

void Insere(char *frag) {
    static int in = 0;
    sem_wait(&vazio); // Espera por um slot vazio
    sem_wait(&mutex); // Trava o mutex para acesso exclusivo ao buffer
    
    Buffer[in] = frag;
    in = (in + 1) % BUFFER;
    
    sem_post(&mutex);
    sem_post(&cheio);
}

void *consumidor(void *arg) {
    long parcial = 0;
    char *bloco_para_processar;

    while (1) {
        bloco_para_processar = Retira();

        if (bloco_para_processar == NULL) break;

        parcial += contar_palavras(bloco_para_processar);
        free(bloco_para_processar);
    }

    long *resultado = malloc(sizeof(long));
    if (resultado == NULL) {
        fprintf(stderr, "Erro de alocação de memória no consumidor\n");
        exit(1);
    }
    *resultado = parcial;
    pthread_exit((void*) resultado);
    return NULL;
}

int main(int argc, char *argv[]){
    if (argc != 3) {
        printf("Uso: %s <arquivo.txt> <num_threads>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[2]);
    if (num_threads <= 0) {
        printf("Número de threads deve ser maior que 0\n");
        return 1;
    }
    if (num_threads > BUFFER) num_threads = BUFFER;

    FILE *arq;
    arq = fopen(argv[1],"r");
    if (arq==NULL) {
        printf("Erro ao abrir o arquivo %s\n", argv[1]);
        return 1;
    }

    iniciar_tabela(); // Inicia tabela de separadores

    pthread_t *tid_consumidores; 
    double start, finish, delta;
    
    // --- Inicialização ---
    sem_init(&vazio, 0, BUFFER); 
    sem_init(&cheio, 0, 0);          
    sem_init(&mutex, 0, 1);         

    tid_consumidores = (pthread_t*) malloc(sizeof(pthread_t) * num_threads);
    if(tid_consumidores == NULL) {
        printf("Erro ao alocar threads"); exit(1);
    }

    GET_TIME(start);

    for (long i = 0; i < num_threads; i++) {
        if (pthread_create(&tid_consumidores[i], NULL, consumidor, (void*)i)) {
            printf("Erro ao criar thread"); exit(1);
        }
    }
    
    // --- Lógica do Produtor ---
    char leitura[TAMANHO_LEITURA + 1];
    char restos[TAMANHO_LEITURA + 1] = "";
    char bloco_processar[TAMANHO_LEITURA * 2 + 1];
    size_t bytes_lidos;

    while ((bytes_lidos = fread(leitura, 1, TAMANHO_LEITURA, arq)) > 0) {
        leitura[bytes_lidos] = '\0';

        strcpy(bloco_processar, restos);
        strcat(bloco_processar, leitura);

        int pos_corte = -1;
        for (int i = (int)strlen(bloco_processar) - 1; i >= 0; i--) {
            if (tabela_separadores[(unsigned char)bloco_processar[i]]) {
                pos_corte = i;
                break;
            }
        }

        if (pos_corte != -1) {
            strcpy(restos, &bloco_processar[pos_corte + 1]);
            bloco_processar[pos_corte + 1] = '\0';
        } else {
            if (strlen(bloco_processar) < TAMANHO_LEITURA)
                strcpy(restos, bloco_processar);
            else
                restos[0] = '\0';
            continue;
        }

        char *bloco_para_fila = (char *)malloc(strlen(bloco_processar) + 1);
        if (bloco_para_fila == NULL) {
            printf("Erro ao alocar bloco para fila\n");
            exit(1);
        }
        strcpy(bloco_para_fila, bloco_processar);
        Insere(bloco_para_fila);
    }

    if (strlen(restos) > 0) {
        char *bloco_final = (char *)malloc(strlen(restos) + 1);
        if (bloco_final == NULL) {
            printf("Erro ao alocar bloco final\n");
            exit(1);
        }
        strcpy(bloco_final, restos);
        Insere(bloco_final);
    }
    for(int i=0; i<num_threads; i++) {
        Insere(NULL);
    }
    // --- Fim da Lógica do Produtor ---
    
    long total = 0;
    long *resp_parcial; 

    for (int i = 0; i < num_threads; i++) {
        pthread_join(tid_consumidores[i], (void**) &resp_parcial);
        total += *resp_parcial; 
        free(resp_parcial);     
    }

    GET_TIME(finish);
    delta = finish - start;

    fclose(arq);
    free(tid_consumidores);
    sem_destroy(&cheio);
    sem_destroy(&vazio);
    sem_destroy(&mutex);

    printf("Arquivo: %s \tNumero de palavras: %ld \tTempo de Execucao: %lf \tNum Threads: %d\n",
           argv[1], total, delta, num_threads);
    
    return 0;
}
