# 設計・仕様ドキュメント

calc-pgm の内部設計と仕様をまとめた開発者向けドキュメントです。
利用者向けの使い方は [README.md](../README.md) を参照してください。

パイプライン: 入力文字列 → Lexer → Parser → Value → Display

## 目次

| ドキュメント | 内容 |
|--------------|------|
| [architecture.md](architecture.md) | モジュール構成・データフロー・`Value` 型 |
| [lexer.md](lexer.md) | Lexer の設計・トークン・数値リテラル |
| [parser.md](parser.md) | Parser の設計・演算子優先順位・特殊形式 |
| [display.md](display.md) | 表示仕様(2進数表示・浮動小数点表示) |
| [error-handling.md](error-handling.md) | Lexer/Parser の 2 段階エラー伝播 |
