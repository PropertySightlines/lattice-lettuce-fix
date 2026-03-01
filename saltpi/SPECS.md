# SaltPi Technical Specification
Version: 0.1.0-draft
Target: Qwen Code implementation

## 1. Executive Summary

SaltPi is a minimal coding agent written in Salt, optimized for Cerebras and Groq free tier limits. It implements observational memory with native binary compression, using Lettuce as the memory backend.

### Key Metrics (Free Tier Constraints)

| Provider | Model | Speed | Context | RPM | TPM | Daily |
|----------|-------|-------|---------|-----|-----|-------|
| Cerebras | gpt-oss-120b | 3000 t/s | 65k | 30 | 60k | 1M |
| Cerebras | llama3.1-8b | 2200 t/s | 8k | 30 | 60k | 1M |
| Groq | llama-3.3-70b-versatile | ~800 t/s | 128k | 30 | 12k | 100k |
| Groq | llama-3.1-8b-instant | ~750 t/s | 128k | 30 | 6k | 500k |

### Design Constraints
- All memory ops < 1ms (Lettuce: 234K ops/sec)
- Context window capped at 50k tokens (leave headroom)
- Compaction runs during rate-limit sleep windows
- Single binary deployment (Salt + embedded Lettuce)

---

## 2. Architecture
┌─────────────────────────────────────────────────────────────────┐ │ SaltPi Agent │ ├─────────────────────────────────────────────────────────────────┤ │ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │ │ │ TUI │ │ Tools │ │ Provider │ │ Rate Limiter │ │ │ │ (Minimal)│ │ R/W/E/B │ │ Router │ │ + Sleep State │ │ │ └────┬─────┘ └────┬─────┘ └────┬─────┘ └────────┬─────────┘ │ │ │ │ │ │ │ │ └─────────────┴─────────────┴─────────────────┘ │ │ │ │ │ ┌────────────────────────▼────────────────────────────────────┐│ │ │ Memory Manager ││ │ │ ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ ││ │ │ │ Raw Block │ │ Observation │ │ Compressed Cold │ ││ │ │ │ (< 30k) │→ │ Block │→ │ Storage (LZ4) │ ││ │ │ │ │ │ (< 40k) │ │ │ ││ │ │ └─────────────┘ └─────────────┘ └─────────────────────┘ ││ │ └─────────────────────────┬───────────────────────────────────┘│ │ │ │ │ ┌─────────────────────────▼───────────────────────────────────┐│ │ │ Lettuce Backend ││ │ │ (Redis-compatible, 567 LOC, 234K ops/s) ││ │ └──────────────────────────────────────────────────────────────┘│ └─────────────────────────────────────────────────────────────────┘

--- ## 3. Core Components ### 3.1 Tool System (Pi-Compatible) ```salt // tools.salt — Four core tools only enum Tool { Read, // Read file contents Write, // Write file (creates if not exists) Edit, // Patch file with diff Bash, // Execute shell command } struct ToolCall { tool: Tool, args: ToolArgs, id: u64, } struct ToolResult { id: u64, success: bool, output: String, error: Option<String>, } // Tool implementations fn tool_read(path: &str) -> ToolResult; fn tool_write(path: &str, content: &str) -> ToolResult; fn tool_edit(path: &str, diff: &str) -> ToolResult; fn tool_bash(cmd: &str, timeout_ms: u64) -> ToolResult;
3.2 Provider Router
3.3 Rate Limiter + Sleep State
3.4 Memory Manager (Observational Memory)
3.5 Lettuce Integration
3.6 Session Management
4. System Prompt
You are a coding assistant. You have four tools:

1. **read** — Read file contents
2. **write** — Write file (creates if not exists)  
3. **edit** — Apply a unified diff to a file
4. **bash** — Run a shell command

Rules:
- Read before editing
- Prefer edit over write for existing files
- Run tests after changes
- Ask if uncertain

Context files are in <context>. Observations are in <observations>.
(~100 tokens — minimal as Pi prescribes)

5. Observer/Reflector Prompts
Observer Prompt (run at 30k raw threshold)
Compress these messages into observations. Format:

🔴 HH:MM <critical fact that affects current task>
🟡 HH:MM <important context>
🟢 HH:MM <routine information>

Rules:
- 🔴 = facts that would break things if forgotten
- 🟡 = preferences, decisions, constraints  
- 🟢 = general context, can be pruned
- Use present tense
- Include file paths when relevant
- Max 20 observations

Messages to compress:
{raw_messages}
Reflector Prompt (run at 40k observation threshold)
Review these observations. Remove redundant or outdated ones.

Current observations:
{observations}

Rules:
- Keep all 🔴 unless explicitly invalidated
- Merge duplicate 🟡 entries
- Aggressively prune 🟢 older than 1 hour
- Output: list of observation IDs to KEEP

Output format:
KEEP: 1, 3, 5, 7, 12, ...
6. CLI Interface
saltpi [OPTIONS] [PROMPT] OPTIONS: -c, --continue Continue most recent session -r, --resume Browse and select from past sessions -s, --session <PATH> Use specific session file -m, --model <MODEL> Override primary model -p, --print Print mode (non-interactive) --no-session Ephemeral mode --provider <NAME> Force provider (cerebras, groq) COMMANDS (in session): /model Switch model /tree Navigate session tree /compact Force compaction /observe Force observation /status Show rate limits, memory stats /session Show session info /quit Exit
7. File Structure
saltpi/ ├── SPECS.md # This file ├── VISION.md # Project vision ├── Cargo.toml # Salt build manifest ├── src/ │ ├── main.salt # Entry point, TUI loop │ ├── agent.salt # Core agent loop │ ├── tools.salt # Read/Write/Edit/Bash │ ├── providers.salt # Cerebras/Groq clients │ ├── rate_limiter.salt # Rate limit tracking + sleep state │ ├── memory.salt # Observational memory manager │ ├── compression.salt # LZ4 wrapper │ ├── session.salt # Session tree management │ ├── lettuce_client.salt # Lettuce integration │ └── tui.salt # Minimal terminal UI ├── prompts/ │ ├── system.md # System prompt │ ├── observer.md # Observer prompt template │ └── reflector.md # Reflector prompt template ├── tests/ │ ├── memory_test.salt │ ├── provider_test.salt │ └── session_test.salt └── docs/ ├── RATE_LIMITS.md # Provider rate limit reference ├── MEMORY.md # Memory system design └──DEVELOPMENT.md # Build instructions

8. Build & Run
# Build (requires Salt toolchain)
salt build --release

# Run
./target/release/saltpi "implement a REST API for todo items"

# With specific provider
./target/release/saltpi --provider cerebras "fix the failing tests"

# Continue session
./target/release/saltpi -c
9. Implementation Phases
Phase 1: Foundation (Week 1-2)
 Basic TUI (input/output)
 Single provider (Cerebras gpt-oss-120b)
 Four tools (Read/Write/Edit/Bash)
 Simple session save/load
 Rate limit header parsing
Phase 2: Memory System (Week 3-4)
 Lettuce integration
 Raw message block
 Observation block
 Observer agent (30k threshold)
 Token counting
Phase 3: Rate-Limit-Native (Week 5-6)
 Multi-provider routing
 Sleep state implementation
 Background compaction during sleep
 Reflector agent (40k threshold)
Phase 4: Compression Layer (Week 7-8)
 LZ4 integration
 Cold tier storage
 Priority-based compression triggers
 Retrieval on demand
Phase 5: Polish (Week 9-10)
 Session tree branching
 Commands (/model, /tree, /status, etc.)
 Error handling, retry logic
 Documentation
10. Success Criteria
Metric	Target
Cold start to first response	< 2 seconds
Memory operation latency	< 1ms (p99)
Context window stability	Never exceed 50k tokens
Daily token usage (full workday)	< 800k (80% of 1M limit)
Session persistence	Zero data loss
Provider failover	< 500ms
11. Research Delegation for Qwen Code
When implementing, delegate research subagents for:

Salt syntax deep-dive — How to do HTTP clients, arena allocation
LZ4 in Salt — Binding or pure implementation
Token counting — tiktoken equivalent for Llama tokenizer
Lettuce internals — How to embed vs. connect
Pi source study — Exact tool JSON schemas, session format
Use @research subagent pattern:

@research: Find how pi formats tool calls in the session JSONL @research: What's the exact Cerebras API response header format
12. References

Pi announcement   https://lucumr.pocoo.org/2026/1/31/pi/
Pi coding agent source    https://github.com/badlogic/pi-mono/tree/main/packages/coding-agent
Observational memory (Mastra)    https://mastra.ai/blog/observational-memory
Pi observational memory extension    https://github.com/GitHubFoxy/pi-observational-memory	
Lattice/Salt language     https://github.com/bneb/lattice
Lettuce Redis clone (567 LOC)    https://github.com/bneb/lattice	
Cerebras API docs    https://docs.cerebras.net/
Groq API docs     https://console.groq.com/docs