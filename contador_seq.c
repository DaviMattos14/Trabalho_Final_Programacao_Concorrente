#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "timer.h"

double start, finish, delta;
const char *separadores = " \t\n\r¹²³£¢¬§ªº°\"!@#$%¨&*()_+`{}^<>:?'=´[]~,.;/";
char tabela_separadores[256] = {0};

void iniciar_tabela() {
    for (int i = 0; i < (int)strlen(separadores); i++) {
        tabela_separadores[(unsigned char)separadores[i]] = 1;
    }
}

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

    const size_t BUFFER = 1024; // 1 KB
    char *texto = (char *)malloc(BUFFER);
    if (texto == NULL)
    {
        printf("--ERRO: Falha ao alocar memoria.\n");
        fclose(arq);
        exit(1);
    }

    /* inicializa a tabela de separadores (substitui strchr) */
    iniciar_tabela();

    int contador = 0;
    int palavra_quebrada = 0; /* flag: indica se o bloco anterior terminou no meio de palavra */

    while (fgets(texto, (int)BUFFER, arq))
    {
        size_t len = strlen(texto);

        /* verifica se o bloco atual começa no meio de uma palavra */
        int comeca_com_char = 0;
        if (len > 0) {
            /* se o primeiro caractere NÃO for separador, então começa com caracter */
            comeca_com_char = (tabela_separadores[(unsigned char)texto[0]] == 0);
        }

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
        if (len > 0 && tabela_separadores[(unsigned char)texto[len - 1]] == 0)
            palavra_quebrada = 1;
        else
            palavra_quebrada = 0;
    }

    fclose(arq);
    GET_TIME(finish);
    delta = finish - start;

    printf("Arquivo: %s \tNumero de palavras: %d \tTempo de Execucao: %lf\n", argv[1], contador, delta);

    free(texto);
    return 0;
}
