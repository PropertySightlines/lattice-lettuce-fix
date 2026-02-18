// Minimal Node.js HTTP server — comparison baseline for Salt benchmark
// Run: node benchmarks/node_http_server.js

const http = require('http');

const server = http.createServer((req, res) => {
    const url = req.url;

    if (url === '/health') {
        res.writeHead(200, {
            'Content-Type': 'application/json',
            'Connection': 'keep-alive'
        });
        res.end('{"status":"ok"}');
        return;
    }

    if (url.startsWith('/echo')) {
        const params = new URL(url, 'http://localhost').searchParams;
        const msg = params.get('msg') || '';
        res.writeHead(200, {
            'Content-Type': 'application/json',
            'Connection': 'keep-alive'
        });
        res.end(msg);
        return;
    }

    res.writeHead(404, {
        'Content-Type': 'application/json',
        'Connection': 'keep-alive'
    });
    res.end('{"error":"not found"}');
});

server.listen(8080, () => {
    console.log('Node.js HTTP Server listening on port 8080');
});
