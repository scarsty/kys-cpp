async function loadGzipWasm(url, imports) {
    const response = await fetch(url);
    if (!response.ok) {
        throw new Error('HTTP ' + response.status + ' for ' + url);
    }
    const stream = response.body.pipeThrough(new DecompressionStream('gzip'));
    return WebAssembly.instantiateStreaming(new Response(stream, {
        headers: { 'Content-Type': 'application/wasm' }
    }), imports);
}
