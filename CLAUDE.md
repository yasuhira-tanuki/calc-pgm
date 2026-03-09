# CLAUDE.md
This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 言語設定
- 常に日本語で会話する

## ビルドと実行

```bash
make          # ビルド (build/*.o → ./calc)
make clean    # 成果物を削除
./calc        # 対話型REPLを起動
```

コンパイラフラグ: `gcc -Wall -Wextra -O2 -std=c11 -Iinclude`

ビルド成果物は `build/` ディレクトリに出力される。`build/` と実行ファイル `calc` は自動生成物のため削除・再生成して問題ない。

## アーキテクチャ

パイプライン: 入力文字列 → Lexer → Parser → Value → Display

### モジュール構成とデータフロー

```
include/value.h          Value型 (static inline) ← すべてのモジュールが依存
include/lexer.h          TKind / Tok / Lex 型の定義、lex_init・lex_next の宣言
include/parser.h         calc_eval の宣言
include/display.h        print_result・print_help・print_types・print_encodings・print_size の宣言、VERSION マクロ

src/lexer.c              文字列 → トークン列 (lex_next で1トークン先読み)
src/parser.c             再帰下降構文解析、calc_eval が公開API
src/display.c            Value を各形式で出力。浮動小数点は %.10f で表示(科学的記数法なし、末尾ゼロ除去)
src/main.c               REPLループ、入力整形、コマンド分岐 (help/types/enc/size/quit/exit)
```

### Value 型

`is_float` フラグで整数(`int64_t`)と浮動小数点(`double`)を区別する。
入力に小数点または指数表記が含まれる場合に float モードへ切り替わる。
ビット演算・シフト・剰余は整数モードのみ許可し、float に対してはエラーを返す。

`fmt` フィールド (`VFmt`) で表示形式を制御する:
- `FMT_ALL` (デフォルト): DEC/HEX/OCT/BIN を全表示
- `FMT_DEC`: `= N` の形式で10進のみ表示 (単位変換関数・`conv()` の結果に使用)
- `FMT_HEX` / `FMT_OCT` / `FMT_BIN`: 各形式のみ表示

### Parser の設計

`parser.c` 内の `Parser` 構造体と `parse_*` 関数群はすべて `static` で非公開。
外部に公開するのは `calc_eval` のみ。エラーは `errmsg` バッファへの書き込みで伝達し、戻り値で判定する (`errmsg[0] == '\0'` なら成功)。

演算子優先順位 (低→高):

```
| → ^ → & → <<,>> → +,- → *,/,% → 単項(-,+,~) → **(べき乗) → 一次式
```

`parse_pow` で処理: `**` は右結合。整数同士はオーバーフロー検出付き整数べき乗。負の指数や float 混在時は double で計算。

`parse_prim` で処理する特殊形式:
- `TK_FUNC_UNIT`: `gib(n)` 等の単位変換。結果は `FMT_DEC`。整数で割り切れる場合は整数、それ以外は float を返す。
- `TK_CONV`: `conv(値, 変換前単位, 変換後単位)` 構文。`value × from_mul ÷ to_mul` を double で計算。異カテゴリ変換はエラー。
- `TK_FUNC_LOG/LOG2/LOG10`: `log(x)`, `log2(x)`, `log10(x)` — 自然対数・2底・10底。引数は正の数のみ。常に float を返す。
- `TK_FUNC_DEC/HEX/OCT/BIN`: 整数のみ受け付け、`fmt` フィールドを設定して返す。

### Lexer の設計

`lex_next` は呼ばれるたびに `L->tok` を次のトークンで上書きする先読み1トークン方式。
`lex_err` は `static` で lexer 内部のみ使用。エラー情報は `Lex.err` / `Lex.errmsg` に保持される。

数値リテラルの対応形式: `0x`(16進)、`0b`(2進)、`0o`(8進)、10進整数、浮動小数点。

`_`（アンダースコア）は `TK_ANS` トークンとして扱われ、直前の計算結果（`prev` Value）を返す。

`Tok` 構造体の `cat` フィールドは `TK_FUNC_UNIT` トークンのカテゴリを保持する:
- `0` = データサイズ (バイト基準)
- `1` = 周波数 (Hz 基準)
- `2` = 時間 (秒基準)

`unit_tbl` の `mul` フィールドは `double` 型で基準単位への換算値を保持する。
時間単位は秒基準のため小数値になる (`ns`=1e-9, `us`=1e-6, `ms`=1e-3, `sec`=1.0)。
基準単位トークン: `b`(1バイト)、`hz`(1Hz)、`sec`(1秒) — `conv()` の変換先として使用可能。

TKind に含まれる主なトークン:
- `TK_POW`: `**` べき乗演算子
- `TK_COMMA`: `,` 区切り (`conv()` の引数区切りに使用)
- `TK_CONV`: `conv` キーワード
- `TK_FUNC_UNIT`: 単位変換関数 (`gib`, `ms` 等)
- `TK_FUNC_LOG` / `TK_FUNC_LOG2` / `TK_FUNC_LOG10`: 対数関数

### エラー伝播

Lexer と Parser の2段階でエラーを管理する。`Lex.err` / `Lex.errmsg` と `Parser.err` / `Parser.errmsg` がそれぞれ独立して保持され、`p_errmsg()` は Lexer エラーを優先して返す。エラー発生後も各 `parse_*` 関数は `p_has_err()` でチェックして早期リターンし、連鎖エラーを防ぐ。

### 2進数表示の仕様

`print_bin()` の挙動: 負数は64bit全体を4ビット区切りで表示、正数は先頭ゼロを省略して最上位ビットから表示。

### 浮動小数点表示の仕様

`%.10f` (小数点以下10桁固定) でフォーマットし、末尾ゼロと末尾ドットを除去して出力する。科学的記数法 (`1e-06` 等) は使用しない。
