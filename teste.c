#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <string.h>
#include <ctype.h>
#include "timer.h" // Assumindo que você tenha este arquivo

// --- Nomes de variáveis de contador_conc.c ---
#define BUFFER 64
#define TAMANHO_LEITURA 1024 // 1KB

char *Buffer[BUFFER]; // O buffer compartilhado (fila de tarefas)
const char *separadores = " \t\n\r¹²³£¢¬§ªº°\"!@#$%¨&*()_+`{}^<>:?'=´[]~,.;/";
sem_t cheio, vazio; // Semáforos de contagem
sem_t mutex; // Mutex para proteger o acesso ao buffer
// ------------------------------------------------

// --- OTIMIZAÇÃO: Tabela de Consulta (Lookup Table) ---
// Em vez de usar strchr(separadores, c) (lento, O(M)), usamos uma
// tabela de acesso O(1).
char lookup_separadores[256] = {0};

/**
 * @brief Inicializa a tabela de consulta de separadores.
 * Marca 1 para cada caractere que é um separador.
 */
void inicializar_lookup() {
    // Seta todos os separadores como '1' na tabela
    for (int i = 0; i < strlen(separadores); i++) {
        lookup_separadores[(unsigned char)separadores[i]] = 1;
    }
}
// --- FIM DA OTIMIZAÇÃO ---


/**
 * @brief Conta palavras usando a tabela de consulta (O(N)).
 *
 * Esta versão é muito mais rápida que a original.
 * Usa uma máquina de estados simples (IN_WORD ou OUT_WORD)
 * e faz uma única passagem pelo texto.
 */
int contar_palavras_otimizado(char *texto)
{
    int contador = 0;
    // Estado: 0 = Fora da palavra (OUT), 1 = Dentro da palavra (IN)
    int estado = 0;
    char *ptr = texto;

    while (*ptr != '\0')
    {
        // Verifica se o caractere atual é um separador usando a tabela O(1)
        if (lookup_separadores[(unsigned char)*ptr]) {
            // É um separador
            estado = 0; // Estamos FORA de uma palavra
        } else {
            // É um caractere de palavra
            if (estado == 0) {
                // Se estávamos FORA, agora entramos (transição 0 -> 1)
                estado = 1;
                contador++; // Contamos a nova palavra
            }
            // Se estado já era 1, continuamos dentro da palavra, não fazemos nada.
        }
        ptr++;
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
        
        // --- OTIMIZAÇÃO: Chama a função de contagem rápida ---
        parcial += contar_palavras_otimizado(bloco_para_processar);
        
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
    pthread_t *tid_consumidores; 

    // --- Início da contagem de tempo ---
    double start, finish, delta;
    GET_TIME(start);

    FILE *arq;
    arq = fopen(argv[1],"r");
    if (arq==NULL) {
        printf("Erro ao abrir o arquivo %s\n", argv[1]);
        return 1;
    }
    
    // --- OTIMIZAÇÃO: Inicializa a tabela de consulta ---
    inicializar_lookup();

    
    // --- Inicialização ---
    sem_init(&vazio, 0, BUFFER); 
    sem_init(&cheio, 0, 0);          
    sem_init(&mutex, 0, 1);         

    tid_consumidores = (pthread_t*) malloc(sizeof(pthread_t) * num_threads);
    if(tid_consumidores == NULL) {
        printf("Erro ao alocar threads"); exit(1);
    }


    // --- Criação das Threads Consumidoras ---
    for (long i = 0; i < num_threads; i++) {
        if (pthread_create(&tid_consumidores[i], NULL, consumidor, (void*)i)) {
            printf("Erro ao criar thread"); exit(1);
        }
    }
    
    // --- Lógica do Produtor --- (Buffer de Restos)
    
    // OTIMIZAÇÃO: 'leitura' agora é lido com 'fread', não 'fgets'
    char leitura[TAMANHO_LEITURA + 1]; // +1 para o '\0'
    size_t bytes_lidos;
    
    char restos[TAMANHO_LEITURA + 1] = "";  
    char bloco_processar[TAMANHO_LEITURA * 2 + 1];
    
    // --- OTIMIZAÇÃO: Troca de fgets() por fread() ---
    // fread() é a função correta para ler blocos de tamanho fixo.
    // Ela não para em quebras de linha ('\n').
    while ((bytes_lidos = fread(leitura, 1, TAMANHO_LEITURA, arq)) > 0) {
        leitura[bytes_lidos] = '\0'; // Garante que é uma string C válida

        strcpy(bloco_processar, restos);
        strcat(bloco_processar, leitura);

        size_t len = strlen(bloco_processar);
        int pos_corte = -1; 

        // --- OTIMIZAÇÃO: Loop O(N) em vez de O(N*M) ---
        // Usa a tabela de consulta em vez de strchr()
        for (int i = len - 1; i >= 0; i--) {
            if (lookup_separadores[(unsigned char)bloco_processar[i]]) {
                pos_corte = i;
                break;
            }
        }
        // --- FIM DA OTIMIZAÇÃO ---

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