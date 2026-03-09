#ifndef PARSER_H
#define PARSER_H

#include "value.h"

/*
 * 式文字列を評価して Value を返す。
 * エラー時は errmsg にメッセージを書き込み val_int(0) を返す。
 * 成功時は errmsg[0] == '\0'。
 */
Value calc_eval(const char *src, Value prev, char *errmsg, int emlen);

#endif /* PARSER_H */
