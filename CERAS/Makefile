CC      ?= cc

CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -DCERAS_SRC_DIR='"$(CURDIR)"'
SRCS    := main.c lexer.c parser.c interp.c value.c
OBJS    := $(SRCS:.c=.o)
HDRS    := lexer.h ast.h parser.h interp.h value.h

ceras: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f ceras *.o

.PHONY: clean
