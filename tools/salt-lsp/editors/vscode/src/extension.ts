// Salt Language — VS Code Extension
//
// Provides syntax highlighting via TextMate grammar (always active).
// Optionally launches the salt-lsp language server for completions/diagnostics.

import * as path from 'path';
import * as fs from 'fs';
import { workspace, ExtensionContext, window } from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
} from 'vscode-languageclient/node';

let client: LanguageClient | undefined;

export function activate(_context: ExtensionContext) {
    // Syntax highlighting is provided by the TextMate grammar in package.json
    // — it works automatically without any code here.

    // Optionally start the LSP server for completions and diagnostics.
    const serverPath = process.env.SALT_LSP_PATH
        || path.join(_context.extensionPath, '..', '..', '..', '..', 'target', 'debug', 'salt-lsp');

    if (!fs.existsSync(serverPath)) {
        console.log(`[Salt] LSP binary not found at ${serverPath} — syntax highlighting still active.`);
        return;
    }

    try {
        const serverOptions: ServerOptions = {
            run: { command: serverPath },
            debug: { command: serverPath },
        };

        const clientOptions: LanguageClientOptions = {
            documentSelector: [{ scheme: 'file', language: 'salt' }],
            synchronize: {
                fileEvents: workspace.createFileSystemWatcher('**/*.salt'),
            },
        };

        client = new LanguageClient(
            'salt-lsp',
            'Salt Language Server',
            serverOptions,
            clientOptions
        );

        client.start().catch((err: Error) => {
            console.log(`[Salt] LSP failed to start: ${err.message} — syntax highlighting still active.`);
            client = undefined;
        });
    } catch (err) {
        console.log(`[Salt] LSP initialization error — syntax highlighting still active.`);
        client = undefined;
    }
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
