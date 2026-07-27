#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdio.h>

#include "value.h"

#define VERSION "1.0"

/* stdout へ出力する既存 API */
void print_result(Value v);
void print_help(void);
void print_types(void);
void print_encodings(void);
void print_size(const char *str);
void print_log_result(double x);

/*
 * 出力先を FILE* で指定する版。
 * print_* はこれらに stdout を渡す薄いラッパー。
 * open_memstream などのメモリストリームへ整形出力を取り込む用途に使う
 * (エージェント連携の C シムが利用)。
 */
void fprint_result(FILE *fp, Value v);
void fprint_types(FILE *fp);
void fprint_encodings(FILE *fp);
void fprint_size(FILE *fp, const char *str);
void fprint_log_result(FILE *fp, double x);

#endif /* DISPLAY_H */
