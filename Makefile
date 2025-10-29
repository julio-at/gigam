CC=cc
CFLAGS=-std=c11 -Wall -Wextra -Wpedantic -O2 -I./include
LDFLAGS=-lmysqlclient

SRC=src/main.c src/cli.c src/db.c
OBJ=$(SRC:.c=.o)

all: gigamctl

gigamctl: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ) gigamctl

.PHONY: all clean

migrate:
	./scripts/migrate.sh

reset:
	./scripts/reset_truncate.sh

hard-reset:
	DB_ROOT_USER=$${DB_ROOT_USER:-root} DB_ROOT_PASS=$${DB_ROOT_PASS:?set} ./scripts/reset_hard.sh --yes

seed:
	./scripts/seed_demo.sh
