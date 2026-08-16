#define _GNU_SOURCE
#include "vbs.h"
#include <gtk/gtk.h>

/* ========== GTK 对话框辅助函数 ========== */
static int gtk_initialized = 0;

static void ensure_gtk(void) {
    if (!gtk_initialized) {
        gtk_init(NULL, NULL);
        gtk_initialized = 1;
    }
}

/* ========== MsgBox 实现 ========== */
static Value fn_msgbox(Interp *interp, int argc, Value *argv) {
    ensure_gtk();
    const char *msg = (argc >= 1) ? val_tostr(argv[0]) : "";
    int buttons = 0;
    if (argc >= 2) buttons = (int)val_toint(argv[1]);

    GtkMessageType msgtype = GTK_MESSAGE_OTHER;
    int icon_type = (buttons >> 4) & 0x07;
    if (icon_type == 1) msgtype = GTK_MESSAGE_WARNING;
    else if (icon_type == 2) msgtype = GTK_MESSAGE_QUESTION;
    else if (icon_type == 3) msgtype = GTK_MESSAGE_ERROR;
    else if (icon_type == 4) msgtype = GTK_MESSAGE_INFO;

    GtkWidget *dialog = gtk_message_dialog_new(NULL,
        GTK_DIALOG_MODAL, msgtype, GTK_BUTTONS_NONE, "%s", msg);

    int btn_type = buttons & 0x0F;
    switch (btn_type) {
        case 0:
            gtk_dialog_add_button(GTK_DIALOG(dialog), "确定", GTK_RESPONSE_OK);
            break;
        case 1:
            gtk_dialog_add_button(GTK_DIALOG(dialog), "确定", GTK_RESPONSE_OK);
            gtk_dialog_add_button(GTK_DIALOG(dialog), "取消", GTK_RESPONSE_CANCEL);
            break;
        case 2:
            gtk_dialog_add_button(GTK_DIALOG(dialog), "是", GTK_RESPONSE_YES);
            gtk_dialog_add_button(GTK_DIALOG(dialog), "否", GTK_RESPONSE_NO);
            break;
        case 3:
            gtk_dialog_add_button(GTK_DIALOG(dialog), "是", GTK_RESPONSE_YES);
            gtk_dialog_add_button(GTK_DIALOG(dialog), "否", GTK_RESPONSE_NO);
            gtk_dialog_add_button(GTK_DIALOG(dialog), "取消", GTK_RESPONSE_CANCEL);
            break;
        case 4:
            gtk_dialog_add_button(GTK_DIALOG(dialog), "是", GTK_RESPONSE_YES);
            gtk_dialog_add_button(GTK_DIALOG(dialog), "否", GTK_RESPONSE_NO);
            break;
        case 5:
            gtk_dialog_add_button(GTK_DIALOG(dialog), "重试", GTK_RESPONSE_OK);
            gtk_dialog_add_button(GTK_DIALOG(dialog), "取消", GTK_RESPONSE_CANCEL);
            break;
        default:
            gtk_dialog_add_button(GTK_DIALOG(dialog), "确定", GTK_RESPONSE_OK);
    }

    gtk_window_set_title(GTK_WINDOW(dialog), "VBScript");
    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    while (gtk_events_pending()) gtk_main_iteration();

    int ret = 1;
    switch (result) {
        case GTK_RESPONSE_OK: ret = 1; break;
        case GTK_RESPONSE_CANCEL: ret = 2; break;
        case GTK_RESPONSE_YES: ret = 6; break;
        case GTK_RESPONSE_NO: ret = 7; break;
        default: ret = 1;
    }
    free((char*)msg);
    return val_int(ret);
}

/* ========== InputBox 实现 ========== */
static Value fn_inputbox(Interp *interp, int argc, Value *argv) {
    ensure_gtk();
    const char *prompt = (argc >= 1) ? val_tostr(argv[0]) : "";
    const char *title = (argc >= 2) ? val_tostr(argv[1]) : "输入";
    const char *default_text = (argc >= 3) ? val_tostr(argv[2]) : "";

    GtkWidget *dialog = gtk_dialog_new_with_buttons(title, NULL,
        GTK_DIALOG_MODAL, "确定", GTK_RESPONSE_OK, "取消", GTK_RESPONSE_CANCEL, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new(prompt);
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), default_text);

    gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 5);
    gtk_widget_show_all(dialog);

    gint result = gtk_dialog_run(GTK_DIALOG(dialog));
    const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
    Value r = (result == GTK_RESPONSE_OK) ? val_str(text) : val_str("");

    gtk_widget_destroy(dialog);
    while (gtk_events_pending()) gtk_main_iteration();

    free((char*)prompt);
    free((char*)title);
    free((char*)default_text);
    return r;
}

/* ========== 字符串函数 ========== */
static Value fn_len(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    long long r = strlen(s);
    free(s);
    return val_int(r);
}

static Value fn_mid(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    long long start = val_toint(argv[1]);
    long long len = (argc >= 3) ? val_toint(argv[2]) : (long long)strlen(s) - start + 1;
    if (start < 1) start = 1;
    if (start > (long long)strlen(s)) { free(s); return val_str(""); }
    long long slen = (long long)strlen(s);
    long long from = start - 1;
    if (from + len > slen) len = slen - from;
    if (len < 0) len = 0;
    char *buf = malloc(len + 1);
    strncpy(buf, s + from, len);
    buf[len] = 0;
    free(s);
    Value r = val_str(buf);
    free(buf);
    return r;
}

static Value fn_left(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    long long len = val_toint(argv[1]);
    if (len <= 0) { free(s); return val_str(""); }
    if (len > (long long)strlen(s)) len = strlen(s);
    char *buf = malloc(len + 1);
    strncpy(buf, s, len);
    buf[len] = 0;
    free(s);
    Value r = val_str(buf);
    free(buf);
    return r;
}

static Value fn_right(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    long long len = val_toint(argv[1]);
    long long slen = strlen(s);
    if (len <= 0) { free(s); return val_str(""); }
    if (len > slen) len = slen;
    char *buf = malloc(len + 1);
    strcpy(buf, s + slen - len);
    free(s);
    Value r = val_str(buf);
    free(buf);
    return r;
}

static Value fn_trim(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
    char *end = start + strlen(start) - 1;
    while (end >= start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) end--;
    long long len = end - start + 1;
    if (len < 0) len = 0;
    char *buf = malloc(len + 1);
    strncpy(buf, start, len);
    buf[len] = 0;
    free(s);
    Value r = val_str(buf);
    free(buf);
    return r;
}

static Value fn_lcase(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    for (char *p = s; *p; p++) *p = tolower(*p);
    Value r = val_str(s);
    free(s);
    return r;
}

static Value fn_ucase(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    for (char *p = s; *p; p++) *p = toupper(*p);
    Value r = val_str(s);
    free(s);
    return r;
}

static Value fn_instr(Interp *interp, int argc, Value *argv) {
    int start = (argc >= 3) ? (int)val_toint(argv[0]) : 1;
    int si = (argc >= 3) ? 1 : 0;
    int stri = (argc >= 3) ? 2 : 1;
    int comp = (argc >= 4) ? (int)val_toint(argv[3]) : 0;
    char *s1 = val_tostr(argv[si]);
    char *s2 = val_tostr(argv[stri]);
    long long r = 0;
    if (start >= 1 && start <= (int)strlen(s1)) {
        char *p;
        if (comp) {
            p = strcasestr(s1 + start - 1, s2);
        } else {
            p = strstr(s1 + start - 1, s2);
        }
        if (p) r = (p - s1) + 1;
    }
    free(s1);
    free(s2);
    return val_int(r);
}

static Value fn_replace(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    char *find = val_tostr(argv[1]);
    char *repl = val_tostr(argv[2]);
    int count = (argc >= 4) ? (int)val_toint(argv[3]) : -1;
    char buf[65536];
    int pos = 0;
    char *p = s;
    int replaced = 0;
    while (*p && pos < 65535) {
        char *f = strstr(p, find);
        if (f && (count < 0 || replaced < count)) {
            strncpy(buf + pos, p, f - p);
            pos += f - p;
            strcpy(buf + pos, repl);
            pos += strlen(repl);
            p = f + strlen(find);
            replaced++;
        } else {
            buf[pos++] = *p++;
        }
    }
    buf[pos] = 0;
    free(s);
    free(find);
    free(repl);
    return val_str(buf);
}

static Value fn_split(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    char *delim = (argc >= 2) ? val_tostr(argv[1]) : strdup(" ");
    int count = (argc >= 3) ? (int)val_toint(argv[2]) : -1;
    VbsArray *a = arr_new_1d(1);
    int n = 0;
    char *p = s;
    char buf[65536];
    int pos = 0;
    while (*p) {
        char *d = strstr(p, delim);
        if (d && (count < 0 || n + 1 < count)) {
            strncpy(buf, p, d - p);
            buf[d - p] = 0;
            if (n >= a->total_size) {
                a->dims[0] = n + 1;
                a->data = realloc(a->data, sizeof(Value) * (n + 1));
                memset(&a->data[n], 0, sizeof(Value) * (a->dims[0] - a->total_size));
                a->total_size = a->dims[0];
            }
            a->data[n++] = val_str(buf);
            p = d + strlen(delim);
            pos = 0;
        } else {
            buf[pos++] = *p++;
            if (pos >= 65535) break;
        }
    }
    buf[pos] = 0;
    if (n >= a->total_size) {
        a->dims[0] = n + 1;
        a->data = realloc(a->data, sizeof(Value) * (n + 1));
        memset(&a->data[n], 0, sizeof(Value) * (a->dims[0] - a->total_size));
        a->total_size = a->dims[0];
    }
    a->data[n++] = val_str(buf);
    a->dims[0] = n;
    free(s);
    free(delim);
    return val_arr(a);
}

static Value fn_join(Interp *interp, int argc, Value *argv) {
    if (argv[0].type != VALTYPE_ARRAY) return val_str("");
    VbsArray *a = argv[0].as.arr;
    char *delim = (argc >= 2) ? val_tostr(argv[1]) : strdup(" ");
    char buf[65536];
    int pos = 0;
    for (int i = 0; i < a->total_size && pos < 65535; i++) {
        if (i > 0) {
            strcpy(buf + pos, delim);
            pos += strlen(delim);
        }
        char *s = val_tostr(a->data[i]);
        strcpy(buf + pos, s);
        pos += strlen(s);
        free(s);
    }
    buf[pos] = 0;
    free(delim);
    return val_str(buf);
}

static Value fn_asc(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    long long r = s[0] ? (unsigned char)s[0] : 0;
    free(s);
    return val_int(r);
}

static Value fn_chr(Interp *interp, int argc, Value *argv) {
    char c = (char)val_toint(argv[0]);
    char buf[2] = {c, 0};
    return val_str(buf);
}

static Value fn_space(Interp *interp, int argc, Value *argv) {
    long long n = val_toint(argv[0]);
    if (n <= 0) return val_str("");
    if (n > 65536) n = 65536;
    char *buf = malloc(n + 1);
    memset(buf, ' ', n);
    buf[n] = 0;
    Value r = val_str(buf);
    free(buf);
    return r;
}

static Value fn_string(Interp *interp, int argc, Value *argv) {
    long long n = val_toint(argv[0]);
    char c = (char)val_toint(argv[1]);
    if (n <= 0) return val_str("");
    if (n > 65536) n = 65536;
    char *buf = malloc(n + 1);
    memset(buf, c, n);
    buf[n] = 0;
    Value r = val_str(buf);
    free(buf);
    return r;
}

static Value fn_strreverse(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    long long len = strlen(s);
    char *buf = malloc(len + 1);
    for (long long i = 0; i < len; i++) {
        buf[i] = s[len - 1 - i];
    }
    buf[len] = 0;
    free(s);
    Value r = val_str(buf);
    free(buf);
    return r;
}

static Value fn_strcomp(Interp *interp, int argc, Value *argv) {
    char *a = val_tostr(argv[0]);
    char *b = val_tostr(argv[1]);
    int comp = (argc >= 3) ? (int)val_toint(argv[2]) : 0;
    int r;
    if (comp) r = strcasecmp(a, b);
    else r = strcmp(a, b);
    free(a);
    free(b);
    if (r < 0) return val_int(-1);
    if (r > 0) return val_int(1);
    return val_int(0);
}

static Value fn_instrrev(Interp *interp, int argc, Value *argv) {
    char *s1 = val_tostr(argv[0]);
    char *s2 = val_tostr(argv[1]);
    int start = (argc >= 3) ? (int)val_toint(argv[2]) : (int)strlen(s1);
    int comp = (argc >= 4) ? (int)val_toint(argv[3]) : 0;
    long long r = 0;
    if (start > (int)strlen(s1)) start = strlen(s1);
    if (start > 0) {
        if (comp) {
            for (int i = start - 1; i >= 0; i--) {
                if (strncasecmp(s1 + i, s2, strlen(s2)) == 0) {
                    r = i + 1;
                    break;
                }
            }
        } else {
            for (int i = start - 1; i >= 0; i--) {
                if (strncmp(s1 + i, s2, strlen(s2)) == 0) {
                    r = i + 1;
                    break;
                }
            }
        }
    }
    free(s1);
    free(s2);
    return val_int(r);
}

static Value fn_ltrim(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    Value r = val_str(p);
    free(s);
    return r;
}

static Value fn_rtrim(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    char *end = s + strlen(s) - 1;
    while (end >= s && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) end--;
    long long len = end - s + 1;
    if (len < 0) len = 0;
    char *buf = malloc(len + 1);
    strncpy(buf, s, len);
    buf[len] = 0;
    Value r = val_str(buf);
    free(buf);
    free(s);
    return r;
}

/* ========== 数学函数 ========== */
static Value fn_abs(Interp *interp, int argc, Value *argv) {
    double d = val_todouble(argv[0]);
    return val_double(fabs(d));
}

static Value fn_int_fn(Interp *interp, int argc, Value *argv) {
    double d = val_todouble(argv[0]);
    return val_double(floor(d));
}

static Value fn_fix(Interp *interp, int argc, Value *argv) {
    double d = val_todouble(argv[0]);
    return val_double(d >= 0 ? floor(d) : ceil(d));
}

static Value fn_round(Interp *interp, int argc, Value *argv) {
    double d = val_todouble(argv[0]);
    int places = (argc >= 2) ? (int)val_toint(argv[1]) : 0;
    double mult = pow(10.0, places);
    return val_double(round(d * mult) / mult);
}

static Value fn_sqr(Interp *interp, int argc, Value *argv) {
    return val_double(sqrt(val_todouble(argv[0])));
}

static Value fn_sgn(Interp *interp, int argc, Value *argv) {
    double d = val_todouble(argv[0]);
    if (d < 0) return val_int(-1);
    if (d > 0) return val_int(1);
    return val_int(0);
}

static Value fn_rnd(Interp *interp, int argc, Value *argv) {
    return val_double((double)rand() / RAND_MAX);
}

static Value fn_sin_fn(Interp *interp, int argc, Value *argv) {
    return val_double(sin(val_todouble(argv[0])));
}

static Value fn_cos_fn(Interp *interp, int argc, Value *argv) {
    return val_double(cos(val_todouble(argv[0])));
}

static Value fn_tan_fn(Interp *interp, int argc, Value *argv) {
    return val_double(tan(val_todouble(argv[0])));
}

static Value fn_atn(Interp *interp, int argc, Value *argv) {
    return val_double(atan(val_todouble(argv[0])));
}

static Value fn_log_fn(Interp *interp, int argc, Value *argv) {
    return val_double(log(val_todouble(argv[0])));
}

static Value fn_exp_fn(Interp *interp, int argc, Value *argv) {
    return val_double(exp(val_todouble(argv[0])));
}

static Value fn_hex(Interp *interp, int argc, Value *argv) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%llX", (unsigned long long)val_toint(argv[0]));
    return val_str(buf);
}

static Value fn_oct(Interp *interp, int argc, Value *argv) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%llo", (unsigned long long)val_toint(argv[0]));
    return val_str(buf);
}

/* ========== 类型转换函数 ========== */
static Value fn_cstr(Interp *interp, int argc, Value *argv) {
    return val_str(val_tostr(argv[0]));
}

static Value fn_cint(Interp *interp, int argc, Value *argv) {
    return val_int(val_toint(argv[0]));
}

static Value fn_cdbl(Interp *interp, int argc, Value *argv) {
    return val_double(val_todouble(argv[0]));
}

static Value fn_cbool(Interp *interp, int argc, Value *argv) {
    return val_bool(val_tobool(argv[0]));
}

static Value fn_clng(Interp *interp, int argc, Value *argv) {
    return val_int(val_toint(argv[0]));
}

static Value fn_csng(Interp *interp, int argc, Value *argv) {
    return val_double(val_todouble(argv[0]));
}

/* ========== 日期时间函数 ========== */
static Value fn_date(Interp *interp, int argc, Value *argv) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d", tm);
    return val_str(buf);
}

static Value fn_time(Interp *interp, int argc, Value *argv) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return val_str(buf);
}

static Value fn_now(Interp *interp, int argc, Value *argv) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return val_str(buf);
}

static Value fn_day(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    struct tm tm = {0};
    if (strptime(s, "%Y-%m-%d", &tm) || strptime(s, "%Y/%m/%d", &tm)) {
        free(s);
        return val_int(tm.tm_mday);
    }
    free(s);
    return val_int(0);
}

static Value fn_month(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    struct tm tm = {0};
    if (strptime(s, "%Y-%m-%d", &tm) || strptime(s, "%Y/%m/%d", &tm)) {
        free(s);
        return val_int(tm.tm_mon + 1);
    }
    free(s);
    return val_int(0);
}

static Value fn_year(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    struct tm tm = {0};
    if (strptime(s, "%Y-%m-%d", &tm) || strptime(s, "%Y/%m/%d", &tm)) {
        free(s);
        return val_int(tm.tm_year + 1900);
    }
    free(s);
    return val_int(0);
}

static Value fn_hour(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    struct tm tm = {0};
    if (strptime(s, "%H:%M:%S", &tm)) {
        free(s);
        return val_int(tm.tm_hour);
    }
    free(s);
    return val_int(0);
}

static Value fn_minute(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    struct tm tm = {0};
    if (strptime(s, "%H:%M:%S", &tm)) {
        free(s);
        return val_int(tm.tm_min);
    }
    free(s);
    return val_int(0);
}

static Value fn_second(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    struct tm tm = {0};
    if (strptime(s, "%H:%M:%S", &tm)) {
        free(s);
        return val_int(tm.tm_sec);
    }
    free(s);
    return val_int(0);
}

static Value fn_weekday(Interp *interp, int argc, Value *argv) {
    char *s = val_tostr(argv[0]);
    struct tm tm = {0};
    if (strptime(s, "%Y-%m-%d", &tm) || strptime(s, "%Y/%m/%d", &tm)) {
        free(s);
        return val_int(tm.tm_wday + 1);
    }
    free(s);
    return val_int(1);
}

static Value fn_dateserial(Interp *interp, int argc, Value *argv) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%04lld-%02lld-%02lld",
        val_toint(argv[0]), val_toint(argv[1]), val_toint(argv[2]));
    return val_str(buf);
}

static Value fn_timeserial(Interp *interp, int argc, Value *argv) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld",
        val_toint(argv[0]), val_toint(argv[1]), val_toint(argv[2]));
    return val_str(buf);
}

static Value fn_datevalue(Interp *interp, int argc, Value *argv) {
    return val_str(val_tostr(argv[0]));
}

static Value fn_timevalue(Interp *interp, int argc, Value *argv) {
    return val_str(val_tostr(argv[0]));
}

/* ========== 杂项函数 ========== */
static Value fn_isnull(Interp *interp, int argc, Value *argv) {
    return val_bool(argv[0].type == VALTYPE_NULL);
}

static Value fn_isempty(Interp *interp, int argc, Value *argv) {
    return val_bool(argv[0].type == VALTYPE_EMPTY);
}

static Value fn_isnumeric(Interp *interp, int argc, Value *argv) {
    if (argv[0].type == VALTYPE_INTEGER || argv[0].type == VALTYPE_DOUBLE) return val_bool(1);
    if (argv[0].type == VALTYPE_STRING) {
        char *end;
        strtod(argv[0].as.str, &end);
        return val_bool(*end == 0);
    }
    return val_bool(0);
}

static Value fn_isobject(Interp *interp, int argc, Value *argv) {
    return val_bool(argv[0].type == VALTYPE_OBJECT);
}

static Value fn_isarray(Interp *interp, int argc, Value *argv) {
    return val_bool(argv[0].type == VALTYPE_ARRAY);
}

static Value fn_typename(Interp *interp, int argc, Value *argv) {
    switch (argv[0].type) {
        case VALTYPE_EMPTY: return val_str("Empty");
        case VALTYPE_NULL: return val_str("Null");
        case VALTYPE_INTEGER: return val_str("Integer");
        case VALTYPE_DOUBLE: return val_str("Double");
        case VALTYPE_STRING: return val_str("String");
        case VALTYPE_BOOL: return val_str("Boolean");
        case VALTYPE_OBJECT: {
            if (argv[0].as.obj && argv[0].as.obj->type) return val_str(argv[0].as.obj->type);
            return val_str("Object");
        }
        case VALTYPE_ARRAY: return val_str("Array()");
        default: return val_str("Unknown");
    }
}

static Value fn_vartype(Interp *interp, int argc, Value *argv) {
    return val_int((int)argv[0].type);
}

static Value fn_rgb(Interp *interp, int argc, Value *argv) {
    long long r = val_toint(argv[0]);
    long long g = val_toint(argv[1]);
    long long b = val_toint(argv[2]);
    return val_int(r + g * 256 + b * 65536);
}

static Value fn_array_fn(Interp *interp, int argc, Value *argv) {
    VbsArray *a = arr_new_1d(argc);
    for (int i = 0; i < argc; i++) {
        a->data[i] = val_clone(argv[i]);
    }
    return val_arr(a);
}

static Value fn_ubound(Interp *interp, int argc, Value *argv) {
    if (argv[0].type != VALTYPE_ARRAY) return val_int(0);
    VbsArray *a = argv[0].as.arr;
    int dim = (argc >= 2) ? (int)val_toint(argv[1]) - 1 : 0;
    if (dim < 0 || dim >= a->dim_count) return val_int(0);
    return val_int(a->dims[dim] - 1);
}

static Value fn_lbound(Interp *interp, int argc, Value *argv) {
    return val_int(0);
}

static Value fn_createobject(Interp *interp, int argc, Value *argv) {
    char *name = val_tostr(argv[0]);
    Object *obj = NULL;
    if (strcasecmp(name, "Scripting.FileSystemObject") == 0 ||
        strcasecmp(name, "FileSystemObject") == 0) {
        obj = fso_create();
    } else if (strcasecmp(name, "Scripting.Dictionary") == 0 ||
        strcasecmp(name, "Dictionary") == 0) {
        obj = dict_create();
    } else {
        interp_error(interp, "CreateObject: 不支持的对象: %s", name);
    }
    free(name);
    return val_obj(obj);
}

static Value fn_wscript_echo(Interp *interp, int argc, Value *argv) {
    for (int i = 0; i < argc; i++) {
        char *s = val_tostr(argv[i]);
        if (i > 0) printf(" ");
        printf("%s", s);
        free(s);
    }
    printf("\n");
    fflush(stdout);
    return val_empty();
}



/* ========== WScript 对象实现 ========== */
static Value wscript_get_prop(Object *obj, const char *name) {
    if (strcasecmp(name, "FullName") == 0) {
        return val_str("vbs");
    }
    if (strcasecmp(name, "Path") == 0) {
        return val_str(".");
    }
    if (strcasecmp(name, "ScriptFullName") == 0) {
        return val_str("");
    }
    if (strcasecmp(name, "ScriptName") == 0) {
        return val_str("");
    }
    if (strcasecmp(name, "Arguments") == 0) {
        return val_obj(obj);
    }
    return val_empty();
}

static Value wscript_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv) {
    if (strcasecmp(name, "Echo") == 0) {
        return fn_wscript_echo(interp, argc, argv);
    }
    if (strcasecmp(name, "Quit") == 0) {
        if (argc >= 1) interp->exit_code = (int)val_toint(argv[0]);
        else interp->exit_code = 0;
        interp->running = 0;
        return val_empty();
    }
    if (strcasecmp(name, "Sleep") == 0) {
        if (argc >= 1) {
            struct timespec ts;
            ts.tv_sec = val_toint(argv[0]) / 1000;
            ts.tv_nsec = (val_toint(argv[0]) % 1000) * 1000000;
            nanosleep(&ts, NULL);
        }
        return val_empty();
    }
    if (strcasecmp(name, "CreateObject") == 0) {
        return fn_createobject(interp, argc, argv);
    }
    return val_empty();
}

Object *wscript_create(const char *path) {
    return obj_new("WScript", NULL, NULL, wscript_get_prop, wscript_call_method);
}

/* ========== FileSystemObject 实现 ========== */
typedef struct {
    char *path;
    FILE *fp;
    int open;
    int for_reading;
} FSOData;

static void fso_destroy(Object *obj) {
    FSOData *d = (FSOData*)obj->data;
    if (d) {
        if (d->fp) fclose(d->fp);
        free(d->path);
        free(d);
    }
}

static Value fso_get_prop(Object *obj, const char *name) {
    return val_empty();
}

static Value textstream_get_prop(Object *obj, const char *name) {
    FSOData *d = (FSOData*)obj->data;
    if (strcasecmp(name, "AtEndOfStream") == 0) {
        if (!d->fp) return val_bool(1);
        int c = fgetc(d->fp);
        if (c == EOF) return val_bool(1);
        ungetc(c, d->fp);
        return val_bool(0);
    }
    return val_empty();
}

static Value textstream_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv) {
    FSOData *d = (FSOData*)obj->data;
    if (strcasecmp(name, "ReadAll") == 0) {
        if (!d->fp) return val_str("");
        fseek(d->fp, 0, SEEK_END);
        long len = ftell(d->fp);
        fseek(d->fp, 0, SEEK_SET);
        char *buf = malloc(len + 1);
        if (buf) {
            if (fread(buf, 1, len, d->fp) == 0 && len > 0) { buf[0] = 0; }
            buf[len] = 0;
            Value r = val_str(buf);
            free(buf);
            return r;
        }
        return val_str("");
    }
    if (strcasecmp(name, "ReadLine") == 0) {
        if (!d->fp) return val_str("");
        char buf[4096];
        if (fgets(buf, sizeof(buf), d->fp) != NULL) {
            char *nl = strchr(buf, '\n');
            if (nl) *nl = 0;
            return val_str(buf);
        }
        return val_str("");
    }
    if (strcasecmp(name, "Read") == 0) {
        if (!d->fp || argc < 1) return val_str("");
        long n = val_toint(argv[0]);
        if (n <= 0) return val_str("");
        char *buf = malloc(n + 1);
        if (buf) {
            long r = fread(buf, 1, n, d->fp);
            buf[r] = 0;
            Value result = val_str(buf);
            free(buf);
            return result;
        }
        return val_str("");
    }
    if (strcasecmp(name, "Write") == 0) {
        if (!d->fp || argc < 1) return val_empty();
        char *s = val_tostr(argv[0]);
        fputs(s, d->fp);
        free(s);
        return val_empty();
    }
    if (strcasecmp(name, "WriteLine") == 0) {
        if (!d->fp) return val_empty();
        if (argc >= 1) {
            char *s = val_tostr(argv[0]);
            fputs(s, d->fp);
            free(s);
        }
        fputs("\n", d->fp);
        return val_empty();
    }
    if (strcasecmp(name, "Close") == 0) {
        if (d->fp) fclose(d->fp);
        d->fp = NULL;
        d->open = 0;
        return val_empty();
    }
    if (strcasecmp(name, "SkipLine") == 0) {
        if (d->fp) {
            char buf[4096];
            if (fgets(buf, sizeof(buf), d->fp) == NULL) {}
        }
        return val_empty();
    }
    return val_empty();
}

static Value fso_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv) {
    if (strcasecmp(name, "CreateTextFile") == 0) {
        if (argc < 1) return val_empty();
        char *path = val_tostr(argv[0]);
        int overwrite = (argc >= 2) ? val_tobool(argv[1]) : 1;
        FILE *fp = fopen(path, overwrite ? "w" : "wx");
        if (!fp) {
            char err[1024];
            snprintf(err, sizeof(err), "无法创建文件: %s", path);
            free(path);
            interp_error(interp, err);
            return val_empty();
        }
        free(path);
        FSOData *d = calloc(1, sizeof(FSOData));
        d->fp = fp;
        d->open = 1;
        d->for_reading = 0;
        return val_obj(obj_new("TextStream", d, fso_destroy, textstream_get_prop, textstream_call_method));
    }
    if (strcasecmp(name, "OpenTextFile") == 0) {
        if (argc < 1) return val_empty();
        char *path = val_tostr(argv[0]);
        int iomode = (argc >= 2) ? (int)val_toint(argv[1]) : 1;
        int create = (argc >= 3) ? val_tobool(argv[2]) : 0;
        if (strchr(path, '/') == NULL && strchr(path, '\\') == NULL) {
            char *full = malloc(strlen(interp->script_dir) + strlen(path) + 2);
            sprintf(full, "%s/%s", interp->script_dir, path);
            free(path);
            path = full;
        }

        FILE *fp = NULL;
        switch (iomode) {
            case 1: fp = fopen(path, "r"); break;
            case 2: fp = fopen(path, "w"); break;
            case 8: fp = fopen(path, "a"); break;
            default: fp = fopen(path, "r"); break;
        }
        if (!fp && create) {
            fp = fopen(path, "w+");
        }
        if (!fp) {
            char err[1024];
            snprintf(err, sizeof(err), "无法打开文件: %s", path);
            free(path);
            interp_error(interp, err);
            return val_empty();
        }
        free(path);
        FSOData *d = calloc(1, sizeof(FSOData));
        d->fp = fp;
        d->open = 1;
        d->for_reading = (iomode == 1);
        return val_obj(obj_new("TextStream", d, fso_destroy, textstream_get_prop, textstream_call_method));
    }
    if (strcasecmp(name, "FileExists") == 0) {
        if (argc < 1) return val_bool(0);
        char *path = val_tostr(argv[0]);
        FILE *fp = fopen(path, "r");
        int exists = (fp != NULL);
        if (fp) fclose(fp);
        free(path);
        return val_bool(exists);
    }
    if (strcasecmp(name, "DeleteFile") == 0) {
        if (argc < 1) return val_empty();
        char *path = val_tostr(argv[0]);
        remove(path);
        free(path);
        return val_empty();
    }
    if (strcasecmp(name, "GetParentFolderName") == 0) {
        if (argc < 1) return val_str("");
        char *path = val_tostr(argv[0]);
        char *p = strrchr(path, '/');
        if (!p) p = strrchr(path, '\\');
        if (p) {
            *p = 0;
            Value r = val_str(path);
            *p = '/';
            free(path);
            return r;
        }
        free(path);
        return val_str("");
    }
    if (strcasecmp(name, "GetFileName") == 0) {
        if (argc < 1) return val_str("");
        char *path = val_tostr(argv[0]);
        char *p = strrchr(path, '/');
        if (!p) p = strrchr(path, '\\');
        if (p) {
            Value r = val_str(p + 1);
            free(path);
            return r;
        }
        return val_str(path);
    }
    if (strcasecmp(name, "GetExtensionName") == 0) {
        if (argc < 1) return val_str("");
        char *path = val_tostr(argv[0]);
        char *p = strrchr(path, '.');
        if (p) {
            Value r = val_str(p + 1);
            free(path);
            return r;
        }
        free(path);
        return val_str("");
    }
    if (strcasecmp(name, "GetBaseName") == 0) {
        if (argc < 1) return val_str("");
        char *path = val_tostr(argv[0]);
        char *p = strrchr(path, '/');
        if (!p) p = strrchr(path, '\\');
        char *name = p ? p + 1 : path;
        char *dot = strrchr(name, '.');
        if (dot) {
            *dot = 0;
            Value r = val_str(name);
            *dot = '.';
            free(path);
            return r;
        }
        Value r = val_str(name);
        free(path);
        return r;
    }
    if (strcasecmp(name, "GetTempName") == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "tmp_%d.tmp", rand());
        return val_str(buf);
    }
    if (strcasecmp(name, "BuildPath") == 0) {
        if (argc < 2) return val_str("");
        char *p1 = val_tostr(argv[0]);
        char *p2 = val_tostr(argv[1]);
        char *buf = malloc(strlen(p1) + strlen(p2) + 2);
        sprintf(buf, "%s/%s", p1, p2);
        Value r = val_str(buf);
        free(buf);
        free(p1);
        free(p2);
        return r;
    }
    return val_empty();
}

Object *fso_create(void) {
    FSOData *d = calloc(1, sizeof(FSOData));
    d->path = NULL;
    d->fp = NULL;
    d->open = 0;
    return obj_new("FileSystemObject", d, fso_destroy, fso_get_prop, fso_call_method);
}

/* ========== Dictionary 对象实现 ========== */
typedef struct {
    char **keys;
    Value *values;
    int count;
    int cap;
} DictData;

static void dict_destroy(Object *obj) {
    DictData *d = (DictData*)obj->data;
    if (d) {
        for (int i = 0; i < d->count; i++) {
            free(d->keys[i]);
            val_free(&d->values[i]);
        }
        free(d->keys);
        free(d->values);
        free(d);
    }
}

static int dict_find(DictData *d, const char *key) {
    for (int i = 0; i < d->count; i++) {
        if (strcasecmp(d->keys[i], key) == 0) return i;
    }
    return -1;
}

static Value dict_get_prop(Object *obj, const char *name) {
    DictData *d = (DictData*)obj->data;
    if (strcasecmp(name, "Count") == 0) {
        return val_int(d->count);
    }
    return val_empty();
}

static Value dict_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv) {
    DictData *d = (DictData*)obj->data;
    if (strcasecmp(name, "Add") == 0) {
        if (argc < 2) return val_empty();
        char *key = val_tostr(argv[0]);
        if (dict_find(d, key) >= 0) {
            free(key);
            return val_empty();
        }
        if (d->count >= d->cap) {
            d->cap = d->cap ? d->cap * 2 : 16;
            d->keys = realloc(d->keys, sizeof(char*) * d->cap);
            d->values = realloc(d->values, sizeof(Value) * d->cap);
        }
        d->keys[d->count] = key;
        d->values[d->count] = val_clone(argv[1]);
        d->count++;
        return val_empty();
    }
    if (strcasecmp(name, "Item") == 0 || strcasecmp(name, "item") == 0) {
        if (argc < 1) return val_empty();
        char *key = val_tostr(argv[0]);
        int idx = dict_find(d, key);
        free(key);
        if (idx >= 0) return val_clone(d->values[idx]);
        return val_empty();
    }
    if (strcasecmp(name, "Exists") == 0) {
        if (argc < 1) return val_bool(0);
        char *key = val_tostr(argv[0]);
        int r = dict_find(d, key) >= 0;
        free(key);
        return val_bool(r);
    }
    if (strcasecmp(name, "Remove") == 0) {
        if (argc < 1) return val_empty();
        char *key = val_tostr(argv[0]);
        int idx = dict_find(d, key);
        free(key);
        if (idx >= 0) {
            free(d->keys[idx]);
            val_free(&d->values[idx]);
            for (int j = idx; j < d->count - 1; j++) {
                d->keys[j] = d->keys[j + 1];
                d->values[j] = d->values[j + 1];
            }
            d->count--;
        }
        return val_empty();
    }
    if (strcasecmp(name, "RemoveAll") == 0) {
        for (int i = 0; i < d->count; i++) {
            free(d->keys[i]);
            val_free(&d->values[i]);
        }
        d->count = 0;
        return val_empty();
    }
    if (strcasecmp(name, "Keys") == 0) {
        VbsArray *a = arr_new_1d(d->count);
        for (int i = 0; i < d->count; i++) {
            a->data[i] = val_str(d->keys[i]);
        }
        return val_arr(a);
    }
    if (strcasecmp(name, "Items") == 0) {
        VbsArray *a = arr_new_1d(d->count);
        for (int i = 0; i < d->count; i++) {
            a->data[i] = val_clone(d->values[i]);
        }
        return val_arr(a);
    }
    return val_empty();
}

Object *dict_create(void) {
    DictData *d = calloc(1, sizeof(DictData));
    d->keys = NULL;
    d->values = NULL;
    d->count = 0;
    d->cap = 0;
    return obj_new("Dictionary", d, dict_destroy, dict_get_prop, dict_call_method);
}

/* ========== 内置函数注册表 ========== */
static BuiltinEntry builtins[] = {
    {"MsgBox", fn_msgbox, 1, 5},
    {"InputBox", fn_inputbox, 1, 3},
    {"Len", fn_len, 1, 1},
    {"Mid", fn_mid, 2, 3},
    {"Left", fn_left, 2, 2},
    {"Right", fn_right, 2, 2},
    {"Trim", fn_trim, 1, 1},
    {"LTrim", fn_ltrim, 1, 1},
    {"RTrim", fn_rtrim, 1, 1},
    {"LCase", fn_lcase, 1, 1},
    {"UCase", fn_ucase, 1, 1},
    {"InStr", fn_instr, 2, 4},
    {"InStrRev", fn_instrrev, 2, 4},
    {"Replace", fn_replace, 3, 6},
    {"Split", fn_split, 1, 3},
    {"Join", fn_join, 1, 2},
    {"Asc", fn_asc, 1, 1},
    {"Chr", fn_chr, 1, 1},
    {"Space", fn_space, 1, 1},
    {"String", fn_string, 2, 2},
    {"StrComp", fn_strcomp, 2, 3},
    {"StrReverse", fn_strreverse, 1, 1},
    {"Abs", fn_abs, 1, 1},
    {"Int", fn_int_fn, 1, 1},
    {"Fix", fn_fix, 1, 1},
    {"Round", fn_round, 1, 2},
    {"Sqr", fn_sqr, 1, 1},
    {"Sgn", fn_sgn, 1, 1},
    {"Rnd", fn_rnd, 0, 1},
    {"Sin", fn_sin_fn, 1, 1},
    {"Cos", fn_cos_fn, 1, 1},
    {"Tan", fn_tan_fn, 1, 1},
    {"Atn", fn_atn, 1, 1},
    {"Log", fn_log_fn, 1, 1},
    {"Exp", fn_exp_fn, 1, 1},
    {"Hex", fn_hex, 1, 1},
    {"Oct", fn_oct, 1, 1},
    {"CStr", fn_cstr, 1, 1},
    {"CInt", fn_cint, 1, 1},
    {"CDbl", fn_cdbl, 1, 1},
    {"CBool", fn_cbool, 1, 1},
    {"CLng", fn_clng, 1, 1},
    {"CSng", fn_csng, 1, 1},
    {"Date", fn_date, 0, 0},
    {"Time", fn_time, 0, 0},
    {"Now", fn_now, 0, 0},
    {"Day", fn_day, 1, 1},
    {"Month", fn_month, 1, 1},
    {"Year", fn_year, 1, 1},
    {"Hour", fn_hour, 1, 1},
    {"Minute", fn_minute, 1, 1},
    {"Second", fn_second, 1, 1},
    {"Weekday", fn_weekday, 1, 2},
    {"DateSerial", fn_dateserial, 3, 3},
    {"TimeSerial", fn_timeserial, 3, 3},
    {"DateValue", fn_datevalue, 1, 1},
    {"TimeValue", fn_timevalue, 1, 1},
    {"IsNull", fn_isnull, 1, 1},
    {"IsEmpty", fn_isempty, 1, 1},
    {"IsNumeric", fn_isnumeric, 1, 1},
    {"IsObject", fn_isobject, 1, 1},
    {"IsArray", fn_isarray, 1, 1},
    {"TypeName", fn_typename, 1, 1},
    {"VarType", fn_vartype, 1, 1},
    {"RGB", fn_rgb, 3, 3},
    {"Array", fn_array_fn, 0, 256},
    {"UBound", fn_ubound, 1, 2},
    {"LBound", fn_lbound, 1, 2},
    {"CreateObject", fn_createobject, 1, 2},
    {"WScript", NULL, 0, 0},
    {NULL, NULL, 0, 0}
};

void builtin_init(Interp *interp) {
    interp_set_var(interp, "WScript", val_obj(wscript_create(interp->script_path)), 0);

    for (int i = 0; builtins[i].name; i++) {
        if (builtins[i].func) {
            interp_set_var(interp, builtins[i].name, val_empty(), 0);
        }
    }

    interp_set_var(interp, "vbOKOnly", val_int(0), 1);
    interp_set_var(interp, "vbOKCancel", val_int(1), 1);
    interp_set_var(interp, "vbAbortRetryIgnore", val_int(2), 1);
    interp_set_var(interp, "vbYesNoCancel", val_int(3), 1);
    interp_set_var(interp, "vbYesNo", val_int(4), 1);
    interp_set_var(interp, "vbRetryCancel", val_int(5), 1);
    interp_set_var(interp, "vbCritical", val_int(16), 1);
    interp_set_var(interp, "vbQuestion", val_int(32), 1);
    interp_set_var(interp, "vbExclamation", val_int(48), 1);
    interp_set_var(interp, "vbInformation", val_int(64), 1);
    interp_set_var(interp, "vbOK", val_int(1), 1);
    interp_set_var(interp, "vbCancel", val_int(2), 1);
    interp_set_var(interp, "vbAbort", val_int(3), 1);
    interp_set_var(interp, "vbRetry", val_int(4), 1);
    interp_set_var(interp, "vbIgnore", val_int(5), 1);
    interp_set_var(interp, "vbYes", val_int(6), 1);
    interp_set_var(interp, "vbNo", val_int(7), 1);
    interp_set_var(interp, "vbBinaryCompare", val_int(0), 1);
    interp_set_var(interp, "vbTextCompare", val_int(1), 1);
    interp_set_var(interp, "vbSunday", val_int(1), 1);
    interp_set_var(interp, "vbMonday", val_int(2), 1);
    interp_set_var(interp, "vbTuesday", val_int(3), 1);
    interp_set_var(interp, "vbWednesday", val_int(4), 1);
    interp_set_var(interp, "vbThursday", val_int(5), 1);
    interp_set_var(interp, "vbFriday", val_int(6), 1);
    interp_set_var(interp, "vbSaturday", val_int(7), 1);
    interp_set_var(interp, "vbUseSystemDayOfWeek", val_int(0), 1);
    interp_set_var(interp, "vbFirstJan1", val_int(1), 1);
    interp_set_var(interp, "vbFirstFourDays", val_int(2), 1);
    interp_set_var(interp, "vbFirstFullWeek", val_int(3), 1);
    interp_set_var(interp, "ForReading", val_int(1), 1);
    interp_set_var(interp, "ForWriting", val_int(2), 1);
    interp_set_var(interp, "ForAppending", val_int(8), 1);
    interp_set_var(interp, "TristateFalse", val_int(0), 1);
    interp_set_var(interp, "TristateTrue", val_int(-1), 1);
    interp_set_var(interp, "TristateUseDefault", val_int(-2), 1);
    interp_set_var(interp, "vbCrLf", val_str("\r\n"), 1);
    interp_set_var(interp, "vbCr", val_str("\r"), 1);
    interp_set_var(interp, "vbLf", val_str("\n"), 1);
    interp_set_var(interp, "vbNewLine", val_str("\r\n"), 1);
    interp_set_var(interp, "vbTab", val_str("\t"), 1);
    interp_set_var(interp, "vbBack", val_str("\b"), 1);
    interp_set_var(interp, "vbFormFeed", val_str("\f"), 1);
    interp_set_var(interp, "vbVerticalTab", val_str("\x0B"), 1);
    interp_set_var(interp, "vbNullChar", val_str("\0"), 1);
    interp_set_var(interp, "vbNullString", val_str(""), 1);
    interp_set_var(interp, "vbSingleQuote", val_str("'"), 1);
    interp_set_var(interp, "vbDoubleQuote", val_str("\""), 1);
    interp_set_var(interp, "vbObjectError", val_int(-2147221504), 1);
    interp_set_var(interp, "Empty", val_empty(), 1);
    interp_set_var(interp, "Null", val_null(), 1);
    interp_set_var(interp, "Nothing", val_nothing(), 1);
    interp_set_var(interp, "vbBlack", val_int(0), 1);
    interp_set_var(interp, "vbRed", val_int(255), 1);
    interp_set_var(interp, "vbGreen", val_int(65280), 1);
    interp_set_var(interp, "vbBlue", val_int(16711680), 1);
    interp_set_var(interp, "vbWhite", val_int(16777215), 1);
    interp_set_var(interp, "vbYellow", val_int(65535), 1);
    interp_set_var(interp, "vbCyan", val_int(16776960), 1);
    interp_set_var(interp, "vbMagenta", val_int(16711935), 1);
    interp_set_var(interp, "vbUserType", val_int(9), 1);
}

Value builtin_call(Interp *interp, const char *name, int argc, Value *argv) {
    for (int i = 0; builtins[i].name; i++) {
        if (strcasecmp(name, builtins[i].name) == 0) {
            if (builtins[i].func) {
                return builtins[i].func(interp, argc, argv);
            }
            break;
        }
    }
    return val_empty();
}