#include "parser.h"
#include "lexer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/*
 * 再帰下降構文解析
 *
 * 優先順位 (低→高):
 *   parse_bitor  : |
 *   parse_bitxor : ^
 *   parse_bitand : &
 *   parse_shift  : << >>
 *   parse_add    : + -
 *   parse_mul    : * / %
 *   parse_unary  : 単項 - + ~
 *   parse_prim   : 数値 | (expr)
 */
typedef struct {
    Lex   lex;
    Value prev;
    int   err;
    char  errmsg[128];
} Parser;

static void p_err(Parser *P, const char *msg) {
    if (!P->err) {
        snprintf(P->errmsg, sizeof(P->errmsg), "構文エラー: %s", msg);
        P->err = 1;
    }
}

static int p_has_err(Parser *P) { return P->err || P->lex.err; }

static const char *p_errmsg(Parser *P) {
    return P->lex.err ? P->lex.errmsg : P->errmsg;
}

/* double の整数表現精度上限 (2^53) を超える場合に warn フラグをセット */
#define DBL_INT_MAX 9007199254740992.0  /* 2^53 */
static Value val_flt_overflow(double v) {
    Value r = val_flt(v);
    if (v > DBL_INT_MAX || v < -DBL_INT_MAX) r.warn = 1;
    return r;
}

/* 前方宣言 */
static Value parse_bitor(Parser *P);
static Value parse_unary(Parser *P);

static Value parse_prim(Parser *P) {
    if (p_has_err(P)) return val_int(0);

    Tok t = P->lex.tok;

    if (t.kind == TK_INT) {
        lex_next(&P->lex);
        return val_int(t.ival);
    }
    if (t.kind == TK_FLOAT) {
        lex_next(&P->lex);
        return val_flt(t.dval);
    }
    if (t.kind == TK_ANS) {
        lex_next(&P->lex);
        return P->prev;
    }
    if (t.kind == TK_LPAREN) {
        lex_next(&P->lex);
        Value v = parse_bitor(P);
        if (!p_has_err(P)) {
            if (P->lex.tok.kind != TK_RPAREN)
                p_err(P, "')' が必要");
            else
                lex_next(&P->lex);
        }
        return v;
    }
    if (t.kind == TK_FUNC_UNIT) {
        double mul = t.dval;
        lex_next(&P->lex);
        if (P->lex.tok.kind != TK_LPAREN) { p_err(P, "'(' が必要"); return val_int(0); }
        lex_next(&P->lex);
        Value v = parse_bitor(P);
        if (!p_has_err(P)) {
            if (P->lex.tok.kind != TK_RPAREN)
                p_err(P, "')' が必要");
            else
                lex_next(&P->lex);
        }
        if (p_has_err(P)) return val_int(0);
        if (!v.is_float) {
            double result = (double)v.ival * mul;
            int64_t iresult = (int64_t)result;
            if ((double)iresult == result) {
                Value r = val_int(iresult); r.fmt = FMT_DEC; return r;
            }
        }
        return val_flt(v.is_float ? v.dval * mul : (double)v.ival * mul);
    }
    if (t.kind == TK_CONV) {
        lex_next(&P->lex);
        if (P->lex.tok.kind != TK_LPAREN) { p_err(P, "'(' が必要"); return val_int(0); }
        lex_next(&P->lex);
        Value v = parse_bitor(P);
        if (p_has_err(P)) return val_int(0);
        if (P->lex.tok.kind != TK_COMMA) { p_err(P, "',' が必要"); return val_int(0); }
        lex_next(&P->lex);
        if (P->lex.tok.kind != TK_FUNC_UNIT) { p_err(P, "変換前単位が必要"); return val_int(0); }
        double  from_mul = P->lex.tok.dval;
        int     from_cat = P->lex.tok.cat;
        lex_next(&P->lex);
        if (P->lex.tok.kind != TK_COMMA) { p_err(P, "',' が必要"); return val_int(0); }
        lex_next(&P->lex);
        if (P->lex.tok.kind != TK_FUNC_UNIT) { p_err(P, "変換後単位が必要"); return val_int(0); }
        double  to_mul   = P->lex.tok.dval;
        int     to_cat   = P->lex.tok.cat;
        lex_next(&P->lex);
        if (P->lex.tok.kind != TK_RPAREN) { p_err(P, "')' が必要"); return val_int(0); }
        lex_next(&P->lex);
        if (from_cat != to_cat) { p_err(P, "単位のカテゴリが異なる"); return val_int(0); }
        /* 計算: value × from_mul ÷ to_mul */
        double base   = (v.is_float ? v.dval : (double)v.ival) * from_mul;
        double result = base / to_mul;
        if (!v.is_float) {
            int64_t iresult = (int64_t)result;
            if ((double)iresult == result) {
                Value r = val_int(iresult); r.fmt = FMT_DEC; return r;
            }
        }
        return val_flt(result);
    }
    if (t.kind == TK_FUNC_LOG || t.kind == TK_FUNC_LOG2 || t.kind == TK_FUNC_LOG10) {
        lex_next(&P->lex);
        if (P->lex.tok.kind != TK_LPAREN) { p_err(P, "'(' が必要"); return val_int(0); }
        lex_next(&P->lex);
        Value v = parse_bitor(P);
        if (!p_has_err(P)) {
            if (P->lex.tok.kind != TK_RPAREN) p_err(P, "')' が必要");
            else lex_next(&P->lex);
        }
        if (p_has_err(P)) return val_int(0);
        double x = to_d(v);
        if (x <= 0.0) { p_err(P, "対数の引数は正の数が必要"); return val_int(0); }
        double result;
        if      (t.kind == TK_FUNC_LOG2)  result = log2(x);
        else if (t.kind == TK_FUNC_LOG10) result = log10(x);
        else                              result = log(x);
        return val_flt(result);
    }
    if (t.kind == TK_FUNC_DEC || t.kind == TK_FUNC_HEX ||
        t.kind == TK_FUNC_OCT || t.kind == TK_FUNC_BIN) {
        lex_next(&P->lex);
        if (P->lex.tok.kind != TK_LPAREN) { p_err(P, "'(' が必要"); return val_int(0); }
        lex_next(&P->lex);
        Value v = parse_bitor(P);
        if (!p_has_err(P)) {
            if (P->lex.tok.kind != TK_RPAREN)
                p_err(P, "')' が必要");
            else
                lex_next(&P->lex);
        }
        if (p_has_err(P)) return val_int(0);
        if (v.is_float) { p_err(P, "変換関数は整数のみ"); return val_int(0); }
        if      (t.kind == TK_FUNC_DEC) v.fmt = FMT_DEC;
        else if (t.kind == TK_FUNC_HEX) v.fmt = FMT_HEX;
        else if (t.kind == TK_FUNC_OCT) v.fmt = FMT_OCT;
        else                             v.fmt = FMT_BIN;
        return v;
    }
    p_err(P, "数値または '(' が必要");
    return val_int(0);
}

static Value parse_pow(Parser *P) {
    Value base = parse_prim(P);
    if (p_has_err(P) || P->lex.tok.kind != TK_POW) return base;

    lex_next(&P->lex);
    Value exp = parse_unary(P);  /* 右結合かつ単項演算子を許容 */
    if (p_has_err(P)) return val_int(0);

    if (base.is_float || exp.is_float) {
        return val_flt(pow(to_d(base), to_d(exp)));
    }
    int64_t b = base.ival, e = exp.ival;
    if (e < 0) {
        return val_flt(pow((double)b, (double)e));
    }
    if (e == 0) return val_int(1);
    /* 整数べき乗 (オーバーフロー検出) */
    int64_t result = 1, bpow = b, epow = e;
    int overflow = 0;
    while (epow > 0 && !overflow) {
        if (epow & 1) {
            if (__builtin_mul_overflow(result, bpow, &result)) overflow = 1;
        }
        epow >>= 1;
        if (epow > 0 && !overflow) {
            if (__builtin_mul_overflow(bpow, bpow, &bpow)) overflow = 1;
        }
    }
    if (overflow) return val_flt_overflow(pow((double)b, (double)e));
    return val_int(result);
}

static Value parse_unary(Parser *P) {
    if (p_has_err(P)) return val_int(0);

    TKind k = P->lex.tok.kind;
    if (k == TK_MINUS) {
        lex_next(&P->lex);
        Value v = parse_unary(P);
        if (v.is_float) return val_flt(-v.dval);
        if (v.ival == INT64_MIN) return val_flt_overflow(-(double)v.ival);
        return val_int(-v.ival);
    }
    if (k == TK_PLUS) {
        lex_next(&P->lex);
        return parse_unary(P);
    }
    if (k == TK_TILDE) {
        lex_next(&P->lex);
        Value v = parse_unary(P);
        if (v.is_float) { p_err(P, "~ は整数のみ"); return val_int(0); }
        return val_int(~v.ival);
    }
    return parse_pow(P);
}

static Value parse_mul(Parser *P) {
    Value l = parse_unary(P);

    while (!p_has_err(P)) {
        TKind k = P->lex.tok.kind;
        if (k != TK_STAR && k != TK_SLASH && k != TK_PERCENT) break;
        lex_next(&P->lex);
        Value r = parse_unary(P);
        if (p_has_err(P)) break;

        if (k == TK_PERCENT) {
            if (l.is_float || r.is_float) { p_err(P, "% は整数のみ"); break; }
            if (r.ival == 0) { p_err(P, "ゼロ除算"); break; }
            l = val_int(l.ival % r.ival);
        } else if (l.is_float || r.is_float) {
            double ld = to_d(l), rd = to_d(r);
            if (k == TK_SLASH && rd == 0.0) { p_err(P, "ゼロ除算"); break; }
            l = val_flt(k == TK_STAR ? ld * rd : ld / rd);
        } else {
            if (k == TK_STAR) {
                int64_t result;
                if (__builtin_mul_overflow(l.ival, r.ival, &result))
                    l = val_flt_overflow((double)l.ival * (double)r.ival);
                else
                    l = val_int(result);
            } else {
                if (r.ival == 0) { p_err(P, "ゼロ除算"); break; }
                /* INT64_MIN / -1 はオーバーフロー */
                if (l.ival == INT64_MIN && r.ival == -1) {
                    l = val_flt_overflow(-(double)INT64_MIN);
                } else if (l.ival % r.ival == 0) {
                    l = val_int(l.ival / r.ival);
                } else {
                    l = val_flt((double)l.ival / (double)r.ival);
                }
            }
        }
    }
    return l;
}

static Value parse_add(Parser *P) {
    Value l = parse_mul(P);

    while (!p_has_err(P)) {
        TKind k = P->lex.tok.kind;
        if (k != TK_PLUS && k != TK_MINUS) break;
        lex_next(&P->lex);
        Value r = parse_mul(P);
        if (p_has_err(P)) break;

        if (l.is_float || r.is_float) {
            l = val_flt(k == TK_PLUS ? to_d(l) + to_d(r) : to_d(l) - to_d(r));
        } else {
            int64_t result;
            int overflow = (k == TK_PLUS)
                ? __builtin_add_overflow(l.ival, r.ival, &result)
                : __builtin_sub_overflow(l.ival, r.ival, &result);
            if (overflow)
                l = val_flt_overflow(k == TK_PLUS ? (double)l.ival + (double)r.ival
                                                   : (double)l.ival - (double)r.ival);
            else
                l = val_int(result);
        }
    }
    return l;
}

static Value parse_shift(Parser *P) {
    Value l = parse_add(P);

    while (!p_has_err(P)) {
        TKind k = P->lex.tok.kind;
        if (k != TK_LSHIFT && k != TK_RSHIFT) break;
        lex_next(&P->lex);
        Value r = parse_add(P);
        if (p_has_err(P)) break;

        if (l.is_float || r.is_float) { p_err(P, "シフトは整数のみ"); break; }
        int64_t sh = r.ival;
        if (sh < 0 || sh >= 64) { p_err(P, "シフト量が範囲外 (0〜63)"); break; }
        uint64_t ul = (uint64_t)l.ival;
        l = val_int((int64_t)(k == TK_LSHIFT ? ul << sh : ul >> sh));
    }
    return l;
}

static Value parse_bitand(Parser *P) {
    Value l = parse_shift(P);

    while (!p_has_err(P) && P->lex.tok.kind == TK_AMP) {
        lex_next(&P->lex);
        Value r = parse_shift(P);
        if (p_has_err(P)) break;
        if (l.is_float || r.is_float) { p_err(P, "& は整数のみ"); break; }
        l = val_int(l.ival & r.ival);
    }
    return l;
}

static Value parse_bitxor(Parser *P) {
    Value l = parse_bitand(P);

    while (!p_has_err(P) && P->lex.tok.kind == TK_CARET) {
        lex_next(&P->lex);
        Value r = parse_bitand(P);
        if (p_has_err(P)) break;
        if (l.is_float || r.is_float) { p_err(P, "^ は整数のみ"); break; }
        l = val_int(l.ival ^ r.ival);
    }
    return l;
}

static Value parse_bitor(Parser *P) {
    Value l = parse_bitxor(P);

    while (!p_has_err(P) && P->lex.tok.kind == TK_PIPE) {
        lex_next(&P->lex);
        Value r = parse_bitxor(P);
        if (p_has_err(P)) break;
        if (l.is_float || r.is_float) { p_err(P, "| は整数のみ"); break; }
        l = val_int(l.ival | r.ival);
    }
    return l;
}

Value calc_eval(const char *src, Value prev, char *errmsg, int emlen) {
    Parser P = {{0}, prev, 0, {0}};
    lex_init(&P.lex, src);
    Value v = parse_bitor(&P);
    if (!p_has_err(&P) && P.lex.tok.kind != TK_EOF)
        p_err(&P, "予期しない文字");
    if (p_has_err(&P)) {
        snprintf(errmsg, emlen, "%s", p_errmsg(&P));
        return val_int(0);
    }
    errmsg[0] = '\0';
    return v;
}
