CC = gcc
CFLAGS = -Wall -Wextra -g
LFLAGS = -pthread -lncurses

## Definições para compilação do supervisor
# SRC_DIRS_SUP := $(shell find . -type d ! -name userTest)
# SRCS_SUP := $(foreach dir,$(SRC_DIRS_SUP),$(filter-out $(dir)/userTest/%.c,$(wildcard $(dir)/*.c)))
SRC_DIRS_SUP := $(shell find . -type d ! -name userTest ! -name benchmark)
SRCS_SUP := $(foreach dir,$(SRC_DIRS_SUP),$(filter-out $(dir)/userTest/%.c $(dir)/benchmark/%.c,$(wildcard $(dir)/*.c)))
OBJS_SUP := $(SRCS_SUP:.c=.o)
TARGET_SUP = Supervisor/Supervisor

## Definições para compilação da lib do usuário (userTest).
# SRC_DIRS_USR := $(shell find . -type d ! -name Supervisor)
# SRCS_USR := $(foreach dir,$(SRC_DIRS_USR),$(filter-out $(dir)/Supervisor/%.c,$(wildcard $(dir)/*.c)))
SRC_DIRS_USR := $(shell find . -type d ! -name Supervisor ! -name benchmark)
SRCS_USR := $(foreach dir,$(SRC_DIRS_USR),$(filter-out $(dir)/Supervisor/%.c $(dir)/benchmark/%.c,$(wildcard $(dir)/*.c)))
OBJS_USR := $(SRCS_USR:.c=.o)
TARGET_USR = userTest/userTest

## Definições para compilação dos programas de benchmark.
SRC_DIRS_BENCH := $(shell find . -type d ! -name Supervisor ! -name userTest)
SRCS_BENCH := $(foreach dir,$(SRC_DIRS_BENCH),$(filter-out $(dir)/Supervisor/%.c $(dir)/userTest/%.c,$(wildcard $(dir)/*.c)))
OBJS_BENCH := $(SRCS_BENCH:.c=.o)
TARGET_BENCH = benchmark/initTime_benchmark

.PHONY: all clean

all: $(TARGET_SUP) $(TARGET_USR) $(TARGET_BENCH)

$(TARGET_SUP): $(OBJS_SUP)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) 

$(TARGET_USR): $(OBJS_USR)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) 

$(TARGET_BENCH): $(OBJS_BENCH)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) 

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET_SUP) $(TARGET_USR) $(OBJS_SUP) $(OBJS_USR)