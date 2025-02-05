# !!!
# !!! You should probably be using `luarocks make` instead of this
# !!!
# !!! This is mostly for quick and dirty testing
# !!!

HEADERS=$(wildcard csrc/*.h) $(wildcard include/ltreesitter/*.h)
SRC=$(wildcard csrc/*.c)
OBJ=$(SRC:.c=.o)

CFLAGS := -Wall -Wextra -Werror -Og -ggdb -std=c99 -pedantic -fPIC
CFLAGS += -I./include
LIBS = -ltree-sitter -llua -ldl

INSTALL_PREFIX:=/usr/local/lib

static: ltreesitter.a
dynamic: ltreesitter.so

ltreesitter.a: $(OBJ) $(HEADERS)
	$(AR) r $@ $(OBJ)

ltreesitter.so: $(OBJ) $(HEADERS)
	$(CC) -shared $(OBJ) -o $@ $(LIBS)

clean:
	rm -f $(OBJ) ltreesitter.a

all: clean ltreesitter.a ltreesitter.so

install: ltreesitter.a ltreesitter.so
	cp ltreesitter.a $(INSTALL_PREFIX)/
	cp ltreesitter.so $(INSTALL_PREFIX)/

.PHONY: clean all
