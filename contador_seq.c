#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "timer.h"

double start, finish, delta;
const char *separadores = " \t\n\r¹²³£¢¬§ªº°\"!@#$%¨&*()_+`{}^<>:?'=´[]~,.;/";

int main(int argc, char *argv[])
{
    FILE *arq;

    if (argc < 2)
    {
        printf("Uso: %s <nome_do_arquivo.txt>\n", argv[0]);
        return 1;
    }

    GET_TIME(start);
    arq = fopen(argv[1], "r");
    if (arq == NULL)
    {
        printf("--ERRO: fopen()\n");
        exit(-1);
    }

    const size_t BUFFER = 1024*10; // 1 KB
    char *texto = (char *)malloc(BUFFER);
    if (texto == NULL)
    {
        printf("--ERRO: Falha ao alocar memoria.\n");
        fclose(arq);
        exit(1);
    }

    int contador = 0;
    int palavra_quebrada = 0; /* flag: indica se o bloco anterior terminou no meio de palavra */

    while (fgets(texto, (int)BUFFER, arq))
    {
        size_t len = strlen(texto);

        /* verifica se o bloco atual começa no meio de uma palavra */
        int comeca_com_char = (len > 0 && strchr(separadores, texto[0]) == NULL);

        /* se o bloco anterior terminou no meio de palavra e o atual começa com uma letra,
           então a mesma palavra foi contada duas vezes (no bloco anterior e aqui).
           Corrige subtraindo 1 antes de contar os tokens deste bloco. */
        if (palavra_quebrada && comeca_com_char)
            contador--;

        char *palavra = strtok(texto, separadores);
        while (palavra != NULL)
        {
            contador++;
            palavra = strtok(NULL, separadores);
        }

        /* atualiza flag: se o último caractere NÃO for separador, então este bloco terminou no meio de uma palavra */
        if (len > 0 && strchr(separadores, texto[len - 1]) == NULL)
            palavra_quebrada = 1;
        else
            palavra_quebrada = 0;
    }

    fclose(arq);
    GET_TIME(finish);
    delta = finish - start;

    //printf("=============================================================================================\n");
    printf("Arquivo: %s \tNumero de palavras: %d \tTempo de Execucao: %lf\n", argv[1], contador, delta);
    //printf("=============================================================================================\n");

    free(texto);
    return 0;
}