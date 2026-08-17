#define _GNU_SOURCE
#include "vbs.h"
#include <gtk/gtk.h>
#include <regex.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>

static void expand_environment(const char *src, char *dst, int dstsize) {
    int pos = 0;
    const char *p = src;
    while (*p && pos < dstsize - 1) {
        if (*p == '%') {
            const char *end = strchr(p + 1, '%');
            if (end) {
                char var[256];
                int len = end - p - 1;
                if (len > 0 && len < 256) {
                    strncpy(var, p + 1, len);
                    var[len] = 0;
                    const char *val = getenv(var);
                    if (val) {
                        strncpy(dst + pos, val, dstsize - pos - 1);
                        pos += strlen(val);
                    }
                    p = end + 1;
                    continue;
                }
            }
        }
        dst[pos++] = *p++;
    }
    dst[pos] = 0;
}

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

static struct tm parse_datetime(const char *s) {
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    if (!strptime(s, "%Y-%m-%d %H:%M:%S", &tm) &&
        !strptime(s, "%Y-%m-%d %H:%M", &tm) &&
        !strptime(s, "%Y-%m-%d", &tm) &&
        !strptime(s, "%Y/%m/%d %H:%M:%S", &tm) &&
        !strptime(s, "%Y/%m/%d", &tm)) {
        strptime(s, "%H:%M:%S", &tm);
    }
    return tm;
}

static void apply_interval(struct tm *tm, const char *iv, long long num) {
    if (!iv) return;
    if (strcasecmp(iv, "yyyy") == 0) tm->tm_year += num;
    else if (strcasecmp(iv, "q") == 0) tm->tm_mon += num * 3;
    else if (strcasecmp(iv, "m") == 0) tm->tm_mon += num;
    else if (strcasecmp(iv, "y") == 0 || strcasecmp(iv, "d") == 0 ||
             strcasecmp(iv, "w") == 0 || strcasecmp(iv, "ww") == 0)
        tm->tm_mday += num;
    else if (strcasecmp(iv, "h") == 0) tm->tm_hour += num;
    else if (strcasecmp(iv, "n") == 0) tm->tm_min += num;
    else if (strcasecmp(iv, "s") == 0) tm->tm_sec += num;
}

static Value fn_dateadd(Interp *interp, int argc, Value *argv) {
    if (argc < 3) return val_str("");
    char *iv = val_tostr(argv[0]);
    long long num = val_toint(argv[1]);
    char *date = val_tostr(argv[2]);
    struct tm tm = parse_datetime(date);
    apply_interval(&tm, iv, num);
    char buf[64];
    if (strpbrk(date, ":") && !strpbrk(date, "-")) {
        strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
    } else {
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    }
    free(iv);
    free(date);
    return val_str(buf);
}

static long long diff_interval(const struct tm *t1, const struct tm *t2, const char *iv) {
    if (!iv) return 0;
    time_t a = mktime((struct tm*)t1);
    time_t b = mktime((struct tm*)t2);
    double diff = difftime(b, a);
    if (strcasecmp(iv, "s") == 0) return (long long)diff;
    if (strcasecmp(iv, "n") == 0) return (long long)(diff / 60);
    if (strcasecmp(iv, "h") == 0) return (long long)(diff / 3600);
    if (strcasecmp(iv, "d") == 0 || strcasecmp(iv, "y") == 0) return (long long)(diff / 86400);
    if (strcasecmp(iv, "w") == 0 || strcasecmp(iv, "ww") == 0) return (long long)(diff / (86400 * 7));
    if (strcasecmp(iv, "m") == 0) {
        long long months = (t2->tm_year - t1->tm_year) * 12 + (t2->tm_mon - t1->tm_mon);
        return months;
    }
    if (strcasecmp(iv, "q") == 0) {
        long long months = (t2->tm_year - t1->tm_year) * 12 + (t2->tm_mon - t1->tm_mon);
        return months / 3;
    }
    if (strcasecmp(iv, "yyyy") == 0) return t2->tm_year - t1->tm_year;
    return (long long)diff;
}

static Value fn_datediff(Interp *interp, int argc, Value *argv) {
    if (argc < 3) return val_int(0);
    char *iv = val_tostr(argv[0]);
    char *d1 = val_tostr(argv[1]);
    char *d2 = val_tostr(argv[2]);
    struct tm t1 = parse_datetime(d1);
    struct tm t2 = parse_datetime(d2);
    long long r = diff_interval(&t1, &t2, iv);
    free(iv);
    free(d1);
    free(d2);
    return val_int(r);
}

static Value fn_datepart(Interp *interp, int argc, Value *argv) {
    if (argc < 2) return val_int(0);
    char *iv = val_tostr(argv[0]);
    char *date = val_tostr(argv[1]);
    struct tm tm = parse_datetime(date);
    long long r = 0;
    if (strcasecmp(iv, "yyyy") == 0) r = tm.tm_year + 1900;
    else if (strcasecmp(iv, "q") == 0) r = tm.tm_mon / 3 + 1;
    else if (strcasecmp(iv, "m") == 0) r = tm.tm_mon + 1;
    else if (strcasecmp(iv, "y") == 0) r = tm.tm_yday + 1;
    else if (strcasecmp(iv, "d") == 0) r = tm.tm_mday;
    else if (strcasecmp(iv, "w") == 0) r = tm.tm_wday + 1;
    else if (strcasecmp(iv, "ww") == 0) r = tm.tm_yday / 7 + 1;
    else if (strcasecmp(iv, "h") == 0) r = tm.tm_hour;
    else if (strcasecmp(iv, "n") == 0) r = tm.tm_min;
    else if (strcasecmp(iv, "s") == 0) r = tm.tm_sec;
    free(iv);
    free(date);
    return val_int(r);
}

static Value fn_timer(Interp *interp, int argc, Value *argv) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    double secs = tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
    return val_double(secs);
}

static Value fn_isdate(Interp *interp, int argc, Value *argv) {
    if (argv[0].type != VALTYPE_STRING) return val_bool(0);
    char *s = val_tostr(argv[0]);
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    int ok = strptime(s, "%Y-%m-%d %H:%M:%S", &tm) ||
             strptime(s, "%Y-%m-%d", &tm) ||
             strptime(s, "%Y/%m/%d", &tm) ||
             strptime(s, "%H:%M:%S", &tm);
    free(s);
    return val_bool(ok ? 1 : 0);
}

static Value fn_formatdatetime(Interp *interp, int argc, Value *argv) {
    if (argc < 1) return val_str("");
    char *s = val_tostr(argv[0]);
    int fmt = (argc >= 2) ? (int)val_toint(argv[1]) : 0;
    struct tm tm = parse_datetime(s);
    char buf[128];
    switch (fmt) {
        case 1: strftime(buf, sizeof(buf), "yyyy年m月d日", &tm); break;
        case 2: strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm); break;
        case 3: strftime(buf, sizeof(buf), "%H:%M", &tm); break;
        case 4: strftime(buf, sizeof(buf), "%H:%M:%S", &tm); break;
        default: strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    }
    free(s);
    return val_str(buf);
}

static Value fn_formatnumber(Interp *interp, int argc, Value *argv) {
    double d = val_todouble(argv[0]);
    int digits = (argc >= 2) ? (int)val_toint(argv[1]) : -1;
    char buf[64];
    if (digits >= 0) {
        snprintf(buf, sizeof(buf), "%.*f", digits, d);
    } else {
        snprintf(buf, sizeof(buf), "%.15g", d);
    }
    return val_str(buf);
}

static Value fn_formatpercent(Interp *interp, int argc, Value *argv) {
    double d = val_todouble(argv[0]);
    int digits = (argc >= 2) ? (int)val_toint(argv[1]) : 2;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f%%", digits, d * 100);
    return val_str(buf);
}

static Value fn_formatcurrency(Interp *interp, int argc, Value *argv) {
    double d = val_todouble(argv[0]);
    int digits = (argc >= 2) ? (int)val_toint(argv[1]) : 2;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", digits, d);
    return val_str(buf);
}

static Value fn_filter(Interp *interp, int argc, Value *argv) {
    if (argv[0].type != VALTYPE_ARRAY) return val_empty();
    VbsArray *src = argv[0].as.arr;
    char *match = val_tostr(argv[1]);
    int include = (argc >= 3) ? val_tobool(argv[2]) : 1;
    int comp = (argc >= 4) ? (int)val_toint(argv[3]) : 0;

    VbsArray *a = arr_new_1d(0);
    int count = 0;
    for (int i = 0; i < src->total_size; i++) {
        char *s = val_tostr(src->data[i]);
        int found = comp ? (strcasestr(s, match) != NULL) : (strstr(s, match) != NULL);
        free(s);
        if (found == include) {
            if (count >= a->total_size) {
                a->dims[0] = count + 1;
                a->data = realloc(a->data, sizeof(Value) * (count + 1));
                memset(&a->data[count], 0, sizeof(Value));
                a->total_size = a->dims[0];
            }
            a->data[count++] = val_clone(src->data[i]);
        }
    }
    a->dims[0] = count;
    free(match);
    return val_arr(a);
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
    } else if (strcasecmp(name, "VBScript.RegExp") == 0 ||
        strcasecmp(name, "RegExp") == 0) {
        obj = regexp_create();
    } else if (strcasecmp(name, "WScript.Shell") == 0 ||
        strcasecmp(name, "WshShell") == 0 ||
        strcasecmp(name, "Shell") == 0) {
        obj = wshshell_create();
    } else if (strcasecmp(name, "WScript.Network") == 0 ||
        strcasecmp(name, "WshNetwork") == 0) {
        obj = obj_new("WshNetwork", NULL, NULL, NULL, NULL);
    } else {
        interp_error_num(interp, 429, "CreateObject: 不支持的对象: %s", name);
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
static void wscript_destroy(Object *obj) {
    (void)obj;
}

static Value wscript_get_prop(Object *obj, const char *name) {
    Interp *interp = (Interp*)obj->data;
    if (strcasecmp(name, "FullName") == 0) {
        return val_str("vbs");
    }
    if (strcasecmp(name, "Path") == 0) {
        return val_str(".");
    }
    if (strcasecmp(name, "ScriptFullName") == 0) {
        return val_str(interp && interp->script_path ? interp->script_path : "");
    }
    if (strcasecmp(name, "ScriptName") == 0) {
        const char *p = interp && interp->script_path ? interp->script_path : "";
        const char *slash = strrchr(p, '/');
        return val_str(slash ? slash + 1 : p);
    }
    if (strcasecmp(name, "Arguments") == 0 ||
        strcasecmp(name, "Arguments.Count") == 0) {
        return val_int(interp ? interp->script_arg_count : 0);
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
    if (strcasecmp(name, "Arguments") == 0) {
        int count = interp ? interp->script_arg_count : 0;
        int idx = (argc >= 1) ? (int)val_toint(argv[0]) : 0;
        if (idx >= 0 && idx < count) {
            return val_str(interp->script_args[idx]);
        }
        return val_str("");
    }
    return val_empty();
}

Object *wscript_create(Interp *interp) {
    return obj_new("WScript", interp, wscript_destroy, wscript_get_prop, wscript_call_method);
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

typedef struct { char *path; } FolderData;
static void folder_destroy(Object *obj);
static Value folder_get_prop(Object *obj, const char *name);
static Value folder_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv);
typedef struct { char *path; } FileData;
static void file_destroy(Object *obj);
static Value file_get_prop(Object *obj, const char *name);
static Value file_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv);
typedef struct { char *letter; } DriveData;
static void drive_destroy(Object *obj);
static Value drive_get_prop(Object *obj, const char *name);

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
    if (strcasecmp(name, "CopyFile") == 0) {
        if (argc < 2) return val_empty();
        char *src = val_tostr(argv[0]);
        char *dest = val_tostr(argv[1]);
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", src, dest);
        if (system(cmd) == -1) {}
        free(src);
        free(dest);
        return val_empty();
    }
    if (strcasecmp(name, "MoveFile") == 0) {
        if (argc < 2) return val_empty();
        char *src = val_tostr(argv[0]);
        char *dest = val_tostr(argv[1]);
        rename(src, dest);
        free(src);
        free(dest);
        return val_empty();
    }
    if (strcasecmp(name, "CreateFolder") == 0) {
        if (argc < 1) return val_empty();
        char *p = val_tostr(argv[0]);
        mkdir(p, 0777);
        free(p);
        return val_empty();
    }
    if (strcasecmp(name, "DeleteFolder") == 0) {
        if (argc < 1) return val_empty();
        char *p = val_tostr(argv[0]);
        rmdir(p);
        free(p);
        return val_empty();
    }
    if (strcasecmp(name, "FolderExists") == 0) {
        if (argc < 1) return val_bool(0);
        char *p = val_tostr(argv[0]);
        struct stat st;
        int r = (stat(p, &st) == 0 && S_ISDIR(st.st_mode));
        free(p);
        return val_bool(r);
    }
    if (strcasecmp(name, "GetFolder") == 0) {
        if (argc < 1) return val_empty();
        FolderData *fd = calloc(1, sizeof(FolderData));
        fd->path = val_tostr(argv[0]);
        return val_obj(obj_new("Folder", fd, folder_destroy, folder_get_prop, folder_call_method));
    }
    if (strcasecmp(name, "GetFile") == 0) {
        if (argc < 1) return val_empty();
        FileData *fd = calloc(1, sizeof(FileData));
        fd->path = val_tostr(argv[0]);
        return val_obj(obj_new("File", fd, file_destroy, file_get_prop, file_call_method));
    }
    if (strcasecmp(name, "GetDrive") == 0) {
        if (argc < 1) return val_empty();
        DriveData *dd = calloc(1, sizeof(DriveData));
        dd->letter = val_tostr(argv[0]);
        return val_obj(obj_new("Drive", dd, drive_destroy, drive_get_prop, NULL));
    }
    if (strcasecmp(name, "GetSpecialFolder") == 0) {
        if (argc < 1) return val_empty();
        int which = (int)val_toint(argv[0]);
        const char *path = ".";
        if (which == 0) path = getenv("WINDIR") ? getenv("WINDIR") : "/tmp";
        else if (which == 1) path = getenv("SYSTEMROOT") ? getenv("SYSTEMROOT") : "/usr";
        else if (which == 2) path = getenv("TEMP") ? getenv("TEMP") : "/tmp";
        FolderData *fd = calloc(1, sizeof(FolderData));
        fd->path = strdup(path);
        return val_obj(obj_new("Folder", fd, folder_destroy, folder_get_prop, folder_call_method));
    }
    return val_empty();
}

/* ========== Folder / File / Drive 对象 ========== */
static void folder_destroy(Object *obj) {
    FolderData *d = (FolderData*)obj->data;
    if (d) { free(d->path); free(d); }
}

static char *path_basename(const char *path) {
    const char *p = strrchr(path, '/');
    if (!p) p = strrchr(path, '\\');
    return strdup(p ? p + 1 : path);
}

static char *time_str(const struct stat *st) {
    struct tm tm;
    localtime_r(&st->st_mtime, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return strdup(buf);
}

static Value folder_get_prop(Object *obj, const char *name) {
    FolderData *d = (FolderData*)obj->data;
    if (!d) return val_empty();
    if (strcasecmp(name, "Path") == 0) return val_str(d->path);
    if (strcasecmp(name, "Name") == 0) {
        char *b = path_basename(d->path);
        Value r = val_str(b);
        free(b);
        return r;
    }
    if (strcasecmp(name, "ParentFolder") == 0) {
        char *copy = strdup(d->path);
        char *slash = strrchr(copy, '/');
        if (!slash) slash = strrchr(copy, '\\');
        if (slash) *slash = 0;
        Value r = val_str(copy);
        free(copy);
        return r;
    }
    if (strcasecmp(name, "Size") == 0) {
        struct stat st;
        if (stat(d->path, &st) == 0) return val_int(st.st_size);
        return val_int(0);
    }
    if (strcasecmp(name, "DateCreated") == 0 ||
        strcasecmp(name, "DateLastModified") == 0) {
        struct stat st;
        if (stat(d->path, &st) == 0) {
            char *s = time_str(&st);
            Value r = val_str(s);
            free(s);
            return r;
        }
        return val_str("");
    }
    return val_empty();
}

static Value folder_files_array(const char *path, int want_files) {
    DIR *dir = opendir(path);
    VbsArray *a = arr_new_1d(0);
    int count = 0;
    if (dir) {
        struct dirent *e;
        while ((e = readdir(dir)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            char full[4096];
            snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
            struct stat st;
            if (stat(full, &st) != 0) continue;
            int is_dir = S_ISDIR(st.st_mode);
            if (want_files && is_dir) continue;
            if (!want_files && !is_dir) continue;
            if (count >= a->total_size) {
                a->dims[0] = count + 1;
                a->data = realloc(a->data, sizeof(Value) * (count + 1));
                memset(&a->data[count], 0, sizeof(Value));
                a->total_size = a->dims[0];
            }
            a->data[count++] = val_str(e->d_name);
        }
        closedir(dir);
    }
    a->dims[0] = count;
    return val_arr(a);
}

static Value folder_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv) {
    FolderData *d = (FolderData*)obj->data;
    if (strcasecmp(name, "Files") == 0) {
        return folder_files_array(d->path, 1);
    }
    if (strcasecmp(name, "SubFolders") == 0) {
        return folder_files_array(d->path, 0);
    }
    if (strcasecmp(name, "Delete") == 0) {
        rmdir(d->path);
        return val_empty();
    }
    if (strcasecmp(name, "Move") == 0 || strcasecmp(name, "Copy") == 0) {
        if (argc < 1) return val_empty();
        char *dest = val_tostr(argv[0]);
        if (strcasecmp(name, "Move") == 0) {
            rename(d->path, dest);
        } else {
            char cmd[4096];
            snprintf(cmd, sizeof(cmd), "cp -r \"%s\" \"%s\"", d->path, dest);
            if (system(cmd) == -1) {}
        }
        free(dest);
        return val_empty();
    }
    return val_empty();
}

static void file_destroy(Object *obj) {
    FileData *d = (FileData*)obj->data;
    if (d) { free(d->path); free(d); }
}

static Value file_get_prop(Object *obj, const char *name) {
    FileData *d = (FileData*)obj->data;
    if (!d) return val_empty();
    if (strcasecmp(name, "Path") == 0) return val_str(d->path);
    if (strcasecmp(name, "Name") == 0) {
        char *b = path_basename(d->path);
        Value r = val_str(b);
        free(b);
        return r;
    }
    struct stat st;
    if (stat(d->path, &st) == 0) {
        if (strcasecmp(name, "Size") == 0) return val_int(st.st_size);
        if (strcasecmp(name, "DateCreated") == 0 ||
            strcasecmp(name, "DateLastModified") == 0) {
            char *s = time_str(&st);
            Value r = val_str(s);
            free(s);
            return r;
        }
    }
    return val_empty();
}

static Value file_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv) {
    FileData *d = (FileData*)obj->data;
    if (strcasecmp(name, "Delete") == 0) {
        remove(d->path);
        return val_empty();
    }
    if (strcasecmp(name, "Move") == 0) {
        if (argc < 1) return val_empty();
        char *dest = val_tostr(argv[0]);
        rename(d->path, dest);
        free(dest);
        return val_empty();
    }
    if (strcasecmp(name, "Copy") == 0) {
        if (argc < 1) return val_empty();
        char *dest = val_tostr(argv[0]);
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", d->path, dest);
        if (system(cmd) == -1) {}
        free(dest);
        return val_empty();
    }
    if (strcasecmp(name, "OpenAsTextStream") == 0) {
        int iomode = (argc >= 1) ? (int)val_toint(argv[0]) : 1;
        FILE *fp = NULL;
        switch (iomode) {
            case 1: fp = fopen(d->path, "r"); break;
            case 2: fp = fopen(d->path, "w"); break;
            case 8: fp = fopen(d->path, "a"); break;
            default: fp = fopen(d->path, "r");
        }
        if (!fp) {
            interp_error(interp, "无法打开文件: %s", d->path);
            return val_empty();
        }
        FSOData *fd = calloc(1, sizeof(FSOData));
        fd->fp = fp;
        fd->open = 1;
        fd->for_reading = (iomode == 1);
        return val_obj(obj_new("TextStream", fd, fso_destroy, textstream_get_prop, textstream_call_method));
    }
    return val_empty();
}

static void drive_destroy(Object *obj) {
    DriveData *d = (DriveData*)obj->data;
    if (d) { free(d->letter); free(d); }
}

static Value drive_get_prop(Object *obj, const char *name) {
    DriveData *d = (DriveData*)obj->data;
    if (!d) return val_empty();
    if (strcasecmp(name, "DriveLetter") == 0) return val_str(d->letter);
    if (strcasecmp(name, "Path") == 0) return val_str(d->letter);
    struct statvfs vfs;
    char mount[64];
    snprintf(mount, sizeof(mount), "/");
    if (statvfs(mount, &vfs) == 0) {
        if (strcasecmp(name, "FreeSpace") == 0)
            return val_int((long long)vfs.f_bavail * vfs.f_frsize);
        if (strcasecmp(name, "TotalSize") == 0)
            return val_int((long long)vfs.f_blocks * vfs.f_frsize);
        if (strcasecmp(name, "AvailableSpace") == 0)
            return val_int((long long)vfs.f_bavail * vfs.f_frsize);
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

/* ========== Err 对象 ========== */
static void err_destroy(Object *obj) {
    ErrData *d = (ErrData*)obj->data;
    if (d) {
        if (d->description) free(d->description);
        free(d);
    }
}

static Value err_get_prop(Object *obj, const char *name) {
    ErrData *d = (ErrData*)obj->data;
    if (!d) return val_empty();
    if (strcasecmp(name, "Number") == 0) return val_int(d->number);
    if (strcasecmp(name, "Description") == 0) return val_str(d->description ? d->description : "");
    if (strcasecmp(name, "Source") == 0) return val_str("");
    if (strcasecmp(name, "HelpFile") == 0) return val_str("");
    if (strcasecmp(name, "HelpContext") == 0) return val_int(0);
    return val_empty();
}

static Value err_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv) {
    ErrData *d = (ErrData*)obj->data;
    if (strcasecmp(name, "Clear") == 0) {
        d->number = 0;
        if (d->description) { free(d->description); d->description = NULL; }
        return val_empty();
    }
    if (strcasecmp(name, "Raise") == 0) {
        if (argc >= 1) {
            int num = (int)val_toint(argv[0]);
            char *desc = (argc >= 3) ? val_tostr(argv[2]) : strdup("Runtime error");
            d->number = num;
            if (d->description) { free(d->description); d->description = NULL; }
            d->description = desc;
            interp_error_num(interp, num, "%s", d->description);
        }
        return val_empty();
    }
    return val_empty();
}

Object *err_create(void) {
    ErrData *d = calloc(1, sizeof(ErrData));
    return obj_new("Err", d, err_destroy, err_get_prop, err_call_method);
}

/* ========== RegExp 对象 ========== */
typedef struct {
    char *pattern;
    int global;
    int ignorecase;
    int multiline;
} RegexpData;

static void regexp_destroy(Object *obj) {
    RegexpData *d = (RegexpData*)obj->data;
    if (d) {
        if (d->pattern) free(d->pattern);
        free(d);
    }
}

/* 将 VBScript 风格的 \d \w \s \b 等转译为 POSIX 正则 */
static char *translate_pattern(const char *src) {
    char *buf = malloc(strlen(src) * 4 + 1);
    int pos = 0;
    for (int i = 0; src[i]; i++) {
        if (src[i] == '\\' && src[i + 1]) {
            char n = src[i + 1];
            switch (n) {
                case 'd': strcpy(buf + pos, "[0-9]"); pos += 5; i++; continue;
                case 'D': strcpy(buf + pos, "[^0-9]"); pos += 6; i++; continue;
                case 'w': strcpy(buf + pos, "[A-Za-z0-9_]"); pos += 12; i++; continue;
                case 'W': strcpy(buf + pos, "[^A-Za-z0-9_]"); pos += 13; i++; continue;
                case 's': strcpy(buf + pos, "[[:space:]]"); pos += 11; i++; continue;
                case 'S': strcpy(buf + pos, "[^[:space:]]"); pos += 12; i++; continue;
                case 'n': buf[pos++] = '\n'; i++; continue;
                case 't': buf[pos++] = '\t'; i++; continue;
                case 'r': buf[pos++] = '\r'; i++; continue;
                case 'f': buf[pos++] = '\f'; i++; continue;
                case 'v': buf[pos++] = '\v'; i++; continue;
                default: break;
            }
        }
        buf[pos++] = src[i];
    }
    buf[pos] = 0;
    return buf;
}

static int regexp_compile(RegexpData *d, regex_t *re) {
    if (!d->pattern) return -1;
    char *pat = translate_pattern(d->pattern);
    int flags = REG_EXTENDED;
    if (!d->ignorecase) flags |= REG_ICASE;
    int rc = regcomp(re, pat, flags);
    free(pat);
    return rc;
}

static Value regexp_get_prop(Object *obj, const char *name) {
    RegexpData *d = (RegexpData*)obj->data;
    if (!d) return val_empty();
    if (strcasecmp(name, "Pattern") == 0) return val_str(d->pattern ? d->pattern : "");
    if (strcasecmp(name, "Global") == 0) return val_bool(d->global);
    if (strcasecmp(name, "IgnoreCase") == 0) return val_bool(d->ignorecase);
    if (strcasecmp(name, "Multiline") == 0) return val_bool(d->multiline);
    return val_empty();
}

static Value regexp_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv) {
    RegexpData *d = (RegexpData*)obj->data;
    if (strcasecmp(name, "set_Pattern") == 0) {
        if (argc >= 1) {
            if (d->pattern) free(d->pattern);
            d->pattern = val_tostr(argv[0]);
        }
        return val_empty();
    }
    if (strcasecmp(name, "set_Global") == 0) {
        if (argc >= 1) d->global = val_tobool(argv[0]);
        return val_empty();
    }
    if (strcasecmp(name, "set_IgnoreCase") == 0) {
        if (argc >= 1) d->ignorecase = val_tobool(argv[0]);
        return val_empty();
    }
    if (strcasecmp(name, "set_Multiline") == 0) {
        if (argc >= 1) d->multiline = val_tobool(argv[0]);
        return val_empty();
    }
    if (strcasecmp(name, "Test") == 0) {
        if (argc < 1) return val_bool(0);
        if (!d->pattern) return val_bool(0);
        char *s = val_tostr(argv[0]);
        regex_t re;
        int rc = regexp_compile(d, &re);
        int matched = 0;
        if (rc == 0) {
            matched = regexec(&re, s, 0, NULL, 0) == 0;
            regfree(&re);
        }
        free(s);
        return val_bool(matched);
    }
    if (strcasecmp(name, "Replace") == 0) {
        if (argc < 2 || !d->pattern) return val_str("");
        char *s = val_tostr(argv[0]);
        char *repl = val_tostr(argv[1]);
        regex_t re;
        int rc = regexp_compile(d, &re);
        char *result;
        if (rc != 0) {
            result = strdup(s);
        } else {
            char buf[65536];
            int pos = 0;
            const char *p = s;
            regmatch_t m;
            int do_global = d->global;
            while (regexec(&re, p, 1, &m, 0) == 0 && pos < 65000) {
                int start = m.rm_so >= 0 ? m.rm_so : 0;
                strncpy(buf + pos, p, start);
                pos += start;
                strcpy(buf + pos, repl);
                pos += strlen(repl);
                p += m.rm_eo;
                if (!do_global) break;
                if (m.rm_eo <= 0) p++;  /* avoid infinite loop on zero-width match */
            }
            strcpy(buf + pos, p);
            pos += strlen(p);
            buf[pos] = 0;
            result = strdup(buf);
            regfree(&re);
        }
        free(s);
        free(repl);
        Value r = val_str(result);
        free(result);
        return r;
    }
    if (strcasecmp(name, "Execute") == 0) {
        if (argc < 1 || !d->pattern) return val_empty();
        char *s = val_tostr(argv[0]);
        regex_t re;
        int rc = regexp_compile(d, &re);
        if (rc != 0) {
            free(s);
            return val_empty();
        }
        const char *p = s;
        regmatch_t m;
        int do_global = d->global;
        VbsArray *a = arr_new_1d(0);
        int count = 0;
        while (regexec(&re, p, 1, &m, 0) == 0) {
            int start = m.rm_so >= 0 ? m.rm_so : 0;
            int length = (m.rm_eo >= 0 ? m.rm_eo : 0) - start;
            if (count >= a->total_size) {
                a->dims[0] = count + 1;
                a->data = realloc(a->data, sizeof(Value) * (count + 1));
                memset(&a->data[count], 0, sizeof(Value));
                a->total_size = a->dims[0];
            }
            char *match = malloc(length + 1);
            strncpy(match, p + start, length);
            match[length] = 0;
            a->data[count++] = val_str(match);
            free(match);
            p += m.rm_eo;
            if (!do_global) break;
            if (m.rm_eo <= 0) p++;
        }
        a->dims[0] = count;
        regfree(&re);
        free(s);
        return val_arr(a);
    }
    return val_empty();
}

Object *regexp_create(void) {
    RegexpData *d = calloc(1, sizeof(RegexpData));
    d->pattern = NULL;
    return obj_new("RegExp", d, regexp_destroy, regexp_get_prop, regexp_call_method);
}

/* ========== WshShell 对象 ========== */
typedef struct {
    int dummy;
} WshShellData;

static void wshshell_destroy(Object *obj) {
    if (obj->data) free(obj->data);
}

static Value wshshell_get_prop(Object *obj, const char *name) {
    if (strcasecmp(name, "CurrentDirectory") == 0) {
        char buf[4096];
        if (getcwd(buf, sizeof(buf))) return val_str(buf);
        return val_str("");
    }
    return val_empty();
}

static Value wshshell_call_method(Object *obj, Interp *interp, const char *name, int argc, Value *argv) {
    if (strcasecmp(name, "Run") == 0) {
        if (argc < 1) return val_int(0);
        char *cmd = val_tostr(argv[0]);
        int wait = (argc >= 3) ? val_tobool(argv[2]) : 0;
        int rc = system(cmd);
        free(cmd);
        if (wait) return val_int(rc);
        return val_int(0);
    }
    if (strcasecmp(name, "ExpandEnvironmentStrings") == 0) {
        if (argc < 1) return val_str("");
        char *s = val_tostr(argv[0]);
        char buf[8192];
        expand_environment(s, buf, sizeof(buf));
        free(s);
        return val_str(buf);
    }
    if (strcasecmp(name, "Popup") == 0) {
        if (argc < 1) return val_int(1);
        ensure_gtk();
        char *msg = val_tostr(argv[0]);
        char *title = (argc >= 3) ? val_tostr(argv[2]) : strdup("WScript");
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
            GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", msg);
        gtk_window_set_title(GTK_WINDOW(dialog), title);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        while (gtk_events_pending()) gtk_main_iteration();
        free(msg);
        free(title);
        return val_int(1);
    }
    if (strcasecmp(name, "SendKeys") == 0) {
        return val_empty();
    }
    if (strcasecmp(name, "AppActivate") == 0) {
        return val_empty();
    }
    if (strcasecmp(name, "CreateShortcut") == 0) {
        return val_empty();
    }
    if (strcasecmp(name, "Exec") == 0) {
        return val_empty();
    }
    return val_empty();
}

Object *wshshell_create(void) {
    WshShellData *d = calloc(1, sizeof(WshShellData));
    return obj_new("WshShell", d, wshshell_destroy, wshshell_get_prop, wshshell_call_method);
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
    {"DateAdd", fn_dateadd, 3, 3},
    {"DateDiff", fn_datediff, 3, 6},
    {"DatePart", fn_datepart, 2, 4},
    {"Timer", fn_timer, 0, 0},
    {"IsDate", fn_isdate, 1, 1},
    {"FormatDateTime", fn_formatdatetime, 1, 2},
    {"FormatNumber", fn_formatnumber, 1, 5},
    {"FormatPercent", fn_formatpercent, 1, 5},
    {"FormatCurrency", fn_formatcurrency, 1, 5},
    {"Filter", fn_filter, 2, 4},
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
    interp_set_var(interp, "WScript", val_obj(wscript_create(interp)), 0);

    Object *err = err_create();
    interp->err_obj = err;
    interp_set_var(interp, "Err", val_obj(err), 0);
    interp_set_var(interp, "RegExp", val_empty(), 0);

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