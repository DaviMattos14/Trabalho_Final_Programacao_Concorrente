#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h> 
#include "timer.h" // Certifique-se de ter este arquivo .h

// Lista de separadores
const char *separadores = " \t\n\r.,;:!?\"'()[]{}*=_@#$<>%&+-/\\|^~`";

// --- Definições do Buffer (Fila) ---

// O "pacote" de trabalho. Contém o ponteiro para os dados
// e o tamanho desses dados.
typedef struct {
    char *data;
    size_t size;
} BufferChunk;

// Struct da Fila (Sem ponteiro para ponteiro)
typedef struct {
    // MUDANÇA: Este é um ponteiro para um array de structs 'BufferChunk',
    // não um ponteiro para um array de ponteiros.
    BufferChunk *chunks; 
    
    int capacity;         // Capacidade máxima da fila
    int count;            // Itens atuais na fila
    int head;             // Posição de retirada (consumidor)
    int tail;             // Posição de inserção (produtor)
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} WorkQueue;

// --- Variáveis Globais ---
WorkQueue queue; // A fila compartilhada
long long total_word_count = 0;
pthread_mutex_t count_lock; // Mutex para o contador global

// --- Funções da Fila (Queue) ---

// MUDANÇA: Aloca espaço para um array de structs 'BufferChunk'
void queue_init(WorkQueue *q, int capacity) {
    q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->capacity = capacity; 
    
    // Aloca espaço para 'capacity' número de STRUCTS 'BufferChunk'
    q->chunks = (BufferChunk*)malloc(sizeof(BufferChunk) * capacity);
    if (q->chunks == NULL) {
        perror("Falha ao alocar a fila (chunks)");
        exit(1);
    }
    
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

// MUDANÇA: A função agora recebe os dados (ponteiro + tamanho)
void queue_put(WorkQueue *q, char *data, size_t size) {
    pthread_mutex_lock(&q->lock);
    
    // Espera se a fila estiver cheia
    while (q->count == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    
    // MUDANÇA: Copia os dados para a struct que JÁ EXISTE na fila
    q->chunks[q->tail].data = data;
    q->chunks[q->tail].size = size;
    
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

// MUDANÇA: A função agora retorna a struct 'BufferChunk' inteira por valor
BufferChunk queue_get(WorkQueue *q) {
    pthread_mutex_lock(&q->lock);
    
    // Espera se a fila estiver vazia
    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    
    // MUDANÇA: Copia a struct para fora da fila
    BufferChunk chunk = q->chunks[q->head];
    
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    
    // Retorna a cópia
    return chunk;
}

// MUDANÇA: Libera apenas o array da fila (o 'data' é liberado pelo consumidor)
void queue_destroy(WorkQueue *q) {
    free(q->chunks); 
    
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

// --- Estrutura de Argumentos para o Produtor ---
typedef struct {
    FILE *arq;
    int buffer_size;
    int num_consumers;
} ProducerArgs;

// --- Thread Produtora (INTELIGENTE) ---
void* producer_thread(void *arg) {
    ProducerArgs *args = (ProducerArgs*)arg;
    FILE *arq = args->arq;
    int buffer_size = args->buffer_size; 

    size_t total_buffer_capacity = (buffer_size * 2) + 1;
    char *main_buffer = (char *)malloc(total_buffer_capacity);
    if (!main_buffer) {
        perror("Produtor: Falha ao alocar buffer principal");
        exit(1);
    }

    size_t remainder_size = 0;
    int is_eof = 0;

    while (!is_eof) {
        size_t bytes_read = fread(main_buffer + remainder_size, 1, buffer_size, arq);

        if (bytes_read == 0) {
            is_eof = 1;
        }

        size_t current_data_size = remainder_size + bytes_read;
        if (current_data_size == 0) {
            break; 
        }

        main_buffer[current_data_size] = '\0';

        long split_pos = -1;
        size_t chunk_size = 0;

        if (is_eof) {
            // Fim do arquivo, envia tudo o que sobrou
            chunk_size = current_data_size;
        } else {
            // Escaneia para trás procurando o último separador
            for (long i = current_data_size - 1; i >= 0; i--) {
                if (strchr(separadores, main_buffer[i]) != NULL) {
                    split_pos = i;
                    break;
                }
            }

            if (split_pos != -1) {
                // Separador encontrado, envia o bloco até ele
                chunk_size = split_pos + 1;
            } else {
                // Palavra gigante, espera por mais dados
                if (current_data_size > buffer_size) {
                    chunk_size = current_data_size;
                } else {
                    chunk_size = 0;
                }
            }
        }

        if (chunk_size > 0) {
            // MUDANÇA: Aloca memória SÓ para os dados (o 'char*')
            char *data_ptr = (char *)malloc(chunk_size + 1);
            if (!data_ptr) {
                perror("Produtor: Falha ao alocar data do chunk");
                continue;
            }
            
            memcpy(data_ptr, main_buffer, chunk_size);
            data_ptr[chunk_size] = '\0';
            
            // MUDANÇA: Coloca o ponteiro de dados e o tamanho na fila
            queue_put(&queue, data_ptr, chunk_size);
            
            // Move o "resto" para o início do buffer
            remainder_size = current_data_size - chunk_size;
            memmove(main_buffer, main_buffer + chunk_size, remainder_size);
        } else {
            // Nenhum chunk enviado, o resto é o buffer todo
            remainder_size = current_data_size;
        }
    }

    free(main_buffer);

    // MUDANÇA: Envia sentinelas (NULL data) para os consumidores
    for (int i = 0; i < args->num_consumers; i++) {
        queue_put(&queue, NULL, 0); 
    }
    
    return NULL;
}

// --- Thread Consumidora (MÁQUINA DE ESTADOS) ---
void* consumer_thread(void *arg) {
    long long local_count = 0;
    int estado = 0; 

    while (1) {
        // MUDANÇA: Pega a struct 'BufferChunk' inteira por valor
        BufferChunk chunk = queue_get(&queue);
        
        // MUDANÇA: Checa o campo .data para a sentinela (fim)
        if (chunk.data == NULL) {
            break; 
        }
        
        estado = 0; // Reseta o estado para cada bloco

        for (size_t i = 0; i < chunk.size; i++) {
            char c = chunk.data[i];
            
            if (strchr(separadores, c) == NULL) {
                // Não é um separador
                if (estado == 0) {
                    local_count++;
                    estado = 1; 
                }
            } else {
                // É um separador
                estado = 0;
            }
        }
        
        // MUDANÇA: Libera o ponteiro de dados que o Produtor alocou
        free(chunk.data);
    }
    
    // Adiciona a contagem local ao total global
    pthread_mutex_lock(&count_lock);
    total_word_count += local_count;
    pthread_mutex_unlock(&count_lock);
    
    return NULL;
}


// --- Função Principal (main) ---
int main(int argc, char *argv[]) {
    double start, end, delta;

    // MUDANÇA: Agora esperamos 5 argumentos
    if (argc < 5) {
        printf("Error! digite %s <arquivo.txt> <tam_buffer_leitura> <num_consumidores> <tam_fila>\n", argv[0]);
        return 1;
    }

    FILE *arq = fopen(argv[1], "r");
    if (arq == NULL) {
        printf("--ERRO: fopen() - %s\n", argv[1]);
        exit(-1);
    }

    int buffer_size = atoi(argv[2]);
    if (buffer_size <= 0) {
        printf("--ERRO: Tamanho de buffer de leitura inválido\n");
        fclose(arq);
        exit(-1);
    }

    int num_consumers = atoi(argv[3]);
    if (num_consumers <= 0) {
        printf("--ERRO: Número de consumidores inválido\n");
        fclose(arq);
        exit(-1);
    }

    // MUDANÇA: Lendo a capacidade da fila da linha de comando
    int queue_capacity = atoi(argv[4]);
    if (queue_capacity <= 0) {
        printf("--ERRO: Tamanho da fila inválido\n");
        fclose(arq);
        exit(-1);
    }

    // MUDANÇA: Passa a capacidade para o inicializador da fila
    queue_init(&queue, queue_capacity);
    pthread_mutex_init(&count_lock, NULL);

    ProducerArgs prod_args;
    prod_args.arq = arq;
    prod_args.buffer_size = buffer_size;
    prod_args.num_consumers = num_consumers;

    // --- Medição de Tempo ---
    GET_TIME(start);

    pthread_t prod_thread_id;
    pthread_create(&prod_thread_id, NULL, producer_thread, &prod_args);

    pthread_t *con_thread_ids = malloc(num_consumers * sizeof(pthread_t));
    if (con_thread_ids == NULL) {
        perror("Falha ao alocar tids dos consumidores"); exit(1);
    }

    for (int i = 0; i < num_consumers; i++) {
        pthread_create(&con_thread_ids[i], NULL, consumer_thread, NULL);
    }

    pthread_join(prod_thread_id, NULL);
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(con_thread_ids[i], NULL);
    }
    
    GET_TIME(end); 
    delta = end - start;
    // --- Fim da Medição ---

    printf("Arquivo: %s\tNumero de palavras: %lld\n", argv[1], total_word_count);
    printf("Tempo: %lf\n", delta);

    // --- Limpeza ---
    free(con_thread_ids);
    fclose(arq);
    queue_destroy(&queue); // Libera o array da fila
    pthread_mutex_destroy(&count_lock);

    return 0;
}