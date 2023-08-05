CPP=g++
LIBS:=
SRC:=
BIN:=jerry_test

SRC+=$(wildcard packages/core/src/*.c)
SRC+=test.c

INCLUDES:=
INCLUDES+=-I packages/core/src

override CFLAGS?=-Wall -s -O2

include lib/.dep/config.mk

OBJ:=$(SRC:.c=.o)
OBJ:=$(OBJ:.cc=.o)

override CFLAGS+=$(INCLUDES)

SRC+=test.c

default: $(BIN)

$(OBJ): $(SRC)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $@
