#include "display.h"

#include <float.h>
#include <math.h>
#include <iconv.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

/* 2進数を4ビット区切り '_' で表示 */
static void print_bin(int64_t iv) {
    uint64_t uv = (uint64_t)iv;
    printf("0b");

    if (iv < 0) {
        /* 負数: 64ビット全体を4ビット区切りで表示 */
        for (int i = 63; i >= 0; i--) {
            putchar('0' + (int)((uv >> i) & 1));
            if (i > 0 && i % 4 == 0) putchar('_');
        }
    } else if (uv == 0) {
        putchar('0');
    } else {
        /* 正数: 先頭ゼロを省略 */
        int msb = 0;
        for (int i = 63; i >= 0; i--)
            if ((uv >> i) & 1) { msb = i; break; }
        for (int i = msb; i >= 0; i--) {
            putchar('0' + (int)((uv >> i) & 1));
            if (i > 0 && i % 4 == 0) putchar('_');
        }
    }
}

void print_result(Value v) {
    if (v.is_float) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.10f", v.dval);
        /* 末尾ゼロと末尾ドットを除去 */
        char *dot = strchr(buf, '.');
        if (dot) {
            char *end = dot;
            while (*end) end++;
            end--;
            while (end > dot && *end == '0') *end-- = '\0';
            if (end == dot) *end = '\0';
        }
        printf("  = %s\n", buf);
        if (v.warn)
            printf("  * 計算結果が桁溢れしています\n");
        return;
    }
    uint64_t uv = (uint64_t)v.ival;
    switch (v.fmt) {
        case FMT_DEC:
            printf("  = %" PRId64 "\n", v.ival);
            break;
        case FMT_HEX:
            printf("  HEX: 0x%" PRIX64 "\n", uv);
            break;
        case FMT_OCT:
            printf("  OCT: 0o%" PRIo64 "\n", uv);
            break;
        case FMT_BIN:
            printf("  BIN: ");
            print_bin(v.ival);
            printf("\n");
            break;
        default: /* FMT_ALL */
            printf("  DEC: %" PRId64 "\n", v.ival);
            printf("  HEX: 0x%" PRIX64 "\n", uv);
            printf("  OCT: 0o%" PRIo64 "\n", uv);
            printf("  BIN: ");
            print_bin(v.ival);
            printf("\n");
            break;
    }
}

void print_help(void) {
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  calc-pgm v" VERSION "  プログラミング用計算機\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("【演算子】\n");
    printf("  算術  : + - * / %% **(べき乗)\n");
    printf("  ビット: & | ^ ~ << >>\n");
    printf("  括弧  : ( )\n\n");
    printf("【数値リテラル】\n");
    printf("  10進  : 42, -42, 3.14\n");
    printf("  16進  : 0xFF, 0x2A\n");
    printf("  2進   : 0b1010\n");
    printf("  8進   : 0o52\n");
    printf("  _     : 前回の計算結果\n\n");
    printf("【対数関数】\n");
    printf("  log(x)   自然対数 (底 e)\n");
    printf("  log2(x)  2を底とする対数\n");
    printf("  log10(x) 10を底とする対数\n\n");
    printf("【表示変換関数】\n");
    printf("  dec(式) 10進のみ表示\n");
    printf("  hex(式) 16進のみ表示\n");
    printf("  oct(式)  8進のみ表示\n");
    printf("  bin(式)  2進のみ表示\n\n");
    printf("【単位変換関数】\n");
    printf("  データサイズ (→ バイト)\n");
    printf("    kib(n) n KiB  mib(n) n MiB  gib(n) n GiB  tib(n) n TiB\n");
    printf("    kb(n)  n KB   mb(n)  n MB   gb(n)  n GB   tb(n)  n TB\n");
    printf("  周波数 (→ Hz)\n");
    printf("    khz(n) n kHz  mhz(n) n MHz  ghz(n) n GHz\n");
    printf("  時間 (→ 秒)\n");
    printf("    ns(n)  n ns   us(n)  n μs   ms(n)  n ms   sec(n) n 秒\n");
    printf("  単位間変換\n");
    printf("    conv(n, 変換前, 変換後)  例: conv(2,gib,mb) conv(500,ms,us)\n");
    printf("    基準単位: b(バイト) hz(Hz) sec(秒)\n\n");
    printf("【コマンドライン引数】\n");
    printf("  -h, --help                このヘルプを表示\n");
    printf("  -e <式>                   式を演算して計算結果を表示\n");
    printf("  -b <式>                   ビット演算式を評価して結果を表示\n");
    printf("  -l <式>                   式を評価し ln / log2 / log10 を一括表示\n");
    printf("  -f <形式> <式>            指定形式で結果を表示 (形式: dec/hex/oct/bin/all)\n");
    printf("  -u <値> <変換前> <変換後> 単位変換  例: -u 2 gib mb  -u 500 ms us\n");
    printf("  -t                        整数型・浮動小数点型の一覧を表示\n");
    printf("  -s <文字列>               文字数と各エンコードのバイト数を表示\n");
    printf("  -c                        文字コードの一覧を表示\n\n");
    printf("【コマンド】\n");
    printf("  help        このヘルプを表示\n");
    printf("  types       整数型の一覧を表示 (サイズ・値の範囲)\n");
    printf("  enc         文字コードの一覧を表示\n");
    printf("  size <文字列>  文字数と各エンコードのバイト数を表示\n");
    printf("  quit / exit 終了\n\n");
    printf("【使用例】\n");
    printf("  > 0xFF & 0x0F\n");
    printf("    DEC: 15  HEX: 0xF  BIN: 0b1111\n\n");
    printf("  > 1 << 8\n");
    printf("    DEC: 256  HEX: 0x100\n\n");
    printf("  > 0b1100 ^ 0b1010\n");
    printf("    DEC: 6  BIN: 0b110\n\n");
    printf("  > (3 + 4) * 2\n");
    printf("    DEC: 14\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

/* 整数型の1行を表示するヘルパー */
static void print_int_row(const char *name, int bytes, int64_t vmin, uint64_t vmax, int is_signed) {
    char minbuf[32], maxbuf[32];
    if (is_signed)
        snprintf(minbuf, sizeof minbuf, "%" PRId64, vmin);
    else
        snprintf(minbuf, sizeof minbuf, "0");
    snprintf(maxbuf, sizeof maxbuf, "%" PRIu64, vmax);
    printf("  %-10s  %4d   %22s   %20s\n", name, bytes, minbuf, maxbuf);
}

void print_types(void) {
    static const struct {
        const char *name;
        int         bytes;
        int         is_signed;
        int64_t     vmin;
        uint64_t    vmax;
    } itbl[] = {
        { "int8_t",   1, 1, INT8_MIN,   (uint64_t)INT8_MAX   },
        { "uint8_t",  1, 0, 0,          UINT8_MAX            },
        { "int16_t",  2, 1, INT16_MIN,  (uint64_t)INT16_MAX  },
        { "uint16_t", 2, 0, 0,          UINT16_MAX           },
        { "int32_t",  4, 1, INT32_MIN,  (uint64_t)INT32_MAX  },
        { "uint32_t", 4, 0, 0,          UINT32_MAX           },
        { "int64_t",  8, 1, INT64_MIN,  (uint64_t)INT64_MAX  },
        { "uint64_t", 8, 0, 0,          UINT64_MAX           },
    };

    /* ── 整数型 ── */
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  整数型\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  %-10s  %4s   %22s   %20s\n", "型名", "byte", "最小値", "最大値");
    printf("  ─────────────────────────────────────────────────────────────────────────\n");
    for (int i = 0; i < (int)(sizeof itbl / sizeof itbl[0]); i++)
        print_int_row(itbl[i].name, itbl[i].bytes,
                      itbl[i].vmin, itbl[i].vmax, itbl[i].is_signed);

    printf("  ─────────────────────────────────────────────────────────────────────────\n");
    /* char: 符号は実装依存 (ARM は unsigned がデフォルト) */
    print_int_row("char", (int)sizeof(char),
                  (int64_t)CHAR_MIN, (uint64_t)(unsigned char)UCHAR_MAX,
                  CHAR_MIN < 0);
    /* long: サイズはプラットフォーム依存 */
    print_int_row("long", (int)sizeof(long),
                  (int64_t)LONG_MIN, (uint64_t)(unsigned long)LONG_MAX, 1);

    /* ── 浮動小数点型 ── */
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  浮動小数点型\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  %-10s  %4s   %22s   %20s   %s\n",
           "型名", "byte", "最小値", "最大値", "有効桁数");
    printf("  ─────────────────────────────────────────────────────────────────────────\n");
    {
        char minbuf[32], maxbuf[32];
        snprintf(minbuf, sizeof minbuf, "%.7e", -(double)FLT_MAX);
        snprintf(maxbuf, sizeof maxbuf, "%.7e",  (double)FLT_MAX);
        printf("  %-10s  %4d   %22s   %20s   約%d桁\n",
               "float", (int)sizeof(float), minbuf, maxbuf, FLT_DIG);

        snprintf(minbuf, sizeof minbuf, "%.7e", -DBL_MAX);
        snprintf(maxbuf, sizeof maxbuf, "%.7e",  DBL_MAX);
        printf("  %-10s  %4d   %22s   %20s   約%d桁\n",
               "double", (int)sizeof(double), minbuf, maxbuf, DBL_DIG);
    }
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

/* UTF-8文字列のUnicodeコードポイント数 */
static int utf8_codepoints(const char *s) {
    int n = 0;
    while (*s) { if ((*s & 0xC0) != 0x80) n++; s++; }
    return n;
}

/* UTF-8→指定エンコード変換後のバイト数。失敗時は -1 */
static int encoded_bytes(const char *str, const char *enc) {
    iconv_t cd = iconv_open(enc, "UTF-8");
    if (cd == (iconv_t)-1) return -1;
    size_t inlen = strlen(str);
    size_t buflen = inlen * 4 + 8;
    char *buf = malloc(buflen);
    if (!buf) { iconv_close(cd); return -1; }
    char *inp = (char *)str, *outp = buf;
    size_t inb = inlen, outb = buflen;
    size_t r = iconv(cd, &inp, &inb, &outp, &outb);
    int result = (r == (size_t)-1) ? -1 : (int)(buflen - outb);
    free(buf); iconv_close(cd);
    return result;
}

void print_size(const char *str) {
    static const struct { const char *label; const char *enc; } tbl[] = {
        { "UTF-8",     "UTF-8"    },
        { "UTF-16LE",  "UTF-16LE" },
        { "UTF-32LE",  "UTF-32LE" },
        { "Shift-JIS", "CP932"    },
        { "EUC-JP",    "EUC-JP"   },
        { "ASCII",     "ASCII"    },
    };
    printf("  文字数: %d (Unicodeコードポイント)\n", utf8_codepoints(str));
    printf("  ─────────────────────────────────────\n");
    for (int i = 0; i < (int)(sizeof tbl / sizeof tbl[0]); i++) {
        int n = encoded_bytes(str, tbl[i].enc);
        if (n < 0) printf("  %-10s : 変換不可\n",        tbl[i].label);
        else       printf("  %-10s : %d byte\n", tbl[i].label, n);
    }
    printf("\n");
}

/* 浮動小数点値を末尾ゼロ除去して表示するヘルパー */
static void print_double(double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.10f", v);
    char *dot = strchr(buf, '.');
    if (dot) {
        char *end = dot;
        while (*end) end++;
        end--;
        while (end > dot && *end == '0') *end-- = '\0';
        if (end == dot) *end = '\0';
    }
    printf("%s", buf);
}

void print_log_result(double x) {
    printf("  ln   : "); print_double(log(x));   printf("\n");
    printf("  log2 : "); print_double(log2(x));  printf("\n");
    printf("  log10: "); print_double(log10(x)); printf("\n");
}

void print_encodings(void) {
    static const struct {
        const char *name;    /* NULL = 区切り線 */
        const char *size;    /* 1文字あたりのバイト数 */
        const char *chars;   /* 収録文字数 */
        const char *note;
    } tbl[] = {
        { "ASCII",     "1",      "128",        "7bit、制御文字含む (0x00-0x7F)" },
        { "UTF-8",     "1~4",    "1,114,112",  "可変長、ASCII後方互換" },
        { "UTF-16",    "2 or 4", "1,114,112",  "BMP:2byte、補助文字:4byte (サロゲートペア)" },
        { "UTF-32",    "4",      "1,114,112",  "固定長、全コードポイントを直接表現" },
        { NULL, NULL, NULL, NULL },
        { "Shift-JIS", "1~2",    "約7,000",    "日本語Windows標準 (CP932)" },
        { "EUC-JP",    "1~3",    "約6,900",    "日本語Unix/Linux標準 (JIS X 0208)" },
    };
    const char *sep  = "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━";
    const char *line = "  ─────────────────────────────────────────────────────────────────────────";

    printf("%s\n  文字コード一覧\n%s\n", sep, sep);
    printf("  %-12s  %-8s  %15s   %s\n", "エンコード", "1文字(byte)", "収録文字数", "備考");
    printf("%s\n", line);
    for (int i = 0; i < (int)(sizeof tbl / sizeof tbl[0]); i++) {
        if (!tbl[i].name) { printf("%s\n", line); continue; }
        printf("  %-12s  %-8s  %15s   %s\n",
               tbl[i].name, tbl[i].size, tbl[i].chars, tbl[i].note);
    }
    printf("%s\n", sep);
}
