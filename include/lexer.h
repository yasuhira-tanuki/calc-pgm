#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>
#include "value.h"

/* トークンの種類 */
typedef enum {
    TK_INT, TK_FLOAT,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT, TK_POW,
    TK_AMP, TK_PIPE, TK_CARET, TK_TILDE,
    TK_LSHIFT, TK_RSHIFT,
    TK_LPAREN, TK_RPAREN,
    TK_COMMA,
    TK_ANS,
    TK_FUNC_DEC, TK_FUNC_HEX, TK_FUNC_OCT, TK_FUNC_BIN,
    TK_FUNC_LOG, TK_FUNC_LOG2, TK_FUNC_LOG10,
    TK_FUNC_UNIT,
    TK_CONV,
    TK_EOF, TK_ERR
} TKind;

typedef struct {
    TKind   kind;
    int64_t ival;
    double  dval;
    int     cat;   /* TK_FUNC_UNIT 時のカテゴリ: 0=data, 1=freq, 2=time */
} Tok;

typedef struct {
    const char *src;
    int         pos;
    Tok         tok;
    int         err;
    char        errmsg[128];
} Lex;

void lex_init(Lex *L, const char *src);
void lex_next(Lex *L);

#endif /* LEXER_H */
