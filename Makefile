# Nome do compilador e flags
CC = gcc
CFLAGS = -Wall

# Nome do executável
SEQ = sequencial.exe
CONC = concorrente.exe

# Arquivos-fonte
SEQ_SRCS = contador_seq.c
CONC_SRCS = contador_conc.c

# Arquivo de log
LOG_FILE = log.txt

# Regra padrão
all: $(SEQ) $(CONC) run

# Compilação
$(SEQ): $(SEQ_SRCS) timer.h
	$(CC) $(CFLAGS) -o $(SEQ) $(SEQ_SRCS)

$(CONC): $(CONC_SRCS) timer.h
	$(CC) $(CFLAGS) -pthread -o $(CONC) $(CONC_SRCS)

# Execuções automáticas
run:
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
	@del $(SEQ) $(CONC)