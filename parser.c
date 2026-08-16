#include "vbs.h"

static AstNode *ast_alloc(AstType type, int line) {
    AstNode *n = calloc(1, sizeof(AstNode));
    if (!n) { fprintf(stderr, "Out of memory\n"); exit(1); }
    n->type = type;
    n->line = line;
    return n;
}

static void parser_error(Parser *p, const char *fmt, ...) {
    if (p->error) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->error_msg, sizeof(p->error_msg), fmt, ap);
    va_end(ap);
    p->error = 1;
}

static Token expect(Parser *p, TokenType type) {
    Token t = lexer_next(p->lexer);
    if (t.type != type) {
        parser_error(p, "第%d行: 期望 %s, 得到 %s", t.line,
            token_type_name(type), token_type_name(t.type));
    }
    return t;
}

static Token expect_ident(Parser *p) {
    Token t = lexer_next(p->lexer);
    if (t.type != TOK_IDENT && t.type != TOK_KEY_TRUE && t.type != TOK_KEY_FALSE) {
        parser_error(p, "第%d行: 期望标识符, 得到 %s", t.line, token_type_name(t.type));
    }
    return t;
}

static AstNode *parse_stmt(Parser *p);
static AstNode *parse_expr(Parser *p);
static AstNode *parse_assign_expr(Parser *p);
static AstNode *parse_block(Parser *p, TokenType end_token, const char *end_name);

/* ========== 解析表达式 ========== */
static AstNode *parse_primary(Parser *p) {
    Token t = lexer_next(p->lexer);
    AstNode *n = NULL;

    switch (t.type) {
        case TOK_NUMBER: {
            n = ast_alloc(AST_LITERAL, t.line);
            char *end;
            if (strchr(t.text, '.') || strchr(t.text, 'e') || strchr(t.text, 'E')) {
                n->as.literal.value = val_double(strtod(t.text, &end));
            } else {
                n->as.literal.value = val_int(strtoll(t.text, &end, 0));
            }
            break;
        }
        case TOK_STRING: {
            n = ast_alloc(AST_LITERAL, t.line);
            n->as.literal.value = val_str(t.text ? t.text : "");
            break;
        }
        case TOK_KEY_TRUE: {
            n = ast_alloc(AST_LITERAL, t.line);
            n->as.literal.value = val_bool(1);
            break;
        }
        case TOK_KEY_FALSE: {
            n = ast_alloc(AST_LITERAL, t.line);
            n->as.literal.value = val_bool(0);
            break;
        }
        case TOK_KEY_NULL: {
            n = ast_alloc(AST_LITERAL, t.line);
            n->as.literal.value = val_null();
            break;
        }
        case TOK_KEY_EMPTY: {
            n = ast_alloc(AST_LITERAL, t.line);
            n->as.literal.value = val_empty();
            break;
        }
        case TOK_KEY_NOTHING: {
            n = ast_alloc(AST_LITERAL, t.line);
            n->as.literal.value = val_nothing();
            break;
        }
        case TOK_IDENT: {
            n = ast_alloc(AST_IDENT, t.line);
            n->as.ident.name = strdup(t.text);
            break;
        }
        case TOK_LPAREN: {
            n = parse_expr(p);
            expect(p, TOK_RPAREN);
            break;
        }
        case TOK_KEY_NEW: {
            n = ast_alloc(AST_NEW_EXPR, t.line);
            Token cls = expect_ident(p);
            n->as.new_expr.class_name = strdup(cls.text);
            n->as.new_expr.args = NULL;
            n->as.new_expr.argc = 0;
            break;
        }
        case TOK_MINUS: {
            n = ast_alloc(AST_UNOP, t.line);
            n->as.unop.op = TOK_MINUS;
            n->as.unop.operand = parse_primary(p);
            break;
        }
        case TOK_KEY_NOT: {
            n = ast_alloc(AST_UNOP, t.line);
            n->as.unop.op = TOK_KEY_NOT;
            n->as.unop.operand = parse_primary(p);
            break;
        }
        default:
            parser_error(p, "第%d行: 意外的标记 %s", t.line, token_type_name(t.type));
            n = ast_alloc(AST_LITERAL, t.line);
            n->as.literal.value = val_empty();
    }
    return n;
}

static AstNode *parse_postfix(Parser *p, AstNode *left) {
    while (1) {
        Token t = lexer_peek(p->lexer);
        if (t.type == TOK_DOT) {
            lexer_next(p->lexer);
            AstNode *prop = ast_alloc(AST_PROP_GET, t.line);
            if (left->type == AST_IDENT) {
                prop->as.prop_get.name = strdup(left->as.ident.name);
            } else {
                prop->as.prop_get.name = NULL;
            }
            Token pname = lexer_next(p->lexer);
            if (pname.type != TOK_IDENT) {
                parser_error(p, "第%d行: 期望属性名", pname.line);
                free(prop);
                return left;
            }
            prop->as.prop_get.prop = strdup(pname.text);

            if (lexer_peek(p->lexer).type == TOK_LPAREN) {
                AstNode *call = ast_alloc(AST_METHOD_CALL, t.line);
                if (left->type == AST_IDENT) {
                    call->as.method_call.obj = strdup(left->as.ident.name);
                } else {
                    call->as.method_call.obj = strdup("");
                }
                call->as.method_call.method = strdup(pname.text);
                lexer_next(p->lexer);
                int cap = 8;
                call->as.method_call.args = malloc(sizeof(AstNode*) * cap);
                call->as.method_call.argc = 0;
                if (lexer_peek(p->lexer).type != TOK_RPAREN) {
                    call->as.method_call.args[call->as.method_call.argc++] = parse_expr(p);
                    while (lexer_peek(p->lexer).type == TOK_COMMA) {
                        lexer_next(p->lexer);
                        if (call->as.method_call.argc >= cap) {
                            cap *= 2;
                            call->as.method_call.args = realloc(call->as.method_call.args, sizeof(AstNode*) * cap);
                        }
                        call->as.method_call.args[call->as.method_call.argc++] = parse_expr(p);
                    }
                }
                expect(p, TOK_RPAREN);
                left = call;
            } else {
                left = prop;
            }
        } else if (t.type == TOK_LPAREN) {
            if (left->type == AST_IDENT) {
                AstNode *call = ast_alloc(AST_FUNCALL, t.line);
                call->as.funcall.name = strdup(left->as.ident.name);
                lexer_next(p->lexer);
                int cap = 8;
                call->as.funcall.args = malloc(sizeof(AstNode*) * cap);
                call->as.funcall.argc = 0;
                if (lexer_peek(p->lexer).type != TOK_RPAREN) {
                    call->as.funcall.args[call->as.funcall.argc++] = parse_expr(p);
                    while (lexer_peek(p->lexer).type == TOK_COMMA) {
                        lexer_next(p->lexer);
                        if (call->as.funcall.argc >= cap) {
                            cap *= 2;
                            call->as.funcall.args = realloc(call->as.funcall.args, sizeof(AstNode*) * cap);
                        }
                        call->as.funcall.args[call->as.funcall.argc++] = parse_expr(p);
                    }
                }
                expect(p, TOK_RPAREN);
                left = call;
            } else {
                break;
            }
        } else if (t.type == TOK_LBRACKET) {
            lexer_next(p->lexer);
            AstNode *idx = ast_alloc(AST_INDEX_GET, t.line);
            if (left->type == AST_IDENT) {
                idx->as.index_get.name = strdup(left->as.ident.name);
            } else {
                idx->as.index_get.name = NULL;
            }
            idx->as.index_get.index = parse_expr(p);
            expect(p, TOK_RBRACKET);
            left = idx;
        } else {
            break;
        }
    }
    return left;
}

static AstNode *parse_power(Parser *p) {
    AstNode *left = parse_primary(p);
    left = parse_postfix(p, left);
    if (lexer_peek(p->lexer).type == TOK_POW) {
        lexer_next(p->lexer);
        AstNode *n = ast_alloc(AST_BINOP, p->lexer->cur.line);
        n->as.binop.op = TOK_POW;
        n->as.binop.left = left;
        n->as.binop.right = parse_power(p);
        return n;
    }
    return left;
}

static AstNode *parse_unary(Parser *p) {
    Token t = lexer_peek(p->lexer);
    if (t.type == TOK_MINUS || t.type == TOK_KEY_NOT) {
        lexer_next(p->lexer);
        AstNode *n = ast_alloc(AST_UNOP, t.line);
        n->as.unop.op = t.type;
        n->as.unop.operand = parse_unary(p);
        return n;
    }
    return parse_power(p);
}

static AstNode *parse_mul(Parser *p) {
    AstNode *left = parse_unary(p);
    while (1) {
        Token t = lexer_peek(p->lexer);
        if (t.type == TOK_MUL || t.type == TOK_DIV || t.type == TOK_INTDIV || t.type == TOK_MOD) {
            lexer_next(p->lexer);
            AstNode *n = ast_alloc(AST_BINOP, t.line);
            n->as.binop.op = t.type;
            n->as.binop.left = left;
            n->as.binop.right = parse_unary(p);
            left = n;
        } else break;
    }
    return left;
}

static AstNode *parse_add(Parser *p) {
    AstNode *left = parse_mul(p);
    while (1) {
        Token t = lexer_peek(p->lexer);
        if (t.type == TOK_PLUS || t.type == TOK_MINUS || t.type == TOK_CONCAT) {
            lexer_next(p->lexer);
            AstNode *n = ast_alloc(AST_BINOP, t.line);
            n->as.binop.op = t.type;
            n->as.binop.left = left;
            n->as.binop.right = parse_mul(p);
            left = n;
        } else break;
    }
    return left;
}

static AstNode *parse_compare(Parser *p) {
    AstNode *left = parse_add(p);
    while (1) {
        Token t = lexer_peek(p->lexer);
        if (t.type == TOK_EQ || t.type == TOK_NEQ || t.type == TOK_LT ||
            t.type == TOK_GT || t.type == TOK_LE || t.type == TOK_GE || t.type == TOK_IS) {
            lexer_next(p->lexer);
            AstNode *n = ast_alloc(AST_BINOP, t.line);
            n->as.binop.op = t.type;
            n->as.binop.left = left;
            n->as.binop.right = parse_add(p);
            left = n;
        } else break;
    }
    return left;
}

static AstNode *parse_and(Parser *p) {
    AstNode *left = parse_compare(p);
    while (lexer_peek(p->lexer).type == TOK_KEY_AND) {
        lexer_next(p->lexer);
        AstNode *n = ast_alloc(AST_BINOP, p->lexer->cur.line);
        n->as.binop.op = TOK_KEY_AND;
        n->as.binop.left = left;
        n->as.binop.right = parse_compare(p);
        left = n;
    }
    return left;
}

static AstNode *parse_or(Parser *p) {
    AstNode *left = parse_and(p);
    while (lexer_peek(p->lexer).type == TOK_KEY_OR) {
        lexer_next(p->lexer);
        AstNode *n = ast_alloc(AST_BINOP, p->lexer->cur.line);
        n->as.binop.op = TOK_KEY_OR;
        n->as.binop.left = left;
        n->as.binop.right = parse_and(p);
        left = n;
    }
    return left;
}

static AstNode *parse_xor(Parser *p) {
    AstNode *left = parse_or(p);
    while (lexer_peek(p->lexer).type == TOK_KEY_XOR) {
        lexer_next(p->lexer);
        AstNode *n = ast_alloc(AST_BINOP, p->lexer->cur.line);
        n->as.binop.op = TOK_KEY_XOR;
        n->as.binop.left = left;
        n->as.binop.right = parse_or(p);
        left = n;
    }
    return left;
}

static AstNode *parse_expr(Parser *p) {
    return parse_xor(p);
}

static int is_stmt_terminator(TokenType t) {
    switch (t) {
        case TOK_NEWLINE: case TOK_LINE_CONTINUE: case TOK_EOF:
        case TOK_ASSIGN: case TOK_COMMA: case TOK_COLON:
        case TOK_RPAREN: case TOK_KEY_NEXT: case TOK_KEY_END_IF:
        case TOK_KEY_ELSE: case TOK_KEY_ELSEIF: case TOK_KEY_CASE:
        case TOK_KEY_END_SELECT: case TOK_KEY_LOOP: case TOK_KEY_WEND:
        case TOK_KEY_END_FUNCTION: case TOK_KEY_END_SUB:
        case TOK_KEY_END_CLASS: case TOK_KEY_THEN:
            return 1;
        default:
            return 0;
    }
}

static AstNode *parse_assign_expr(Parser *p) {
    AstNode *left = parse_expr(p);
    Token t = lexer_peek(p->lexer);

    if ((left->type == AST_PROP_GET || left->type == AST_IDENT) && !is_stmt_terminator(t.type)) {
        AstNode *call = ast_alloc(AST_METHOD_CALL, t.line);
        if (left->type == AST_PROP_GET) {
            call->as.method_call.obj = left->as.prop_get.name ?
                strdup(left->as.prop_get.name) : strdup("");
            call->as.method_call.method = strdup(left->as.prop_get.prop);
        } else {
            call->as.method_call.obj = strdup("");
            call->as.method_call.method = strdup(left->as.ident.name);
        }
        int cap = 8;
        call->as.method_call.args = malloc(sizeof(AstNode*) * cap);
        call->as.method_call.argc = 0;
        call->as.method_call.args[call->as.method_call.argc++] = parse_expr(p);
        while (lexer_peek(p->lexer).type == TOK_COMMA) {
            lexer_next(p->lexer);
            if (call->as.method_call.argc >= cap) {
                cap *= 2;
                call->as.method_call.args = realloc(call->as.method_call.args, sizeof(AstNode*) * cap);
            }
            call->as.method_call.args[call->as.method_call.argc++] = parse_expr(p);
        }
        ast_free(left);
        return call;
    }

    if (t.type == TOK_ASSIGN) {
        lexer_next(p->lexer);
        if (left->type == AST_IDENT) {
            AstNode *n = ast_alloc(AST_ASSIGN, t.line);
            n->as.assign.name = strdup(left->as.ident.name);
            n->as.assign.value = parse_assign_expr(p);
            ast_free(left);
            return n;
        } else if (left->type == AST_INDEX_GET) {
            AstNode *n = ast_alloc(AST_ASSIGN, t.line);
            char buf[256];
            snprintf(buf, sizeof(buf), "%s[]", left->as.index_get.name);
            n->as.assign.name = strdup(buf);
            n->as.assign.value = parse_assign_expr(p);
            ast_free(left);
            return n;
        } else if (left->type == AST_PROP_GET) {
            AstNode *n = ast_alloc(AST_ASSIGN, t.line);
            char buf[256];
            snprintf(buf, sizeof(buf), "%s.%s", left->as.prop_get.name, left->as.prop_get.prop);
            n->as.assign.name = strdup(buf);
            n->as.assign.value = parse_assign_expr(p);
            ast_free(left);
            return n;
        } else {
            parser_error(p, "第%d行: 无效的赋值目标", t.line);
            ast_free(left);
            AstNode *n = ast_alloc(AST_LITERAL, t.line);
            n->as.literal.value = val_empty();
            return n;
        }
    }
    return left;
}

/* ========== 解析语句 ========== */
static AstNode *parse_var_decl(Parser *p) {
    AstNode *n = ast_alloc(AST_VAR_DECL, p->lexer->cur.line);
    int cap = 8;
    n->as.var_decl.names = malloc(sizeof(char*) * cap);
    n->as.var_decl.count = 0;
    n->as.var_decl.init_expr = NULL;

    do {
        lexer_skip_newlines(p->lexer);
        Token t = lexer_next(p->lexer);
        if (t.type != TOK_IDENT) {
            parser_error(p, "第%d行: 期望变量名", t.line);
            break;
        }
        if (n->as.var_decl.count >= cap) {
            cap *= 2;
            n->as.var_decl.names = realloc(n->as.var_decl.names, sizeof(char*) * cap);
        }
        n->as.var_decl.names[n->as.var_decl.count++] = strdup(t.text);
    } while (lexer_peek(p->lexer).type == TOK_COMMA && (lexer_next(p->lexer), 1));

    return n;
}

static AstNode *parse_const_decl(Parser *p) {
    AstNode *n = ast_alloc(AST_CONST_DECL, p->lexer->cur.line);
    int cap = 8;
    n->as.const_decl.names = malloc(sizeof(char*) * cap);
    n->as.const_decl.count = 0;
    n->as.const_decl.init_expr = NULL;

    do {
        lexer_skip_newlines(p->lexer);
        Token t = lexer_next(p->lexer);
        if (t.type != TOK_IDENT) {
            parser_error(p, "第%d行: 期望常量名", t.line);
            break;
        }
        if (n->as.const_decl.count >= cap) {
            cap *= 2;
            n->as.const_decl.names = realloc(n->as.const_decl.names, sizeof(char*) * cap);
        }
        n->as.const_decl.names[n->as.const_decl.count++] = strdup(t.text);
        lexer_skip_newlines(p->lexer);
        expect(p, TOK_ASSIGN);
        lexer_skip_newlines(p->lexer);
        AstNode *expr = parse_expr(p);
        if (n->as.const_decl.init_expr == NULL) {
            n->as.const_decl.init_expr = expr;
        }
    } while (lexer_peek(p->lexer).type == TOK_COMMA && (lexer_next(p->lexer), 1));

    return n;
}

static AstNode *parse_if_block(Parser *p) {
    AstNode *n = ast_alloc(AST_BLOCK, p->lexer->cur.line);
    int cap = 32;
    n->as.block.stmts = malloc(sizeof(AstNode*) * cap);
    n->as.block.count = 0;
    while (!p->error) {
        lexer_skip_newlines(p->lexer);
        Token t = lexer_peek(p->lexer);
        if (t.type == TOK_EOF || t.type == TOK_KEY_END_IF ||
            t.type == TOK_KEY_ELSE || t.type == TOK_KEY_ELSEIF) break;
        if (n->as.block.count >= cap) {
            cap *= 2;
            n->as.block.stmts = realloc(n->as.block.stmts, sizeof(AstNode*) * cap);
        }
        n->as.block.stmts[n->as.block.count++] = parse_stmt(p);
    }
    return n;
}

static AstNode *parse_if_stmt(Parser *p) {
    AstNode *n = ast_alloc(AST_IF, p->lexer->cur.line);
    n->as.if_stmt.cond = parse_expr(p);
    lexer_skip_newlines(p->lexer);
    expect(p, TOK_KEY_THEN);

    int ecap = 8;
    n->as.if_stmt.elseifs = malloc(sizeof(AstNode*) * ecap);
    n->as.if_stmt.elseif_count = 0;
    n->as.if_stmt.else_body = NULL;

    if (lexer_peek(p->lexer).type == TOK_NEWLINE) {
        lexer_next(p->lexer);
        n->as.if_stmt.body = parse_if_block(p);
    } else {
        AstNode *single = ast_alloc(AST_BLOCK, p->lexer->cur.line);
        single->as.block.stmts = malloc(sizeof(AstNode*) * 2);
        single->as.block.count = 0;
        single->as.block.stmts[single->as.block.count++] = parse_stmt(p);
        if (lexer_peek(p->lexer).type == TOK_KEY_ELSE) {
            lexer_next(p->lexer);
            single->as.block.stmts[single->as.block.count++] = parse_stmt(p);
        }
        n->as.if_stmt.body = single;
    }

    while (!p->error) {
        lexer_skip_newlines(p->lexer);
        Token t = lexer_peek(p->lexer);
        if (t.type == TOK_KEY_ELSEIF) {
            lexer_next(p->lexer);
            AstNode *ei = ast_alloc(AST_IF, t.line);
            ei->as.if_stmt.cond = parse_expr(p);
            lexer_skip_newlines(p->lexer);
            expect(p, TOK_KEY_THEN);
            if (lexer_peek(p->lexer).type == TOK_NEWLINE) {
                lexer_next(p->lexer);
                ei->as.if_stmt.body = parse_if_block(p);
            } else {
                ei->as.if_stmt.body = parse_stmt(p);
            }
            ei->as.if_stmt.elseifs = NULL;
            ei->as.if_stmt.elseif_count = 0;
            ei->as.if_stmt.else_body = NULL;
            if (n->as.if_stmt.elseif_count >= ecap) {
                ecap *= 2;
                n->as.if_stmt.elseifs = realloc(n->as.if_stmt.elseifs, sizeof(AstNode*) * ecap);
            }
            n->as.if_stmt.elseifs[n->as.if_stmt.elseif_count++] = ei;
        } else if (t.type == TOK_KEY_ELSE) {
            lexer_next(p->lexer);
            lexer_skip_newlines(p->lexer);
            if (lexer_peek(p->lexer).type == TOK_NEWLINE) {
                lexer_next(p->lexer);
                n->as.if_stmt.else_body = parse_if_block(p);
            } else {
                n->as.if_stmt.else_body = parse_stmt(p);
            }
        } else if (t.type == TOK_KEY_END_IF) {
            lexer_next(p->lexer);
            break;
        } else {
            break;
        }
    }

    return n;
}

static AstNode *parse_for_stmt(Parser *p) {
    AstNode *n = ast_alloc(AST_FOR, p->lexer->cur.line);
    Token var = expect_ident(p);
    n->as.for_stmt.var = strdup(var.text);
    expect(p, TOK_ASSIGN);
    n->as.for_stmt.start = parse_expr(p);
    lexer_skip_newlines(p->lexer);
    expect(p, TOK_KEY_TO);
    lexer_skip_newlines(p->lexer);
    n->as.for_stmt.end = parse_expr(p);
    lexer_skip_newlines(p->lexer);
    if (lexer_peek(p->lexer).type == TOK_KEY_STEP) {
        lexer_next(p->lexer);
        lexer_skip_newlines(p->lexer);
        n->as.for_stmt.step = parse_expr(p);
    } else {
        n->as.for_stmt.step = NULL;
    }
    lexer_skip_newlines(p->lexer);
    n->as.for_stmt.body = parse_block(p, TOK_KEY_NEXT, "Next");
    expect(p, TOK_KEY_NEXT);
    if (lexer_peek(p->lexer).type == TOK_IDENT) {
        lexer_next(p->lexer);
    }
    return n;
}

static AstNode *parse_do_loop(Parser *p) {
    AstNode *n = ast_alloc(AST_DO_LOOP, p->lexer->cur.line);
    n->as.do_loop.pre_test = 0;
    n->as.do_loop.until = 0;
    n->as.do_loop.cond = NULL;

    if (lexer_peek(p->lexer).type == TOK_KEY_WHILE) {
        lexer_next(p->lexer);
        n->as.do_loop.pre_test = 1;
        n->as.do_loop.cond = parse_expr(p);
    } else if (lexer_peek(p->lexer).type == TOK_KEY_UNTIL) {
        lexer_next(p->lexer);
        n->as.do_loop.pre_test = 1;
        n->as.do_loop.until = 1;
        n->as.do_loop.cond = parse_expr(p);
    }
    lexer_skip_newlines(p->lexer);
    n->as.do_loop.body = parse_block(p, TOK_KEY_LOOP, "Loop");
    expect(p, TOK_KEY_LOOP);
    if (lexer_peek(p->lexer).type == TOK_KEY_WHILE) {
        lexer_next(p->lexer);
        n->as.do_loop.pre_test = 0;
        n->as.do_loop.cond = parse_expr(p);
    } else if (lexer_peek(p->lexer).type == TOK_KEY_UNTIL) {
        lexer_next(p->lexer);
        n->as.do_loop.pre_test = 0;
        n->as.do_loop.until = 1;
        n->as.do_loop.cond = parse_expr(p);
    }
    return n;
}

static AstNode *parse_while_wend(Parser *p) {
    AstNode *n = ast_alloc(AST_WHILE_WEND, p->lexer->cur.line);
    n->as.while_wend.cond = parse_expr(p);
    lexer_skip_newlines(p->lexer);
    n->as.while_wend.body = parse_block(p, TOK_KEY_WEND, "WEnd");
    expect(p, TOK_KEY_WEND);
    return n;
}

static AstNode *parse_select(Parser *p) {
    AstNode *n = ast_alloc(AST_SELECT, p->lexer->cur.line);
    expect(p, TOK_KEY_CASE);
    n->as.select_stmt.expr = parse_expr(p);
    lexer_skip_newlines(p->lexer);

    int cap = 8;
    n->as.select_stmt.cases = malloc(sizeof(AstNode*) * cap);
    n->as.select_stmt.case_count = 0;
    n->as.select_stmt.else_case = NULL;

    while (lexer_peek(p->lexer).type != TOK_KEY_END_SELECT && !p->error) {
        lexer_skip_newlines(p->lexer);
        if (lexer_peek(p->lexer).type == TOK_KEY_CASE) {
            lexer_next(p->lexer);
            if (lexer_peek(p->lexer).type == TOK_KEY_ELSE) {
                lexer_next(p->lexer);
                lexer_skip_newlines(p->lexer);
                n->as.select_stmt.else_case = ast_alloc(AST_CASE_ELSE, p->lexer->cur.line);
                n->as.select_stmt.else_case->as.case_else.body = parse_block(p, 0, NULL);
                continue;
            }
            AstNode *c = ast_alloc(AST_CASE, p->lexer->cur.line);
            int ecap = 8;
            c->as.case_stmt.exprs = malloc(sizeof(AstNode*) * ecap);
            c->as.case_stmt.count = 0;
            c->as.case_stmt.exprs[c->as.case_stmt.count++] = parse_expr(p);
            while (lexer_peek(p->lexer).type == TOK_COMMA) {
                lexer_next(p->lexer);
                if (c->as.case_stmt.count >= ecap) {
                    ecap *= 2;
                    c->as.case_stmt.exprs = realloc(c->as.case_stmt.exprs, sizeof(AstNode*) * ecap);
                }
                c->as.case_stmt.exprs[c->as.case_stmt.count++] = parse_expr(p);
            }
            lexer_skip_newlines(p->lexer);
            c->as.case_stmt.body = parse_block(p, 0, NULL);
            if (n->as.select_stmt.case_count >= cap) {
                cap *= 2;
                n->as.select_stmt.cases = realloc(n->as.select_stmt.cases, sizeof(AstNode*) * cap);
            }
            n->as.select_stmt.cases[n->as.select_stmt.case_count++] = c;
        } else if (lexer_peek(p->lexer).type == TOK_KEY_ELSE) {
            lexer_next(p->lexer);
            lexer_skip_newlines(p->lexer);
            n->as.select_stmt.else_case = ast_alloc(AST_CASE_ELSE, p->lexer->cur.line);
            n->as.select_stmt.else_case->as.case_else.body = parse_block(p, 0, NULL);
        } else break;
    }
    expect(p, TOK_KEY_END_SELECT);
    return n;
}

static AstNode *parse_func_decl(Parser *p, int is_function) {
    AstNode *n = ast_alloc(is_function ? AST_FUNC_DECL : AST_SUB_DECL, p->lexer->cur.line);
    Token name = expect_ident(p);
    n->as.func_decl.name = strdup(name.text);
    n->as.func_decl.is_function = is_function;

    expect(p, TOK_LPAREN);
    int cap = 8;
    n->as.func_decl.params = malloc(sizeof(char*) * cap);
    n->as.func_decl.param_count = 0;
    if (lexer_peek(p->lexer).type != TOK_RPAREN) {
        Token pn = expect_ident(p);
        n->as.func_decl.params[n->as.func_decl.param_count++] = strdup(pn.text);
        while (lexer_peek(p->lexer).type == TOK_COMMA) {
            lexer_next(p->lexer);
            if (n->as.func_decl.param_count >= cap) {
                cap *= 2;
                n->as.func_decl.params = realloc(n->as.func_decl.params, sizeof(char*) * cap);
            }
            pn = expect_ident(p);
            n->as.func_decl.params[n->as.func_decl.param_count++] = strdup(pn.text);
        }
    }
    expect(p, TOK_RPAREN);
    lexer_skip_newlines(p->lexer);

    n->as.func_decl.body = parse_block(p, is_function ? TOK_KEY_END_FUNCTION : TOK_KEY_END_SUB,
        is_function ? "End Function" : "End Sub");
    expect(p, is_function ? TOK_KEY_END_FUNCTION : TOK_KEY_END_SUB);
    return n;
}

static AstNode *parse_with(Parser *p) {
    AstNode *n = ast_alloc(AST_WITH, p->lexer->cur.line);
    n->as.with_stmt.obj_expr = parse_expr(p);
    lexer_skip_newlines(p->lexer);
    n->as.with_stmt.body = parse_block(p, TOK_KEY_END_SELECT, "End With");
    expect(p, TOK_KEY_END_SELECT);
    return n;
}

static AstNode *parse_block(Parser *p, TokenType end_token, const char *end_name) {
    AstNode *n = ast_alloc(AST_BLOCK, p->lexer->cur.line);
    int cap = 32;
    n->as.block.stmts = malloc(sizeof(AstNode*) * cap);
    n->as.block.count = 0;

    while (!p->error) {
        lexer_skip_newlines(p->lexer);
        Token t = lexer_peek(p->lexer);
        if (t.type == TOK_EOF) break;
        if (end_token != 0 && t.type == end_token) break;
        if (end_token == 0 && (t.type == TOK_KEY_CASE || t.type == TOK_KEY_END_SELECT ||
            t.type == TOK_KEY_ELSE || t.type == TOK_KEY_END_IF || t.type == TOK_KEY_LOOP ||
            t.type == TOK_KEY_WEND || t.type == TOK_KEY_NEXT || t.type == TOK_KEY_END_FUNCTION ||
            t.type == TOK_KEY_END_SUB || t.type == TOK_KEY_END_CLASS)) break;

        if (n->as.block.count >= cap) {
            cap *= 2;
            n->as.block.stmts = realloc(n->as.block.stmts, sizeof(AstNode*) * cap);
        }
        n->as.block.stmts[n->as.block.count++] = parse_stmt(p);
    }

    if (n->as.block.count == 0) {
        free(n->as.block.stmts);
        n->as.block.stmts = NULL;
    }
    return n;
}

static AstNode *parse_stmt(Parser *p) {
    lexer_skip_newlines(p->lexer);
    Token t = lexer_peek(p->lexer);

    switch (t.type) {
        case TOK_KEY_REM: {
            lexer_next(p->lexer);
            AstNode *n = ast_alloc(AST_REM_STMT, t.line);
            char buf[4096];
            int pos = 0;
            while (p->lexer->pos < p->lexer->len && p->lexer->src[p->lexer->pos] != '\n') {
                buf[pos++] = p->lexer->src[p->lexer->pos++];
                p->lexer->col++;
            }
            buf[pos] = 0;
            n->as.rem_stmt.text = strdup(buf);
            return n;
        }
        case TOK_KEY_DIM: {
            lexer_next(p->lexer);
            return parse_var_decl(p);
        }
        case TOK_KEY_CONST: {
            lexer_next(p->lexer);
            return parse_const_decl(p);
        }
        case TOK_KEY_IF: {
            lexer_next(p->lexer);
            return parse_if_stmt(p);
        }
        case TOK_KEY_FOR: {
            lexer_next(p->lexer);
            return parse_for_stmt(p);
        }
        case TOK_KEY_DO: {
            lexer_next(p->lexer);
            return parse_do_loop(p);
        }
        case TOK_KEY_WHILE: {
            lexer_next(p->lexer);
            return parse_while_wend(p);
        }
        case TOK_KEY_SELECT: {
            lexer_next(p->lexer);
            return parse_select(p);
        }
        case TOK_KEY_FUNCTION:
        case TOK_KEY_SUB: {
            int is_func = (t.type == TOK_KEY_FUNCTION);
            lexer_next(p->lexer);
            return parse_func_decl(p, is_func);
        }
        case TOK_KEY_WITH: {
            lexer_next(p->lexer);
            return parse_with(p);
        }
        case TOK_KEY_CALL: {
            lexer_next(p->lexer);
            AstNode *n = ast_alloc(AST_CALL, t.line);
            Token name = expect_ident(p);
            n->as.call.name = strdup(name.text);
            n->as.call.args = NULL;
            n->as.call.argc = 0;
            if (lexer_peek(p->lexer).type == TOK_LPAREN) {
                lexer_next(p->lexer);
                int cap = 8;
                n->as.call.args = malloc(sizeof(AstNode*) * cap);
                if (lexer_peek(p->lexer).type != TOK_RPAREN) {
                    n->as.call.args[n->as.call.argc++] = parse_expr(p);
                    while (lexer_peek(p->lexer).type == TOK_COMMA) {
                        lexer_next(p->lexer);
                        if (n->as.call.argc >= cap) {
                            cap *= 2;
                            n->as.call.args = realloc(n->as.call.args, sizeof(AstNode*) * cap);
                        }
                        n->as.call.args[n->as.call.argc++] = parse_expr(p);
                    }
                }
                expect(p, TOK_RPAREN);
            }
            return n;
        }
        case TOK_KEY_SET: {
            lexer_next(p->lexer);
            AstNode *expr = parse_expr(p);
            lexer_skip_newlines(p->lexer);
            expect(p, TOK_ASSIGN);
            lexer_skip_newlines(p->lexer);
            AstNode *n = ast_alloc(AST_SET_ASSIGN, t.line);
            if (expr->type == AST_IDENT) {
                n->as.set_assign.name = strdup(expr->as.ident.name);
            } else {
                n->as.set_assign.name = strdup("");
                parser_error(p, "第%d行: Set 需要变量名", t.line);
            }
            n->as.set_assign.value = parse_expr(p);
            ast_free(expr);
            return n;
        }
        case TOK_KEY_ON: {
            lexer_next(p->lexer);
            AstNode *n = ast_alloc(AST_ON_ERROR, t.line);
            expect(p, TOK_KEY_ERROR);
            lexer_skip_newlines(p->lexer);
            t = lexer_next(p->lexer);
            if (t.type == TOK_KEY_RESUME) {
                lexer_skip_newlines(p->lexer);
                t = lexer_next(p->lexer);
                if (t.type == TOK_KEY_NEXT_STMT || t.type == TOK_KEY_NEXT) {
                    n->as.on_error.mode = 1;
                } else {
                    n->as.on_error.mode = 2;
                }
            } else if (t.type == TOK_KEY_GOTO) {
                t = lexer_next(p->lexer);
                n->as.on_error.mode = 3;
            } else {
                n->as.on_error.mode = 0;
            }
            return n;
        }
        case TOK_KEY_EXIT: {
            lexer_next(p->lexer);
            t = lexer_next(p->lexer);
            if (t.type == TOK_KEY_DO) {
                return ast_alloc(AST_EXIT_DO, t.line);
            } else if (t.type == TOK_KEY_FOR) {
                return ast_alloc(AST_EXIT_FOR, t.line);
            } else if (t.type == TOK_KEY_FUNCTION) {
                return ast_alloc(AST_EXIT_FUNC, t.line);
            } else if (t.type == TOK_KEY_SUB) {
                return ast_alloc(AST_EXIT_SUB, t.line);
            }
            parser_error(p, "第%d行: Exit 需要 Do/For/Function/Sub", t.line);
            return ast_alloc(AST_LITERAL, t.line);
        }
        case TOK_KEY_RANDOMIZE: {
            lexer_next(p->lexer);
            return ast_alloc(AST_RANDOMIZE, t.line);
        }
        case TOK_KEY_OPTION: {
            lexer_next(p->lexer);
            t = lexer_next(p->lexer);
            if (t.type == TOK_KEY_EXPLICIT) {
                return ast_alloc(AST_OPTION_EXPLICIT, t.line);
            }
            parser_error(p, "第%d行: Option 需要 Explicit", t.line);
            return ast_alloc(AST_LITERAL, t.line);
        }
        case TOK_KEY_ERASE: {
            lexer_next(p->lexer);
            AstNode *n = ast_alloc(AST_ERASE, t.line);
            Token name = expect_ident(p);
            n->as.erase.name = strdup(name.text);
            return n;
        }
        case TOK_KEY_REDIM: {
            lexer_next(p->lexer);
            AstNode *n = ast_alloc(AST_REDIM, t.line);
            int preserve = 0;
            if (lexer_peek(p->lexer).type == TOK_KEY_PRESERVE) {
                lexer_next(p->lexer);
                preserve = 1;
            }
            n->as.redim.preserve = preserve;
            Token name = expect_ident(p);
            n->as.redim.name = strdup(name.text);
            int dcap = 4;
            n->as.redim.dims = malloc(sizeof(int) * dcap);
            n->as.redim.dims_count = 0;
            expect(p, TOK_LPAREN);
            while (lexer_peek(p->lexer).type != TOK_RPAREN && !p->error) {
                if (n->as.redim.dims_count > 0) expect(p, TOK_COMMA);
                AstNode *d = parse_expr(p);
                if (d->type == AST_LITERAL && d->as.literal.value.type == VALTYPE_INTEGER) {
                    if (n->as.redim.dims_count >= dcap) {
                        dcap *= 2;
                        n->as.redim.dims = realloc(n->as.redim.dims, sizeof(int) * dcap);
                    }
                    n->as.redim.dims[n->as.redim.dims_count++] = (int)d->as.literal.value.as.integer;
                }
                ast_free(d);
            }
            expect(p, TOK_RPAREN);
            return n;
        }
        default: {
            return parse_assign_expr(p);
        }
    }
}

Parser *parser_new(Lexer *lx) {
    Parser *p = calloc(1, sizeof(Parser));
    if (!p) { fprintf(stderr, "Out of memory\n"); exit(1); }
    p->lexer = lx;
    p->error = 0;
    p->error_msg[0] = 0;
    return p;
}

void parser_free(Parser *p) {
    if (p->program) ast_free(p->program);
    free(p);
}

int parser_parse(Parser *p) {
    p->program = ast_alloc(AST_PROGRAM, 1);
    int cap = 64;
    p->program->as.program.stmts = malloc(sizeof(AstNode*) * cap);
    p->program->as.program.count = 0;

    while (!p->error) {
        lexer_skip_newlines(p->lexer);
        Token t = lexer_peek(p->lexer);
        if (t.type == TOK_EOF) break;

        if (p->program->as.program.count >= cap) {
            cap *= 2;
            p->program->as.program.stmts = realloc(p->program->as.program.stmts, sizeof(AstNode*) * cap);
        }
        p->program->as.program.stmts[p->program->as.program.count++] = parse_stmt(p);
    }

    if (p->error) {
        fprintf(stderr, "解析错误: %s\n", p->error_msg);
        return 0;
    }
    return 1;
}

void ast_free(AstNode *node) {
    if (!node) return;
    switch (node->type) {
        case AST_PROGRAM:
        case AST_BLOCK:
            for (int i = 0; i < node->as.block.count; i++)
                ast_free(node->as.block.stmts[i]);
            free(node->as.block.stmts);
            break;
        case AST_VAR_DECL:
            for (int i = 0; i < node->as.var_decl.count; i++)
                free(node->as.var_decl.names[i]);
            free(node->as.var_decl.names);
            if (node->as.var_decl.init_expr) ast_free(node->as.var_decl.init_expr);
            break;
        case AST_CONST_DECL:
            for (int i = 0; i < node->as.const_decl.count; i++)
                free(node->as.const_decl.names[i]);
            free(node->as.const_decl.names);
            if (node->as.const_decl.init_expr) ast_free(node->as.const_decl.init_expr);
            break;
        case AST_ASSIGN:
            free(node->as.assign.name);
            if (node->as.assign.value) ast_free(node->as.assign.value);
            break;
        case AST_SET_ASSIGN:
            free(node->as.set_assign.name);
            if (node->as.set_assign.value) ast_free(node->as.set_assign.value);
            break;
        case AST_BINOP:
            ast_free(node->as.binop.left);
            ast_free(node->as.binop.right);
            break;
        case AST_UNOP:
            ast_free(node->as.unop.operand);
            break;
        case AST_LITERAL:
            val_free(&node->as.literal.value);
            break;
        case AST_IDENT:
            free(node->as.ident.name);
            break;
        case AST_IF:
            ast_free(node->as.if_stmt.cond);
            ast_free(node->as.if_stmt.body);
            if (node->as.if_stmt.else_body) ast_free(node->as.if_stmt.else_body);
            for (int i = 0; i < node->as.if_stmt.elseif_count; i++)
                ast_free(node->as.if_stmt.elseifs[i]);
            free(node->as.if_stmt.elseifs);
            break;
        case AST_FOR:
            free(node->as.for_stmt.var);
            ast_free(node->as.for_stmt.start);
            ast_free(node->as.for_stmt.end);
            if (node->as.for_stmt.step) ast_free(node->as.for_stmt.step);
            ast_free(node->as.for_stmt.body);
            break;
        case AST_DO_LOOP:
            if (node->as.do_loop.cond) ast_free(node->as.do_loop.cond);
            ast_free(node->as.do_loop.body);
            break;
        case AST_WHILE_WEND:
            ast_free(node->as.while_wend.cond);
            ast_free(node->as.while_wend.body);
            break;
        case AST_SELECT:
            ast_free(node->as.select_stmt.expr);
            for (int i = 0; i < node->as.select_stmt.case_count; i++)
                ast_free(node->as.select_stmt.cases[i]);
            free(node->as.select_stmt.cases);
            if (node->as.select_stmt.else_case) ast_free(node->as.select_stmt.else_case);
            break;
        case AST_CASE:
            for (int i = 0; i < node->as.case_stmt.count; i++)
                ast_free(node->as.case_stmt.exprs[i]);
            free(node->as.case_stmt.exprs);
            ast_free(node->as.case_stmt.body);
            break;
        case AST_CASE_ELSE:
            ast_free(node->as.case_else.body);
            break;
        case AST_FUNC_DECL:
        case AST_SUB_DECL:
            free(node->as.func_decl.name);
            for (int i = 0; i < node->as.func_decl.param_count; i++)
                free(node->as.func_decl.params[i]);
            free(node->as.func_decl.params);
            ast_free(node->as.func_decl.body);
            break;
        case AST_CALL:
            free(node->as.call.name);
            for (int i = 0; i < node->as.call.argc; i++)
                ast_free(node->as.call.args[i]);
            free(node->as.call.args);
            break;
        case AST_FUNCALL:
            if (node->as.funcall.name) free(node->as.funcall.name);
            for (int i = 0; i < node->as.funcall.argc; i++)
                ast_free(node->as.funcall.args[i]);
            free(node->as.funcall.args);
            break;
        case AST_METHOD_CALL:
            free(node->as.method_call.obj);
            free(node->as.method_call.method);
            for (int i = 0; i < node->as.method_call.argc; i++)
                ast_free(node->as.method_call.args[i]);
            free(node->as.method_call.args);
            break;
        case AST_PROP_GET:
            if (node->as.prop_get.name) free(node->as.prop_get.name);
            free(node->as.prop_get.prop);
            break;
        case AST_INDEX_GET:
            if (node->as.index_get.name) free(node->as.index_get.name);
            ast_free(node->as.index_get.index);
            break;
        case AST_NEW_EXPR:
            free(node->as.new_expr.class_name);
            break;
        case AST_WITH:
            ast_free(node->as.with_stmt.obj_expr);
            ast_free(node->as.with_stmt.body);
            break;
        case AST_REDIM:
            free(node->as.redim.name);
            free(node->as.redim.dims);
            break;
        case AST_ON_ERROR:
            break;
        case AST_ERASE:
            free(node->as.erase.name);
            break;
        case AST_REM_STMT:
            free(node->as.rem_stmt.text);
            break;
        case AST_FOR_EACH:
            free(node->as.for_each.var);
            ast_free(node->as.for_each.expr);
            ast_free(node->as.for_each.body);
            break;
        default:
            break;
    }
    free(node);
}