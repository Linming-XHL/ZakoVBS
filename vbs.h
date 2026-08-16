#ifndef VBS_H
#define VBS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <setjmp.h>

/* ========== 值类型 ========== */
typedef enum {
    VALTYPE_EMPTY,
    VALTYPE_NULL,
    VALTYPE_INTEGER,
    VALTYPE_DOUBLE,
    VALTYPE_STRING,
    VALTYPE_BOOL,
    VALTYPE_OBJECT,
    VALTYPE_ARRAY,
    VALTYPE_NOTHING
} ValType;

typedef struct Value Value;
typedef struct Object Object;
typedef struct VbsArray VbsArray;
typedef struct Interp Interp;
typedef struct AstNode AstNode;
typedef struct Env Env;

struct Value {
    ValType type;
    union {
        long long integer;
        double dbl;
        char *str;
        int boolean;
        Object *obj;
        VbsArray *arr;
    } as;
};

struct VbsArray {
    int dim_count;
    int *dims;
    int total_size;
    int refcount;
    Value *data;
};

typedef Value (*BuiltinFunc)(Interp *interp, int argc, Value *argv);

struct Object {
    char *type;
    int refcount;
    void *data;
    void (*destroy)(Object *obj);
    Value (*get_prop)(Object *obj, const char *name);
    Value (*call_method)(Object *obj, Interp *interp, const char *name, int argc, Value *argv);
};

typedef struct {
    char *name;
    Value value;
    int is_const;
} Variable;

/* ========== 词法标记 ========== */
typedef enum {
    TOK_EOF,
    TOK_IDENT, TOK_NUMBER, TOK_STRING,
    TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, TOK_INTDIV, TOK_MOD, TOK_POW, TOK_CONCAT,
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_IS,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACKET, TOK_RBRACKET,
    TOK_DOT, TOK_COMMA, TOK_COLON, TOK_NEWLINE, TOK_ASSIGN,
    TOK_KEY_DIM, TOK_KEY_SET, TOK_KEY_IF, TOK_KEY_THEN, TOK_KEY_ELSE,
    TOK_KEY_ELSEIF, TOK_KEY_END_IF, TOK_KEY_FOR, TOK_KEY_NEXT,
    TOK_KEY_DO, TOK_KEY_WHILE, TOK_KEY_UNTIL, TOK_KEY_LOOP,
    TOK_KEY_WEND, TOK_KEY_WITH, TOK_KEY_FUNCTION, TOK_KEY_SUB,
    TOK_KEY_END_FUNCTION, TOK_KEY_END_SUB, TOK_KEY_CALL, TOK_KEY_EXIT,
    TOK_KEY_SELECT, TOK_KEY_CASE, TOK_KEY_END_SELECT,
    TOK_KEY_ON, TOK_KEY_ERROR, TOK_KEY_RESUME, TOK_KEY_NEXT_STMT,
    TOK_KEY_CONST, TOK_KEY_PUBLIC, TOK_KEY_PRIVATE,
    TOK_KEY_TRUE, TOK_KEY_FALSE, TOK_KEY_NOTHING, TOK_KEY_NULL,
    TOK_KEY_EMPTY, TOK_KEY_REDIM, TOK_KEY_PRESERVE,
    TOK_KEY_CLASS, TOK_KEY_END_CLASS, TOK_KEY_NEW, TOK_KEY_EACH, TOK_KEY_IN,
    TOK_KEY_TO, TOK_KEY_STEP, TOK_KEY_AND, TOK_KEY_OR, TOK_KEY_NOT, TOK_KEY_XOR,
    TOK_KEY_EXIT_DO, TOK_KEY_EXIT_FOR, TOK_KEY_EXIT_FUNCTION, TOK_KEY_EXIT_SUB,
    TOK_KEY_RANDOMIZE, TOK_KEY_OPTION, TOK_KEY_EXPLICIT, TOK_KEY_ERASE,
    TOK_KEY_TYPE, TOK_KEY_END_TYPE, TOK_KEY_GET, TOK_KEY_LET, TOK_KEY_PROPERTY,
    TOK_KEY_END_PROPERTY, TOK_KEY_DEFAULT, TOK_KEY_REM,
    TOK_KEY_GOTO,
    TOK_LINE_CONTINUE
} TokenType;

typedef struct {
    TokenType type;
    char *text;
    int line;
    int col;
} Token;

/* ========== AST 节点类型 ========== */
typedef enum {
    AST_PROGRAM,
    AST_BLOCK,
    AST_VAR_DECL,
    AST_CONST_DECL,
    AST_ASSIGN, AST_SET_ASSIGN,
    AST_BINOP, AST_UNOP,
    AST_LITERAL, AST_IDENT, AST_ARRAY_LITERAL,
    AST_IF, AST_FOR, AST_FOR_EACH, AST_DO_LOOP, AST_WHILE_WEND,
    AST_SELECT, AST_CASE, AST_CASE_ELSE,
    AST_FUNC_DECL, AST_SUB_DECL, AST_CALL, AST_FUNCALL,
    AST_METHOD_CALL, AST_PROP_GET, AST_INDEX_GET,
    AST_NEW_EXPR, AST_WITH, AST_REDIM,
    AST_EXIT_DO, AST_EXIT_FOR, AST_EXIT_FUNC, AST_EXIT_SUB,
    AST_ON_ERROR, AST_ERASE, AST_REM_STMT,
    AST_LABEL, AST_GOTO, AST_RANDOMIZE,
    AST_OPTION_EXPLICIT, AST_ARRAY_DECL
} AstType;

struct AstNode {
    AstType type;
    int line;
    union {
        struct { AstNode **stmts; int count; } program;
        struct { AstNode **stmts; int count; } block;
        struct { char **names; int count; AstNode *init_expr; } var_decl;
        struct { char **names; int count; AstNode *init_expr; } const_decl;
        struct { char *name; AstNode *value; AstNode *index; } assign;
        struct { char *name; AstNode *value; } set_assign;
        struct { int op; AstNode *left, *right; } binop;
        struct { int op; AstNode *operand; } unop;
        struct { Value value; } literal;
        struct { char *name; } ident;
        struct { AstNode **elems; int count; } array_literal;
        struct { AstNode *cond; AstNode *body; AstNode *else_body; AstNode **elseifs; int elseif_count; } if_stmt;
        struct { char *var; AstNode *start; AstNode *end; AstNode *step; AstNode *body; } for_stmt;
        struct { char *var; AstNode *expr; AstNode *body; } for_each;
        struct { int pre_test; int until; AstNode *cond; AstNode *body; } do_loop;
        struct { AstNode *cond; AstNode *body; } while_wend;
        struct { AstNode *expr; AstNode **cases; int case_count; AstNode *else_case; } select_stmt;
        struct { AstNode **exprs; int count; AstNode *body; } case_stmt;
        struct { AstNode *body; } case_else;
        struct { char *name; char **params; int param_count; AstNode *body; int is_function; } func_decl;
        struct { char *name; AstNode **args; int argc; } call;
        struct { char *name; AstNode **args; int argc; } funcall;
        struct { char *obj; char *method; AstNode **args; int argc; } method_call;
        struct { char *name; char *prop; } prop_get;
        struct { char *name; AstNode *index; } index_get;
        struct { char *class_name; AstNode *args; int argc; } new_expr;
        struct { AstNode *obj_expr; AstNode *body; } with_stmt;
        struct { char *name; int dims_count; int *dims; int preserve; } redim;
        struct { int mode; } on_error;
        struct { char *name; } erase;
        struct { char *text; } rem_stmt;
        struct { char *name; } label;
        struct { char *name; } goto_stmt;
        struct { AstNode *dims; int dims_count; } array_decl;
    } as;
};

/* ========== 环境（作用域） ========== */
struct Env {
    Env *parent;
    Variable *vars;
    int var_count;
    int var_cap;
};

/* ========== 解释器 ========== */
typedef struct {
    char *name;
    AstNode *node;
} FuncEntry;

struct Interp {
    Env *global_env;
    Env *current_env;
    AstNode *program;
    int option_explicit;
    int error_occured;
    int error_line;
    int cur_line;
    char error_msg[1024];
    int on_error_resume;
    jmp_buf error_jmp;
    int running;
    int exit_code;
    Value *func_results;
    int func_result_count;
    int func_result_cap;
    char *script_path;
    char *script_dir;
    int trace;
    FuncEntry *funcs;
    int func_count;
    int func_cap;
    int exiting_func;
};

/* ========== 内置函数注册 ========== */
typedef struct {
    char *name;
    BuiltinFunc func;
    int min_args;
    int max_args;
} BuiltinEntry;

/* ========== 值操作函数 ========== */
Value val_int(long long v);
Value val_double(double v);
Value val_str(const char *s);
Value val_bool(int b);
Value val_empty(void);
Value val_null(void);
Value val_nothing(void);
Value val_obj(Object *obj);
Value val_arr(VbsArray *a);
void val_free(Value *v);
Value val_clone(Value v);
char *val_tostr(Value v);
long long val_toint(Value v);
double val_todouble(Value v);
int val_tobool(Value v);
int val_isnum(Value v);

/* ========== 对象系统 ========== */
Object *obj_new(const char *type, void *data,
    void (*destroy)(Object*),
    Value (*get_prop)(Object*, const char*),
    Value (*call_method)(Object*, Interp*, const char*, int, Value*));
void obj_ref(Object *obj);
void obj_unref(Object *obj);
void obj_destroy(Object *obj);

/* ========== 数组操作 ========== */
VbsArray *arr_new(int dims, int *sizes);
VbsArray *arr_new_1d(int size);
void arr_free(VbsArray *a);
int arr_idx(VbsArray *a, int dims, int *indices);

/* ========== 词法分析器 ========== */
typedef struct {
    const char *src;
    int pos;
    int len;
    int line;
    int col;
    Token cur;
    Token peek;
    int has_peek;
} Lexer;

typedef struct {
    int pos;
    int len;
    int line;
    int col;
    int has_peek;
    Token peek;
} LexerSnapshot;

Lexer *lexer_new(const char *src);
void lexer_free(Lexer *lx);
Token lexer_next(Lexer *lx);
Token lexer_peek(Lexer *lx);
void lexer_skip_newlines(Lexer *lx);
LexerSnapshot lexer_save(Lexer *lx);
void lexer_restore(Lexer *lx, LexerSnapshot s);
const char *token_type_name(TokenType t);

/* ========== 解析器 ========== */
typedef struct {
    Lexer *lexer;
    AstNode *program;
    int error;
    char error_msg[1024];
} Parser;

Parser *parser_new(Lexer *lx);
void parser_free(Parser *p);
int parser_parse(Parser *p);
void ast_free(AstNode *node);
void ast_print(AstNode *node, int depth);

/* ========== 解释器 ========== */
Interp *interp_new(AstNode *prog, const char *path);
void interp_free(Interp *interp);
int interp_run(Interp *interp);
Value interp_eval(Interp *interp, AstNode *node);
void interp_set_var(Interp *interp, const char *name, Value val, int is_const);
Value interp_get_var(Interp *interp, const char *name);
int interp_has_var(Interp *interp, const char *name);
void interp_push_env(Interp *interp);
void interp_pop_env(Interp *interp);
void interp_error(Interp *interp, const char *fmt, ...);

/* ========== 内置函数初始化 ========== */
void builtin_init(Interp *interp);
Value builtin_call(Interp *interp, const char *name, int argc, Value *argv);

/* ========== WScript 对象 ========== */
Object *wscript_create(const char *path);

/* ========== FileSystemObject ========== */
Object *fso_create(void);

/* ========== Dictionary 对象 ========== */
Object *dict_create(void);

#endif