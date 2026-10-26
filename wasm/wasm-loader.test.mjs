import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {gzipSync} from 'node:zlib';

const source = fs.readFileSync(new URL('./wasm-loader.js', import.meta.url), 'utf8');
const binary = Buffer.from('0061736d010000000105016000017f03020100070a0106616e7377657200000a06010400412a0b', 'hex');

function loader(fetch) {
    return vm.runInNewContext(source + '\nloadGzipWasm;', {
        fetch, Response, DecompressionStream, WebAssembly
    });
}

test('streams gzip from an ordinary static response into a working WASM instance', async () => {
    const requests = [];
    const load = loader(async url => {
        requests.push(url);
        return new Response(gzipSync(binary), {headers: {'Content-Type': 'application/gzip'}});
    });
    const result = await load('kyschess.wasm.gz', {});
    assert.equal(result.instance.exports.answer(), 42);
    assert.ok(result.module instanceof WebAssembly.Module);
    assert.deepEqual(requests, ['kyschess.wasm.gz']);
});

test('reports an HTTP failure without attempting to instantiate the error document', async () => {
    const load = loader(async () => new Response('Not found', {status: 404}));
    await assert.rejects(load('kyschess.wasm.gz', {}), /HTTP 404 for kyschess.wasm.gz/);
});

test('rejects damaged gzip and invalid decompressed WASM', async () => {
    for (const bytes of [Buffer.from('not gzip'), gzipSync(Buffer.from('not wasm'))]) {
        const load = loader(async () => new Response(bytes));
        await assert.rejects(load('kyschess.wasm.gz', {}));
    }
});

test('propagates a network failure', async () => {
    const load = loader(async () => { throw new Error('connection failed'); });
    await assert.rejects(load('kyschess.wasm.gz', {}), /connection failed/);
});

function lazyAssets(paths, fetchAsset) {
    const nodes = new Map(paths.map(path => [path, {_lazyUrl: 'kys' + path}]));
    const FS = {lookupPath(path) {
        const node = nodes.get(path);
        if (!node) throw new Error('ENOENT');
        return {node};
    }};
    const create = vm.runInNewContext(source + '\ncreateLazyAssetLoader;', {fetch: fetchAsset, Uint8Array});
    return {assets: create(FS), nodes};
}

test('portraits stay pending without blocking and share a request with a file open', async () => {
    const finishes = [];
    const requests = [];
    const {assets, nodes} = lazyAssets(['/game/resource/head/index.ka', '/game/resource/head/7.webp'], url => {
        requests.push(url);
        return new Promise(resolve => { finishes.push(resolve); });
    });
    const path = '/game/resource/head';
    assert.equal(assets.prepareTexture(path, 7), 0);
    const node = nodes.get(path + '/7.webp');
    const pending = assets.load(node);
    assert.equal(assets.load(node), pending);
    assert.equal(assets.prepareTexture(path, 7), 0);
    assert.equal(requests.length, 2);
    assert.equal(node._loaded, undefined);
    finishes[1](new Response(new Uint8Array([1, 2, 3])));
    await pending;
    assert.deepEqual(node.contents, new Uint8Array([1, 2, 3]));
    assert.equal(node.usedBytes, 3);
    assert.equal(node._loaded, true);
    // The index is still pending, so group initialization must still wait.
    assert.equal(assets.prepareTexture(path, 7), 0);
    finishes[0](new Response('index'));
    await assets.load(nodes.get(path + '/index.ka'));
    assert.equal(assets.prepareTexture(path, 7), 1);
});

test('ready portraits and their indices are reused without further fetches', async () => {
    let requests = 0;
    const {assets, nodes} = lazyAssets(['/head/index.txt', '/head/2.png'], async () => {
        requests++;
        return new Response('image');
    });
    assert.equal(assets.prepareTexture('/head', 2), 0);
    await Promise.all([...nodes.values()].map(node => assets.load(node)));
    assert.equal(assets.prepareTexture('/head', 2), 1);
    assert.equal(assets.prepareTexture('/head', 2), 1);
    assert.equal(requests, 2);
});

test('packed groups share their archive and atlas groups prepare their metadata and disk fallback', async () => {
    for (const files of [['/icons.zip'], ['/icons.atlas', '/icons.atlas.json', '/icons/1.webp']]) {
        const requests = [];
        const {assets, nodes} = lazyAssets(files, async url => {
            requests.push(url);
            return new Response('asset');
        });
        assert.equal(assets.prepareTexture('/icons', 1), 0);
        await Promise.all([...nodes.values()].map(node => assets.load(node)));
        assert.equal(assets.prepareTexture('/icons', 1), 1);
        assert.equal(assets.prepareTexture('/icons', 2), 1);
        assert.deepEqual(requests, files.map(path => 'kys' + path));
    }
});

test('failed background images report failure without decoding empty data or refetching every frame', async () => {
    for (const result of [() => new Response('missing', {status: 404}), () => { throw new Error('offline'); }]) {
        let requests = 0;
        const {assets, nodes} = lazyAssets(['/head/1.webp'], async () => {
            requests++;
            return result();
        });
        assert.equal(assets.prepareTexture('/head', 1), 0);
        const node = nodes.get('/head/1.webp');
        await assert.rejects(assets.load(node), /HTTP 404|offline/);
        assert.equal(assets.prepareTexture('/head', 1), -1);
        assert.equal(node._loaded, undefined);
        assert.equal(node.contents, undefined);
        assert.equal(requests, 1);
    }
});

test('absent optional images do not fetch nonexistent files', () => {
    const {assets} = lazyAssets([], () => assert.fail('unexpected fetch'));
    assert.equal(assets.prepareTexture('/optional', 10), 1);
});
