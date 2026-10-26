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

// Share background UI requests with synchronous file opens. A pending request
// never marks its empty manifest entry as usable file data.
function createLazyAssetLoader(FS, fetchAsset = fetch) {
    const textures = new Map();
    function find(path) {
        try { return FS.lookupPath(path, {follow: true}).node; }
        catch (_) { return null; }
    }

    function load(node) {
        if (node._loaded) return Promise.resolve();
        if (!node._loadPromise) {
            node._loadPromise = fetchAsset(node._lazyUrl, {cache: 'no-cache'})
                .then(response => {
                    if (!response.ok) throw new Error('HTTP ' + response.status + ' for ' + node._lazyUrl);
                    return response.arrayBuffer();
                }).then(buffer => {
                    node.contents = new Uint8Array(buffer);
                    node.usedBytes = buffer.byteLength;
                    node._loaded = true;
                }).catch(error => {
                    node._loadError = error;
                    throw error;
                });
        }
        return node._loadPromise;
    }

    function prepareTexture(path, id) {
        const key = path + '/' + id;
        let nodes = textures.get(key);
        if (!nodes) {
            const zip = find(path + '.zip');
            const atlas = find(path + '.atlas');
            nodes = [];
            const add = filename => {
                const node = find(filename);
                if (node && node._lazyUrl) nodes.push(node);
            };
            if (zip) {
                add(path + '.zip');
            } else {
                if (atlas) {
                    add(path + '.atlas');
                    add(path + '.atlas.json');
                }
                add(path + '/index.txt');
                add(path + '/index.ka');
                for (const suffix of ['', ...Array.from({length: 10}, (_, i) => '_' + i)]) {
                    add(path + '/' + id + suffix + '.webp');
                    add(path + '/' + id + suffix + '.png');
                }
            }
            textures.set(key, nodes);
        }
        let state = 1;
        for (const node of nodes) {
            if (node._loadError) return -1;
            if (!node._loaded) {
                state = 0;
                // Keep errors visible to the UI and to a later FS.open, without
                // producing an unhandled rejection for a background request.
                load(node).catch(() => {});
            }
        }
        return state;
    }

    return {find, load, prepareTexture};
}
