#include "parser.h"
#include "display.h"

#include <stdio.h>
#include <string.h>

#define MAX_INPUT 1024

/* 2つ目以降の引数を式として結合して評価し結果を表示する共通処理 */
static int run_expr_from_args(int argc, char *argv[], const char *usage) {
    if (argc < 3) {
        fprintf(stderr, "%s\n", usage);
        return 1;
    }

    char expr[MAX_INPUT];
    int pos = 0;
    for (int i = 2; i < argc; i++) {
        if (i > 2 && pos < MAX_INPUT - 1)
            expr[pos++] = ' ';
        const char *s = argv[i];
        while (*s && pos < MAX_INPUT - 1)
            expr[pos++] = *s++;
    }
    expr[pos] = '\0';

    char  errmsg[256];
    Value result = calc_eval(expr, val_int(0), errmsg, sizeof(errmsg));
    if (errmsg[0]) {
        fprintf(stderr, "エラー: %s\n", errmsg);
        return 1;
    }
    print_result(result);
    printf("\n");
    return 0;
}

/* -e モード: 式を評価して結果を表示 */
static int run_eval_mode(int argc, char *argv[]) {
    return run_expr_from_args(argc, argv, "使用法: calc -e <式>");
}

/* -b モード: ビット演算式を評価して結果を表示 */
static int run_bitwise_mode(int argc, char *argv[]) {
    return run_expr_from_args(argc, argv, "使用法: calc -b <式>");
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "-e") == 0)
            return run_eval_mode(argc, argv);
        if (strcmp(argv[1], "-b") == 0)
            return run_bitwise_mode(argc, argv);
        fprintf(stderr, "不明なオプション: %s\n使用法: calc -e <式> / calc -b <式>\n", argv[1]);
        return 1;
    }
    char  input[MAX_INPUT];
    char  errmsg[256];
    Value prev = val_int(0);

    printf("calc-pgm v" VERSION " - プログラミング用計算機\n");
    printf("'help' でヘルプ, 'quit' または 'exit' で終了\n\n");

    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin))
            break;

        /* 末尾の空白・改行を除去 */
        int len = (int)strlen(input);
        while (len > 0 &&
               (input[len-1] == '\n' || input[len-1] == '\r' ||
                input[len-1] == ' '  || input[len-1] == '\t'))
            input[--len] = '\0';

        /* 先頭の空白をスキップ */
        char *p = input;
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '\0') continue;

        if (strcmp(p, "quit") == 0 || strcmp(p, "exit") == 0)
            break;
        if (strcmp(p, "help") == 0) {
            print_help();
            continue;
        }
        if (strcmp(p, "types") == 0) {
            print_types();
            continue;
        }
        if (strcmp(p, "enc") == 0) {
            print_encodings();
            continue;
        }
        if (strcmp(p, "size") == 0 || strncmp(p, "size ", 5) == 0) {
            const char *s = (p[4] == ' ') ? p + 5 : "";
            while (*s == ' ' || *s == '\t') s++;
            if (*s == '\0') printf("  使用法: size <文字列>\n\n");
            else            print_size(s);
            continue;
        }

        Value result = calc_eval(p, prev, errmsg, sizeof(errmsg));
        if (errmsg[0])
            printf("  エラー: %s\n", errmsg);
        else {
            print_result(result);
            prev = result;
        }
        printf("\n");
    }

    printf("終了します。\n");
    return 0;
}
