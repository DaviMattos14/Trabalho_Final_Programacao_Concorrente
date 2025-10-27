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
TESTE_SRCS = cont_c.c

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

# Limpeza
clean:
	@del $(SEQ) $(CONC) $(TESTE)

# ...
# Adicionada a dependência $(TESTE)
teste: $(TESTE) $(SEQ)
	@echo "Iniciando arquivo teste"
	@$(SEQ) arquivo_GB.txt
	@$(TESTE) arquivo_GB.txt 2
	@$(TESTE) arquivo_GB.txt 4
	@$(TESTE) arquivo_GB.txt 8
	@$(TESTE) arquivo_GB.txt 16
	@echo "FIM"