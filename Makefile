CC      ?= cc
CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -O2
INCLUDES= -I./include
LDFLAGS += -lmysqlclient

SRC = src/main.c src/cli.c src/db.c
OBJ = $(SRC:.c=.o)

.PHONY: all clean run

all: gigamctl

gigamctl: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJ) gigamctl

run: all
	./gigamctl
