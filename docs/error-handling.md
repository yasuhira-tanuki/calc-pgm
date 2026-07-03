# エラー伝播

Lexer と Parser の2段階でエラーを管理する。`Lex.err` / `Lex.errmsg` と
`Parser.err` / `Parser.errmsg` がそれぞれ独立して保持され、`p_errmsg()` は
Lexer エラーを優先して返す。エラー発生後も各 `parse_*` 関数は `p_has_err()` で
チェックして早期リターンし、連鎖エラーを防ぐ。
