# calc-pgm-mcp

calc-pgm の計算エンジン(C)を FFI で静的に埋め込んだ、単一バイナリの **MCP サーバー**。
外部プロセスや Python ランタイムを必要とせず、calc-pgm の全機能を MCP 対応クライアントへ公開する。

設計の詳細は [../docs/agent-tools.md](../docs/agent-tools.md) を参照。

## ビルド

```bash
cd agent-tools
cargo build --release      # 生成物: target/release/calc-pgm-mcp
```

`build.rs` が `../src/{lexer,parser,display}.c` と `csrc/calc_shim.c` をコンパイルして
バイナリに静的リンクする(REPL/CLI 用の `../src/main.c` は含めない)。

## テスト

```bash
cargo test                 # C エンジンを埋め込んだ状態で FFI ラッパーを検証
```

## 提供ツール

| ツール | 内容 |
|--------|------|
| `evaluate_expression` | 数式・ビット演算式を評価 |
| `evaluate_bitwise` | ビット演算式を評価(`evaluate_expression` と同じ評価器) |
| `calculate_logarithms` | ln / log2 / log10 を一括表示 |
| `format_number` | 指定形式(dec/hex/oct/bin/all)で表示 |
| `convert_unit` | 単位変換(データサイズ/周波数/時間) |
| `get_char_size` | 文字数・各エンコードのバイト数 |
| `list_types` | 整数型・浮動小数点型の一覧 |
| `list_encodings` | 文字コードの一覧 |

## MCP クライアントへの登録

stdio トランスポートで動作する(クライアントがバイナリを子プロセスとして起動)。
`command` は必ず**絶対パス**を指定する。

**Claude Code**

```bash
claude mcp add calc-pgm -- /absolute/path/to/agent-tools/target/release/calc-pgm-mcp
```

**設定ファイル形式(Claude Desktop 等)**

```json
{
  "mcpServers": {
    "calc-pgm": {
      "command": "/absolute/path/to/agent-tools/target/release/calc-pgm-mcp"
    }
  }
}
```

## 手動疎通(デバッグ用)

サーバーは stdin から JSON-RPC を受け取り stdout に応答する。引数なしで直接起動すると
入力待ちで固まって見えるが正常。1 行ずつ流すと応答が返り、stdin が閉じると終了する。

```bash
printf '%s\n' \
'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"t","version":"0"}}}' \
'{"jsonrpc":"2.0","method":"notifications/initialized"}' \
'{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' \
'{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"evaluate_expression","arguments":{"expression":"1 + 2 * 3"}}}' \
| ./target/release/calc-pgm-mcp
```

## LangChain / LangGraph(ReAct)からの利用

`langchain-mcp-adapters` で MCP ツールを LangChain ツールへ変換し、ReAct エージェントから
利用する(`@tool` の手書きは不要)。**この連携コードは calc-pgm リポジトリの外**(ReAct 側)に置く。

セットアップ:

```bash
python3 -m venv .venv && source .venv/bin/activate
pip install langgraph langchain-mcp-adapters langchain-anthropic
export ANTHROPIC_API_KEY=sk-ant-...
```

`react_test.py`:

```python
import asyncio
from langchain_mcp_adapters.client import MultiServerMCPClient
from langgraph.prebuilt import create_react_agent
from langchain_anthropic import ChatAnthropic

BIN = "/absolute/path/to/agent-tools/target/release/calc-pgm-mcp"

async def main():
    client = MultiServerMCPClient({
        "calc-pgm": {"command": BIN, "args": [], "transport": "stdio"}
    })
    tools = await client.get_tools()
    print("tools:", [t.name for t in tools])

    agent = create_react_agent(ChatAnthropic(model="claude-opus-4-8"), tools)
    result = await agent.ainvoke({
        "messages": [("user", "2GiB は何 MB？ 0xFF を2進数で。1024 の log2 は？")]
    })
    print(result["messages"][-1].content)

if __name__ == "__main__":
    asyncio.run(main())
```

実行:

```bash
python react_test.py
```
