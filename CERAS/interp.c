#define _POSIX_C_SOURCE 200809L 

#include "interp.h"
#include "value.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct { char *name; Value value; int mut; } Binding;
typedef struct Env {
    struct Env *parent;
    Binding *bindings;
    int count;
} Env;

static Env *env_new(Env *parent) {
    Env *e = malloc(sizeof(Env));
    e->parent = parent;
    e->bindings = NULL;
    e->count = 0;
    return e;
}

static void env_define(Env *env, char *name, Value v, int mut) {
    env->bindings = realloc(env->bindings, sizeof(Binding) * (size_t)(env->count + 1));
    env->bindings[env->count].name = name;
    env->bindings[env->count].value = v;
    env->bindings[env->count].mut = mut;
    env->count++;
}

static Binding *env_find(Env *env, const char *name) {
    for (Env *e = env; e; e = e->parent)
        for (int i = e->count - 1; i >= 0; i--)
            if (strcmp(e->bindings[i].name, name) == 0) return &e->bindings[i];
    return NULL;
}



typedef struct ReturnFrame {
    struct ReturnFrame *prev;
    jmp_buf jmp;
    Value value;
} ReturnFrame;

typedef struct {
    Env *globals;
    EnumDecl **ens; int enum_count;
    ReturnFrame *return_stack;
    jmp_buf err_jmp;
    const char *source_text;  
    int real_fn_count;   
} Interp;

static _Noreturn void rt_error(Interp *it, int line, const char *fmt, ...) {
    fprintf(stderr, "linha %d: erro de execucao: ", line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    longjmp(it->err_jmp, 1);
}

static EnumDecl *find_enum(Interp *it, const char *name) {
    for (int i = 0; i < it->enum_count; i++)
        if (strcmp(it->ens[i]->name, name) == 0) return it->ens[i];
    return NULL;
}

static Value make_string_from_lexeme(const char *raw, int raw_len) {
    char *buf = malloc((size_t)raw_len);
    int n = 0;
    for (int i = 1; i < raw_len - 1; i++) {
        char c = raw[i];
        if (c == '\\') {
            i++;
            switch (raw[i]) {
            case 'n': buf[n++] = '\n'; break;
            case 't': buf[n++] = '\t'; break;
            case 'r': buf[n++] = '\r'; break;
            case '0': buf[n++] = '\0'; break;
            case '"': buf[n++] = '"';  break;
            case '\\': buf[n++] = '\\'; break;
            default: buf[n++] = raw[i]; break;
            }
        } else {
            buf[n++] = c;
        }
    }
    return v_string_owned(buf, n);
}

static char *unescape_lexeme_to_cstr(const char *raw, int raw_len) {
    char *buf = malloc((size_t)raw_len + 1);
    int n = 0;
    for (int i = 1; i < raw_len - 1; i++) {
        char c = raw[i];
        if (c == '\\') {
            i++;
            switch (raw[i]) {
            case 'n': buf[n++] = '\n'; break;
            case 't': buf[n++] = '\t'; break;
            case 'r': buf[n++] = '\r'; break;
            case '"': buf[n++] = '"';  break;
            case '\\': buf[n++] = '\\'; break;
            default: buf[n++] = raw[i]; break;
            }
        } else {
            buf[n++] = c;
        }
    }
    buf[n] = '\0';
    return buf;
}


static Value eval(Interp *it, Env *env, Expr *e);
static void exec_stmt(Interp *it, Env *env, Stmt *s);

static double as_f(const Value *v) { return v->kind == V_INT ? (double)v->as.i : v->as.f; }
static int is_num(const Value *v) { return v->kind == V_INT || v->kind == V_FLOAT; }

static Value eval_binary(Interp *it, int line, BinOp op, Value l, Value r) {
    switch (op) {
    case BIN_ADD:
        if (l.kind == V_STRING && r.kind == V_STRING) {
            int n = l.as.s.len + r.as.s.len;
            char *buf = malloc((size_t)n ? (size_t)n : 1);
            memcpy(buf, l.as.s.data, (size_t)l.as.s.len);
            memcpy(buf + l.as.s.len, r.as.s.data, (size_t)r.as.s.len);
            return v_string_owned(buf, n);
        }
        goto arith;
    case BIN_SUB: case BIN_MUL: case BIN_DIV:
    arith:
        if (!is_num(&l) || !is_num(&r))
            rt_error(it, line, "operandos invalidos para operador aritmetico (%s, %s)",
                     value_type_name(&l), value_type_name(&r));
        if (l.kind == V_INT && r.kind == V_INT) {
            long long a = l.as.i, b = r.as.i;
            switch (op) {
            case BIN_ADD: return v_int(a + b);
            case BIN_SUB: return v_int(a - b);
            case BIN_MUL: return v_int(a * b);
            case BIN_DIV:
                if (b == 0) rt_error(it, line, "divisao por zero");
                return v_int(a / b);
            default: break;
            }
        }
        {
            double a = as_f(&l), b = as_f(&r);
            switch (op) {
            case BIN_ADD: return v_float(a + b);
            case BIN_SUB: return v_float(a - b);
            case BIN_MUL: return v_float(a * b);
            case BIN_DIV: return v_float(a / b);
            default: break;
            }
        }
        break;
    case BIN_MOD:
        if (l.kind != V_INT || r.kind != V_INT)
            rt_error(it, line, "'%%' exige dois inteiros");
        if (r.as.i == 0) rt_error(it, line, "divisao por zero em '%%'");
        return v_int(l.as.i % r.as.i);
    case BIN_EQ: return v_bool(value_equal(&l, &r));
    case BIN_NE: return v_bool(!value_equal(&l, &r));
    case BIN_LT: case BIN_LE: case BIN_GT: case BIN_GE:
        if (!is_num(&l) || !is_num(&r))
            rt_error(it, line, "comparacao exige numeros (%s, %s)",
                     value_type_name(&l), value_type_name(&r));
        {
            double a = as_f(&l), b = as_f(&r);
            switch (op) {
            case BIN_LT: return v_bool(a < b);
            case BIN_LE: return v_bool(a <= b);
            case BIN_GT: return v_bool(a > b);
            case BIN_GE: return v_bool(a >= b);
            default: break;
            }
        }
        break;
    case BIN_AND: case BIN_OR: break; /* tratados em eval() com curto-circuito */
    }
    rt_error(it, line, "operador binario invalido");
    return v_nil();
}


static Value call_value(Interp *it, Value fn, Value *args, int argc, int line) {
    if (fn.kind != V_FUNC)
        rt_error(it, line, "valor nao e chamavel (tipo %s)", value_type_name(&fn));
    FnDecl *decl = fn.as.func.decl;
    if (argc != decl->param_count)
        rt_error(it, line, "%s espera %d argumento(s), recebeu %d",
                 decl->name ? decl->name : "<lambda>", decl->param_count, argc);

    Env *fenv = env_new((Env *)fn.as.func.env);
    for (int i = 0; i < argc; i++) env_define(fenv, decl->params[i], args[i], 1);

    ReturnFrame frame;
    frame.prev = it->return_stack;
    it->return_stack = &frame;

    Value result;
    if (setjmp(frame.jmp) == 0) {
        result = eval(it, fenv, decl->body);
    } else {
        result = frame.value;
    }
    it->return_stack = frame.prev;
    return result;
}


#ifndef CERAS_SRC_DIR
#define CERAS_SRC_DIR "."
#endif

static void write_c_string_literal(FILE *out, const char *s) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '\\') fputs("\\\\", out);
        else if (*p == '"') fputs("\\\"", out);
        else if (*p == '\n') fputs("\\n\"\n\"", out); 
        else if (*p == '\r') fputs("\\r", out);
        else if (*p < 0x20) fprintf(out, "\\x%02x", *p);
        else fputc(*p, out);
    }
    fputc('"', out);
}

static void do_occulta(Interp *it, const char *path, const char *fn_name, int line) {
    Binding *b = env_find(it->globals, fn_name);
    if (!b || b->value.kind != V_FUNC)
        rt_error(it, line, "'%s' nao e uma functio conhecida para occulta", fn_name);
    if (b->value.as.func.decl->param_count != 0)
        rt_error(it, line, "occulta exige uma functio sem parametros ('%s' tem %d)",
                 fn_name, b->value.as.func.decl->param_count);
    if (strchr(path, '\''))
        rt_error(it, line, "occulta: caminho de saida nao pode conter aspas simples");

    char dir[] = "/tmp/ceras_occulta_XXXXXX";
    if (!mkdtemp(dir)) rt_error(it, line, "occulta: nao foi possivel criar diretorio temporario");

    char runner_path[600], junk_path[600], log_path[600], cmd[8192];
    snprintf(runner_path, sizeof runner_path, "%s/runner.c", dir);
    snprintf(junk_path, sizeof junk_path, "%s/junk.c", dir);
    snprintf(log_path, sizeof log_path, "%s/gcc.log", dir);

    FILE *rf = fopen(runner_path, "w");
    if (!rf) rt_error(it, line, "occulta: nao foi possivel escrever runner.c");
    fprintf(rf, "#include \"parser.h\"\n#include \"interp.h\"\n\n");
    fprintf(rf, "static const char *CERAS_SRC =\n");
    write_c_string_literal(rf, it->source_text);
    fprintf(rf, ";\n\nint main(void) {\n");
    fprintf(rf, "    Program *prog = parse_program(CERAS_SRC);\n");
    fprintf(rf, "    if (!prog) return 1;\n");
    fprintf(rf, "    return interpret_entry(prog, CERAS_SRC, ");
    write_c_string_literal(rf, fn_name);
    fprintf(rf, ", 1);\n}\n");
    fclose(rf);

    int n_junk = it->real_fn_count * 2;
    if (n_junk < 10) n_junk = 10;

    FILE *jf = fopen(junk_path, "w");
    if (!jf) rt_error(it, line, "occulta: nao foi possivel escrever junk.c");
    fprintf(jf, "#include <stdint.h>\n\n");
    for (int i = 0; i < n_junk; i++) {
        fprintf(jf,
            "static int64_t ceras_junk_%d(int64_t x) {\n"
            "    int64_t a = x ^ %dLL;\n"
            "    for (int k = 0; k < %d; k++) a = (a * 2654435761LL + k) ^ (a >> 3);\n"
            "    return a;\n"
            "}\n",
            i, (int)((i * 2654435761u) & 0xffffu), 5 + (i % 7));
    }
    fprintf(jf, "\ntypedef int64_t (*ceras_junk_fn)(int64_t);\n");
    fprintf(jf, "__attribute__((used)) static ceras_junk_fn ceras_junk_registry[] = {\n");
    for (int i = 0; i < n_junk; i++) fprintf(jf, "    ceras_junk_%d,\n", i);
    fprintf(jf, "};\n");
    fprintf(jf, "__attribute__((used)) const int ceras_junk_count = %d;\n", n_junk);
    fclose(jf);

    snprintf(cmd, sizeof cmd,
        "gcc -std=c11 -O2 -I'%s' -o '%s' '%s' '%s' '%s/lexer.c' '%s/parser.c' "
        "'%s/interp.c' '%s/value.c' -DCERAS_SRC_DIR='\"%s\"' > '%s' 2>&1",
        CERAS_SRC_DIR, path, runner_path, junk_path,
        CERAS_SRC_DIR, CERAS_SRC_DIR, CERAS_SRC_DIR, CERAS_SRC_DIR, CERAS_SRC_DIR, log_path);

    int rc = system(cmd);
    if (rc != 0) {
        FILE *lf = fopen(log_path, "r");
        char buf[2000] = {0};
        if (lf) { size_t rd = fread(buf, 1, sizeof buf - 1, lf); (void)rd; fclose(lf); }
        rt_error(it, line, "occulta: gcc falhou ao compilar '%s':\n%s", path, buf);
    }
    fprintf(stderr, "occulta: '%s' gerado (%d funcoes reais, %d funcoes-isca)\n",
            path, it->real_fn_count, n_junk);
}

static Value eval(Interp *it, Env *env, Expr *e) {
    switch (e->kind) {
    case E_INT:    return v_int(e->as.int_v);
    case E_FLOAT:  return v_float(e->as.float_v);
    case E_BOOL:   return v_bool(e->as.bool_v);
    case E_STRING: return make_string_from_lexeme(e->as.string_v.data, e->as.string_v.len);

    case E_IDENT: {
        Binding *b = env_find(env, e->as.ident);
        if (!b) rt_error(it, e->line, "variavel nao definida: '%s'", e->as.ident);
        return b->value;
    }

    case E_UNARY: {
        Value v = eval(it, env, e->as.unary.operand);
        if (e->as.unary.op == UN_NOT) return v_bool(!value_truthy(&v));
        if (v.kind == V_INT) return v_int(-v.as.i);
        if (v.kind == V_FLOAT) return v_float(-v.as.f);
        rt_error(it, e->line, "'-' exige numero, recebeu %s", value_type_name(&v));
    }

    case E_BINARY: {
        if (e->as.binary.op == BIN_AND) {
            Value l = eval(it, env, e->as.binary.left);
            if (!value_truthy(&l)) return v_bool(0);
            Value r = eval(it, env, e->as.binary.right);
            return v_bool(value_truthy(&r));
        }
        if (e->as.binary.op == BIN_OR) {
            Value l = eval(it, env, e->as.binary.left);
            if (value_truthy(&l)) return v_bool(1);
            Value r = eval(it, env, e->as.binary.right);
            return v_bool(value_truthy(&r));
        }
        Value l = eval(it, env, e->as.binary.left);
        Value r = eval(it, env, e->as.binary.right);
        return eval_binary(it, e->line, e->as.binary.op, l, r);
    }

    case E_ASSIGN: {
        Value v = eval(it, env, e->as.assign.value);
        Binding *b = env_find(env, e->as.assign.name);
        if (!b) rt_error(it, e->line, "variavel nao declarada: '%s'", e->as.assign.name);
        if (!b->mut) rt_error(it, e->line, "'%s' nao e mutabile", e->as.assign.name);
        b->value = v;
        return v;
    }

    case E_LAMBDA: {
        Value v;
        v.kind = V_FUNC;
        v.as.func.decl = e->as.lambda.fn;
        v.as.func.env = env;  
        return v;
    }

    case E_CALL: {
        int argc = e->as.call.args.count;
        Value *args = argc ? malloc(sizeof(Value) * (size_t)argc) : NULL;
        for (int i = 0; i < argc; i++) args[i] = eval(it, env, e->as.call.args.items[i]);

        if (e->as.call.callee->kind == E_IDENT) {
            const char *name = e->as.call.callee->as.ident;
            if (strcmp(name, "scribe") == 0) {
                for (int i = 0; i < argc; i++) {
                    if (i) printf(" ");
                    value_print(&args[i]);
                }
                printf("\n");
                free(args);
                return v_nil();
            }
            if (strcmp(name, "tamanho") == 0) {
                if (argc != 1 || args[0].kind != V_ARRAY)
                    rt_error(it, e->line, "'tamanho' espera 1 argumento do tipo array");
                Value r = v_int(args[0].as.arr.count);
                free(args);
                return r;
            }
        }
        Value fnval = eval(it, env, e->as.call.callee);
        Value r = call_value(it, fnval, args, argc, e->line);
        free(args);
        return r;
    }

    case E_ENUM_NEW: {
        EnumDecl *en = find_enum(it, e->as.enum_new.type_name);
        if (!en) rt_error(it, e->line, "genus nao definido: '%s'", e->as.enum_new.type_name);
        EnumVariant *var = NULL;
        for (int i = 0; i < en->variant_count; i++)
            if (strcmp(en->variants[i].name, e->as.enum_new.variant) == 0) { var = &en->variants[i]; break; }
        if (!var) rt_error(it, e->line, "'%s' nao tem variante '%s'",
                           e->as.enum_new.type_name, e->as.enum_new.variant);
        int argc = e->as.enum_new.args.count;
        if (argc != var->arity)
            rt_error(it, e->line, "%s::%s espera %d campo(s), recebeu %d",
                     e->as.enum_new.type_name, e->as.enum_new.variant, var->arity, argc);
        Value *fields = argc ? malloc(sizeof(Value) * (size_t)argc) : NULL;
        for (int i = 0; i < argc; i++) fields[i] = eval(it, env, e->as.enum_new.args.items[i]);
        Value v;
        v.kind = V_ENUM;
        v.as.en.type_name = e->as.enum_new.type_name;
        v.as.en.variant = e->as.enum_new.variant;
        v.as.en.fields = fields;
        v.as.en.field_count = argc;
        return v;
    }

    case E_ARRAY: {
        int n = e->as.array.count;
        Value *items = n ? malloc(sizeof(Value) * (size_t)n) : NULL;
        for (int i = 0; i < n; i++) items[i] = eval(it, env, e->as.array.items[i]);
        Value v;
        v.kind = V_ARRAY;
        v.as.arr.items = items;
        v.as.arr.count = n;
        return v;
    }

    case E_INDEX: {
        Value arr = eval(it, env, e->as.index.array);
        if (arr.kind != V_ARRAY)
            rt_error(it, e->line, "'[...]' exige um array, recebeu %s", value_type_name(&arr));
        Value idx = eval(it, env, e->as.index.index);
        if (idx.kind != V_INT)
            rt_error(it, e->line, "indice precisa ser int, recebeu %s", value_type_name(&idx));
        if (idx.as.i < 0 || idx.as.i >= arr.as.arr.count)
            rt_error(it, e->line, "indice %lld fora dos limites (tamanho %d)",
                     idx.as.i, arr.as.arr.count);
        return arr.as.arr.items[idx.as.i];
    }

    case E_INDEX_ASSIGN: {
        if (e->as.index_assign.array->kind == E_IDENT) {
            Binding *b = env_find(env, e->as.index_assign.array->as.ident);
            if (!b) rt_error(it, e->line, "variavel nao declarada: '%s'",
                             e->as.index_assign.array->as.ident);
            if (!b->mut) rt_error(it, e->line, "'%s' nao e mutabile",
                                  e->as.index_assign.array->as.ident);
        }
        Value arr = eval(it, env, e->as.index_assign.array);
        if (arr.kind != V_ARRAY)
            rt_error(it, e->line, "'[...]' exige um array, recebeu %s", value_type_name(&arr));
        Value idx = eval(it, env, e->as.index_assign.index);
        if (idx.kind != V_INT)
            rt_error(it, e->line, "indice precisa ser int, recebeu %s", value_type_name(&idx));
        if (idx.as.i < 0 || idx.as.i >= arr.as.arr.count)
            rt_error(it, e->line, "indice %lld fora dos limites (tamanho %d)",
                     idx.as.i, arr.as.arr.count);
        Value v = eval(it, env, e->as.index_assign.value);
        arr.as.arr.items[idx.as.i] = v;
        return v;
    }

    case E_IF: {
        Value c = eval(it, env, e->as.if_e.cond);
        if (value_truthy(&c)) return eval(it, env, e->as.if_e.then_b);
        if (e->as.if_e.else_b) return eval(it, env, e->as.if_e.else_b);
        return v_nil();
    }

    case E_MATCH: {
        Value subj = eval(it, env, e->as.match.subject);
        if (subj.kind != V_ENUM)
            rt_error(it, e->line, "iudicium exige um valor de genus, recebeu %s",
                     value_type_name(&subj));
        for (int i = 0; i < e->as.match.arm_count; i++) {
            MatchArm *arm = &e->as.match.arms[i];
            if (strcmp(arm->type_name, subj.as.en.type_name) != 0) continue;
            if (strcmp(arm->variant, subj.as.en.variant) != 0) continue;
            if (arm->binder_count != subj.as.en.field_count)
                rt_error(it, e->line, "padrao %s::%s espera %d campo(s), valor tem %d",
                         arm->type_name, arm->variant, arm->binder_count, subj.as.en.field_count);
            Env *menv = env_new(env);
            for (int k = 0; k < arm->binder_count; k++)
                env_define(menv, arm->binders[k], subj.as.en.fields[k], 0);
            return eval(it, menv, arm->body);
        }
        rt_error(it, e->line, "iudicium sem braco para %s::%s",
                 subj.as.en.type_name, subj.as.en.variant);
    }

    case E_BLOCK: {
        Env *benv = env_new(env);
        for (int i = 0; i < e->as.block.stmts.count; i++)
            exec_stmt(it, benv, e->as.block.stmts.items[i]);
        if (e->as.block.tail) return eval(it, benv, e->as.block.tail);
        return v_nil();
    }
    }
    rt_error(it, e->line, "expressao desconhecida (bug interno)");
    return v_nil();
}

static void exec_stmt(Interp *it, Env *env, Stmt *s) {
    switch (s->kind) {
    case S_LET: {
        if (s->as.let_s.init->kind == E_LAMBDA) {
      
            env_define(env, s->as.let_s.name, v_nil(), 1);
            Value v = eval(it, env, s->as.let_s.init);
            Binding *b = env_find(env, s->as.let_s.name);
            b->value = v;
            b->mut = s->as.let_s.mut;
        } else {
            Value v = eval(it, env, s->as.let_s.init);
            env_define(env, s->as.let_s.name, v, s->as.let_s.mut);
        }
        break;
    }
    case S_EXPR:
        eval(it, env, s->as.expr_s);
        break;
    case S_WHILE:
        while (1) {
            Value c = eval(it, env, s->as.while_s.cond);
            if (!value_truthy(&c)) break;
            eval(it, env, s->as.while_s.body);
        }
        break;
    case S_FOR: {
        Value iter = eval(it, env, s->as.for_s.iterable);
        if (iter.kind != V_ARRAY)
            rt_error(it, s->line, "'for ... in' exige um array, recebeu %s",
                     value_type_name(&iter));
        for (int i = 0; i < iter.as.arr.count; i++) {
            Env *lenv = env_new(env);
            env_define(lenv, s->as.for_s.var, iter.as.arr.items[i], 0);
            eval(it, lenv, s->as.for_s.body);
        }
        break;
    }
    case S_RETURN: {
        Value v = s->as.return_s ? eval(it, env, s->as.return_s) : v_nil();
        if (!it->return_stack) rt_error(it, s->line, "reverte fora de uma functio");
        it->return_stack->value = v;
        longjmp(it->return_stack->jmp, 1);
    }
    case S_OCCULTA: {
        char *path = unescape_lexeme_to_cstr(s->as.occulta_s.path,
                                             (int)strlen(s->as.occulta_s.path));
        do_occulta(it, path, s->as.occulta_s.fn_name, s->line);
        free(path);
        break;
    }
    }
}

int interpret_entry(Program *prog, const char *source_text,
                    const char *entry_name, int print_result) {
    Interp it;
    it.globals = env_new(NULL);
    it.ens = NULL; it.enum_count = 0;
    it.return_stack = NULL;
    it.source_text = source_text;
    it.real_fn_count = 0;

    for (int i = 0; i < prog->count; i++) {
        Item *item = &prog->items[i];
        if (item->kind == ITEM_FN) {
            it.real_fn_count++;
        } else {
            it.ens = realloc(it.ens, sizeof(EnumDecl *) * (size_t)(it.enum_count + 1));
            it.ens[it.enum_count++] = &item->as.en;
        }
    }

    if (setjmp(it.err_jmp)) return 1;
    for (int i = 0; i < prog->count; i++) {
        if (prog->items[i].kind != ITEM_FN) continue;
        Value v;
        v.kind = V_FUNC;
        v.as.func.decl = &prog->items[i].as.fn;
        v.as.func.env = it.globals;
        env_define(it.globals, prog->items[i].as.fn.name, v, 0);
    }

    Binding *entry = env_find(it.globals, entry_name);
    if (!entry || entry->value.kind != V_FUNC) {
        fprintf(stderr, "erro: nenhuma functio '%s' encontrada\n", entry_name);
        return 1;
    }
    if (entry->value.as.func.decl->param_count != 0) {
        fprintf(stderr, "erro: '%s' precisa ter zero parametros para ser ponto de entrada\n",
                entry_name);
        return 1;
    }
    Value r = call_value(&it, entry->value, NULL, 0, 0);
    if (print_result && r.kind != V_NIL) {
        value_print(&r);
        printf("\n");
    }
    return 0;
}

int interpret(Program *prog, const char *source_text) {
    return interpret_entry(prog, source_text, "principal", 0);
}
