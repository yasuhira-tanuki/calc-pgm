#include "lexer.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void lex_err(Lex *L, const char *msg) {
    if (!L->err) {
        snprintf(L->errmsg, sizeof(L->errmsg),
                 "字句エラー: %s (位置 %d)", msg, L->pos + 1);
        L->err = 1;
    }
    L->tok.kind = TK_ERR;
}

void lex_next(Lex *L) {
    /* 空白スキップ */
    while (L->src[L->pos] == ' ' || L->src[L->pos] == '\t')
        L->pos++;

    char c = L->src[L->pos];

    if (c == '\0') { L->tok.kind = TK_EOF; return; }

    /* 数値 */
    if (isdigit((unsigned char)c)) {
        if (c == '0') {
            char n = L->src[L->pos + 1];
            /* 16進 0x... */
            if (n == 'x' || n == 'X') {
                L->pos += 2;
                if (!isxdigit((unsigned char)L->src[L->pos])) {
                    lex_err(L, "16進数の桁がない"); return;
                }
                char *end;
                L->tok.ival = (int64_t)strtoull(L->src + L->pos, &end, 16);
                L->pos = (int)(end - L->src);
                L->tok.kind = TK_INT;
                return;
            }
            /* 2進 0b... */
            if (n == 'b' || n == 'B') {
                L->pos += 2;
                if (L->src[L->pos] != '0' && L->src[L->pos] != '1') {
                    lex_err(L, "2進数の桁がない"); return;
                }
                int64_t v = 0;
                while (L->src[L->pos] == '0' || L->src[L->pos] == '1')
                    v = (v << 1) | (L->src[L->pos++] - '0');
                L->tok.ival = v;
                L->tok.kind = TK_INT;
                return;
            }
            /* 8進 0o... */
            if (n == 'o' || n == 'O') {
                L->pos += 2;
                if (L->src[L->pos] < '0' || L->src[L->pos] > '7') {
                    lex_err(L, "8進数の桁がない"); return;
                }
                char *end;
                L->tok.ival = (int64_t)strtoull(L->src + L->pos, &end, 8);
                L->pos = (int)(end - L->src);
                L->tok.kind = TK_INT;
                return;
            }
        }
        /* 10進 (整数 or 浮動小数点) */
        {
            const char *p = L->src + L->pos;
            char *end;
            const char *q = p;
            while (isdigit((unsigned char)*q)) q++;
            if (*q == '.' || *q == 'e' || *q == 'E') {
                L->tok.dval = strtod(p, &end);
                L->tok.kind = TK_FLOAT;
            } else {
                L->tok.ival = (int64_t)strtoll(p, &end, 10);
                L->tok.kind = TK_INT;
            }
            L->pos = (int)(end - L->src);
        }
        return;
    }

    /* 識別子・関数名 */
    if (isalpha((unsigned char)c)) {
        /* 単位変換関数テーブル (乗数はすべて基準単位への換算値) */
        static const struct { const char *name; double mul; int cat; } unit_tbl[] = {
            /* データサイズ: バイト基準 (cat=0) */
            {"b",   1.0,                              0},
            {"kib", 1024.0,                           0},
            {"mib", 1024.0 * 1024,                    0},
            {"gib", 1024.0 * 1024 * 1024,             0},
            {"tib", 1024.0 * 1024 * 1024 * 1024,      0},
            {"kb",  1000.0,                           0},
            {"mb",  1000.0 * 1000,                    0},
            {"gb",  1000.0 * 1000 * 1000,             0},
            {"tb",  1000.0 * 1000 * 1000 * 1000,      0},
            /* 周波数: Hz 基準 (cat=1) */
            {"hz",  1.0,                              1},
            {"khz", 1000.0,                           1},
            {"mhz", 1000.0 * 1000,                    1},
            {"ghz", 1000.0 * 1000 * 1000,             1},
            /* 時間: 秒基準 (cat=2) */
            {"ns",  1e-9,                             2},
            {"us",  1e-6,                             2},
            {"ms",  1e-3,                             2},
            {"sec", 1.0,                              2},
            {NULL, 0.0, 0}
        };
        char buf[32];
        int i = 0;
        buf[i++] = c;
        L->pos++;
        while (i < 31 && isalnum((unsigned char)L->src[L->pos]))
            buf[i++] = L->src[L->pos++];
        buf[i] = '\0';
        if (strcmp(buf, "dec") == 0) { L->tok.kind = TK_FUNC_DEC; return; }
        if (strcmp(buf, "hex") == 0) { L->tok.kind = TK_FUNC_HEX; return; }
        if (strcmp(buf, "oct") == 0) { L->tok.kind = TK_FUNC_OCT; return; }
        if (strcmp(buf, "bin") == 0) { L->tok.kind = TK_FUNC_BIN; return; }
        if (strcmp(buf, "conv")  == 0) { L->tok.kind = TK_CONV;       return; }
        if (strcmp(buf, "log")   == 0) { L->tok.kind = TK_FUNC_LOG;   return; }
        if (strcmp(buf, "log2")  == 0) { L->tok.kind = TK_FUNC_LOG2;  return; }
        if (strcmp(buf, "log10") == 0) { L->tok.kind = TK_FUNC_LOG10; return; }
        for (int j = 0; unit_tbl[j].name; j++) {
            if (strcmp(buf, unit_tbl[j].name) == 0) {
                L->tok.kind = TK_FUNC_UNIT;
                L->tok.dval = unit_tbl[j].mul;
                L->tok.cat  = unit_tbl[j].cat;
                return;
            }
        }
        lex_err(L, "不明な識別子");
        return;
    }

    /* 演算子・記号 */
    L->pos++;
    switch (c) {
        case '+': L->tok.kind = TK_PLUS;    break;
        case '-': L->tok.kind = TK_MINUS;   break;
        case '*':
            if (L->src[L->pos] == '*') { L->pos++; L->tok.kind = TK_POW; }
            else L->tok.kind = TK_STAR;
            break;
        case '/': L->tok.kind = TK_SLASH;   break;
        case '%': L->tok.kind = TK_PERCENT; break;
        case '&': L->tok.kind = TK_AMP;     break;
        case '|': L->tok.kind = TK_PIPE;    break;
        case '^': L->tok.kind = TK_CARET;   break;
        case '~': L->tok.kind = TK_TILDE;   break;
        case '(': L->tok.kind = TK_LPAREN;  break;
        case ')': L->tok.kind = TK_RPAREN;  break;
        case ',': L->tok.kind = TK_COMMA;   break;
        case '_': L->tok.kind = TK_ANS;     break;
        case '<':
            if (L->src[L->pos] == '<') { L->pos++; L->tok.kind = TK_LSHIFT; }
            else lex_err(L, "'<<' が必要");
            break;
        case '>':
            if (L->src[L->pos] == '>') { L->pos++; L->tok.kind = TK_RSHIFT; }
            else lex_err(L, "'>>' が必要");
            break;
        default:
            lex_err(L, "不明な文字");
            break;
    }
}

void lex_init(Lex *L, const char *src) {
    L->src = src; L->pos = 0; L->err = 0; L->errmsg[0] = '\0';
    lex_next(L);
}
