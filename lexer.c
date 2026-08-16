#include "vbs.h"

static int is_vbs_ident_start(char c) {
    return isalpha(c) || c == '_';
}

static int is_vbs_ident(char c) {
    return isalnum(c) || c == '_';
}

static int is_hex_char(char c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static int is_oct_char(char c) {
    return c >= '0' && c <= '7';
}

Lexer *lexer_new(const char *src) {
    Lexer *lx = calloc(1, sizeof(Lexer));
    if (!lx) { fprintf(stderr, "Out of memory\n"); exit(1); }
    lx->src = src;
    lx->len = strlen(src);
    lx->pos = 0;
    lx->line = 1;
    lx->col = 1;
    lx->has_peek = 0;
    return lx;
}

void lexer_free(Lexer *lx) {
    if (lx->cur.text) free(lx->cur.text);
    if (lx->peek.text) free(lx->peek.text);
    free(lx);
}

static Token make_token(Lexer *lx, TokenType type, const char *text) {
    Token t;
    t.type = type;
    t.text = text ? strdup(text) : NULL;
    t.line = lx->line;
    t.col = lx->col;
    return t;
}

static Token make_token_char(Lexer *lx, TokenType type, char c) {
    char buf[2] = {c, 0};
    return make_token(lx, type, buf);
}

static Token make_keyword(Lexer *lx, TokenType type, const char *a, const char *b) {
    char *full = malloc(strlen(a) + strlen(b) + 2);
    if (!full) { fprintf(stderr, "Out of memory\n"); exit(1); }
    sprintf(full, "%s %s", a, b);
    Token t = make_token(lx, type, full);
    free(full);
    return t;
}

static void skip_whitespace(Lexer *lx) {
    while (lx->pos < lx->len && (lx->src[lx->pos] == ' ' || lx->src[lx->pos] == '\t' || lx->src[lx->pos] == '\r')) {
        if (lx->src[lx->pos] == '\t') lx->col += 4 - ((lx->col - 1) % 4);
        else lx->col++;
        lx->pos++;
    }
}

static void skip_to_eol(Lexer *lx) {
    while (lx->pos < lx->len && lx->src[lx->pos] != '\n') lx->pos++;
}

static TokenType lookup_keyword(const char *word) {
    if (strcasecmp(word, "Dim") == 0) return TOK_KEY_DIM;
    if (strcasecmp(word, "Set") == 0) return TOK_KEY_SET;
    if (strcasecmp(word, "If") == 0) return TOK_KEY_IF;
    if (strcasecmp(word, "Then") == 0) return TOK_KEY_THEN;
    if (strcasecmp(word, "Else") == 0) return TOK_KEY_ELSE;
    if (strcasecmp(word, "ElseIf") == 0) return TOK_KEY_ELSEIF;
    if (strcasecmp(word, "For") == 0) return TOK_KEY_FOR;
    if (strcasecmp(word, "Next") == 0) return TOK_KEY_NEXT;
    if (strcasecmp(word, "Do") == 0) return TOK_KEY_DO;
    if (strcasecmp(word, "While") == 0) return TOK_KEY_WHILE;
    if (strcasecmp(word, "Until") == 0) return TOK_KEY_UNTIL;
    if (strcasecmp(word, "Loop") == 0) return TOK_KEY_LOOP;
    if (strcasecmp(word, "WEnd") == 0) return TOK_KEY_WEND;
    if (strcasecmp(word, "With") == 0) return TOK_KEY_WITH;
    if (strcasecmp(word, "Function") == 0) return TOK_KEY_FUNCTION;
    if (strcasecmp(word, "Sub") == 0) return TOK_KEY_SUB;
    if (strcasecmp(word, "Call") == 0) return TOK_KEY_CALL;
    if (strcasecmp(word, "Exit") == 0) return TOK_KEY_EXIT;
    if (strcasecmp(word, "Select") == 0) return TOK_KEY_SELECT;
    if (strcasecmp(word, "Case") == 0) return TOK_KEY_CASE;
    if (strcasecmp(word, "On") == 0) return TOK_KEY_ON;
    if (strcasecmp(word, "Error") == 0) return TOK_KEY_ERROR;
    if (strcasecmp(word, "Resume") == 0) return TOK_KEY_RESUME;
    if (strcasecmp(word, "Const") == 0) return TOK_KEY_CONST;
    if (strcasecmp(word, "Public") == 0) return TOK_KEY_PUBLIC;
    if (strcasecmp(word, "Private") == 0) return TOK_KEY_PRIVATE;
    if (strcasecmp(word, "True") == 0) return TOK_KEY_TRUE;
    if (strcasecmp(word, "False") == 0) return TOK_KEY_FALSE;
    if (strcasecmp(word, "Nothing") == 0) return TOK_KEY_NOTHING;
    if (strcasecmp(word, "Null") == 0) return TOK_KEY_NULL;
    if (strcasecmp(word, "Empty") == 0) return TOK_KEY_EMPTY;
    if (strcasecmp(word, "ReDim") == 0) return TOK_KEY_REDIM;
    if (strcasecmp(word, "Preserve") == 0) return TOK_KEY_PRESERVE;
    if (strcasecmp(word, "Class") == 0) return TOK_KEY_CLASS;
    if (strcasecmp(word, "New") == 0) return TOK_KEY_NEW;
    if (strcasecmp(word, "Each") == 0) return TOK_KEY_EACH;
    if (strcasecmp(word, "In") == 0) return TOK_KEY_IN;
    if (strcasecmp(word, "To") == 0) return TOK_KEY_TO;
    if (strcasecmp(word, "Step") == 0) return TOK_KEY_STEP;
    if (strcasecmp(word, "And") == 0) return TOK_KEY_AND;
    if (strcasecmp(word, "Or") == 0) return TOK_KEY_OR;
    if (strcasecmp(word, "Not") == 0) return TOK_KEY_NOT;
    if (strcasecmp(word, "Xor") == 0) return TOK_KEY_XOR;
    if (strcasecmp(word, "Mod") == 0) return TOK_MOD;
    if (strcasecmp(word, "Randomize") == 0) return TOK_KEY_RANDOMIZE;
    if (strcasecmp(word, "Option") == 0) return TOK_KEY_OPTION;
    if (strcasecmp(word, "Explicit") == 0) return TOK_KEY_EXPLICIT;
    if (strcasecmp(word, "Erase") == 0) return TOK_KEY_ERASE;
    if (strcasecmp(word, "Type") == 0) return TOK_KEY_TYPE;
    if (strcasecmp(word, "Get") == 0) return TOK_KEY_GET;
    if (strcasecmp(word, "Let") == 0) return TOK_KEY_LET;
    if (strcasecmp(word, "Property") == 0) return TOK_KEY_PROPERTY;
    if (strcasecmp(word, "Default") == 0) return TOK_KEY_DEFAULT;
    if (strcasecmp(word, "GoTo") == 0) return TOK_KEY_GOTO;
    if (strcasecmp(word, "Is") == 0) return TOK_IS;
    if (strcasecmp(word, "Rem") == 0) return TOK_KEY_REM;
    return TOK_IDENT;
}

static Token read_string(Lexer *lx) {
    lx->pos++; lx->col++;
    char buf[65536];
    int pos = 0;
    while (lx->pos < lx->len) {
        char c = lx->src[lx->pos];
        if (c == '"') {
            lx->pos++; lx->col++;
            if (lx->pos < lx->len && lx->src[lx->pos] == '"') {
                buf[pos++] = '"';
                lx->pos++; lx->col++;
            } else {
                buf[pos] = 0;
                return make_token(lx, TOK_STRING, buf);
            }
        } else if (c == '\n') {
            buf[pos] = 0;
            return make_token(lx, TOK_STRING, buf);
        } else {
            buf[pos++] = c;
            lx->pos++; lx->col++;
            if (pos >= 65535) break;
        }
    }
    buf[pos] = 0;
    return make_token(lx, TOK_STRING, buf);
}

static Token read_number(Lexer *lx) {
    char buf[256];
    int pos = 0;

    if (lx->pos + 1 < lx->len && lx->src[lx->pos] == '&') {
        lx->pos++; lx->col++;
        if (lx->src[lx->pos] == 'H' || lx->src[lx->pos] == 'h') {
            lx->pos++; lx->col++;
            while (lx->pos < lx->len && is_hex_char(lx->src[lx->pos])) {
                buf[pos++] = lx->src[lx->pos++];
                lx->col++;
            }
            buf[pos] = 0;
            return make_token(lx, TOK_NUMBER, buf);
        } else if (lx->src[lx->pos] == 'O' || lx->src[lx->pos] == 'o') {
            lx->pos++; lx->col++;
            while (lx->pos < lx->len && is_oct_char(lx->src[lx->pos])) {
                buf[pos++] = lx->src[lx->pos++];
                lx->col++;
            }
            buf[pos] = 0;
            return make_token(lx, TOK_NUMBER, buf);
        } else {
            strcpy(buf, "0");
            return make_token(lx, TOK_NUMBER, buf);
        }
    }

    while (lx->pos < lx->len && isdigit(lx->src[lx->pos])) {
        buf[pos++] = lx->src[lx->pos++];
        lx->col++;
    }
    if (lx->pos < lx->len && lx->src[lx->pos] == '.') {
        buf[pos++] = lx->src[lx->pos++];
        lx->col++;
        while (lx->pos < lx->len && isdigit(lx->src[lx->pos])) {
            buf[pos++] = lx->src[lx->pos++];
            lx->col++;
        }
    }
    if (lx->pos < lx->len && (lx->src[lx->pos] == 'e' || lx->src[lx->pos] == 'E')) {
        buf[pos++] = lx->src[lx->pos++];
        lx->col++;
        if (lx->pos < lx->len && (lx->src[lx->pos] == '+' || lx->src[lx->pos] == '-')) {
            buf[pos++] = lx->src[lx->pos++];
            lx->col++;
        }
        while (lx->pos < lx->len && isdigit(lx->src[lx->pos])) {
            buf[pos++] = lx->src[lx->pos++];
            lx->col++;
        }
    }
    buf[pos] = 0;
    return make_token(lx, TOK_NUMBER, buf);
}

static Token read_identifier_or_keyword(Lexer *lx) {
    char buf[4096];
    int pos = 0;
    while (lx->pos < lx->len && is_vbs_ident(lx->src[lx->pos])) {
        buf[pos++] = lx->src[lx->pos++];
        lx->col++;
    }
    buf[pos] = 0;

    if (strcasecmp(buf, "End") == 0) {
        skip_whitespace(lx);
        char next[256];
        int npos = 0;
        int saved = lx->pos;
        int saved_col = lx->col;
        while (lx->pos < lx->len && is_vbs_ident(lx->src[lx->pos]) && npos < 255) {
            next[npos++] = lx->src[lx->pos++];
            lx->col++;
        }
        next[npos] = 0;
        if (strcasecmp(next, "If") == 0) {
            return make_keyword(lx, TOK_KEY_END_IF, buf, next);
        }
        if (strcasecmp(next, "Function") == 0) {
            return make_keyword(lx, TOK_KEY_END_FUNCTION, buf, next);
        }
        if (strcasecmp(next, "Sub") == 0) {
            return make_keyword(lx, TOK_KEY_END_SUB, buf, next);
        }
        if (strcasecmp(next, "Select") == 0) {
            return make_keyword(lx, TOK_KEY_END_SELECT, buf, next);
        }
        if (strcasecmp(next, "Class") == 0) {
            return make_keyword(lx, TOK_KEY_END_CLASS, buf, next);
        }
        if (strcasecmp(next, "With") == 0) {
            return make_keyword(lx, TOK_KEY_END_SELECT, buf, next);
        }
        if (strcasecmp(next, "Property") == 0) {
            return make_keyword(lx, TOK_KEY_END_PROPERTY, buf, next);
        }
        if (strcasecmp(next, "Type") == 0) {
            return make_keyword(lx, TOK_KEY_END_TYPE, buf, next);
        }
        lx->pos = saved;
        lx->col = saved_col;
        return make_token(lx, TOK_KEY_END_IF, buf);
    }

    if (strcasecmp(buf, "Exit") == 0) {
        skip_whitespace(lx);
        char next[256];
        int npos = 0;
        int saved = lx->pos;
        int saved_col = lx->col;
        while (lx->pos < lx->len && is_vbs_ident(lx->src[lx->pos]) && npos < 255) {
            next[npos++] = lx->src[lx->pos++];
            lx->col++;
        }
        next[npos] = 0;
        if (strcasecmp(next, "Do") == 0) {
            return make_keyword(lx, TOK_KEY_EXIT_DO, buf, next);
        }
        if (strcasecmp(next, "For") == 0) {
            return make_keyword(lx, TOK_KEY_EXIT_FOR, buf, next);
        }
        if (strcasecmp(next, "Function") == 0) {
            return make_keyword(lx, TOK_KEY_EXIT_FUNCTION, buf, next);
        }
        if (strcasecmp(next, "Sub") == 0) {
            return make_keyword(lx, TOK_KEY_EXIT_SUB, buf, next);
        }
        lx->pos = saved;
        lx->col = saved_col;
        return make_token(lx, TOK_KEY_EXIT, buf);
    }

    TokenType kw = lookup_keyword(buf);
    return make_token(lx, kw, buf);
}

static Token lexer_read_raw(Lexer *lx);

Token lexer_next(Lexer *lx) {
    if (lx->has_peek) {
        lx->has_peek = 0;
        Token t = lx->peek;
        lx->peek.text = NULL;
        return t;
    }
    return lexer_read_raw(lx);
}

static Token lexer_read_raw(Lexer *lx) {
    skip_whitespace(lx);

    if (lx->pos >= lx->len) {
        return make_token(lx, TOK_EOF, NULL);
    }

    char c = lx->src[lx->pos];

    if (c == '\'') {
        skip_to_eol(lx);
        return lexer_read_raw(lx);
    }

    if (c == '\n') {
        lx->pos++; lx->line++; lx->col = 1;
        return make_token(lx, TOK_NEWLINE, NULL);
    }

    if (c == '"') {
        return read_string(lx);
    }

    if (c == '&') {
        if (lx->pos + 1 < lx->len && lx->src[lx->pos + 1] == '&') {
            lx->pos += 2; lx->col += 2;
            return make_token(lx, TOK_KEY_AND, "And");
        } else {
            lx->pos++; lx->col++;
            return make_token_char(lx, TOK_CONCAT, '&');
        }
    }

    if (c == '_' && lx->pos + 1 < lx->len && lx->src[lx->pos + 1] == '\n') {
        lx->pos += 2; lx->line++; lx->col = 1;
        return make_token(lx, TOK_LINE_CONTINUE, NULL);
    }

    if (isdigit(c) || c == '.') {
        if (c == '.' && lx->pos + 1 < lx->len && !isdigit(lx->src[lx->pos + 1])) {
            lx->pos++; lx->col++;
            return make_token_char(lx, TOK_DOT, '.');
        }
        return read_number(lx);
    }

    if (is_vbs_ident_start(c)) {
        return read_identifier_or_keyword(lx);
    }

    switch (c) {
        case '+': lx->pos++; lx->col++; return make_token_char(lx, TOK_PLUS, '+');
        case '-': lx->pos++; lx->col++; return make_token_char(lx, TOK_MINUS, '-');
        case '*': lx->pos++; lx->col++; return make_token_char(lx, TOK_MUL, '*');
        case '/': lx->pos++; lx->col++; return make_token_char(lx, TOK_DIV, '/');
        case '\\': lx->pos++; lx->col++; return make_token_char(lx, TOK_INTDIV, '\\');
        case '^': lx->pos++; lx->col++; return make_token_char(lx, TOK_POW, '^');
        case '(': lx->pos++; lx->col++; return make_token_char(lx, TOK_LPAREN, '(');
        case ')': lx->pos++; lx->col++; return make_token_char(lx, TOK_RPAREN, ')');
        case '[': lx->pos++; lx->col++; return make_token_char(lx, TOK_LBRACKET, '[');
        case ']': lx->pos++; lx->col++; return make_token_char(lx, TOK_RBRACKET, ']');
        case ',': lx->pos++; lx->col++; return make_token_char(lx, TOK_COMMA, ',');
        case ':': lx->pos++; lx->col++; return make_token_char(lx, TOK_COLON, ':');

        case '=':
            lx->pos++; lx->col++;
            if (lx->pos < lx->len && lx->src[lx->pos] == '=') {
                lx->pos++; lx->col++;
                return make_token(lx, TOK_EQ, "==");
            } else {
                return make_token_char(lx, TOK_ASSIGN, '=');
            }

        case '<':
            lx->pos++; lx->col++;
            if (lx->pos < lx->len && lx->src[lx->pos] == '=') {
                lx->pos++; lx->col++;
                return make_token(lx, TOK_LE, "<=");
            } else if (lx->pos < lx->len && lx->src[lx->pos] == '>') {
                lx->pos++; lx->col++;
                return make_token(lx, TOK_NEQ, "<>");
            } else {
                return make_token_char(lx, TOK_LT, '<');
            }

        case '>':
            lx->pos++; lx->col++;
            if (lx->pos < lx->len && lx->src[lx->pos] == '=') {
                lx->pos++; lx->col++;
                return make_token(lx, TOK_GE, ">=");
            } else {
                return make_token_char(lx, TOK_GT, '>');
            }

        default:
            lx->pos++; lx->col++;
            return make_token(lx, TOK_EOF, NULL);
    }
}

Token lexer_peek(Lexer *lx) {
    if (!lx->has_peek) {
        lx->peek = lexer_read_raw(lx);
        lx->has_peek = 1;
    }
    return lx->peek;
}

void lexer_skip_newlines(Lexer *lx) {
    while (1) {
        Token t = lexer_peek(lx);
        if (t.type == TOK_NEWLINE || t.type == TOK_LINE_CONTINUE) {
            lexer_next(lx);
        } else break;
    }
}

LexerSnapshot lexer_save(Lexer *lx) {
    LexerSnapshot s;
    s.pos = lx->pos;
    s.len = lx->len;
    s.line = lx->line;
    s.col = lx->col;
    s.has_peek = lx->has_peek;
    s.peek = lx->peek;
    s.peek.text = lx->has_peek && lx->peek.text ? strdup(lx->peek.text) : NULL;
    return s;
}

void lexer_restore(Lexer *lx, LexerSnapshot s) {
    if (lx->has_peek && lx->peek.text) free(lx->peek.text);
    lx->pos = s.pos;
    lx->len = s.len;
    lx->line = s.line;
    lx->col = s.col;
    lx->has_peek = s.has_peek;
    lx->peek = s.peek;
}

const char *token_type_name(TokenType t) {
    switch (t) {
        case TOK_EOF: return "EOF";
        case TOK_IDENT: return "IDENTIFIER";
        case TOK_NUMBER: return "NUMBER";
        case TOK_STRING: return "STRING";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_MUL: return "*";
        case TOK_DIV: return "/";
        case TOK_INTDIV: return "\\";
        case TOK_MOD: return "Mod";
        case TOK_POW: return "^";
        case TOK_CONCAT: return "&";
        case TOK_EQ: return "=";
        case TOK_NEQ: return "<>";
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_LE: return "<=";
        case TOK_GE: return ">=";
        case TOK_IS: return "Is";
        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_LBRACKET: return "[";
        case TOK_RBRACKET: return "]";
        case TOK_DOT: return ".";
        case TOK_COMMA: return ",";
        case TOK_COLON: return ":";
        case TOK_NEWLINE: return "NEWLINE";
        case TOK_ASSIGN: return "=";
        case TOK_KEY_DIM: return "Dim";
        case TOK_KEY_SET: return "Set";
        case TOK_KEY_IF: return "If";
        case TOK_KEY_THEN: return "Then";
        case TOK_KEY_ELSE: return "Else";
        case TOK_KEY_ELSEIF: return "ElseIf";
        case TOK_KEY_END_IF: return "End If";
        case TOK_KEY_FOR: return "For";
        case TOK_KEY_NEXT: return "Next";
        case TOK_KEY_DO: return "Do";
        case TOK_KEY_WHILE: return "While";
        case TOK_KEY_UNTIL: return "Until";
        case TOK_KEY_LOOP: return "Loop";
        case TOK_KEY_WEND: return "WEnd";
        case TOK_KEY_WITH: return "With";
        case TOK_KEY_FUNCTION: return "Function";
        case TOK_KEY_SUB: return "Sub";
        case TOK_KEY_END_FUNCTION: return "End Function";
        case TOK_KEY_END_SUB: return "End Sub";
        case TOK_KEY_CALL: return "Call";
        case TOK_KEY_EXIT: return "Exit";
        case TOK_KEY_SELECT: return "Select";
        case TOK_KEY_CASE: return "Case";
        case TOK_KEY_END_SELECT: return "End Select";
        case TOK_KEY_ON: return "On";
        case TOK_KEY_ERROR: return "Error";
        case TOK_KEY_RESUME: return "Resume";
        case TOK_KEY_NEXT_STMT: return "Next";
        case TOK_KEY_CONST: return "Const";
        case TOK_KEY_TRUE: return "True";
        case TOK_KEY_FALSE: return "False";
        case TOK_KEY_NOTHING: return "Nothing";
        case TOK_KEY_NULL: return "Null";
        case TOK_KEY_EMPTY: return "Empty";
        case TOK_KEY_REDIM: return "ReDim";
        case TOK_KEY_CLASS: return "Class";
        case TOK_KEY_END_CLASS: return "End Class";
        case TOK_KEY_NEW: return "New";
        case TOK_KEY_EACH: return "Each";
        case TOK_KEY_IN: return "In";
        case TOK_KEY_TO: return "To";
        case TOK_KEY_STEP: return "Step";
        case TOK_KEY_AND: return "And";
        case TOK_KEY_OR: return "Or";
        case TOK_KEY_NOT: return "Not";
        case TOK_KEY_XOR: return "Xor";
        case TOK_KEY_RANDOMIZE: return "Randomize";
        case TOK_KEY_OPTION: return "Option";
        case TOK_KEY_EXPLICIT: return "Explicit";
        case TOK_KEY_ERASE: return "Erase";
        case TOK_KEY_REM: return "Rem";
        case TOK_KEY_GOTO: return "GoTo";
        case TOK_LINE_CONTINUE: return "_";
        case TOK_KEY_EXIT_DO: return "Exit Do";
        case TOK_KEY_EXIT_FOR: return "Exit For";
        case TOK_KEY_EXIT_FUNCTION: return "Exit Function";
        case TOK_KEY_EXIT_SUB: return "Exit Sub";
        default: return "UNKNOWN";
    }
}