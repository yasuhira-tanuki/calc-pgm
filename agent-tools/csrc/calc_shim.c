/*
 * calc-pgm の計算エンジン(calc_eval)と表示整形(display)を、
 * FFI から文字列として利用するための薄い C シム。
 *
 * display の各関数を open_memstream 上の FILE* へ出力させ、その内容を
 * 呼び出し側のバッファに詰めて返す。CLI と整形結果を一致させ、
 * かつプロセスの stdout には一切触れない(MCP の stdio 通信路と非干渉)。
 *
 * 戻り値が int の関数: 0=成功、1=エラー(err にメッセージを格納)。
 */

#define _POSIX_C_SOURCE 200809L /* open_memstream */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "display.h"
#include "value.h"

#define SHIM_EXPR_MAX 1024

/* fn(fp, arg...) が書き出した内容を out(サイズ outsz)へ格納する共通処理 */
static void capture_result(Value v, char *out, int outsz) {
    char  *buf = NULL;
    size_t len = 0;
    FILE  *m = open_memstream(&buf, &len);
    if (m) {
        fprint_result(m, v);
        fclose(m);
    }
    snprintf(out, outsz, "%s", buf ? buf : "");
    free(buf);
}

/* -e / -b: 式を演算して結果を整形 */
int calc_eval_str(const char *expr, char *out, int outsz, char *err, int errsz) {
    char  errmsg[256];
    Value v = calc_eval(expr, val_int(0), errmsg, (int)sizeof errmsg);
    if (errmsg[0]) { snprintf(err, errsz, "%s", errmsg); return 1; }
    capture_result(v, out, outsz);
    return 0;
}

/* -f: 形式 (dec/hex/oct/bin/all) 指定表示 */
int calc_format_str(const char *fmt_str, const char *expr,
                    char *out, int outsz, char *err, int errsz) {
    VFmt fmt;
    if      (strcmp(fmt_str, "dec") == 0) fmt = FMT_DEC;
    else if (strcmp(fmt_str, "hex") == 0) fmt = FMT_HEX;
    else if (strcmp(fmt_str, "oct") == 0) fmt = FMT_OCT;
    else if (strcmp(fmt_str, "bin") == 0) fmt = FMT_BIN;
    else if (strcmp(fmt_str, "all") == 0) fmt = FMT_ALL;
    else { snprintf(err, errsz, "不明な形式: %s (dec/hex/oct/bin/all)", fmt_str); return 1; }

    char  errmsg[256];
    Value v = calc_eval(expr, val_int(0), errmsg, (int)sizeof errmsg);
    if (errmsg[0]) { snprintf(err, errsz, "%s", errmsg); return 1; }
    if (v.is_float && fmt != FMT_ALL && fmt != FMT_DEC) {
        snprintf(err, errsz, "hex/oct/bin は整数のみ対応");
        return 1;
    }
    v.fmt = fmt;
    capture_result(v, out, outsz);
    return 0;
}

/* -u: 単位変換 conv(<値>, <変換前>, <変換後>) */
int calc_conv_str(const char *value, const char *from, const char *to,
                  char *out, int outsz, char *err, int errsz) {
    char expr[SHIM_EXPR_MAX];
    int  n = snprintf(expr, sizeof expr, "conv(%s, %s, %s)", value, from, to);
    if (n < 0 || n >= (int)sizeof expr) { snprintf(err, errsz, "式が長すぎます"); return 1; }

    char  errmsg[256];
    Value v = calc_eval(expr, val_int(0), errmsg, (int)sizeof errmsg);
    if (errmsg[0]) { snprintf(err, errsz, "%s", errmsg); return 1; }
    capture_result(v, out, outsz);
    return 0;
}

/* -l: ln / log2 / log10 を一括表示 */
int calc_log_str(const char *expr, char *out, int outsz, char *err, int errsz) {
    char  errmsg[256];
    Value v = calc_eval(expr, val_int(0), errmsg, (int)sizeof errmsg);
    if (errmsg[0]) { snprintf(err, errsz, "%s", errmsg); return 1; }
    double x = v.is_float ? v.dval : (double)v.ival;
    if (x <= 0.0) { snprintf(err, errsz, "対数の引数は正の数が必要"); return 1; }

    char  *buf = NULL;
    size_t len = 0;
    FILE  *m = open_memstream(&buf, &len);
    if (m) { fprint_log_result(m, x); fclose(m); }
    snprintf(out, outsz, "%s", buf ? buf : "");
    free(buf);
    return 0;
}

/* -s: 文字数・各エンコードのバイト数 */
void calc_size_str(const char *text, char *out, int outsz) {
    char  *buf = NULL;
    size_t len = 0;
    FILE  *m = open_memstream(&buf, &len);
    if (m) { fprint_size(m, text); fclose(m); }
    snprintf(out, outsz, "%s", buf ? buf : "");
    free(buf);
}

/* -t: 整数型・浮動小数点型の一覧 */
void calc_types_str(char *out, int outsz) {
    char  *buf = NULL;
    size_t len = 0;
    FILE  *m = open_memstream(&buf, &len);
    if (m) { fprint_types(m); fclose(m); }
    snprintf(out, outsz, "%s", buf ? buf : "");
    free(buf);
}

/* -c: 文字コードの一覧 */
void calc_encodings_str(char *out, int outsz) {
    char  *buf = NULL;
    size_t len = 0;
    FILE  *m = open_memstream(&buf, &len);
    if (m) { fprint_encodings(m); fclose(m); }
    snprintf(out, outsz, "%s", buf ? buf : "");
    free(buf);
}
