#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <ctype.h>
#include "timer.h" // Assumindo que você tenha este arquivo

// --- Nomes de variáveis de contador_conc.c ---
#define BUFFER 100
#define TAMANHO_LEITURA 1024 * 100// 100KB

char *Buffer[BUFFER]; // O buffer compartilhado (fila de tarefas)
const char *separadores = " \t\n\r¹²³£¢¬§ªº°\"!@#$%¨&*()_+`{}^<>:?'=´[]~,.;/";
sem_t cheio, vazio; // Semáforos de contagem
sem_t mutex; // Mutex para proteger o acesso ao buffer
// ------------------------------------------------


int contar_palavras(char *texto)
{
    int contador = 0;
    char *ptr = texto; // Ponteiro para percorrer o texto

    while (*ptr != '\0')
    {
        // Pula os separadores
        ptr += strspn(ptr, separadores);
        if (*ptr == '\0') {
            break;
        }
        // Encontra o fim da palavra
        size_t len_palavra = strcspn(ptr, separadores);

        if (len_palavra > 0)
        {
            contador++;
            ptr += len_palavra;
        }
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
    
    Buffer[in] = frag; // Usa 'Buffer'
    in = (in + 1) % BUFFER; // Usa 'BUFFER'
    
    sem_post(&mutex); // Libera o mutex
    sem_post(&cheio); // Sinaliza que há um novo slot cheio
}

void *consumidor(void *arg) {
    long parcial = 0; // Contador parcial
    char *bloco_para_processar;

    while (1) {
        bloco_para_processar = Retira(); 

        // Se o bloco for NULL, é o sinal de término
        if (bloco_para_processar == NULL) {
            break; 
        }
        parcial += contar_palavras(bloco_para_processar);
        
        free(bloco_para_processar); // Libera a memória que o produtor alocou
    }

    // Aloca memória para o resultado parcial e o retorna
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

    // --- Início da contagem de tempo ---
    GET_TIME(start);

    // --- Criação das Threads Consumidoras ---
    for (long i = 0; i < num_threads; i++) {
        if (pthread_create(&tid_consumidores[i], NULL, consumidor, (void*)i)) {
            printf("Erro ao criar thread"); exit(1);
        }
    }
    
    // --- Lógica do Produtor --- (Buffer de Restos)
    char leitura[TAMANHO_LEITURA + 1]; 
    char restos[TAMANHO_LEITURA + 1] = "";  
    char bloco_processar[TAMANHO_LEITURA * 2 + 1];
    
    while (fgets(leitura, TAMANHO_LEITURA + 1, arq)) {
        strcpy(bloco_processar, restos);
        strcat(bloco_processar, leitura);

        size_t len = strlen(bloco_processar);
        int pos_corte = -1; 

        for (int i = len - 1; i >= 0; i--) {
            if (strchr(separadores, bloco_processar[i]) != NULL) {
                pos_corte = i;
                break;
            }
        }

        if (pos_corte != -1) {
            strcpy(restos, &bloco_processar[pos_corte + 1]);
            bloco_processar[pos_corte + 1] = '\0';
        } 
        else {
            strcpy(restos, bloco_processar);
            continue; 
        }

        char *bloco_para_fila = (char*) malloc(strlen(bloco_processar) + 1);
        if(bloco_para_fila == NULL) {
            printf("Erro ao alocar bloco para fila"); exit(1);
        }
        strcpy(bloco_para_fila, bloco_processar);
        
        // 'Insere' já foi definida acima
        Insere(bloco_para_fila);
    }
    
    // --- Fim do Arquivo ---
    if (strlen(restos) > 0) {
        char *bloco_final = (char*) malloc(strlen(restos) + 1);
        if(bloco_final == NULL) {
            printf("Erro ao alocar bloco final"); exit(1);
        }
        strcpy(bloco_final, restos);
        Insere(bloco_final);
    }

    for(int i=0; i<num_threads; i++) {
        Insere(NULL);
    }
    
    // --- Agregação de resultados ---
    long total = 0;
    long *resp_parcial; 

    for (int i = 0; i < num_threads; i++) {
        pthread_join(tid_consumidores[i], (void**) &resp_parcial);
        total += *resp_parcial; 
        free(resp_parcial);     
    }
    
    // --- Fim da contagem de tempo ---
    GET_TIME(finish);
    delta = finish - start;

    fclose(arq);
    free(tid_consumidores);
    sem_destroy(&cheio);
    sem_destroy(&vazio);
    sem_destroy(&mutex);

    // --- Impressão final ---
    printf("Arquivo: %s \tNumero de palavras: %ld \tTempo de Execucao: %lf \tNum Threads: %d\n", argv[1], total, delta, num_threads);
    
    return 0;
}