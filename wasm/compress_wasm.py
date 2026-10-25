"""為瀏覽器串流解壓縮產生可重現的 gzip WASM。"""
import argparse
import gzip
from pathlib import Path


def compress_wasm(source: Path) -> Path:
    output = source.with_suffix(source.suffix + '.gz')
    binary = source.read_bytes()
    output.write_bytes(gzip.compress(binary, compresslevel=9, mtime=0))
    print(f'WASM gzip: {len(binary):,} -> {output.stat().st_size:,} bytes ({output})')
    return output


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    compress_wasm(parser.parse_args().source)
