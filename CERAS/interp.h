#ifndef CERAS_INTERP_H
#define CERAS_INTERP_H

#include "ast.h"

int interpret(Program *prog, const char *source_text);

int interpret_entry(Program *prog, const char *source_text,
                    const char *entry_name, int print_result);

#endif
