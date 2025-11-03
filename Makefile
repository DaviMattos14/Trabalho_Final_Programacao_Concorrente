# Nome do compilador e flags
CC = gcc
CFLAGS = -Wall

# Nome do executável
SEQ = sequencial.exe
CONC = concorrente.exe
TESTE = cont_c.exe

# Arquivos-fonte
SEQ_SRCS = contador_seq.c
CONC_SRCS = contador_conc.c
TESTE_SRCS = contador_conc.c

# Arquivo de log
LOG_FILE = log.txt

# Regra padrão
all: $(SEQ) $(CONC) run

# Compilação
$(SEQ): $(SEQ_SRCS) timer.h
	$(CC) $(CFLAGS) -o $(SEQ) $(SEQ_SRCS)

$(CONC): $(CONC_SRCS) timer.h
	$(CC) $(CFLAGS) -pthread -o $(CONC) $(CONC_SRCS)

$(TESTE): $(TESTE_SRCS) timer.h
	$(CC) $(CFLAGS) -pthread -o $(TESTE) $(TESTE_SRCS)
# Execuções automáticas
run: $(SEQ) $(CONC)
	@( \
		echo "== INICIANDO EXECUCAO ==" & \
		echo "---------- 100 KB ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) arquivo_KB.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8) do $(CONC) arquivo_KB.txt %%t) & \
		\
		echo "---------- 1.5 MB ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) arquivo_MB.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8) do $(CONC) arquivo_MB.txt %%t) & \
		\
		echo "---------- 500 MB ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) arquivo_500MB.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8) do $(CONC) arquivo_500MB.txt %%t) & \
		\
		echo "---------- 1GB ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) arquivo1GB.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8) do $(CONC) arquivo1GB.txt %%t) & \
		\
		echo "---------- 2GB ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) arquivo2GB.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8) do $(CONC) arquivo2GB.txt %%t) & \
		\
		echo "---------- 4GB ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) arquivo4GB.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8) do $(CONC) arquivo4GB.txt %%t) & \
		\
		echo "== FIM ==" \
	) > $(LOG_FILE) 2>&1

# Limpeza
clean:
	@del $(SEQ) $(CONC) $(TESTE)

# ...
# Adicionada a dependência $(TESTE)
corretude: $(SEQ) $(CONC)
	@( \
		echo "== INICIANDO EXECUCAO ==" & \
		echo "---------- Vazio ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) vazio.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8 16) do $(CONC) vazio.txt %%t) & \
		\
		echo "---------- Especiais ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) especiais.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8 16) do $(CONC) especiais.txt %%t) & \
		\
		echo "---------- Letras ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) letras.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8 16) do $(CONC) letras.txt %%t) & \
		\
		echo "---------- KB ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) arquivo_KB.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8 16) do $(CONC) arquivo_KB.txt %%t) & \
		\
		echo "---------- MB ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) arquivo_MB.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8 16) do $(CONC) arquivo_MB.txt %%t) & \
		\
		echo "---------- GB ----------" & \
		echo "----> SEQUENCIAL" & \
		$(SEQ) arquivo_GB.txt & \
		echo "----> CONCORRENTE" & \
		(for %%t in (2 4 8 16) do $(CONC) arquivo_GB.txt %%t) & \
		\
		echo "== FIM ==" \
	) > $(LOG_FILE) 2>&1

desempenho: $(TESTE) $(SEQ)
	@echo "Iniciando arquivo teste"
	@$(SEQ) old-newspaper.txt
	@$(TESTE) old-newspaper.txt 4
	@$(TESTE) old-newspaper.txt 8
	@echo "FIM"

run10:
	@for /L %%i in (1,1,10) do ( \
		echo Executando %%i... && \
		$(MAKE) -s run && \
		@move /Y $(LOG_FILE) log%%i.txt > NUL \
	)


