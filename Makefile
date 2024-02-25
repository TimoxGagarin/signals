CC = gcc
CFLAGS = -g2 -ggdb -I./headers -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE -W -Wall -Wno-unused-parameter -Wno-unused-variable -std=c11 -pedantic

.SUFFIXES:
.SUFFIXES: .c .o

DEBUG = ./build/debug
RELEASE = ./build/release
ENVS = CHILD_PATH=$(DEBUG)/child

OUT_DIR = $(DEBUG)
MSG_INFO = "Запуск производится в режиме debug..."
vpath %.c src
vpath %.h src
vpath %.o build/debug

ifeq ($(MODE), release)
	CFLAGS = -I./headers -W -Wall -Wno-unused-parameter -Wno-unused-variable -std=c11 -pedantic
	OUT_DIR = $(RELEASE)
	ENVS = CHILD_PATH=$(RELEASE)/child
	MSG_INFO = "Запуск производится в режиме release"
	vpath %.o build/release
endif

parent_objects = $(OUT_DIR)/parent.o $(OUT_DIR)/parents_utils.o $(OUT_DIR)/general_utils.o
child_objects = $(OUT_DIR)/child.o $(OUT_DIR)/general_utils.o
parent_prog = $(OUT_DIR)/parent
child_prog = $(OUT_DIR)/child
parent_args = $(OUT_DIR)/child

run: all
	@echo $(MSG_INFO)
	@$(ENVS) ./$(parent_prog) $(options) $(parent_args)

all: $(child_prog) $(parent_prog)

$(child_prog) : $(child_objects)
	@$(CC) $(CFLAGS) $(child_objects) -o $@

$(parent_prog) : $(parent_objects)
	@$(CC) $(CFLAGS) $(parent_objects) -o $@
	
$(OUT_DIR)/%.o : %.c
	@$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean
clean:
	@rm -rf $(DEBUG)/* $(RELEASE)/* test