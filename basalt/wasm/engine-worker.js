// =============================================================================
// Basalt Engine Worker — Brutalist 6-Export WASM Bridge
// =============================================================================
//
// Web Worker that wraps the 6 WASM exports into an async message API.
// JS owns: BPE tokenization, string decoding, UI rendering.
// WASM owns: math (forward passes, sampling, RoPE).
//
// Messages IN:
//   { type: 'LOAD_MODEL', modelUrl, tokenizerUrl }
//   { type: 'RUN_PROMPT', prompt, maxNewTokens }
//   { type: 'STOP' }
//
// Messages OUT:
//   { type: 'STATUS', message }
//   { type: 'READY', config: { dim, hidden_dim, ... } }
//   { type: 'TOKEN', tokenId, text }
//   { type: 'DONE', totalTokens, elapsedMs }
//   { type: 'ERROR', message }
//
// =============================================================================

let wasm = null;
let vocab = null;        // Map<string, number> — BPE encode
let vocabDecode = null;  // Map<number, string> — BPE decode
let running = false;

// ── BPE Tokenizer (JS-side, O(1) hashmap lookups) ───────────────────────────

async function loadTokenizer(url) {
    const response = await fetch(url);
    const buffer = await response.arrayBuffer();
    const view = new DataView(buffer);
    let offset = 0;

    const maxTokenLen = view.getInt32(offset, true); offset += 4;
    const vocabSize = view.getInt32(offset, true); offset += 4;

    vocab = new Map();
    vocabDecode = new Map();

    for (let i = 0; i < vocabSize; i++) {
        const score = view.getFloat32(offset, true); offset += 4;
        const len = view.getInt32(offset, true); offset += 4;
        const bytes = new Uint8Array(buffer, offset, len); offset += len;
        const text = new TextDecoder().decode(bytes);
        vocab.set(text, i);
        vocabDecode.set(i, text);
    }

    return { vocabSize, maxTokenLen };
}

function encodePrompt(text) {
    // Simple BPE: character-level fallback + greedy merge
    const tokens = [];

    // Start with character-level tokenization
    const chars = [...text];
    const pieces = chars.map(c => {
        const id = vocab.get(c);
        return id !== undefined ? id : 0; // UNK
    });

    // BOS token
    tokens.push(1);

    // Greedy merge pass
    let i = 0;
    while (i < chars.length) {
        let bestLen = 1;
        let bestId = pieces[i];

        // Try progressively longer substrings
        for (let len = 2; len <= Math.min(20, chars.length - i); len++) {
            const substr = chars.slice(i, i + len).join('');
            const id = vocab.get(substr);
            if (id !== undefined) {
                bestLen = len;
                bestId = id;
            }
        }

        tokens.push(bestId);
        i += bestLen;
    }

    return tokens;
}

function decodeToken(id) {
    return vocabDecode?.get(id) ?? `<${id}>`;
}

// ── WASM Lifecycle ──────────────────────────────────────────────────────────

async function loadModel(modelUrl, tokenizerUrl) {
    postMessage({ type: 'STATUS', message: 'Loading model...' });

    // 1. Fetch model binary
    const modelResponse = await fetch(modelUrl);
    const modelBytes = new Uint8Array(await modelResponse.arrayBuffer());

    // 2. Load tokenizer (optional)
    if (tokenizerUrl) {
        postMessage({ type: 'STATUS', message: 'Loading tokenizer...' });
        await loadTokenizer(tokenizerUrl);
    }

    // 3. Instantiate WASM
    postMessage({ type: 'STATUS', message: 'Initializing WASM...' });
    const importObject = {
        env: {
            log_status: (ptr, len) => {
                const bytes = new Uint8Array(wasm.exports.memory.buffer, ptr, len);
                const text = new TextDecoder().decode(bytes);
                console.log('[basalt]', text);
            }
        }
    };

    const wasmResponse = await fetch('/basalt.wasm');
    const wasmModule = await WebAssembly.instantiateStreaming(wasmResponse, importObject);
    wasm = wasmModule.instance;

    // 4. Copy model into WASM memory
    const modelPtr = wasm.exports.basalt_alloc(modelBytes.byteLength);
    new Uint8Array(wasm.exports.memory.buffer, modelPtr, modelBytes.byteLength)
        .set(modelBytes);

    // 5. Initialize engine
    const status = wasm.exports.basalt_init(modelPtr, modelBytes.byteLength);
    if (status !== 0) {
        throw new Error('basalt_init failed (bad model file?)');
    }

    // 6. Read config via unified getter
    const config = {
        dim: Number(wasm.exports.basalt_get_config(0n)),
        hidden_dim: Number(wasm.exports.basalt_get_config(1n)),
        n_layers: Number(wasm.exports.basalt_get_config(2n)),
        n_heads: Number(wasm.exports.basalt_get_config(3n)),
        n_kv_heads: Number(wasm.exports.basalt_get_config(4n)),
        vocab_size: Number(wasm.exports.basalt_get_config(5n)),
        seq_len: Number(wasm.exports.basalt_get_config(6n)),
    };

    postMessage({ type: 'READY', config });
}

async function runPrompt(prompt, maxNewTokens = 128) {
    if (!wasm) throw new Error('Model not loaded');
    running = true;

    // 1. Tokenize (JS-side, O(1) hashmap)
    const tokens = encodePrompt(prompt);
    postMessage({ type: 'STATUS', message: `Prefilling ${tokens.length} tokens...` });

    // 2. Write tokens into WASM memory (bulk)
    const tokensPtr = wasm.exports.basalt_alloc(tokens.length * 8);
    const tokensView = new BigInt64Array(
        wasm.exports.memory.buffer, tokensPtr, tokens.length
    );
    for (let i = 0; i < tokens.length; i++) {
        tokensView[i] = BigInt(tokens[i]);
    }

    // 3. Bulk ingest — 1 boundary crossing for entire prompt
    wasm.exports.basalt_ingest_prompt(tokensPtr, BigInt(tokens.length));

    // 4. Generate loop
    postMessage({ type: 'STATUS', message: 'Generating...' });
    const startMs = performance.now();
    let totalTokens = 0;

    for (let step = 0; step < maxNewTokens; step++) {
        if (!running) break;

        const tokenId = Number(wasm.exports.basalt_generate_next());
        if (tokenId < 0) break; // EOS or seq_len hit

        totalTokens++;
        const text = decodeToken(tokenId);
        postMessage({ type: 'TOKEN', tokenId, text });
    }

    const elapsedMs = performance.now() - startMs;
    postMessage({ type: 'DONE', totalTokens, elapsedMs });
    running = false;
}

// ── Message Handler ─────────────────────────────────────────────────────────

self.onmessage = async (e) => {
    try {
        switch (e.data.type) {
            case 'LOAD_MODEL':
                await loadModel(e.data.modelUrl, e.data.tokenizerUrl);
                break;
            case 'RUN_PROMPT':
                await runPrompt(e.data.prompt, e.data.maxNewTokens);
                break;
            case 'STOP':
                running = false;
                break;
        }
    } catch (err) {
        postMessage({ type: 'ERROR', message: err.message });
    }
};
