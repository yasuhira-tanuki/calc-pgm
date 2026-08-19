//! calc-pgm MCP サーバー。
//!
//! calc-pgm の計算エンジン(C)を FFI で埋め込み(engine モジュール)、
//! 各機能を MCP ツールとして stdio 経由で公開する。単一バイナリで動作し、
//! 外部プロセスや Python ランタイムを必要としない。

mod engine;

use rmcp::{
    handler::server::{router::tool::ToolRouter, wrapper::Parameters},
    model::{CallToolResult, ContentBlock, Implementation, ServerCapabilities, ServerInfo},
    tool, tool_handler, tool_router,
    transport::stdio,
    ErrorData, ServerHandler, ServiceExt,
};
use serde::Deserialize;

/// Result<String, String>(engine の戻り)を MCP のツール結果へ変換する。
/// 計算エラーは isError=true のツール結果として返し、LLM が自己修正できるようにする。
fn tool_result(r: Result<String, String>) -> CallToolResult {
    match r {
        Ok(text) => CallToolResult::success(vec![ContentBlock::text(text)]),
        Err(msg) => CallToolResult::error(vec![ContentBlock::text(format!("エラー: {msg}"))]),
    }
}

#[derive(Debug, Deserialize, schemars::JsonSchema)]
pub struct ExprArgs {
    /// 評価する式。例: "1 + 2 * 3", "0xFF & 0x0F", "2 ** 10"
    pub expression: String,
}

#[derive(Debug, Deserialize, schemars::JsonSchema)]
pub struct FormatArgs {
    /// 表示形式。dec / hex / oct / bin / all のいずれか
    pub format: String,
    /// 評価する式
    pub expression: String,
}

#[derive(Debug, Deserialize, schemars::JsonSchema)]
pub struct ConvertArgs {
    /// 変換する値(式も可)。例: "2", "1+1"
    pub value: String,
    /// 変換前の単位。例: "gib", "ms", "mhz"
    pub from_unit: String,
    /// 変換後の単位。例: "mb", "us", "ghz"
    pub to_unit: String,
}

#[derive(Debug, Deserialize, schemars::JsonSchema)]
pub struct TextArgs {
    /// 対象の文字列
    pub text: String,
}

#[derive(Clone)]
pub struct CalcServer {
    // tool_router はマクロ生成の ServerHandler 実装から参照される
    // (dead_code 解析では検出されないため明示的に許可)。
    #[allow(dead_code)]
    tool_router: ToolRouter<CalcServer>,
}

#[tool_router]
impl CalcServer {
    pub fn new() -> Self {
        Self { tool_router: Self::tool_router() }
    }

    /// 数式を評価し、結果を DEC/HEX/OCT/BIN 等で表示する。算術・ビット演算・べき乗・
    /// 進数リテラル(0x/0o/0b)・単位変換関数・対数関数などに対応。例: "1 + 2 * 3"
    /// 計算式の答えが必要なときにまず呼ぶ汎用ツール。基数を指定したい場合は format_number、
    /// ビット演算が主目的の場合は evaluate_bitwise を使う。
    #[tool]
    async fn evaluate_expression(&self, Parameters(a): Parameters<ExprArgs>) -> Result<CallToolResult, ErrorData> {
        Ok(tool_result(engine::evaluate(&a.expression)))
    }

    /// ビット演算式を評価する(evaluate_expression と同じ評価器。ビット演算用途を明示するためのツール)。
    /// 例: "0xFF & 0x0F", "1 << 8", "~0", "0b1100 ^ 0b1010"
    /// マスク・シフト・フラグ操作などビット演算が主目的のときに呼ぶ。結果は evaluate_expression と同じ。
    #[tool]
    async fn evaluate_bitwise(&self, Parameters(a): Parameters<ExprArgs>) -> Result<CallToolResult, ErrorData> {
        Ok(tool_result(engine::evaluate(&a.expression)))
    }

    /// 式を評価し、指定形式で結果を表示する。format は dec/hex/oct/bin/all。
    /// hex/oct/bin は整数のみ対応(浮動小数点はエラー)。
    /// 出力の基数を指定したいとき(16 進だけ見たい、全基数を並べて見たい)に呼ぶ。
    #[tool]
    async fn format_number(&self, Parameters(a): Parameters<FormatArgs>) -> Result<CallToolResult, ErrorData> {
        Ok(tool_result(engine::format_number(&a.format, &a.expression)))
    }

    /// 単位変換を行う。同一カテゴリ(データサイズ/周波数/時間)内でのみ変換可能。
    /// 例: value="2", from_unit="gib", to_unit="mb"
    /// GiB→MB、ms→us、MHz→GHz のように単位をまたいだ値が必要なときに呼ぶ。
    #[tool]
    async fn convert_unit(&self, Parameters(a): Parameters<ConvertArgs>) -> Result<CallToolResult, ErrorData> {
        Ok(tool_result(engine::convert_unit(&a.value, &a.from_unit, &a.to_unit)))
    }

    /// 式を評価し、その値の ln(自然対数)/ log2 / log10 を一括表示する。値は正の数が必要。
    /// ビット幅の見積もりや桁数の概算など、底の異なる対数をまとめて見たいときに呼ぶ。
    #[tool]
    async fn calculate_logarithms(&self, Parameters(a): Parameters<ExprArgs>) -> Result<CallToolResult, ErrorData> {
        Ok(tool_result(engine::logarithms(&a.expression)))
    }

    /// 文字列の Unicode コードポイント数と、各エンコード(UTF-8/16/32, Shift-JIS, EUC-JP, ASCII)
    /// でのバイト数を表示する。
    /// 文字列がバッファやカラム長に収まるかを、エンコードごとに確認したいときに呼ぶ。
    #[tool]
    async fn get_char_size(&self, Parameters(a): Parameters<TextArgs>) -> Result<CallToolResult, ErrorData> {
        Ok(tool_result(engine::char_size(&a.text)))
    }

    /// C 言語の整数型・浮動小数点型のサイズ・値の範囲・有効桁数の一覧を表示する。
    /// 型を選ぶとき、値が収まるか(オーバーフローしないか)を確認したいときに呼ぶ。
    #[tool]
    async fn list_types(&self) -> Result<CallToolResult, ErrorData> {
        Ok(CallToolResult::success(vec![ContentBlock::text(engine::types())]))
    }

    /// 文字コード(ASCII, UTF-8/16/32, Shift-JIS, EUC-JP)のバイト数・収録文字数・備考の一覧を表示する。
    /// エンコードを選ぶとき、および get_char_size の結果を解釈する前提を確認したいときに呼ぶ。
    #[tool]
    async fn list_encodings(&self) -> Result<CallToolResult, ErrorData> {
        Ok(CallToolResult::success(vec![ContentBlock::text(engine::encodings())]))
    }
}

#[tool_handler]
impl ServerHandler for CalcServer {
    fn get_info(&self) -> ServerInfo {
        // ServerInfo は #[non_exhaustive] のため default() を基点に上書きする。
        let mut info = ServerInfo::default();
        // from_build_env() は rmcp クレート名を拾うため、サーバー名を明示する。
        info.server_info = Implementation::new("calc-pgm", env!("CARGO_PKG_VERSION"));
        info.capabilities = ServerCapabilities::builder().enable_tools().build();
        info.instructions = Some(
            "calc-pgm: プログラミング用計算機。式評価・ビット演算・進数変換・単位変換・\
             対数・文字サイズ・型/文字コード一覧を提供します。整数は int64、\
             ビット演算・シフト・剰余は整数のみ対応です。"
                .into(),
        );
        info
    }
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let service = CalcServer::new().serve(stdio()).await?;
    service.waiting().await?;
    Ok(())
}
