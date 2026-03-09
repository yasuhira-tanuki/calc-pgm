#include "parser.h"
#include "display.h"

#include <stdio.h>
#include <string.h>

#define MAX_INPUT 1024

int main(void) {
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
