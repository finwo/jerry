CPP=g++

LIBS:=
SRC:=

SRC+=$(wildcard src/*.c)
SRC+=$(wildcard src/*/*.c)

override CFLAGS?=-Wall -s -O2

INCLUDES:=
INCLUDES+=-I src

include lib/.dep/config.mk

OBJ:=$(SRC:.c=.o)
OBJ:=$(OBJ:.cc=.o)

override CFLAGS+=$(INCLUDES)

default: $(BIN)

$(BIN): $(OBJ)
	$(CPP) $(LDFLAGS) $(OBJ) -o $@
