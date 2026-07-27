# 表示仕様

`display.c` は `Value` を各形式で出力する。表示形式は `Value.fmt` (`VFmt`) で制御される
(→ [architecture.md の Value 型](architecture.md#value-型))。

## 出力先の指定 (2 系統の API)

整形ロジックは出力先を `FILE*` で受ける `fprint_*` 系に実装し、`print_*` 系はそれに `stdout` を
渡すだけの薄いラッパーとする。両者の出力内容は完全に同一。

| API | 用途 |
|-----|------|
| `print_result` / `print_types` / `print_encodings` / `print_size` / `print_log_result` | CLI・REPL からの `stdout` 出力 |
| `fprint_result` / `fprint_types` / `fprint_encodings` / `fprint_size` / `fprint_log_result` | 任意の `FILE*` へ出力 |

`fprint_*` は `open_memstream` で作ったメモリストリームへ整形結果を取り込む用途を想定している
(→ [agent-tools.md](agent-tools.md) の C シム)。これにより CLI とエージェント連携層で整形仕様を
共有し、二重管理を避ける。

`print_help` のみラッパーを持たず `stdout` 固定 (CLI 専用のため)。

## 2進数表示の仕様

`fprint_bin()` の挙動: 負数は64bit全体を4ビット区切りで表示、正数は先頭ゼロを省略して最上位ビットから表示。

## 浮動小数点表示の仕様

`%.10f` (小数点以下10桁固定) でフォーマットし、末尾ゼロと末尾ドットを除去して出力する。
科学的記数法 (`1e-06` 等) は使用しない。
