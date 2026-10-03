#include <stdio.h>
#include <stdlib.h>

#include "ast.h"
#include "parser.h"
#include "interp.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)size, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "uso: %s arquivo.ceras\n", argv[0]);
        return 2;
    }

    char *src = read_file(argv[1]);
    if (!src) return 2;

    Program *prog = parse_program(src);
    if (!prog) return 1; 

    int status = interpret(prog, src);

    free(src);
    return status;
}
