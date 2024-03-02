CC = gcc
CFLAGS = -Wall -Wextra -pthread

## Definições para compilação do supervisor
# Lista de todos os diretórios encontrados recursivamente, exceto "userTest"
SRC_DIRS_SUP := $(shell find . -type d ! -name userTest)

# Lista de todos os arquivos .c, exceto os encontrados em "userTest"
SRCS_SUP := $(foreach dir,$(SRC_DIRS_SUP),$(filter-out $(dir)/userTest/%.c,$(wildcard $(dir)/*.c)))

# Gere os objetos correspondentes
OBJS_SUP := $(SRCS_SUP:.c=.o)

# Nome do alvo final
TARGET_SUP = Supervisor/Supervisor

## Definições para compilação da lib do usuário (userTest).
# Lista de todos os diretórios encontrados recursivamente, exceto "Supervisor"
SRC_DIRS_USR := $(shell find . -type d ! -name Supervisor)

# Lista de todos os arquivos .c, exceto os encontrados em "Supervisor"
SRCS_USR := $(foreach dir,$(SRC_DIRS_USR),$(filter-out $(dir)/Supervisor/%.c,$(wildcard $(dir)/*.c)))

# Gere os objetos correspondentes
OBJS_USR := $(SRCS_USR:.c=.o)

# Nome do alvo final
TARGET_USR = userTest/userTest

# Alvo padrão "all" para construir o programa
.PHONY: all clean

all: $(TARGET_SUP) $(TARGET_USR)

sup: $(TARGET_SUP)

usr: $(TARGET_USR)

$(TARGET_SUP): $(OBJS_SUP)
	$(CC) $(CFLAGS) -lncurses $^ -o $@

$(TARGET_USR): $(OBJS_USR)
	$(CC) $(CFLAGS) $^ -o $@

# Regra para compilar os arquivos .c em arquivos .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Regra para limpar os arquivos gerados
clean:
	rm -f $(TARGET_SUP) $(TARGET_USR) $(OBJS_SUP)
