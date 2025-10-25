#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include "timer.h"

#define BUFFER 100         /* número de blocos no buffer */
#define TAMANHO_LEITURA 1024 /* 1KB por leitura de disco */

const char *separadores = " \t\n\r¹²³£¢¬§ªº°\"!@#$%¨&*()_+`{}^<>:?'=´[]~,.;/";

char *Buffer[BUFFER];     /* buffer circular compartilhado */
int in = 0, out = 0; /* índices circulares */
int fim = 0;         /* flag: produção encerrada */

/* semáforos */
sem_t cheio, vazio, mutex;

pthread_t *consumidores;


int contar_palavras(char *texto)
{
    int contador = 0;
    char *ptr = texto; // Ponteiro para percorrer o texto

    while (*ptr != '\0')
    {
        ptr += strspn(ptr, separadores);
        if (*ptr == '\0') {
            break;
        }
        size_t len_palavra = strcspn(ptr, separadores);

        if (len_palavra > 0)
        {
            contador++;
            ptr += len_palavra;
        }
    }
    return contador;
}


void Insere(char *bloco)
{
    sem_wait(&vazio);
    sem_wait(&mutex);

    Buffer[in] = bloco;
    in = (in + 1) % BUFFER;

    sem_post(&mutex);
    sem_post(&cheio);
}


char *Retira()
{
    char *bloco;

    sem_wait(&cheio);
    sem_wait(&mutex);

    bloco = Buffer[out];
    Buffer[out] = NULL;
    out = (out + 1) % BUFFER;

    sem_post(&mutex);
    sem_post(&vazio);

    return bloco;
}

void *consumidor(void *arg)
{
    long parcial = 0;
    char *texto;

    while (1)
    {
        texto = Retira();
        if (texto == NULL)
        {
            if (fim)
                break;
            else
                continue;
        }

        parcial += contar_palavras(texto);
        free(texto);
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

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Uso: %s <arquivo.txt> <num_threads>\n", argv[0]);
        return 1;
    }

    FILE *arq = fopen(argv[1], "r");
    if (!arq)
    {
        perror("Erro ao abrir arquivo");
        exit(1);
    }

    int nthreads = atoi(argv[2]);
    if (nthreads < 1) nthreads = 1;
    if (nthreads > BUFFER) nthreads = BUFFER;

    consumidores = malloc(nthreads * sizeof(pthread_t));

    sem_init(&mutex, 0, 1);
    sem_init(&cheio, 0, 0);
    sem_init(&vazio, 0, BUFFER);

    double start, finish, delta;
    GET_TIME(start);

    for (int i = 0; i < nthreads; i++)
    {
        pthread_create(&consumidores[i], NULL, consumidor, NULL);
    }


    char *buffer_leitura = malloc(TAMANHO_LEITURA);
    char *overlap = NULL; 
    size_t overlap_len = 0;
    size_t bytes_lidos;

    if (!buffer_leitura) {
        perror("Erro ao alocar buffer de leitura");
        exit(1);
    }

    while ((bytes_lidos = fread(buffer_leitura, 1, TAMANHO_LEITURA, arq)) > 0)
    {
        size_t tamanho_total = overlap_len + bytes_lidos;
        char *bloco_processar = malloc(tamanho_total + 1); 
        
        if(overlap) {
            memcpy(bloco_processar, overlap, overlap_len);
            free(overlap);
            overlap = NULL;
        }
        memcpy(bloco_processar + overlap_len, buffer_leitura, bytes_lidos);
        bloco_processar[tamanho_total] = '\0';

        int pos_corte = tamanho_total - 1;
        while (pos_corte >= 0 && strchr(separadores, bloco_processar[pos_corte]) == NULL) {
            pos_corte--;
        }

        if (pos_corte < 0) {
            overlap = bloco_processar; 
            overlap_len = tamanho_total;
            continue; 
        }

        size_t len_bloco_fila = pos_corte + 1;
        char *bloco_para_fila = malloc(len_bloco_fila + 1);
        memcpy(bloco_para_fila, bloco_processar, len_bloco_fila);
        bloco_para_fila[len_bloco_fila] = '\0';
        
        overlap_len = tamanho_total - len_bloco_fila;
        if (overlap_len > 0) {
            overlap = malloc(overlap_len + 1); 
            memcpy(overlap, bloco_processar + len_bloco_fila, overlap_len);
            overlap[overlap_len] = '\0';
        } else {
            overlap = NULL;
            overlap_len = 0;
        }
        
        free(bloco_processar);
        Insere(bloco_para_fila); 
    }

    if (overlap) {
        Insere(overlap);
    }

    free(buffer_leitura);
    fclose(arq);


    sem_wait(&mutex);
    fim = 1;
    sem_post(&mutex);


    for (int i = 0; i < nthreads; i++)
        Insere(NULL); /* acorda consumidores */
    
    long total = 0;
    long *resp_parcial; // Ponteiro para receber o valor retornado
    

    for (int i = 0; i < nthreads; i++)
    {
        pthread_join(consumidores[i], (void**) &resp_parcial);
        total += *resp_parcial; // Soma o valor parcial
        free(resp_parcial);     // Libera a memória alocada pela thread
    }
    
    GET_TIME(finish);
    delta = finish - start;

    printf("=============================================================================================\n");
    printf("Arquivo: %s \tNumero de palavras: %ld \tTempo de Execucao: %lf \tNum Threads: %d\n", argv[1], total, delta, nthreads);
    printf("=============================================================================================\n");

    free(consumidores);
    sem_destroy(&mutex);
    sem_destroy(&cheio);
    sem_destroy(&vazio);

    return 0;
}