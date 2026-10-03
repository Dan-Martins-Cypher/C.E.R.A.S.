#include "value.h"

#include <stdio.h>
#include <string.h>

Value v_nil(void) { Value v; v.kind = V_NIL; return v; }
Value v_bool(int b) { Value v; v.kind = V_BOOL; v.as.b = b != 0; return v; }
Value v_int(long long i) { Value v; v.kind = V_INT; v.as.i = i; return v; }
Value v_float(double f) { Value v; v.kind = V_FLOAT; v.as.f = f; return v; }

Value v_string_owned(char *data, int len) {
    Value v;
    v.kind = V_STRING;
    v.as.s.data = data;
    v.as.s.len = len;
    return v;
}

int value_truthy(const Value *v) {
    switch (v->kind) {
    case V_NIL:    return 0;
    case V_BOOL:   return v->as.b;
    case V_INT:    return v->as.i != 0;
    case V_FLOAT:  return v->as.f != 0.0;
    case V_STRING: return v->as.s.len != 0;
    case V_ARRAY:  return v->as.arr.count != 0;
    case V_ENUM:   return 1;
    case V_FUNC:   return 1;
    }
    return 0;
}

int value_equal(const Value *a, const Value *b) {
    if (a->kind != b->kind) {
      
        if (a->kind == V_INT && b->kind == V_FLOAT) return (double)a->as.i == b->as.f;
        if (a->kind == V_FLOAT && b->kind == V_INT) return a->as.f == (double)b->as.i;
        return 0;
    }
    switch (a->kind) {
    case V_NIL:    return 1;
    case V_BOOL:   return a->as.b == b->as.b;
    case V_INT:    return a->as.i == b->as.i;
    case V_FLOAT:  return a->as.f == b->as.f;
    case V_STRING:
        return a->as.s.len == b->as.s.len &&
               memcmp(a->as.s.data, b->as.s.data, (size_t)a->as.s.len) == 0;
    case V_ARRAY:
        if (a->as.arr.count != b->as.arr.count) return 0;
        for (int i = 0; i < a->as.arr.count; i++)
            if (!value_equal(&a->as.arr.items[i], &b->as.arr.items[i])) return 0;
        return 1;
    case V_ENUM:
        if (strcmp(a->as.en.type_name, b->as.en.type_name) != 0) return 0;
        if (strcmp(a->as.en.variant, b->as.en.variant) != 0) return 0;
        if (a->as.en.field_count != b->as.en.field_count) return 0;
        for (int i = 0; i < a->as.en.field_count; i++)
            if (!value_equal(&a->as.en.fields[i], &b->as.en.fields[i])) return 0;
        return 1;
    case V_FUNC:
        return a->as.func.decl == b->as.func.decl && a->as.func.env == b->as.func.env;
    }
    return 0;
}

void value_print(const Value *v) {
    switch (v->kind) {
    case V_NIL:    printf("nil"); break;
    case V_BOOL:   printf("%s", v->as.b ? "verum" : "falsum"); break;
    case V_INT:    printf("%lld", v->as.i); break;
    case V_FLOAT:  printf("%g", v->as.f); break;
    case V_STRING: printf("%.*s", v->as.s.len, v->as.s.data); break;
    case V_ARRAY:
        printf("[");
        for (int i = 0; i < v->as.arr.count; i++) {
            if (i) printf(", ");
            value_print(&v->as.arr.items[i]);
        }
        printf("]");
        break;
    case V_ENUM:
        printf("%s::%s", v->as.en.type_name, v->as.en.variant);
        if (v->as.en.field_count > 0) {
            printf("(");
            for (int i = 0; i < v->as.en.field_count; i++) {
                if (i) printf(", ");
                value_print(&v->as.en.fields[i]);
            }
            printf(")");
        }
        break;
    case V_FUNC:
        printf("<functio%s>", v->as.func.decl->name ? v->as.func.decl->name : " anonima");
        break;
    }
}

const char *value_type_name(const Value *v) {
    switch (v->kind) {
    case V_NIL:    return "nil";
    case V_BOOL:   return "bool";
    case V_INT:    return "int";
    case V_FLOAT:  return "float";
    case V_STRING: return "string";
    case V_ARRAY:  return "array";
    case V_ENUM:   return "genus";
    case V_FUNC:   return "functio";
    }
    return "?";
}
