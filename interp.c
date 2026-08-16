#include "vbs.h"

/* ========== 值操作实现 ========== */
Value val_empty(void) {
    Value v;
    v.type = VALTYPE_EMPTY;
    memset(&v.as, 0, sizeof(v.as));
    return v;
}

Value val_null(void) {
    Value v;
    v.type = VALTYPE_NULL;
    memset(&v.as, 0, sizeof(v.as));
    return v;
}

Value val_nothing(void) {
    Value v;
    v.type = VALTYPE_NOTHING;
    memset(&v.as, 0, sizeof(v.as));
    return v;
}

Value val_int(long long i) {
    Value v;
    v.type = VALTYPE_INTEGER;
    v.as.integer = i;
    return v;
}

Value val_double(double d) {
    Value v;
    v.type = VALTYPE_DOUBLE;
    v.as.dbl = d;
    return v;
}

Value val_str(const char *s) {
    Value v;
    v.type = VALTYPE_STRING;
    v.as.str = s ? strdup(s) : strdup("");
    return v;
}

Value val_bool(int b) {
    Value v;
    v.type = VALTYPE_BOOL;
    v.as.boolean = b ? 1 : 0;
    return v;
}

Value val_obj(Object *obj) {
    Value v;
    v.type = VALTYPE_OBJECT;
    v.as.obj = obj;
    if (obj) obj_ref(obj);
    return v;
}

Value val_arr(VbsArray *a) {
    Value v;
    v.type = VALTYPE_ARRAY;
    v.as.arr = a;
    return v;
}

void val_free(Value *v) {
    if (v->type == VALTYPE_STRING && v->as.str) {
        free(v->as.str);
        v->as.str = NULL;
    } else if (v->type == VALTYPE_OBJECT && v->as.obj) {
        obj_unref(v->as.obj);
        v->as.obj = NULL;
    } else if (v->type == VALTYPE_ARRAY && v->as.arr) {
        v->as.arr->refcount--;
        if (v->as.arr->refcount <= 0) {
            arr_free(v->as.arr);
        }
        v->as.arr = NULL;
    }
    v->type = VALTYPE_EMPTY;
}

Value val_clone(Value v) {
    if (v.type == VALTYPE_STRING) {
        return val_str(v.as.str);
    }
    if (v.type == VALTYPE_OBJECT && v.as.obj) {
        obj_ref(v.as.obj);
    }
    if (v.type == VALTYPE_ARRAY && v.as.arr) {
        v.as.arr->refcount++;
    }
    return v;
}

int val_isnum(Value v) {
    return v.type == VALTYPE_INTEGER || v.type == VALTYPE_DOUBLE;
}

long long val_toint(Value v) {
    switch (v.type) {
        case VALTYPE_INTEGER: return v.as.integer;
        case VALTYPE_DOUBLE: return (long long)v.as.dbl;
        case VALTYPE_BOOL: return v.as.boolean ? -1 : 0;
        case VALTYPE_STRING: {
            char *end;
            long long r = strtoll(v.as.str, &end, 0);
            if (*end == 0) return r;
            double d = strtod(v.as.str, &end);
            if (*end == 0) return (long long)d;
            return 0;
        }
        case VALTYPE_EMPTY: return 0;
        case VALTYPE_NULL: return 0;
        default: return 0;
    }
}

double val_todouble(Value v) {
    switch (v.type) {
        case VALTYPE_INTEGER: return (double)v.as.integer;
        case VALTYPE_DOUBLE: return v.as.dbl;
        case VALTYPE_BOOL: return v.as.boolean ? -1.0 : 0.0;
        case VALTYPE_STRING: {
            char *end;
            double r = strtod(v.as.str, &end);
            if (*end == 0) return r;
            return 0.0;
        }
        case VALTYPE_EMPTY: return 0.0;
        default: return 0.0;
    }
}

int val_tobool(Value v) {
    switch (v.type) {
        case VALTYPE_BOOL: return v.as.boolean;
        case VALTYPE_INTEGER: return v.as.integer != 0;
        case VALTYPE_DOUBLE: return v.as.dbl != 0.0;
        case VALTYPE_STRING: return v.as.str && v.as.str[0] != 0;
        case VALTYPE_EMPTY: return 0;
        case VALTYPE_NULL: return 0;
        case VALTYPE_OBJECT: return 1;
        case VALTYPE_ARRAY: return 1;
        default: return 0;
    }
}

char *val_tostr(Value v) {
    static char buf[128];
    switch (v.type) {
        case VALTYPE_EMPTY: return strdup("");
        case VALTYPE_NULL: return strdup("Null");
        case VALTYPE_NOTHING: return strdup("Nothing");
        case VALTYPE_INTEGER: snprintf(buf, sizeof(buf), "%lld", (long long)v.as.integer); return strdup(buf);
        case VALTYPE_DOUBLE: {
            snprintf(buf, sizeof(buf), "%.15g", v.as.dbl);
            if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E')) {
                strcat(buf, ".0");
            }
            return strdup(buf);
        }
        case VALTYPE_BOOL: return strdup(v.as.boolean ? "True" : "False");
        case VALTYPE_STRING: return strdup(v.as.str ? v.as.str : "");
        case VALTYPE_OBJECT: {
            snprintf(buf, sizeof(buf), "[Object: %s]", v.as.obj->type);
            return strdup(buf);
        }
        case VALTYPE_ARRAY: return strdup("(Array)");
        default: return strdup("");
    }
}

/* ========== 对象系统 ========== */
Object *obj_new(const char *type, void *data,
    void (*destroy)(Object*),
    Value (*get_prop)(Object*, const char*),
    Value (*call_method)(Object*, Interp*, const char*, int, Value*)) {
    Object *obj = calloc(1, sizeof(Object));
    if (!obj) { fprintf(stderr, "Out of memory\n"); exit(1); }
    obj->type = strdup(type);
    obj->data = data;
    obj->destroy = destroy;
    obj->get_prop = get_prop;
    obj->call_method = call_method;
    obj->refcount = 1;
    return obj;
}

void obj_ref(Object *obj) {
    if (obj) obj->refcount++;
}

void obj_unref(Object *obj) {
    if (!obj) return;
    obj->refcount--;
    if (obj->refcount <= 0) {
        obj_destroy(obj);
    }
}

void obj_destroy(Object *obj) {
    if (!obj) return;
    if (obj->destroy) obj->destroy(obj);
    free(obj->type);
    free(obj);
}

/* ========== 数组操作 ========== */
VbsArray *arr_new(int dims, int *sizes) {
    VbsArray *a = calloc(1, sizeof(VbsArray));
    if (!a) { fprintf(stderr, "Out of memory\n"); exit(1); }
    a->dim_count = dims;
    a->dims = malloc(sizeof(int) * dims);
    a->total_size = 1;
    for (int i = 0; i < dims; i++) {
        a->dims[i] = sizes[i];
        a->total_size *= sizes[i];
    }
    a->refcount = 1;
    a->data = calloc(a->total_size, sizeof(Value));
    return a;
}

VbsArray *arr_new_1d(int size) {
    return arr_new(1, &size);
}

void arr_free(VbsArray *a) {
    if (!a) return;
    for (int i = 0; i < a->total_size; i++) {
        val_free(&a->data[i]);
    }
    free(a->data);
    free(a->dims);
    free(a);
}

int arr_idx(VbsArray *a, int dims, int *indices) {
    int idx = 0;
    int mul = 1;
    for (int i = dims - 1; i >= 0; i--) {
        idx += indices[i] * mul;
        mul *= a->dims[i];
    }
    return idx;
}

/* ========== 环境操作 ========== */
static Env *env_new(Env *parent) {
    Env *env = calloc(1, sizeof(Env));
    if (!env) { fprintf(stderr, "Out of memory\n"); exit(1); }
    env->parent = parent;
    env->vars = NULL;
    env->var_count = 0;
    env->var_cap = 0;
    return env;
}

static void env_free(Env *env) {
    if (!env) return;
    for (int i = 0; i < env->var_count; i++) {
        free(env->vars[i].name);
        val_free(&env->vars[i].value);
    }
    free(env->vars);
    free(env);
}

static Variable *env_find(Env *env, const char *name) {
    for (Env *e = env; e; e = e->parent) {
        for (int i = 0; i < e->var_count; i++) {
            if (strcasecmp(e->vars[i].name, name) == 0) {
                return &e->vars[i];
            }
        }
    }
    return NULL;
}

static Variable *env_find_local(Env *env, const char *name) {
    for (int i = 0; i < env->var_count; i++) {
        if (strcasecmp(env->vars[i].name, name) == 0) {
            return &env->vars[i];
        }
    }
    return NULL;
}

static void env_set(Env *env, const char *name, Value val, int is_const) {
    Variable *v = env_find_local(env, name);
    if (v) {
        if (v->is_const) return;
        val_free(&v->value);
        v->value = val;
        return;
    }
    if (env->var_count >= env->var_cap) {
        env->var_cap = env->var_cap ? env->var_cap * 2 : 16;
        env->vars = realloc(env->vars, sizeof(Variable) * env->var_cap);
    }
    env->vars[env->var_count].name = strdup(name);
    env->vars[env->var_count].value = val;
    env->vars[env->var_count].is_const = is_const;
    env->var_count++;
}

/* ========== 解释器实现 ========== */
Interp *interp_new(AstNode *prog, const char *path) {
    Interp *interp = calloc(1, sizeof(Interp));
    if (!interp) { fprintf(stderr, "Out of memory\n"); exit(1); }
    interp->program = prog;
    interp->global_env = env_new(NULL);
    interp->current_env = interp->global_env;
    interp->option_explicit = 0;
    interp->on_error_resume = 0;
    interp->error_occured = 0;
    interp->running = 1;
    interp->exit_code = 0;
    interp->error_msg[0] = 0;
    interp->func_results = NULL;
    interp->func_result_count = 0;
    interp->func_result_cap = 0;
    interp->trace = 0;

    if (path) {
        interp->script_path = strdup(path);
        char *p = strrchr(interp->script_path, '/');
        if (p) {
            interp->script_dir = strdup(interp->script_path);
            interp->script_dir[p - interp->script_path] = 0;
        } else {
            interp->script_dir = strdup(".");
        }
    } else {
        interp->script_path = strdup("");
        interp->script_dir = strdup(".");
    }

    builtin_init(interp);
    return interp;
}

void interp_free(Interp *interp) {
    if (!interp) return;
    env_free(interp->global_env);
    free(interp->script_path);
    free(interp->script_dir);
    for (int i = 0; i < interp->func_result_count; i++) {
        val_free(&interp->func_results[i]);
    }
    free(interp->func_results);
    for (int i = 0; i < interp->func_count; i++) {
        free(interp->funcs[i].name);
    }
    free(interp->funcs);
    free(interp);
}

void interp_error(Interp *interp, const char *fmt, ...) {
    va_list ap;
    char tmp[1024];
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (strlen(tmp) > 900) tmp[900] = 0;
    snprintf(interp->error_msg, sizeof(interp->error_msg), "第%d行: %s",
        interp->cur_line ? interp->cur_line : interp->error_line, tmp);
    interp->error_line = interp->cur_line;
    interp->error_occured = 1;
    if (!interp->on_error_resume) {
        interp->running = 0;
    }
}

void interp_push_env(Interp *interp) {
    Env *new_env = env_new(interp->current_env);
    interp->current_env = new_env;
}

void interp_pop_env(Interp *interp) {
    if (interp->current_env == interp->global_env) return;
    Env *old = interp->current_env;
    interp->current_env = old->parent;
    old->parent = NULL;
    env_free(old);
}

int interp_has_var(Interp *interp, const char *name) {
    return env_find(interp->current_env, name) != NULL;
}

Value interp_get_var(Interp *interp, const char *name) {
    Variable *v = env_find(interp->current_env, name);
    if (!v) {
        if (interp->option_explicit) {
            interp_error(interp, "变量未定义: %s", name);
            return val_empty();
        }
        env_set(interp->current_env, name, val_empty(), 0);
        return val_empty();
    }
    return val_clone(v->value);
}

void interp_set_var(Interp *interp, const char *name, Value val, int is_const) {
    Variable *v = env_find(interp->current_env, name);
    if (v) {
        if (v->is_const && !is_const) return;
        val_free(&v->value);
        v->value = val;
        v->is_const = is_const;
        return;
    }
    env_set(interp->current_env, name, val, is_const);
}

static void interp_register_func(Interp *interp, const char *name, AstNode *node) {
    for (int i = 0; i < interp->func_count; i++) {
        if (strcasecmp(interp->funcs[i].name, name) == 0) {
            free(interp->funcs[i].name);
            interp->funcs[i].name = strdup(name);
            interp->funcs[i].node = node;
            return;
        }
    }
    if (interp->func_count >= interp->func_cap) {
        interp->func_cap = interp->func_cap ? interp->func_cap * 2 : 32;
        interp->funcs = realloc(interp->funcs, sizeof(FuncEntry) * interp->func_cap);
    }
    interp->funcs[interp->func_count].name = strdup(name);
    interp->funcs[interp->func_count].node = node;
    interp->func_count++;
}

static FuncEntry *interp_find_func(Interp *interp, const char *name) {
    for (int i = 0; i < interp->func_count; i++) {
        if (strcasecmp(interp->funcs[i].name, name) == 0) {
            return &interp->funcs[i];
        }
    }
    return NULL;
}

static Value interp_call_userfunc(Interp *interp, FuncEntry *fn, int argc, Value *argv) {
    AstNode *decl = fn->node;
    interp_push_env(interp);

    for (int i = 0; i < decl->as.func_decl.param_count; i++) {
        if (i < argc) {
            interp_set_var(interp, decl->as.func_decl.params[i], val_clone(argv[i]), 0);
        } else {
            interp_set_var(interp, decl->as.func_decl.params[i], val_empty(), 0);
        }
    }

    int saved_exiting = interp->exiting_func;
    interp->exiting_func = 0;
    interp_eval(interp, decl->as.func_decl.body);
    interp->exiting_func = saved_exiting;

    Value r = val_empty();
    if (decl->as.func_decl.is_function) {
        Variable *v = env_find(interp->current_env, decl->as.func_decl.name);
        if (v) {
            r = val_clone(v->value);
        }
    }
    interp_pop_env(interp);
    return r;
}

static Value interp_eval_binop(Interp *interp, int op, Value left, Value right) {
    Value r = val_empty();

    switch (op) {
        case TOK_PLUS: {
            if (left.type == VALTYPE_STRING || right.type == VALTYPE_STRING) {
                char *ls = val_tostr(left);
                char *rs = val_tostr(right);
                char *buf = malloc(strlen(ls) + strlen(rs) + 1);
                strcpy(buf, ls);
                strcat(buf, rs);
                r = val_str(buf);
                free(buf);
                free(ls);
                free(rs);
            } else {
                r = val_double(val_todouble(left) + val_todouble(right));
            }
            break;
        }
        case TOK_MINUS: r = val_double(val_todouble(left) - val_todouble(right)); break;
        case TOK_MUL: r = val_double(val_todouble(left) * val_todouble(right)); break;
        case TOK_DIV: {
            double rd = val_todouble(right);
            if (rd == 0.0) { val_free(&left); val_free(&right); interp_error(interp, "除以零"); return val_empty(); }
            r = val_double(val_todouble(left) / rd);
            break;
        }
        case TOK_INTDIV: {
            long long l = val_toint(left), rv = val_toint(right);
            if (rv == 0) { interp_error(interp, "除以零"); return val_empty(); }
            r = val_int(l / rv);
            break;
        }
        case TOK_MOD: {
            long long l = val_toint(left), rv = val_toint(right);
            if (rv == 0) { interp_error(interp, "除以零"); return val_empty(); }
            r = val_int(l % rv);
            break;
        }
        case TOK_POW: r = val_double(pow(val_todouble(left), val_todouble(right))); break;
        case TOK_CONCAT: {
            char *ls = val_tostr(left);
            char *rs = val_tostr(right);
            char *buf = malloc(strlen(ls) + strlen(rs) + 1);
            strcpy(buf, ls);
            strcat(buf, rs);
            r = val_str(buf);
            free(buf);
            free(ls);
            free(rs);
            break;
        }
        case TOK_EQ: r = val_bool(val_todouble(left) == val_todouble(right)); break;
        case TOK_NEQ: {
            if (left.type == VALTYPE_STRING || right.type == VALTYPE_STRING) {
                char *ls = val_tostr(left);
                char *rs = val_tostr(right);
                r = val_bool(strcmp(ls, rs) != 0);
                free(ls);
                free(rs);
            } else {
                r = val_bool(val_todouble(left) != val_todouble(right));
            }
            break;
        }
        case TOK_LT: {
            if (left.type == VALTYPE_STRING || right.type == VALTYPE_STRING) {
                char *ls = val_tostr(left);
                char *rs = val_tostr(right);
                r = val_bool(strcmp(ls, rs) < 0);
                free(ls);
                free(rs);
            } else {
                r = val_bool(val_todouble(left) < val_todouble(right));
            }
            break;
        }
        case TOK_GT: {
            if (left.type == VALTYPE_STRING || right.type == VALTYPE_STRING) {
                char *ls = val_tostr(left);
                char *rs = val_tostr(right);
                r = val_bool(strcmp(ls, rs) > 0);
                free(ls);
                free(rs);
            } else {
                r = val_bool(val_todouble(left) > val_todouble(right));
            }
            break;
        }
        case TOK_LE: {
            if (left.type == VALTYPE_STRING || right.type == VALTYPE_STRING) {
                char *ls = val_tostr(left);
                char *rs = val_tostr(right);
                r = val_bool(strcmp(ls, rs) <= 0);
                free(ls);
                free(rs);
            } else {
                r = val_bool(val_todouble(left) <= val_todouble(right));
            }
            break;
        }
        case TOK_GE: {
            if (left.type == VALTYPE_STRING || right.type == VALTYPE_STRING) {
                char *ls = val_tostr(left);
                char *rs = val_tostr(right);
                r = val_bool(strcmp(ls, rs) >= 0);
                free(ls);
                free(rs);
            } else {
                r = val_bool(val_todouble(left) >= val_todouble(right));
            }
            break;
        }
        case TOK_KEY_AND: r = val_bool(val_tobool(left) && val_tobool(right)); break;
        case TOK_KEY_OR: r = val_bool(val_tobool(left) || val_tobool(right)); break;
        case TOK_KEY_XOR: r = val_bool(val_tobool(left) != val_tobool(right)); break;
        default:
            interp_error(interp, "未知运算符: %d", op);
    }

    val_free(&left);
    val_free(&right);
    return r;
}

Value interp_eval(Interp *interp, AstNode *node) {
    if (!node || !interp->running) return val_empty();
    interp->cur_line = node->line;

    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK: {
            Value r = val_empty();
            for (int i = 0; i < node->as.block.count && interp->running && !interp->exiting_func; i++) {
                val_free(&r);
                r = interp_eval(interp, node->as.block.stmts[i]);
            }
            return r;
        }

        case AST_VAR_DECL: {
            Value init = node->as.var_decl.init_expr ?
                interp_eval(interp, node->as.var_decl.init_expr) : val_empty();
            for (int i = 0; i < node->as.var_decl.count; i++) {
                interp_set_var(interp, node->as.var_decl.names[i],
                    val_clone(init), 0);
            }
            val_free(&init);
            return val_empty();
        }

        case AST_CONST_DECL: {
            Value v = node->as.const_decl.init_expr ?
                interp_eval(interp, node->as.const_decl.init_expr) : val_empty();
            for (int i = 0; i < node->as.const_decl.count; i++) {
                interp_set_var(interp, node->as.const_decl.names[i],
                    val_clone(v), 1);
            }
            val_free(&v);
            return val_empty();
        }

        case AST_ASSIGN: {
            Value v = interp_eval(interp, node->as.assign.value);
            char *name = node->as.assign.name;
            char *idx = strstr(name, "[]");
            if (idx) {
                *idx = 0;
                Value av = interp_get_var(interp, name);
                if (av.type == VALTYPE_ARRAY && av.as.arr) {
                    Value iv = interp_eval(interp, node->as.assign.value);
                    long long index = val_toint(iv);
                    val_free(&iv);
                    if (index >= 0 && index < av.as.arr->total_size) {
                        val_free(&av.as.arr->data[index]);
                        av.as.arr->data[index] = val_clone(v);
                    }
                }
                val_free(&av);
                *idx = '[';
            } else {
                interp_set_var(interp, name, v, 0);
            }
            return val_empty();
        }

        case AST_SET_ASSIGN: {
            Value v = interp_eval(interp, node->as.set_assign.value);
            interp_set_var(interp, node->as.set_assign.name, v, 0);
            return val_empty();
        }

        case AST_BINOP: {
            Value left = interp_eval(interp, node->as.binop.left);
            Value right = interp_eval(interp, node->as.binop.right);
            return interp_eval_binop(interp, node->as.binop.op, left, right);
        }

        case AST_UNOP: {
            Value v = interp_eval(interp, node->as.unop.operand);
            if (node->as.unop.op == TOK_MINUS) {
                if (v.type == VALTYPE_INTEGER) {
                    v.as.integer = -v.as.integer;
                } else {
                    v.as.dbl = -val_todouble(v);
                    v.type = VALTYPE_DOUBLE;
                }
            } else if (node->as.unop.op == TOK_KEY_NOT) {
                int b = !val_tobool(v);
                val_free(&v);
                v = val_bool(b);
            }
            return v;
        }

        case AST_LITERAL: {
            return val_clone(node->as.literal.value);
        }

        case AST_IDENT: {
            return interp_get_var(interp, node->as.ident.name);
        }

        case AST_IF: {
            Value cond = interp_eval(interp, node->as.if_stmt.cond);
            if (val_tobool(cond)) {
                val_free(&cond);
                return interp_eval(interp, node->as.if_stmt.body);
            }
            val_free(&cond);

            for (int i = 0; i < node->as.if_stmt.elseif_count; i++) {
                AstNode *ei = node->as.if_stmt.elseifs[i];
                Value ec = interp_eval(interp, ei->as.if_stmt.cond);
                if (val_tobool(ec)) {
                    val_free(&ec);
                    return interp_eval(interp, ei->as.if_stmt.body);
                }
                val_free(&ec);
            }

            if (node->as.if_stmt.else_body) {
                return interp_eval(interp, node->as.if_stmt.else_body);
            }
            return val_empty();
        }

        case AST_FOR: {
            Value start = interp_eval(interp, node->as.for_stmt.start);
            Value end = interp_eval(interp, node->as.for_stmt.end);
            double step_val = 1.0;
            if (node->as.for_stmt.step) {
                Value step = interp_eval(interp, node->as.for_stmt.step);
                step_val = val_todouble(step);
                val_free(&step);
            }
            double i = val_todouble(start);
            double end_val = val_todouble(end);
            val_free(&start);
            val_free(&end);

            for (; (step_val > 0 ? i <= end_val : i >= end_val) && interp->running; i += step_val) {
                interp_set_var(interp, node->as.for_stmt.var, val_double(i), 0);
                interp_eval(interp, node->as.for_stmt.body);
            }
            return val_empty();
        }

        case AST_DO_LOOP: {
            while (interp->running) {
                if (node->as.do_loop.pre_test && node->as.do_loop.cond) {
                    Value cond = interp_eval(interp, node->as.do_loop.cond);
                    int b = val_tobool(cond);
                    val_free(&cond);
                    if (node->as.do_loop.until ? b : !b) break;
                }
                interp_eval(interp, node->as.do_loop.body);
                if (interp->error_occured && interp->on_error_resume) {
                    interp->error_occured = 0;
                }
                if (!node->as.do_loop.pre_test && node->as.do_loop.cond) {
                    Value cond = interp_eval(interp, node->as.do_loop.cond);
                    int b = val_tobool(cond);
                    val_free(&cond);
                    if (node->as.do_loop.until ? b : !b) break;
                }
            }
            return val_empty();
        }

        case AST_WHILE_WEND: {
            while (interp->running) {
                Value cond = interp_eval(interp, node->as.while_wend.cond);
                int b = val_tobool(cond);
                val_free(&cond);
                if (!b) break;
                interp_eval(interp, node->as.while_wend.body);
            }
            return val_empty();
        }

        case AST_SELECT: {
            Value expr = interp_eval(interp, node->as.select_stmt.expr);
            for (int i = 0; i < node->as.select_stmt.case_count && !interp->error_occured; i++) {
                AstNode *c = node->as.select_stmt.cases[i];
                for (int j = 0; j < c->as.case_stmt.count; j++) {
                    Value cv = interp_eval(interp, c->as.case_stmt.exprs[j]);
                    int match = 0;
                    if (expr.type == VALTYPE_STRING || cv.type == VALTYPE_STRING) {
                        char *es = val_tostr(expr);
                        char *cs = val_tostr(cv);
                        match = strcmp(es, cs) == 0;
                        free(es);
                        free(cs);
                    } else {
                        match = val_todouble(expr) == val_todouble(cv);
                    }
                    val_free(&cv);
                    if (match) {
                        val_free(&expr);
                        return interp_eval(interp, c->as.case_stmt.body);
                    }
                }
            }
            if (node->as.select_stmt.else_case) {
                val_free(&expr);
                return interp_eval(interp, node->as.select_stmt.else_case->as.case_else.body);
            }
            val_free(&expr);
            return val_empty();
        }

        case AST_CASE_ELSE: {
            return interp_eval(interp, node->as.case_else.body);
        }

        case AST_FUNCALL: {
            int argc = node->as.funcall.argc;
            Value *args = NULL;
            if (argc > 0) {
                args = malloc(sizeof(Value) * argc);
                for (int i = 0; i < argc; i++) {
                    args[i] = interp_eval(interp, node->as.funcall.args[i]);
                }
            }
            char *name = node->as.funcall.name;
            FuncEntry *fn = interp_find_func(interp, name);
            if (fn) {
                Value r = interp_call_userfunc(interp, fn, argc, args);
                for (int i = 0; i < argc; i++) val_free(&args[i]);
                free(args);
                return r;
            }
            Variable *var = env_find(interp->current_env, name);
            if (var && var->value.type == VALTYPE_ARRAY && var->value.as.arr && argc >= 1) {
                long long i = val_toint(args[0]);
                for (int k = 0; k < argc; k++) val_free(&args[k]);
                free(args);
                if (i >= 0 && i < var->value.as.arr->total_size) {
                    return val_clone(var->value.as.arr->data[i]);
                }
                return val_empty();
            }
            Value r = builtin_call(interp, name, argc, args);
            for (int i = 0; i < argc; i++) val_free(&args[i]);
            free(args);
            return r;
        }

        case AST_CALL: {
            int argc = node->as.call.argc;
            Value *args = NULL;
            if (argc > 0) {
                args = malloc(sizeof(Value) * argc);
                for (int i = 0; i < argc; i++) {
                    args[i] = interp_eval(interp, node->as.call.args[i]);
                }
            }
            FuncEntry *fn = interp_find_func(interp, node->as.call.name);
            if (fn) {
                Value r = interp_call_userfunc(interp, fn, argc, args);
                for (int i = 0; i < argc; i++) val_free(&args[i]);
                free(args);
                val_free(&r);
                return val_empty();
            }
            Value r = builtin_call(interp, node->as.call.name, argc, args);
            for (int i = 0; i < argc; i++) val_free(&args[i]);
            free(args);
            val_free(&r);
            return val_empty();
        }

        case AST_METHOD_CALL: {
            int argc = node->as.method_call.argc;
            Value *args = NULL;
            if (argc > 0) {
                args = malloc(sizeof(Value) * argc);
                for (int i = 0; i < argc; i++) {
                    args[i] = interp_eval(interp, node->as.method_call.args[i]);
                }
            }
            Value r = val_empty();
            if (node->as.method_call.obj[0] == 0) {
                FuncEntry *fn = interp_find_func(interp, node->as.method_call.method);
                if (fn) {
                    r = interp_call_userfunc(interp, fn, argc, args);
                } else {
                    r = builtin_call(interp, node->as.method_call.method, argc, args);
                }
            } else {
                Value obj = interp_get_var(interp, node->as.method_call.obj);
                if (obj.type == VALTYPE_OBJECT && obj.as.obj && obj.as.obj->call_method) {
                    r = obj.as.obj->call_method(obj.as.obj, interp,
                        node->as.method_call.method, argc, args);
                } else {
                    interp_error(interp, "对象不支持方法: %s", node->as.method_call.method);
                }
                val_free(&obj);
            }
            for (int i = 0; i < argc; i++) val_free(&args[i]);
            free(args);
            return r;
        }

        case AST_PROP_GET: {
            Value obj = interp_get_var(interp, node->as.prop_get.name);
            Value r = val_empty();
            if (obj.type == VALTYPE_OBJECT && obj.as.obj && obj.as.obj->get_prop) {
                r = obj.as.obj->get_prop(obj.as.obj, node->as.prop_get.prop);
                if (r.type == VALTYPE_EMPTY && obj.as.obj->call_method) {
                    val_free(&r);
                    r = obj.as.obj->call_method(obj.as.obj, interp,
                        node->as.prop_get.prop, 0, NULL);
                }
            }
            val_free(&obj);
            return r;
        }

        case AST_NEW_EXPR: {
            if (strcasecmp(node->as.new_expr.class_name, "FileSystemObject") == 0 ||
                strcasecmp(node->as.new_expr.class_name, "Scripting.FileSystemObject") == 0) {
                return val_obj(fso_create());
            }
            if (strcasecmp(node->as.new_expr.class_name, "Dictionary") == 0 ||
                strcasecmp(node->as.new_expr.class_name, "Scripting.Dictionary") == 0) {
                return val_obj(dict_create());
            }
            interp_error(interp, "未知的类: %s", node->as.new_expr.class_name);
            return val_empty();
        }

        case AST_ON_ERROR: {
            interp->on_error_resume = (node->as.on_error.mode == 1);
            return val_empty();
        }

        case AST_OPTION_EXPLICIT: {
            interp->option_explicit = 1;
            return val_empty();
        }

        case AST_RANDOMIZE: {
            srand(time(NULL));
            return val_empty();
        }

        case AST_ERASE: {
            interp_set_var(interp, node->as.erase.name, val_empty(), 0);
            return val_empty();
        }

        case AST_REDIM: {
            int size = 1;
            for (int i = 0; i < node->as.redim.dims_count; i++) {
                size *= (node->as.redim.dims[i] + 1);
            }
            VbsArray *a = arr_new_1d(size);
            interp_set_var(interp, node->as.redim.name, val_arr(a), 0);
            return val_empty();
        }

        case AST_REM_STMT: {
            return val_empty();
        }

        case AST_FUNC_DECL:
        case AST_SUB_DECL: {
            interp_register_func(interp, node->as.func_decl.name, node);
            return val_empty();
        }

        case AST_EXIT_FUNC:
        case AST_EXIT_SUB: {
            interp->exiting_func = 1;
            return val_empty();
        }

        case AST_INDEX_GET: {
            Value arr = interp_get_var(interp, node->as.index_get.name);
            Value idx = interp_eval(interp, node->as.index_get.index);
            Value r = val_empty();
            if (arr.type == VALTYPE_ARRAY && arr.as.arr) {
                long long i = val_toint(idx);
                if (i >= 0 && i < arr.as.arr->total_size) {
                    r = val_clone(arr.as.arr->data[i]);
                }
            }
            val_free(&arr);
            val_free(&idx);
            return r;
        }

        default:
            return val_empty();
    }
}

int interp_run(Interp *interp) {
    interp_eval(interp, interp->program);
    if (interp->error_occured && !interp->on_error_resume) {
        return 1;
    }
    return interp->exit_code;
}