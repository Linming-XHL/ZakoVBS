#include "vbs.h"
#include <gtk/gtk.h>

static char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "错误: 无法打开文件 '%s'\n", path);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) {
        fprintf(stderr, "错误: 内存不足\n");
        fclose(fp);
        return NULL;
    }
    if (fread(buf, 1, len, fp) != (size_t)len) {}
    buf[len] = 0;
    fclose(fp);
    return buf;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: vbs <文件路径> [参数...]\n");
        fprintf(stderr, "运行 VBScript 脚本文件\n");
        return 1;
    }

    gtk_init(&argc, &argv);

    const char *path = argv[1];
    char *src = read_file(path);
    if (!src) return 1;

    Lexer *lx = lexer_new(src);
    Parser *parser = parser_new(lx);

    if (!parser_parse(parser)) {
        fprintf(stderr, "语法错误\n");
        free(src);
        lexer_free(lx);
        parser_free(parser);
        return 1;
    }

    Interp *interp = interp_new(parser->program, path);

    for (int i = 2; i < argc; i++) {
        interp_add_arg(interp, argv[i]);
    }

    int result = interp_run(interp);

    if (interp->error_occured && !interp->on_error_resume) {
        fprintf(stderr, "运行时错误: %s\n", interp->error_msg);
        result = 1;
    }

    interp_free(interp);
    parser_free(parser);
    lexer_free(lx);
    free(src);

    return result;
}