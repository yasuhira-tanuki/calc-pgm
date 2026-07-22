# エージェント連携 (agent-tools)

`calc-pgm` の全機能を AI エージェントやプログラムから利用するための連携層を定義する。
実体はリポジトリ直下の `agent-tools/` に置く **Rust 製の単一バイナリ MCP サーバー**。

## 目的と方針

オーバーヘッドを最小化し、配布を単純化するため次の方針を採る。

- **単一バイナリ**: Python 等の外部ランタイムに依存しない。1 つの実行ファイルで完結させる。
- **プロセス生成を排除**: `calc-pgm` を毎回 `exec` するのではなく、計算エンジン(`calc_eval`)を
  **バイナリに静的に埋め込み、FFI で直接呼ぶ**。呼び出しごとの `fork`/`exec` を無くす。
- **標準プロトコルで公開**: MCP (Model Context Protocol) サーバーとして公開し、
  Claude Code / Claude Desktop / LangGraph(adapter 経由)など MCP 対応クライアント全般から使えるようにする。
- **本体の計算ロジックは不変**: `src/` の計算ロジックには手を入れず、埋め込みと表示整形のための
  薄い C シムのみを追加する。

### オーバーヘッド設計上の要点

| 層 | 対処 |
|----|------|
| プロセス生成(`fork`/`exec`) | **無くす**。エンジンを埋め込み、FFI 関数呼び出し(μ秒オーダー)にする |
| 言語ランタイム | **無くす**。Rust の単一静的バイナリ。FFI は `extern "C"` でほぼゼロコスト |
| 計算 | C 本体(`lexer.c`/`parser.c`)がそのまま担う |

## 構成

```
agent-tools/
├── Cargo.toml            # Rust プロジェクト定義(rmcp などに依存)
├── build.rs              # cc クレートで C エンジン + シムをコンパイルし静的リンク
├── csrc/
│   └── calc_shim.c       # calc_eval / 各 display をバッファ経由で呼ぶ薄い C API
├── src/
│   ├── main.rs           # MCP サーバー起動(stdio トランスポート)
│   ├── engine.rs         # FFI 宣言(extern "C")+ 安全な Rust ラッパー
│   └── tools.rs          # 全モードを MCP ツールとして公開
└── README.md
```

### ビルドと埋め込み

- `cargo build --release` で単一バイナリ(例: `calc-pgm-mcp`)を生成する。
- `build.rs` が `cc` クレートで **`src/lexer.c` / `src/parser.c` / `src/display.c` と `csrc/calc_shim.c`** を
  コンパイルし、Rust バイナリに静的リンクする。REPL/CLI 用の `src/main.c` は**使わない**(エンジンのみ埋め込む)。
- 生成物は自己完結した 1 ファイル。実行時に `calc-pgm` バイナリや Python を必要としない。

## FFI 境界と C シム

`calc_eval`(→ [parser.md](parser.md))は `Value` を返し、DEC/HEX/OCT/BIN 等の整形は `display.c` が
`stdout` へ出力する。FFI から**文字列**として結果を得るため、`csrc/calc_shim.c` に薄い C API を追加し、
`display` の整形ロジックを **`FILE*`/バッファ経由で再利用**する(CLI と出力が完全一致し、整形の二重管理を避ける)。

- 計算ロジック(`calc_eval` 本体)には手を入れない。追加するのは表示・受け渡し用の薄い API のみ。
- Rust 側(`engine.rs`)は `extern "C"` 宣言を安全な Rust 関数に包み、エラーは `Result` で返す。
  バッファ長・NUL 終端・UTF-8 検証は Rust 側で保証する。

## 公開する MCP ツール(全モード)

CLI の各モード(→ [cli.md](cli.md))に対応するツールを公開する。実体は C シム経由で
`calc_eval` および各 `display` 関数を呼ぶ。

| MCP ツール | 対応 CLI | 内容 |
|-----------|----------|------|
| `evaluate` | `-e` | 式評価 |
| `bitwise` | `-b` | ビット演算式の評価 |
| `format_number` | `-f` | 指定形式(`dec`/`hex`/`oct`/`bin`/`all`)で表示 |
| `convert_unit` | `-u` | 単位変換 `conv(値, 前, 後)` |
| `logarithms` | `-l` | ln / log2 / log10 を一括表示 |
| `char_size` | `-s` | 文字数・各エンコードのバイト数 |
| `list_types` | `-t` | 整数型・浮動小数点型の一覧 |
| `list_encodings` | `-c` | 文字コードの一覧 |

各ツールの説明文(description)がそのまま LLM 向けのヒントになる。具体例を含める。

## クライアントからの利用

- **Claude Code / Claude Desktop**: MCP サーバーとして登録し、上記ツールを呼び出す。
- **LangChain / LangGraph**(当初の用途・別リポジトリ): `langchain-mcp-adapters` で MCP ツールを
  LangChain ツールへ変換して利用する。`@tool` の手書きは不要。

いずれの場合も **LangChain 等への依存はこのリポジトリに持ち込まない**(MCP 標準で疎結合に保つ)。

## 設計判断メモ

- **なぜ単一バイナリ + FFI 埋め込みか**: オーバーヘッドの支配項は「呼び出しごとのプロセス生成」であり、
  エンジンを埋め込んで FFI 直呼びにすれば解消できる。加えて Python 等のランタイム依存を排し、配布を
  1 ファイルに単純化する。
- **なぜ Rust か**: `cc` クレートで C ソースをそのままビルドして静的リンクでき、単一バイナリ化が素直。
  FFI は `extern "C"` でほぼゼロコスト。MCP は公式 `rmcp` クレートを利用する。
- **なぜ薄い C シムか**: `display` の整形を再利用して CLI と出力を一致させ、整形仕様
  (→ [display.md](display.md))の二重管理を避けるため。計算ロジックは不変に保つ。
- **なぜ `agent-tools/` か**: 「AI エージェントから使うツール」という主目的を名前で表し、C 本体(`src/`)とは
  別ディレクトリに隔離する。
