"""Verify generated SF3 audio, metadata, packaging sidecars and asset selection."""
import gzip
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from wasm.prepare_soundfont import SAMPLE_HEADER, chunk, convert_sf2, prepare_soundfont


def fixture():
    wave = (np.sin(np.arange(4096) * 2 * np.pi * 440 / 22050) * 12000).astype('<i2')
    pcm = wave.tobytes() + bytes(92)
    headers = []
    for i in range(2):
        start = i * (4096 + 46)
        headers.append(SAMPLE_HEADER.pack(f'sample{i}'.encode(), start, start + 4096,
                                          start + 128, start + 3968, 22050, 60, -2, 1 - i, 2 << i))
    headers.append(SAMPLE_HEADER.pack(b'EOS', 0, 0, 0, 0, 0, 0, 0, 0, 0))
    metadata = b'fixture metadata\0'
    data = b'sfbk' + chunk(b'LIST', b'INFO' + chunk(b'ifil', struct.pack('<HH', 2, 1))
                          + chunk(b'INAM', metadata))
    data += chunk(b'LIST', b'sdta' + chunk(b'smpl', pcm * 2))
    data += chunk(b'LIST', b'pdta' + chunk(b'phdr', metadata)
                  + chunk(b'shdr', b''.join(headers)))
    return chunk(b'RIFF', data), wave


class WasmSoundFontTests(unittest.TestCase):
    def test_samples_decode_and_preserve_loops_links_and_metadata(self):
        original, wave = fixture()
        result = convert_sf2(original)
        self.assertEqual(struct.unpack_from('<I', result, 4)[0] + 8, len(result))
        self.assertEqual(struct.unpack_from('<HH', result, result.index(b'ifil') + 8), (3, 0))
        self.assertIn(chunk(b'INAM', b'fixture metadata\0'), result)
        self.assertIn(chunk(b'phdr', b'fixture metadata\0'), result)
        header_at = result.index(b'shdr')
        sample_at = result.index(b'smpl') + 8
        for i in range(2):
            header = SAMPLE_HEADER.unpack_from(result, header_at + 8 + i * SAMPLE_HEADER.size)
            decoded, rate = sf.read(io.BytesIO(result[sample_at + header[1]:sample_at + header[2]]))
            self.assertEqual(rate, 22050)
            self.assertEqual(len(decoded), len(wave))
            self.assertGreater(np.corrcoef(wave, decoded)[0, 1], 0.99)
            self.assertEqual(header[3:5], (128, 3968))
            self.assertEqual(header[6:10], (60, -2, 1 - i, (2 << i) | 0x10))

    def test_preparation_reuses_current_output_and_refreshes_stale_sidecar(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'mid.sf2'
            output = Path(directory) / 'mid.sf3'
            source.write_bytes(fixture()[0])
            prepare_soundfont(source, output)
            generated_at = output.stat().st_mtime_ns
            prepare_soundfont(source, output)
            self.assertEqual(output.stat().st_mtime_ns, generated_at)
            compressed = output.with_suffix('.sf3.gz')
            compressed.write_bytes(b'stale')
            os.utime(compressed, ns=(0, 0))
            prepare_soundfont(source, output)
            self.assertEqual(gzip.decompress(compressed.read_bytes()), output.read_bytes())

    def test_manifest_selects_sf3_and_excludes_native_bank_and_http_sidecars(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            music = root / 'game/music'
            music.mkdir(parents=True)
            for name in ('mid.sf2', 'mid.sf3', 'mid.sf3.gz', 'mid.sf3.br', '16.mid'):
                (music / name).write_bytes(b'fixture')
            manifest = root / 'manifest.js'
            subprocess.run([sys.executable, str(ROOT / 'wasm/gen_manifest.py'),
                            str(music.parent), str(manifest)], check=True, capture_output=True)
            data = json.loads(manifest.read_text(encoding='utf-8').split('files: ', 1)[1].split('\n', 1)[0])
            self.assertEqual(data, ['/game/music/16.mid', '/game/music/mid.sf3'])


if __name__ == '__main__':
    unittest.main()
