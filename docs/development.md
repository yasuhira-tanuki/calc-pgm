# 開発者向けガイド

ビルド方法・依存関係・開発時の操作をまとめる。
内部設計は [architecture.md](architecture.md)、機能仕様は [features.md](features.md) を参照。

## 前提環境

- C11 対応の C コンパイラ(既定は `gcc`)
- `make`
- POSIX 環境(Linux 等)。文字コード変換に `iconv`(glibc 同梱)を使用する。

## ビルド

```bash
make          # ビルド (build/*.o → ./calc-pgm)
make clean    # build/ と実行ファイルを削除
```

- `make`(= `make all`)は `src/*.c` を `build/*.o` にコンパイルし、`calc-pgm` にリンクする。
- `build/` ディレクトリはビルド時に自動生成される。
- コンパイラを変える場合は `make CC=clang` のように上書きできる。

## コンパイラフラグ

```
-Wall -Wextra -O2 -std=c11 -Iinclude
```

| フラグ | 意味 |
|--------|------|
| `-Wall -Wextra` | 警告を広く有効化(警告ゼロを維持する) |
| `-O2` | 最適化 |
| `-std=c11` | C11 準拠 |
| `-Iinclude` | ヘッダ探索パスに `include/` を追加 |

リンク時に数学関数用の `-lm` を付与する(`log` / `log2` / `log10` / `pow` で使用)。

## ビルド成果物

- `build/*.o` … 中間オブジェクト
- `calc-pgm` … 実行ファイル

いずれも自動生成物で、`.gitignore` 管理下にある。削除・再生成して差し支えない。

## 実行

```bash
./calc-pgm            # 対話モード (REPL) を起動
./calc-pgm -h         # ヘルプ
./calc-pgm -e "1 + 2" # 引数モードで式を評価
```

CLI の全モードと入出力の仕様は [cli.md](cli.md) を参照。

## 開発フロー

- ソース構成とデータフローは [architecture.md](architecture.md) を参照。
- ソースを編集したら `make` で差分ビルドされる(変更した `.c` のみ再コンパイル)。
- ヘッダ(`include/*.h`)を変更した場合は `make clean && make` で全体を再ビルドすると確実。
- 動作確認は `./calc-pgm -e "<式>"` で個別に、または REPL で対話的に行う。
