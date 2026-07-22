# CLAUDE.md
This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 言語設定
- 常に日本語で会話する

## ビルドと実行

```bash
make          # ビルド (build/*.o → ./calc-pgm)
make clean    # 成果物を削除
./calc-pgm    # 対話型REPLを起動
```

コンパイラフラグ: `gcc -Wall -Wextra -O2 -std=c11 -Iinclude`

ビルド成果物は `build/` ディレクトリに出力される。`build/` と実行ファイル `calc-pgm` は自動生成物のため削除・再生成して問題ない。

## アーキテクチャ

パイプライン: 入力文字列 → Lexer → Parser → Value → Display

詳細な設計・仕様は `docs/` を参照:

- [docs/architecture.md](docs/architecture.md) — モジュール構成・データフロー・`Value` 型
- [docs/lexer.md](docs/lexer.md) — Lexer の設計・トークン・数値リテラル
- [docs/parser.md](docs/parser.md) — Parser の設計・演算子優先順位・特殊形式
- [docs/display.md](docs/display.md) — 表示仕様(2進数表示・浮動小数点表示)
- [docs/error-handling.md](docs/error-handling.md) — Lexer/Parser の 2 段階エラー伝播
- [docs/agent-tools.md](docs/agent-tools.md) — AI エージェント連携(`agent-tools/`: Rust 製の単一バイナリ MCP サーバー、C エンジンを FFI 埋め込み)
