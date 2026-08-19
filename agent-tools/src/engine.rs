//! calc-pgm の C シム(csrc/calc_shim.c)への FFI 束縛と、安全な Rust ラッパー。
//!
//! 各関数は結果文字列(display と同じ整形)を `Ok`、計算エラーのメッセージを `Err`
//! で返す。呼び出しは `LOCK` で直列化し、C エンジンの再入可能性に依存せず
//! スレッド安全を保証する(1 回の計算は μ秒オーダーで競合コストは無視できる)。

use std::ffi::{c_char, CString};
use std::sync::Mutex;

extern "C" {
    fn calc_eval_str(expr: *const c_char, out: *mut c_char, outsz: i32, err: *mut c_char, errsz: i32) -> i32;
    fn calc_format_str(fmt: *const c_char, expr: *const c_char, out: *mut c_char, outsz: i32, err: *mut c_char, errsz: i32) -> i32;
    fn calc_conv_str(value: *const c_char, from: *const c_char, to: *const c_char, out: *mut c_char, outsz: i32, err: *mut c_char, errsz: i32) -> i32;
    fn calc_log_str(expr: *const c_char, out: *mut c_char, outsz: i32, err: *mut c_char, errsz: i32) -> i32;
    fn calc_size_str(text: *const c_char, out: *mut c_char, outsz: i32);
    fn calc_types_str(out: *mut c_char, outsz: i32);
    fn calc_encodings_str(out: *mut c_char, outsz: i32);
}

/// C エンジン呼び出しの直列化ロック。
static LOCK: Mutex<()> = Mutex::new(());

/// 出力バッファ長。型一覧・文字コード一覧の表でも十分な余裕を持たせる。
const OUT_BUF: usize = 65536;
/// エラーメッセージバッファ長(C 側 errmsg と同じ 256)。
const ERR_BUF: usize = 256;

/// &str を CString へ。内部 NUL はエラー。
fn to_cstring(s: &str) -> Result<CString, String> {
    CString::new(s).map_err(|_| "入力に NUL 文字を含めることはできません".to_string())
}

/// C の char バッファ(NUL 終端)を Rust の String へ(不正 UTF-8 は置換)。
fn read_cbuf(buf: &[c_char]) -> String {
    let bytes = unsafe { std::slice::from_raw_parts(buf.as_ptr() as *const u8, buf.len()) };
    let n = bytes.iter().position(|&c| c == 0).unwrap_or(bytes.len());
    String::from_utf8_lossy(&bytes[..n]).trim_end().to_string()
}

/// -e: 式の演算
pub fn evaluate(expr: &str) -> Result<String, String> {
    let e = to_cstring(expr)?;
    let mut out = vec![0 as c_char; OUT_BUF];
    let mut err = vec![0 as c_char; ERR_BUF];
    let _g = LOCK.lock().unwrap();
    let rc = unsafe {
        calc_eval_str(e.as_ptr(), out.as_mut_ptr(), OUT_BUF as i32, err.as_mut_ptr(), ERR_BUF as i32)
    };
    if rc != 0 { Err(read_cbuf(&err)) } else { Ok(read_cbuf(&out)) }
}

/// -f: 形式指定表示 (fmt: dec/hex/oct/bin/all)
pub fn format_number(fmt: &str, expr: &str) -> Result<String, String> {
    let f = to_cstring(fmt)?;
    let e = to_cstring(expr)?;
    let mut out = vec![0 as c_char; OUT_BUF];
    let mut err = vec![0 as c_char; ERR_BUF];
    let _g = LOCK.lock().unwrap();
    let rc = unsafe {
        calc_format_str(f.as_ptr(), e.as_ptr(), out.as_mut_ptr(), OUT_BUF as i32, err.as_mut_ptr(), ERR_BUF as i32)
    };
    if rc != 0 { Err(read_cbuf(&err)) } else { Ok(read_cbuf(&out)) }
}

/// -u: 単位変換 conv(value, from, to)
pub fn convert_unit(value: &str, from: &str, to: &str) -> Result<String, String> {
    let v = to_cstring(value)?;
    let fr = to_cstring(from)?;
    let t = to_cstring(to)?;
    let mut out = vec![0 as c_char; OUT_BUF];
    let mut err = vec![0 as c_char; ERR_BUF];
    let _g = LOCK.lock().unwrap();
    let rc = unsafe {
        calc_conv_str(v.as_ptr(), fr.as_ptr(), t.as_ptr(), out.as_mut_ptr(), OUT_BUF as i32, err.as_mut_ptr(), ERR_BUF as i32)
    };
    if rc != 0 { Err(read_cbuf(&err)) } else { Ok(read_cbuf(&out)) }
}

/// -l: ln / log2 / log10
pub fn logarithms(expr: &str) -> Result<String, String> {
    let e = to_cstring(expr)?;
    let mut out = vec![0 as c_char; OUT_BUF];
    let mut err = vec![0 as c_char; ERR_BUF];
    let _g = LOCK.lock().unwrap();
    let rc = unsafe {
        calc_log_str(e.as_ptr(), out.as_mut_ptr(), OUT_BUF as i32, err.as_mut_ptr(), ERR_BUF as i32)
    };
    if rc != 0 { Err(read_cbuf(&err)) } else { Ok(read_cbuf(&out)) }
}

/// -s: 文字数・各エンコードのバイト数
pub fn char_size(text: &str) -> Result<String, String> {
    let t = to_cstring(text)?;
    let mut out = vec![0 as c_char; OUT_BUF];
    let _g = LOCK.lock().unwrap();
    unsafe { calc_size_str(t.as_ptr(), out.as_mut_ptr(), OUT_BUF as i32) };
    Ok(read_cbuf(&out))
}

/// -t: 整数型・浮動小数点型の一覧
pub fn types() -> String {
    let mut out = vec![0 as c_char; OUT_BUF];
    let _g = LOCK.lock().unwrap();
    unsafe { calc_types_str(out.as_mut_ptr(), OUT_BUF as i32) };
    read_cbuf(&out)
}

/// -c: 文字コードの一覧
pub fn encodings() -> String {
    let mut out = vec![0 as c_char; OUT_BUF];
    let _g = LOCK.lock().unwrap();
    unsafe { calc_encodings_str(out.as_mut_ptr(), OUT_BUF as i32) };
    read_cbuf(&out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn eval_basic() {
        let s = evaluate("1 + 2").unwrap();
        assert!(s.contains("DEC: 3"), "got: {s}");
    }

    #[test]
    fn eval_bitwise() {
        let s = evaluate("0xFF & 0x0F").unwrap();
        assert!(s.contains("DEC: 15") && s.contains("HEX: 0xF"), "got: {s}");
    }

    #[test]
    fn format_bin() {
        let s = format_number("bin", "255").unwrap();
        assert_eq!(s, "  BIN: 0b1111_1111");
    }

    #[test]
    fn format_float_hex_is_error() {
        let e = format_number("hex", "3.14").unwrap_err();
        assert!(e.contains("整数のみ"), "got: {e}");
    }

    #[test]
    fn convert_unit_ok() {
        let s = convert_unit("2", "gib", "mb").unwrap();
        assert!(s.contains("2147.483648"), "got: {s}");
    }

    #[test]
    fn logarithms_ok() {
        let s = logarithms("1024").unwrap();
        assert!(s.contains("log2 : 10"), "got: {s}");
    }

    #[test]
    fn logarithms_nonpositive_is_error() {
        assert!(logarithms("0").is_err());
    }

    #[test]
    fn eval_error() {
        assert!(evaluate("1 +").is_err());
    }

    #[test]
    fn types_and_encodings_nonempty() {
        assert!(types().contains("int64_t"));
        assert!(encodings().contains("UTF-8"));
    }

    #[test]
    fn char_size_ok() {
        let s = char_size("Hello").unwrap();
        assert!(s.contains("文字数: 5"), "got: {s}");
    }
}
