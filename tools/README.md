# Tools

Developer tooling for the Salt ecosystem.

## Components

| Tool | Description | Status |
|------|-------------|--------|
| [`sp`](sp/) | **Salt Package Manager** — builds projects from `salt.toml`, content-addressed caching, dependency resolution | 🚧 Early |
| [`salt-lsp`](salt-lsp/) | **Language Server** — syntax highlighting, go-to-definition, hover, diagnostics for VS Code | ✅ Published |
| [`salt-build`](salt-build/) | **Build Scripts** — shell and Python helpers for the MLIR → LLVM compilation pipeline | ✅ Functional |

## Salt LSP

The Salt language server provides IDE support:

```bash
# Build the LSP server
cd salt-lsp && cargo build --release

# Install the VS Code extension
cd salt-lsp/editors/vscode && npm install && npm run compile
```

See [`salt-lsp/`](salt-lsp/) for details.

## Package Manager (`sp`)

`sp` reads a `salt.toml` manifest and compiles Salt projects:

```bash
cd tools/sp && cargo build --release

# In a Salt project directory:
sp build
sp build --release
```

See [`sp/`](sp/) for details.
