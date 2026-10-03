#include "parser.h"
#include "lexer.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



typedef struct {
    Lexer lx;
    Token cur;
    jmp_buf err_jmp;
    const char *filename;
} Parser;

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) { fprintf(stderr, "sem memoria\n"); exit(1); }
    return p;
}

static char *xstrndup(const char *s, size_t n) {
    char *r = xmalloc(n + 1);
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static Expr *new_expr(ExprKind k, int line) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = k;
    e->line = line;
    return e;
}
static Stmt *new_stmt(StmtKind k, int line) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = k;
    s->line = line;
    return s;
}


static void list_push_expr(ExprList *l, Expr *e) {
    l->items = realloc(l->items, sizeof(Expr *) * (size_t)(l->count + 1));
    l->items[l->count++] = e;
}
static void list_push_stmt(StmtList *l, Stmt *s) {
    l->items = realloc(l->items, sizeof(Stmt *) * (size_t)(l->count + 1));
    l->items[l->count++] = s;
}

static void advance_p(Parser *p) {
    for (;;) {
        p->cur = lexer_next(&p->lx);
        if (p->cur.kind != T_ERROR) break;
        fprintf(stderr, "%s:%d:%d: erro lexico: %s\n",
                p->filename, p->cur.line, p->cur.col, p->cur.error);
        longjmp(p->err_jmp, 1);
    }
}

static void error_at(Parser *p, const char *msg) {
    fprintf(stderr, "%s:%d:%d: erro de sintaxe: %s (perto de '%.*s')\n",
            p->filename, p->cur.line, p->cur.col, msg,
            (int)p->cur.length, p->cur.start);
    longjmp(p->err_jmp, 1);
}

static int check(Parser *p, TokenKind k) { return p->cur.kind == k; }

static Token expect(Parser *p, TokenKind k, const char *what) {
    if (!check(p, k)) error_at(p, what);
    Token t = p->cur;
    advance_p(p);
    return t;
}

static char *expect_ident(Parser *p, const char *what) {
    Token t = expect(p, T_IDENT, what);
    return xstrndup(t.start, t.length);
}

static int match_p(Parser *p, TokenKind k) {
    if (!check(p, k)) return 0;
    advance_p(p);
    return 1;
}

/* ---------- expressões ---------- */

static Expr *parse_expr(Parser *p);
static Expr *parse_block(Parser *p);

static int is_block_like(const Expr *e) {
    return e->kind == E_IF || e->kind == E_MATCH || e->kind == E_BLOCK;
}

static Expr *parse_array(Parser *p) {
    int line = p->cur.line;
    expect(p, T_LBRACKET, "esperava '['");
    Expr *e = new_expr(E_ARRAY, line);
    e->as.array.items = NULL;
    e->as.array.count = 0;
    if (!check(p, T_RBRACKET)) {
        do {
            if (check(p, T_RBRACKET)) break; /* permite vírgula sobrando */
            list_push_expr(&e->as.array, parse_expr(p));
        } while (match_p(p, T_COMMA));
    }
    expect(p, T_RBRACKET, "esperava ']' no final da lista");
    return e;
}

static Expr *parse_if(Parser *p) {
    int line = p->cur.line;
    expect(p, T_IF, "esperava 'if'");
    Expr *e = new_expr(E_IF, line);
    e->as.if_e.cond = parse_expr(p);
    e->as.if_e.then_b = parse_block(p);
    e->as.if_e.else_b = NULL;
    if (match_p(p, T_ELSE)) {
        e->as.if_e.else_b = check(p, T_IF) ? parse_if(p) : parse_block(p);
    }
    return e;
}

static Expr *parse_match(Parser *p) {
    int line = p->cur.line;
    expect(p, T_MATCH, "esperava 'iudicium'");
    Expr *e = new_expr(E_MATCH, line);
    e->as.match.subject = parse_expr(p);
    expect(p, T_LBRACE, "esperava '{' apos o sujeito do iudicium");

    MatchArm *arms = NULL;
    int count = 0;
    while (!check(p, T_RBRACE)) {
        arms = realloc(arms, sizeof(MatchArm) * (size_t)(count + 1));
        MatchArm *arm = &arms[count++];
        arm->type_name = expect_ident(p, "esperava o nome do tipo no padrao");
        expect(p, T_COLONCOLON, "esperava '::' no padrao");
        arm->variant = expect_ident(p, "esperava o nome da variante");
        arm->binders = NULL;
        arm->binder_count = 0;
        if (match_p(p, T_LPAREN)) {
            if (!check(p, T_RPAREN)) {
                do {
                    arm->binders = realloc(arm->binders,
                        sizeof(char *) * (size_t)(arm->binder_count + 1));
                    arm->binders[arm->binder_count++] =
                        expect_ident(p, "esperava um nome de campo no padrao");
                } while (match_p(p, T_COMMA));
            }
            expect(p, T_RPAREN, "esperava ')' no padrao");
        }
        expect(p, T_FATARROW, "esperava '=>' no braco do iudicium");
        arm->body = parse_expr(p);
        if (!check(p, T_RBRACE)) expect(p, T_COMMA, "esperava ',' entre bracos do iudicium");
    }
    expect(p, T_RBRACE, "esperava '}' no final do iudicium");
    e->as.match.arms = arms;
    e->as.match.arm_count = count;
    return e;
}

static Expr *parse_block(Parser *p) {
    int line = p->cur.line;
    expect(p, T_LBRACE, "esperava '{'");
    Expr *e = new_expr(E_BLOCK, line);
    e->as.block.stmts.items = NULL;
    e->as.block.stmts.count = 0;
    e->as.block.tail = NULL;

    while (!check(p, T_RBRACE)) {
        if (check(p, T_LET)) {
            int sline = p->cur.line;
            advance_p(p);
            Stmt *s = new_stmt(S_LET, sline);
            s->as.let_s.mut = match_p(p, T_MUT);
            s->as.let_s.name = expect_ident(p, "esperava o nome da variavel");
            expect(p, T_ASSIGN, "esperava '=' na declaracao");
            s->as.let_s.init = parse_expr(p);
            expect(p, T_SEMI, "esperava ';' apos a declaracao");
            list_push_stmt(&e->as.block.stmts, s);
        } else if (check(p, T_WHILE)) {
            int sline = p->cur.line;
            advance_p(p);
            Stmt *s = new_stmt(S_WHILE, sline);
            s->as.while_s.cond = parse_expr(p);
            s->as.while_s.body = parse_block(p);
            list_push_stmt(&e->as.block.stmts, s);
        } else if (check(p, T_FOR)) {
            int sline = p->cur.line;
            advance_p(p);
            Stmt *s = new_stmt(S_FOR, sline);
            s->as.for_s.var = expect_ident(p, "esperava a variavel do for");
            expect(p, T_IN, "esperava 'in' no for");
            s->as.for_s.iterable = parse_expr(p);
            s->as.for_s.body = parse_block(p);
            list_push_stmt(&e->as.block.stmts, s);
        } else if (check(p, T_WRAP)) {
            int sline = p->cur.line;
            advance_p(p);
            Stmt *s = new_stmt(S_OCCULTA, sline);
            Token path = expect(p, T_STRING, "esperava uma string com o caminho de saida");
            s->as.occulta_s.path = xstrndup(path.start, path.length);
            s->as.occulta_s.fn_name = expect_ident(p, "esperava o nome da functio a compilar");
            expect(p, T_SEMI, "esperava ';' apos occulta");
            list_push_stmt(&e->as.block.stmts, s);
        } else if (check(p, T_RETURN)) {
            int sline = p->cur.line;
            advance_p(p);
            Stmt *s = new_stmt(S_RETURN, sline);
            s->as.return_s = check(p, T_SEMI) ? NULL : parse_expr(p);
            expect(p, T_SEMI, "esperava ';' apos reverte");
            list_push_stmt(&e->as.block.stmts, s);
        } else {
            Expr *inner = parse_expr(p);
            if (check(p, T_SEMI)) {
                advance_p(p);
                Stmt *s = new_stmt(S_EXPR, inner->line);
                s->as.expr_s = inner;
                list_push_stmt(&e->as.block.stmts, s);
            } else if (check(p, T_RBRACE)) {
                e->as.block.tail = inner;
                break;
            } else if (is_block_like(inner)) {

                Stmt *s = new_stmt(S_EXPR, inner->line);
                s->as.expr_s = inner;
                list_push_stmt(&e->as.block.stmts, s);
            } else {
                error_at(p, "esperava ';' ou '}' apos a expressao");
            }
        }
    }
    expect(p, T_RBRACE, "esperava '}' no final do bloco");
    return e;
}

static ExprList parse_args(Parser *p) {
    ExprList args = {0};
    expect(p, T_LPAREN, "esperava '('");
    if (!check(p, T_RPAREN)) {
        do {
            list_push_expr(&args, parse_expr(p));
        } while (match_p(p, T_COMMA));
    }
    expect(p, T_RPAREN, "esperava ')'");
    return args;
}

static Expr *parse_primary(Parser *p) {
    int line = p->cur.line;
    if (check(p, T_INT)) {
        Expr *e = new_expr(E_INT, line);
        e->as.int_v = strtoll(p->cur.start, NULL, 10);
        advance_p(p);
        return e;
    }
    if (check(p, T_FLOAT)) {
        Expr *e = new_expr(E_FLOAT, line);
        e->as.float_v = strtod(p->cur.start, NULL);
        advance_p(p);
        return e;
    }
    if (check(p, T_STRING)) {
        Expr *e = new_expr(E_STRING, line);
        e->as.string_v.data = (char *)p->cur.start;
        e->as.string_v.len = (int)p->cur.length;
        advance_p(p);
        return e;
    }
    if (check(p, T_TRUE) || check(p, T_FALSE)) {
        Expr *e = new_expr(E_BOOL, line);
        e->as.bool_v = check(p, T_TRUE);
        advance_p(p);
        return e;
    }
    if (check(p, T_LBRACKET)) return parse_array(p);
    if (check(p, T_LBRACE)) return parse_block(p);
    if (check(p, T_IF)) return parse_if(p);
    if (check(p, T_MATCH)) return parse_match(p);
    if (check(p, T_FN)) {

        advance_p(p);
        FnDecl *fn = xmalloc(sizeof(FnDecl));
        fn->name = NULL;
        fn->params = NULL;
        fn->param_count = 0;
        expect(p, T_LPAREN, "esperava '(' na lambda");
        if (!check(p, T_RPAREN)) {
            do {
                fn->params = realloc(fn->params, sizeof(char *) * (size_t)(fn->param_count + 1));
                fn->params[fn->param_count++] = expect_ident(p, "esperava um parametro");
            } while (match_p(p, T_COMMA));
        }
        expect(p, T_RPAREN, "esperava ')' na lambda");
        fn->body = parse_block(p);
        Expr *e = new_expr(E_LAMBDA, line);
        e->as.lambda.fn = fn;
        return e;
    }
    if (check(p, T_LPAREN)) {
        advance_p(p);
        Expr *e = parse_expr(p);
        expect(p, T_RPAREN, "esperava ')'");
        return e;
    }
    if (check(p, T_IDENT)) {
        char *name = expect_ident(p, "esperava um identificador");
        if (check(p, T_COLONCOLON)) {
            advance_p(p);
            Expr *e = new_expr(E_ENUM_NEW, line);
            e->as.enum_new.type_name = name;
            e->as.enum_new.variant = expect_ident(p, "esperava o nome da variante");
            e->as.enum_new.args.items = NULL;
            e->as.enum_new.args.count = 0;
            if (check(p, T_LPAREN)) e->as.enum_new.args = parse_args(p);
            return e;
        }
        Expr *e = new_expr(E_IDENT, line);
        e->as.ident = name;
        return e;
    }
    error_at(p, "esperava uma expressao");
    return NULL; 
}


static Expr *parse_postfix(Parser *p) {
    Expr *e = parse_primary(p);
    for (;;) {
        if (check(p, T_LBRACKET)) {
            int line = p->cur.line;
            advance_p(p);
            Expr *idx = parse_expr(p);
            expect(p, T_RBRACKET, "esperava ']' apos o indice");
            Expr *ie = new_expr(E_INDEX, line);
            ie->as.index.array = e;
            ie->as.index.index = idx;
            e = ie;
        } else if (check(p, T_LPAREN)) {
            int line = p->cur.line;
            Expr *ce = new_expr(E_CALL, line);
            ce->as.call.callee = e;
            ce->as.call.args = parse_args(p);
            e = ce;
        } else {
            break;
        }
    }
    return e;
}

static Expr *parse_unary(Parser *p) {
    int line = p->cur.line;
    if (check(p, T_MINUS) || check(p, T_NOT)) {
        UnOp op = check(p, T_MINUS) ? UN_NEG : UN_NOT;
        advance_p(p);
        Expr *e = new_expr(E_UNARY, line);
        e->as.unary.op = op;
        e->as.unary.operand = parse_unary(p);
        return e;
    }
    return parse_postfix(p);
}


static Expr *parse_bin_level(Parser *p, Expr *(*next)(Parser *),
                             const TokenKind *kinds, const BinOp *ops, int n) {
    Expr *left = next(p);
    for (;;) {
        int matched = 0;
        for (int i = 0; i < n; i++) {
            if (check(p, kinds[i])) {
                int line = p->cur.line;
                advance_p(p);
                Expr *right = next(p);
                Expr *e = new_expr(E_BINARY, line);
                e->as.binary.op = ops[i];
                e->as.binary.left = left;
                e->as.binary.right = right;
                left = e;
                matched = 1;
                break;
            }
        }
        if (!matched) return left;
    }
}

static Expr *parse_mul(Parser *p) {
    static const TokenKind ks[] = { T_STAR, T_SLASH, T_PERCENT };
    static const BinOp ops[] = { BIN_MUL, BIN_DIV, BIN_MOD };
    return parse_bin_level(p, parse_unary, ks, ops, 3);
}
static Expr *parse_add(Parser *p) {
    static const TokenKind ks[] = { T_PLUS, T_MINUS };
    static const BinOp ops[] = { BIN_ADD, BIN_SUB };
    return parse_bin_level(p, parse_mul, ks, ops, 2);
}
static Expr *parse_cmp(Parser *p) {
    static const TokenKind ks[] = { T_LT, T_LE, T_GT, T_GE };
    static const BinOp ops[] = { BIN_LT, BIN_LE, BIN_GT, BIN_GE };
    return parse_bin_level(p, parse_add, ks, ops, 4);
}
static Expr *parse_eq(Parser *p) {
    static const TokenKind ks[] = { T_EQ, T_NE };
    static const BinOp ops[] = { BIN_EQ, BIN_NE };
    return parse_bin_level(p, parse_cmp, ks, ops, 2);
}
static Expr *parse_and(Parser *p) {
    static const TokenKind ks[] = { T_AND };
    static const BinOp ops[] = { BIN_AND };
    return parse_bin_level(p, parse_eq, ks, ops, 1);
}
static Expr *parse_or(Parser *p) {
    static const TokenKind ks[] = { T_OR };
    static const BinOp ops[] = { BIN_OR };
    return parse_bin_level(p, parse_and, ks, ops, 1);
}

static Expr *parse_assignment(Parser *p) {
    Expr *left = parse_or(p);
    if (check(p, T_ASSIGN)) {
        int line = p->cur.line;
        advance_p(p);
        if (left->kind == E_IDENT) {
            Expr *e = new_expr(E_ASSIGN, line);
            e->as.assign.name = left->as.ident;
            e->as.assign.value = parse_assignment(p);
            return e;
        }
        if (left->kind == E_INDEX) {
            Expr *e = new_expr(E_INDEX_ASSIGN, line);
            e->as.index_assign.array = left->as.index.array;
            e->as.index_assign.index = left->as.index.index;
            e->as.index_assign.value = parse_assignment(p);
            return e;
        }
        error_at(p, "alvo invalido para atribuicao");
    }
    return left;
}

static Expr *parse_expr(Parser *p) { return parse_assignment(p); }

/* ---------- itens de topo ---------- */

static FnDecl parse_fn(Parser *p) {
    expect(p, T_FN, "esperava 'functio'");
    FnDecl fn = {0};
    fn.name = expect_ident(p, "esperava o nome da funcao");
    expect(p, T_LPAREN, "esperava '('");
    if (!check(p, T_RPAREN)) {
        do {
            fn.params = realloc(fn.params, sizeof(char *) * (size_t)(fn.param_count + 1));
            fn.params[fn.param_count++] = expect_ident(p, "esperava um parametro");
        } while (match_p(p, T_COMMA));
    }
    expect(p, T_RPAREN, "esperava ')'");
    fn.body = parse_block(p);
    return fn;
}

static EnumDecl parse_enum(Parser *p) {
    expect(p, T_ENUM, "esperava 'genus'");
    EnumDecl en = {0};
    en.name = expect_ident(p, "esperava o nome do genus");
    expect(p, T_LBRACE, "esperava '{'");
    while (!check(p, T_RBRACE)) {
        en.variants = realloc(en.variants, sizeof(EnumVariant) * (size_t)(en.variant_count + 1));
        EnumVariant *v = &en.variants[en.variant_count++];
        v->name = expect_ident(p, "esperava o nome da variante");
        v->arity = 0;
        if (match_p(p, T_LPAREN)) {
            if (!check(p, T_RPAREN)) {
                do {
                    free(expect_ident(p, "esperava um campo"));
                    v->arity++;
                } while (match_p(p, T_COMMA));
            }
            expect(p, T_RPAREN, "esperava ')'");
        }
        if (!check(p, T_RBRACE)) expect(p, T_COMMA, "esperava ',' entre variantes");
    }
    expect(p, T_RBRACE, "esperava '}'");
    return en;
}

Program *parse_program(const char *src) {
    Parser p;
    p.filename = "<arquivo>";
    lexer_init(&p.lx, src);

    if (setjmp(p.err_jmp)) return NULL;

    advance_p(&p);
    Item *items = NULL;
    int count = 0;
    while (!check(&p, T_END)) {
        items = realloc(items, sizeof(Item) * (size_t)(count + 1));
        if (check(&p, T_FN)) {
            items[count].kind = ITEM_FN;
            items[count].as.fn = parse_fn(&p);
        } else if (check(&p, T_ENUM)) {
            items[count].kind = ITEM_ENUM;
            items[count].as.en = parse_enum(&p);
        } else {
            error_at(&p, "esperava 'functio' ou 'genus' no nivel principal");
        }
        count++;
    }
    Program *prog = xmalloc(sizeof(Program));
    prog->items = items;
    prog->count = count;
    return prog;
}
