#ifndef CERAS_AST_H
#define CERAS_AST_H

typedef enum {
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_MOD,
    BIN_EQ, BIN_NE, BIN_LT, BIN_LE, BIN_GT, BIN_GE,
    BIN_AND, BIN_OR,
} BinOp;

typedef enum { UN_NEG, UN_NOT } UnOp;

typedef enum {
    E_INT, E_FLOAT, E_STRING, E_BOOL,
    E_IDENT,
    E_UNARY, E_BINARY, E_ASSIGN,
    E_CALL,             
    E_LAMBDA,               
    E_ARRAY,
    E_INDEX,                  
    E_INDEX_ASSIGN,         
    E_IF,                          
    E_MATCH,
    E_BLOCK,                      
} ExprKind;

typedef struct Expr Expr;
typedef struct Stmt Stmt;

typedef struct { char *name; char **params; int param_count; Expr *body; } FnDecl;

typedef struct { Expr **items; int count; } ExprList;
typedef struct { Stmt **items; int count; } StmtList;

typedef struct {
    char *type_name;  
    char *variant;
    char **binders;    
    int binder_count;
    Expr *body;
} MatchArm;

struct Expr {
    ExprKind kind;
    int line;
    union {
        long long int_v;
        double float_v;
        struct { char *data; int len; } string_v;
        int bool_v;
        char *ident;
        struct { UnOp op; Expr *operand; } unary;
        struct { BinOp op; Expr *left; Expr *right; } binary;
        struct { char *name; Expr *value; } assign;
        struct { Expr *callee; ExprList args; } call;
        struct { FnDecl *fn; } lambda;
        struct { char *type_name; char *variant; ExprList args; } enum_new;
        ExprList array;
        struct { Expr *array; Expr *index; } index;
        struct { Expr *array; Expr *index; Expr *value; } index_assign;
        struct { Expr *cond; Expr *then_b; Expr *else_b; } if_e; 
        struct { Expr *subject; MatchArm *arms; int arm_count; } match;
        struct { StmtList stmts; Expr *tail; } block;
    } as;
};

typedef enum { S_LET, S_EXPR, S_WHILE, S_FOR, S_RETURN, S_OCCULTA } StmtKind;

struct Stmt {
    StmtKind kind;
    int line;
    union {
        struct { char *name; int mut; Expr *init; } let_s;
        Expr *expr_s;
        struct { Expr *cond; Expr *body; } while_s;  
        struct { char *var; Expr *iterable; Expr *body; } for_s; 
        Expr *return_s; 
        struct { char *path; char *fn_name; } occulta_s; 
    } as;
};

typedef struct { char *name; int arity; } EnumVariant;
typedef struct { char *name; EnumVariant *variants; int variant_count; } EnumDecl;

typedef enum { ITEM_FN, ITEM_ENUM } ItemKind;
typedef struct {
    ItemKind kind;
    union { FnDecl fn; EnumDecl en; } as;
} Item;

typedef struct { Item *items; int count; } Program;

#endif
