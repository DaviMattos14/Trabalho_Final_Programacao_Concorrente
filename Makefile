# Nome do compilador e flags
CC = gcc
CFLAGS = -Wall 

# Nome do executável
TARGET = seq

# Arquivos-fonte
SRCS = contador_seq.c

# Regra padrão
all: $(TARGET) run

# Compilação
$(TARGET): $(SRCS) timer.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Execuções automáticas
run:
	@echo "== Executando testes =="
	@./$(TARGET) teste.txt 
	@./$(TARGET) arquivo_KB.txt 
	@./$(TARGET) arquivo_MB.txt 
	@./$(TARGET) arquivo_GB.txt 
	@echo "== Fim =="

# Limpeza
clean:
	rm -f $(TARGET)
