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
