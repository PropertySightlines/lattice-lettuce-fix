//! Salt LSP Backend — LanguageServer trait implementation
//!
//! Handles document sync, diagnostics, and completion requests.

use tower_lsp::jsonrpc::Result;
use tower_lsp::lsp_types::*;
use tower_lsp::{Client, LanguageServer};
use std::sync::Arc;
use tokio::sync::RwLock;
use std::collections::HashMap;

use crate::completion;
use crate::diagnostics;

/// In-memory document store for open files.
pub struct DocumentState {
    /// URI → full text content
    pub documents: HashMap<Url, String>,
}

pub struct SaltBackend {
    client: Client,
    state: Arc<RwLock<DocumentState>>,
}

impl SaltBackend {
    pub fn new(client: Client) -> Self {
        Self {
            client,
            state: Arc::new(RwLock::new(DocumentState {
                documents: HashMap::new(),
            })),
        }
    }

    /// Publish diagnostics for a document after it changes.
    async fn publish_diagnostics(&self, uri: Url, text: &str) {
        let diags = diagnostics::diagnose(text);
        self.client
            .publish_diagnostics(uri, diags, None)
            .await;
    }
}

#[tower_lsp::async_trait]
impl LanguageServer for SaltBackend {
    async fn initialize(&self, _: InitializeParams) -> Result<InitializeResult> {
        Ok(InitializeResult {
            capabilities: ServerCapabilities {
                text_document_sync: Some(TextDocumentSyncCapability::Kind(
                    TextDocumentSyncKind::FULL,
                )),
                completion_provider: Some(CompletionOptions {
                    resolve_provider: Some(false),
                    trigger_characters: Some(vec![
                        ".".to_string(),
                        ":".to_string(),
                    ]),
                    ..Default::default()
                }),
                hover_provider: Some(HoverProviderCapability::Simple(true)),
                ..Default::default()
            },
            server_info: Some(ServerInfo {
                name: "salt-lsp".to_string(),
                version: Some("0.1.0".to_string()),
            }),
        })
    }

    async fn initialized(&self, _: InitializedParams) {
        self.client
            .log_message(MessageType::INFO, "Salt LSP server initialized")
            .await;
    }

    async fn shutdown(&self) -> Result<()> {
        Ok(())
    }

    async fn did_open(&self, params: DidOpenTextDocumentParams) {
        let uri = params.text_document.uri.clone();
        let text = params.text_document.text.clone();

        {
            let mut state = self.state.write().await;
            state.documents.insert(uri.clone(), text.clone());
        }

        self.publish_diagnostics(uri, &text).await;
    }

    async fn did_change(&self, params: DidChangeTextDocumentParams) {
        let uri = params.text_document.uri.clone();

        // We use FULL sync, so the first change contains the entire text
        if let Some(change) = params.content_changes.into_iter().next() {
            let text = change.text;
            {
                let mut state = self.state.write().await;
                state.documents.insert(uri.clone(), text.clone());
            }
            self.publish_diagnostics(uri, &text).await;
        }
    }

    async fn did_close(&self, params: DidCloseTextDocumentParams) {
        let mut state = self.state.write().await;
        state.documents.remove(&params.text_document.uri);
    }

    async fn completion(&self, params: CompletionParams) -> Result<Option<CompletionResponse>> {
        let uri = &params.text_document_position.text_document.uri;
        let position = params.text_document_position.position;

        let state = self.state.read().await;
        let text = match state.documents.get(uri) {
            Some(t) => t.as_str(),
            None => return Ok(None),
        };

        let items = completion::complete(text, position);
        Ok(Some(CompletionResponse::Array(items)))
    }

    async fn hover(&self, params: HoverParams) -> Result<Option<Hover>> {
        let uri = &params.text_document_position_params.text_document.uri;
        let position = params.text_document_position_params.position;

        let state = self.state.read().await;
        let text = match state.documents.get(uri) {
            Some(t) => t.as_str(),
            None => return Ok(None),
        };

        // Extract the word under cursor for basic hover
        let word = extract_word_at(text, position);
        if let Some(info) = completion::keyword_info(&word) {
            return Ok(Some(Hover {
                contents: HoverContents::Markup(MarkupContent {
                    kind: MarkupKind::Markdown,
                    value: info.to_string(),
                }),
                range: None,
            }));
        }

        Ok(None)
    }
}

/// Extract the word at the given cursor position.
fn extract_word_at(text: &str, position: Position) -> String {
    let lines: Vec<&str> = text.lines().collect();
    let line_idx = position.line as usize;
    if line_idx >= lines.len() {
        return String::new();
    }
    let line = lines[line_idx];
    let col = position.character as usize;
    if col > line.len() {
        return String::new();
    }

    let bytes = line.as_bytes();
    let mut start = col;
    let mut end = col;

    // Scan backwards for word start
    while start > 0 && is_ident_char(bytes[start - 1]) {
        start -= 1;
    }
    // Scan forwards for word end
    while end < bytes.len() && is_ident_char(bytes[end]) {
        end += 1;
    }

    line[start..end].to_string()
}

fn is_ident_char(b: u8) -> bool {
    b.is_ascii_alphanumeric() || b == b'_'
}
