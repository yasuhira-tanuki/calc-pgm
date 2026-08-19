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
- **本体の計算ロジックは不変**: `src/` の計算ロジック(`lexer.c`/`parser.c`)には手を入れない。
  表示(`display.c`)は出力先を `FILE*` に一般化する挙動不変の小改修のみ行い、埋め込み・受け渡し用の
  薄い C シムを追加する。

### オーバーヘッド設計上の要点

| 層 | 対処 |
|----|------|
| プロセス生成(`fork`/`exec`) | **無くす**。エンジンを埋め込み、FFI 関数呼び出し(μ秒オーダー)にする |
| 言語ランタイム | **無くす**。Rust の単一バイナリ(外部依存は libc / libm のみ)。FFI は `extern "C"` でほぼゼロコスト |
| 計算 | C 本体(`lexer.c`/`parser.c`)がそのまま担う |

## 構成

```
agent-tools/
├── Cargo.toml            # Rust プロジェクト定義(rmcp などに依存)
├── Cargo.lock            # 依存バージョンの固定(バイナリクレートのためコミットする)
├── build.rs              # cc クレートで C エンジン + シムをコンパイルし静的リンク
├── .gitignore            # /target(ビルド成果物)を除外
├── csrc/
│   └── calc_shim.c       # calc_eval / 各 display をバッファ経由で呼ぶ薄い C API
├── src/
│   ├── main.rs           # MCP サーバー(ツール定義・stdio トランスポート起動)
│   └── engine.rs         # FFI 宣言(extern "C")+ 安全な Rust ラッパー
└── README.md
```

### ビルドと埋め込み

- `cargo build --release` で単一バイナリ(例: `calc-pgm-mcp`)を生成する。
- `build.rs` が `cc` クレートで **`src/lexer.c` / `src/parser.c` / `src/display.c` と `csrc/calc_shim.c`** を
  コンパイルし、Rust バイナリに静的リンクする。REPL/CLI 用の `src/main.c` は**使わない**(エンジンのみ埋め込む)。
- 生成物は 1 ファイルで完結する。実行時に `calc-pgm` バイナリや Python ランタイムを必要としない
  (共有ライブラリへの依存は libc / libm / libgcc_s のみ)。

## FFI 境界と C シム

`calc_eval`(→ [parser.md](parser.md))は `Value` を返し、DEC/HEX/OCT/BIN 等の整形は `display.c` が
`stdout` へ出力する。FFI から**文字列**として結果を得るため、`csrc/calc_shim.c` に薄い C API を追加し、
`display` の整形ロジックを **`FILE*`/バッファ経由で再利用**する(CLI と出力が完全一致し、整形の二重管理を避ける)。

- 計算ロジック(`calc_eval` 本体)には手を入れない。追加するのは表示・受け渡し用の薄い API のみ。
- C シムは `open_memstream` 上の `FILE*` へ `fprint_*` を出力させる。プロセスの `stdout` には
  一切書かないため、MCP の stdio 通信路と干渉しない。
- Rust 側(`engine.rs`)は `extern "C"` 宣言を安全な Rust 関数に包み、計算エラーは `Result::Err` で返す。

### Rust 側で保証する境界条件

| 項目 | 扱い |
|------|------|
| スレッド安全 | C エンジンの再入可能性に依存せず、全 FFI 呼び出しを `Mutex` で直列化する。1 回の計算は μ秒オーダーのため競合コストは無視できる |
| NUL 終端 | 入力は `CString` へ変換し、内部 NUL を含む文字列はエラーとして弾く |
| バッファ長 | 出力バッファは 64KiB 固定 (`OUT_BUF`)、エラーバッファは 256B (C 側 `errmsg` と同値)。長さは常に C へ明示的に渡す |
| 出力の切り詰め | C 側は `snprintf` で書き込むため、64KiB を超える出力は黙って切り詰められる。現行の最大出力(型一覧・文字コード一覧)は数 KB で余裕がある |
| UTF-8 | `String::from_utf8_lossy` で読み取る。不正バイトはエラーとせず置換文字に変換する(パニックを起こさない方を優先) |

## 公開する MCP ツール(全モード)

CLI の各モード(→ [cli.md](cli.md))に対応するツールを公開する。実体は C シム経由で
`calc_eval` および各 `display` 関数を呼ぶ。

| MCP ツール | 対応 CLI | 内容 |
|-----------|----------|------|
| `evaluate_expression` | `-e` | 式の演算 |
| `evaluate_bitwise` | `-b` | ビット演算を含む式の演算 |
| `calculate_logarithms` | `-l` | ln / log2 / log10 を一括表示 |
| `format_number` | `-f` | 指定形式(`dec`/`hex`/`oct`/`bin`/`all`)で表示 |
| `convert_unit` | `-u` | 単位変換 `conv(値, 前, 後)` |
| `get_char_size` | `-s` | 文字数・各エンコードのバイト数 |
| `list_types` | `-t` | 整数型・浮動小数点型の一覧 |
| `list_encodings` | `-c` | 文字コードの一覧 |

**ツール名の付け方は tanuki-blueprint の MCP 規約に従う**(snake_case・動詞から始める・
プロダクト名を入れない)。クライアントが `mcp__<サーバー名>__<ツール名>` の形で名前空間を
付けるため、ツール名の側にプロダクト名は入れない。表の並びは規約側のツール台帳と同じ順にしてある。

各ツールの説明文(description)がそのまま LLM 向けのヒントになる。**何をするかに加えて
「いつ呼ぶべきか」を書く**(具体例も含める)。`evaluate_bitwise` は `evaluate_expression` と
演算処理が同一のため、使い分けの手がかりは description にしかない。

## クライアントからの利用

いずれの方法でも、まず release ビルドしたバイナリの**絶対パス**を用意する
(`agent-tools/target/release/calc-pgm-mcp`)。stdio トランスポートのため、クライアントが
このバイナリを子プロセスとして起動する(ローカル利用)。

### Claude Code

```bash
claude mcp add calc-pgm -- /absolute/path/to/agent-tools/target/release/calc-pgm-mcp
```

### 設定ファイル形式(Claude Desktop 等)

```json
{
  "mcpServers": {
    "calc-pgm": {
      "command": "/absolute/path/to/agent-tools/target/release/calc-pgm-mcp"
    }
  }
}
```

### 手動疎通(デバッグ用)

サーバーは stdin から JSON-RPC を受け取り stdout に応答する。1 行ずつ流して確認できる。

```bash
printf '%s\n' \
'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"t","version":"0"}}}' \
'{"jsonrpc":"2.0","method":"notifications/initialized"}' \
'{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' \
'{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"evaluate_expression","arguments":{"expression":"1 + 2 * 3"}}}' \
| ./target/release/calc-pgm-mcp
```

### LangChain / LangGraph(当初の用途・別リポジトリ)

`langchain-mcp-adapters` で MCP ツールを LangChain ツールへ変換し、ReAct エージェント等から
利用する(`@tool` の手書きは不要)。

```python
from langchain_mcp_adapters.client import MultiServerMCPClient
from langgraph.prebuilt import create_react_agent
from langchain_anthropic import ChatAnthropic

client = MultiServerMCPClient({
    "calc-pgm": {
        "command": "/absolute/path/to/agent-tools/target/release/calc-pgm-mcp",
        "args": [],
        "transport": "stdio",
    }
})
tools = await client.get_tools()
agent = create_react_agent(ChatAnthropic(model="claude-opus-4-8"), tools)
```

いずれの場合も **LangChain 等への依存はこのリポジトリに持ち込まない**(MCP 標準で疎結合に保つ)。
具体的な実行手順は [../agent-tools/README.md](../agent-tools/README.md) を参照。

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
