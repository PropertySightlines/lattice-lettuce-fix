---
description: Build and test the Salt LSP language server
---

# Salt LSP Workflow

## Build the LSP server

// turbo
1. Build the LSP binary:
```bash
cd tools/salt-lsp && cargo build
```

// turbo
2. Run LSP unit tests:
```bash
cd tools/salt-lsp && cargo test
```

## Install VS Code extension (development)

3. Install npm dependencies:
```bash
cd tools/salt-lsp/editors/vscode && npm install
```

4. Compile the extension:
```bash
cd tools/salt-lsp/editors/vscode && npm run compile
```

5. Launch VS Code with the extension:
```bash
code --extensionDevelopmentPath=tools/salt-lsp/editors/vscode .
```

## Notes
- The LSP binary path is auto-detected from `target/debug/salt-lsp`
- Set `SALT_LSP_PATH` environment variable to override the binary location
- TextMate grammar provides syntax highlighting even without the LSP running
