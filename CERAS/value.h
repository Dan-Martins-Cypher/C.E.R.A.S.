#ifndef CERAS_VALUE_H
#define CERAS_VALUE_H

#include "ast.h"



typedef enum { V_NIL, V_BOOL, V_INT, V_FLOAT, V_STRING, V_ARRAY, V_ENUM, V_FUNC } VKind;
typedef struct Value Value;

struct Value {
    VKind kind;
    union {
        int b;
        long long i;
        double f;
        struct { char *data; int len; } s;
        struct { Value *items; int count; } arr;
        struct { char *type_name; char *variant; Value *fields; int field_count; } en;
     
        struct { FnDecl *decl; void *env; } func;
    } as;
};

Value v_nil(void);
Value v_bool(int b);
Value v_int(long long i);
Value v_float(double f);
Value v_string_owned(char *data, int len); 

int value_truthy(const Value *v);
int value_equal(const Value *a, const Value *b);
void value_print(const Value *v);         
const char *value_type_name(const Value *v);

#endif
