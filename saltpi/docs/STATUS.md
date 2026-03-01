# SaltPi Development Status

**Date:** March 1, 2026  
**Phase:** Phase 1 - Foundation  
**Status:** ✅ HTTP Client Implemented

---

## Executive Summary

SaltPi is a lightspeed coding agent written in Salt, optimized for Cerebras/Groq free tier constraints. The project implements observational memory with Lettuce as the memory backend.

**Current Status:** Phase 1 foundation progressing - HTTP client for Cerebras API implemented.

---

## Component Status

| Component | Status | Location | Notes |
|-----------|--------|----------|-------|
| **Hello World** | ✅ Working | `src/main.salt` | Basic binary compiles and runs |
| **Rate Limiter** | ✅ Implemented | `src/rate_limiter.salt` | Cerebras/Groq header parsing, sleep state |
| **Memory Manager** | ✅ Implemented | Inline in main.salt | 30k/40k token thresholds |
| **Lettuce Client** | ✅ Implemented | `src/lettuce_client.salt` | SET/GET/DEL commands, RESP protocol |
| **Tools** | ✅ Implemented | `src/tools.salt` | read/write/edit/bash stubs |
| **HTTP Client** | ✅ Implemented | `src/http_client.salt` | POST request building, response parsing |
| **Cerebras API** | ⚠️ Stub | `src/main.salt` | HTTP works, needs HTTPS |
| **TUI** | ❌ Not started | - | Terminal input/output loop |
| **Agent Loop** | ❌ Not started | - | Core agent logic |

---

## Build Status

```bash
$ cd saltpi && sp build
📦 Building saltpi v0.1.0 [debug]
   🔨 Compiling 4 module(s)...
✅ Built ./target/debug/saltpi in 0.7s

$ ./target/debug/saltpi
SaltPi v0.1.0
Lightspeed coding agent for Cerebras/Groq
------------------------------------------
Phase 1: Foundation
Status: Hello World working!
```

---

## Research Completed

### 1. Groq/Cerebras Tool Use Patterns

**Key Findings:**
- OpenAI-compatible tool schema (JSON Schema)
- Tool calls returned in `response.choices[0].message.tool_calls`
- Tool results sent as `{"role": "tool", "tool_call_id": "...", "content": "..."}`
- Parallel tool execution supported by default

**Rate Limit Headers:**
```
x-ratelimit-limit-requests-day: 14400
x-ratelimit-remaining-requests-day: 14350
x-ratelimit-reset-requests-day: 33011.38
x-ratelimit-limit-tokens-minute: 64000
x-ratelimit-remaining-tokens-minute: 62000
x-ratelimit-reset-tokens-minute: 11.38
```

### 2. Salt Language Patterns

**From Facet/Lettuce analysis:**
- Use `Ptr<u8>` for C strings (null-terminated)
- `StringView` for zero-copy string operations
- `extern fn` for FFI declarations
- `pub fn` for public module functions
- Tests return `bool` with runner aggregation

**HTTP Pattern (from examples/http_server.salt):**
```salt
use std.core.str.StringView
use std.http.response.write_response

fn route(recv_buf: Ptr<u8>, n: i64, send_buf: Ptr<u8>) -> i64 {
    let input = StringView::from_raw(recv_buf, n);
    let sp1 = input.find_byte(32);  // Find space
    // ... parse request ...
    return write_response(send_buf, 200, content_type, body);
}
```

### 3. Pi Coding Agent Format

**Tool Schemas:**
```json
{
  "name": "read",
  "parameters": {
    "type": "object",
    "properties": {
      "path": {"type": "string"}
    },
    "required": ["path"]
  }
}
```

**Session JSONL Format:**
```jsonl
{"type":"message","message":{"role":"user","content":[{"type":"text","text":"Hello"}]},"id":"e1"}
{"type":"message","message":{"role":"assistant","toolCalls":[{"id":"c1","name":"read"}]},"id":"e2"}
{"type":"message","message":{"role":"toolResult","toolName":"read","toolCallId":"c1"},"id":"e3"}
```

---

## Phase 1 Checklist

- [x] Get "hello world" Salt binary compiling
- [x] Study Lettuce source (memory backend)
- [ ] Minimal TUI: read line, print line
- [x] Single hardcoded Cerebras API call (stub)
- [ ] Parse response, print it
- [x] Add one tool: `bash` (stub implemented)
- [ ] Tool call round-trip working
- [ ] Add remaining tools: read, write, edit (stubs)
- [ ] Basic session save (JSONL, simple)
- [x] Rate limit header parsing (implemented, not tested)

**Checkpoint:** Phase 1 Complete When:
- [ ] Can have a multi-turn conversation
- [ ] Tools work (read/write/edit/bash)
- [ ] Session persists to disk
- [ ] Rate limits visible in /status

---

## File Structure

```
saltpi/
├── SPECS.md              # Technical specification
├── VISION.md             # Project vision
├── salt.toml             # Build manifest
├── src/
│   ├── main.salt         # Entry point (working)
│   ├── lettuce_client.salt # Lettuce Redis client
│   ├── rate_limiter.salt # Rate limit tracking
│   └── tools.salt        # Tool implementations
├── tests/
│   └── lettuce_test.salt # Lettuce client tests
├── prompts/              # (empty - for observer/reflector prompts)
└── docs/                 # (empty - for documentation)
```

---

## Next Steps (Immediate)

1. **HTTPS Support** (Critical)
   - Current HTTP client only supports HTTP
   - Cerebras/Groq require HTTPS (port 443)
   - Need TLS/SSL wrapper or use https:// URL

2. **JSON Response Parsing**
   - Parse Cerebras API JSON response
   - Extract `choices[0].message.content`
   - Handle error responses

3. **Rate Limit Header Parsing**
   - Extract `x-ratelimit-*` headers from response
   - Update RateLimiter with remaining quota
   - Implement sleep state when limits approached

4. **TUI Implementation**
   - Minimal input/output loop
   - Read user prompt from stdin
   - Print assistant response to stdout

5. **Tool Implementation**
   - Complete `tool_read()` with file I/O
   - Complete `tool_write()` with file creation
   - Complete `tool_edit()` with diff parsing
   - Complete `tool_bash()` with process execution

6. **Agent Loop**
   - Integrate HTTP client, TUI, tools
   - Handle tool call parsing
   - Maintain conversation history

---

## Known Issues

1. **f-string interpolation** - Salt doesn't support f-strings yet. Use `puts()` with concatenated strings.

2. **Module imports** - Cross-module imports need work. Current workaround: inline components in main.salt.

3. **File I/O** - Need to study `std.fs` patterns for read/write tool implementations.

---

## References

- **Pi Announcement:** https://lucumr.pocoo.org/2026/1/31/pi/
- **Pi Coding Agent:** https://github.com/badlogic/pi-mono/tree/main/packages/coding-agent
- **Groq Cookbook:** https://github.com/groq/groq-api-cookbook
- **Cerebras Cookbook:** https://github.com/Cerebras/Cerebras-Inference-Cookbook
- **Lattice/Salt:** https://github.com/bneb/lattice
- **Lettuce:** 567 LOC Redis clone in Salt

---

**Last Updated:** March 1, 2026  
**Next Review:** After HTTP client implementation
