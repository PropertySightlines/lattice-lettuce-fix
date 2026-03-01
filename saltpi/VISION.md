# SaltPi Vision

## One-Liner
A lightspeed coding agent written in Salt, designed around Cerebras/Groq free tier constraints, with native in-memory observational compaction.

## The Bet
Cerebras delivers 3000 tok/sec. Groq delivers similar speeds. The bottleneck isn't inference — it's rate limits and context management. What if we designed an agent that treats rate limits as *architecture*, not obstacles?

## Core Insight
| Traditional Agent | SaltPi |
|-------------------|--------|
| Waits during rate limit | Compacts memory |
| Single context window | Tiered: Hot (raw) → Warm (observations) → Cold (compressed binary) |
| LLM-only compaction | LLM semantic + Salt binary compression |
| TypeScript/Python runtime | Salt (arena allocation, 234K ops/sec Lettuce backend) |

## Design Principles

### 1. Minimal Core (Pi Philosophy)
- 4 tools: Read, Write, Edit, Bash
- Shortest possible system prompt
- Agent extends itself via code

### 2. Rate-Limit-Native
- Track remaining quota via response headers
- Enter "sleep state" when approaching limits
- Use sleep windows for background compaction/compression
- Burst into parallel subagents when quota permits

### 3. Observational Memory (Mastra/Letta-Inspired)
- Two-block context: Observations + Raw messages
- Observer threshold: 30k tokens → compress
- Reflector threshold: 40k tokens → garbage collect
- Emoji priorities: 🔴 critical / 🟡 important / 🟢 routine

### 4. Salt-Native Compression Layer
- Lettuce backend (567 LOC, 234K ops/sec)
- LZ4/zstd compression on 🟢-tier observations
- Arena allocation for predictable latency
- Zero-copy IPC between agent components

### 5. Multi-Provider Orchestration
Primary: Cerebras gpt-oss-120b (65k context, fast) Fallback: Groq llama-3.3-70b (12k TPM, reliable) Burst: Cerebras llama3.1-8b (2200 tok/sec, high quota) Observer: Groq llama-3.1-8b-instant (14.4K RPD, cheap)

## What Success Looks Like - Full coding session on free tier without hitting walls - Context window never exhausted (compaction keeps it bounded) - Sub-100ms memory operations (Lettuce speed) - Agent that improves itself session-over-session ## Non-Goals (v1) - GUI/web interface (terminal only) - Paid tier optimization (free tier first) - Multi-user (single developer workflow) - Plugin marketplace (self-extension only)