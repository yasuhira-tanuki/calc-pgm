// calc-pgm の計算エンジン(C)と表示用 C シムをコンパイルし、
// Rust バイナリへ静的にリンクする。REPL/CLI 用の src/main.c は含めない。

use std::path::PathBuf;

fn main() {
    // build.rs の CWD はマニフェストディレクトリ (agent-tools/)。
    // リポジトリルートは 1 つ上。
    let root = PathBuf::from("..");
    let src = root.join("src");
    let include = root.join("include");

    let engine_srcs = ["lexer.c", "parser.c", "display.c"];

    let mut build = cc::Build::new();
    build
        .include(&include)
        .flag_if_supported("-std=c11")
        .warnings(false);
    for f in &engine_srcs {
        build.file(src.join(f));
    }
    build.file("csrc/calc_shim.c");
    build.compile("calcengine");

    // display.c / parser.c が数学関数 (log, log2, log10, pow) を使うため libm をリンク。
    println!("cargo:rustc-link-lib=m");

    // 再ビルドトリガ
    println!("cargo:rerun-if-changed=csrc/calc_shim.c");
    println!("cargo:rerun-if-changed={}", include.display());
    for f in &engine_srcs {
        println!("cargo:rerun-if-changed={}", src.join(f).display());
    }
}
