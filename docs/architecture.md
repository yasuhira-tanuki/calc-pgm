# アーキテクチャ

パイプライン: 入力文字列 → Lexer → Parser → Value → Display

## モジュール構成とデータフロー

```
include/value.h          Value型 (static inline) ← すべてのモジュールが依存
include/lexer.h          TKind / Tok / Lex 型の定義、lex_init・lex_next の宣言
include/parser.h         calc_eval の宣言
include/display.h        print_result・print_help・print_types・print_encodings・print_size・print_log_result の宣言、VERSION マクロ

src/lexer.c              文字列 → トークン列 (lex_next で1トークン先読み)
src/parser.c             再帰下降構文解析、calc_eval が公開API
src/display.c            Value を各形式で出力。浮動小数点は %.10f で表示(科学的記数法なし、末尾ゼロ除去)
src/main.c               REPLループ、入力整形、コマンド分岐 (help/types/enc/size/quit/exit)
```

各モジュールの詳細:

- [Lexer の設計](lexer.md)
- [Parser の設計](parser.md)
- [表示仕様](display.md)
- [エラー伝播](error-handling.md)

## Value 型

`is_float` フラグで整数(`int64_t`)と浮動小数点(`double`)を区別する。
入力に小数点または指数表記が含まれる場合に float モードへ切り替わる。
ビット演算・シフト・剰余は整数モードのみ許可し、float に対してはエラーを返す。

`fmt` フィールド (`VFmt`) で表示形式を制御する:

- `FMT_ALL` (デフォルト): DEC/HEX/OCT/BIN を全表示
- `FMT_DEC`: `= N` の形式で10進のみ表示 (単位変換関数・`conv()` の結果に使用)
- `FMT_HEX` / `FMT_OCT` / `FMT_BIN`: 各形式のみ表示

`FMT_DEC` などの非デフォルト書式は単位変換関数・`conv()`・表示変換関数の結果にのみ付く。
その結果をさらに別の演算と組み合わせると、演算結果は `val_int()` / `val_flt()` で
新たに生成されるため `fmt` は `FMT_ALL` に戻る (例: `gib(2) * 512` は DEC/HEX/OCT/BIN を全表示)。

`warn` フィールドは桁溢れ警告フラグ。整数演算のオーバーフローや `double` の整数表現精度上限 (2^53) 超過時に
`val_flt_overflow()` が `1` をセットし、`print_result()` が `* 計算結果が桁溢れしています` を追記表示する。
