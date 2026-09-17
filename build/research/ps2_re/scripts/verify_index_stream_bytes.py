"""Compare streamed packed-index parser inputs with original whole-lump inputs."""
from pathlib import Path
import hashlib
import json
import struct
import zipfile

root = Path(__file__).resolve().parents[4]
rows = []
with zipfile.ZipFile(root / 'build/release/BaseEF/xbox0.pk3') as archive:
    maps = sorted(n.rsplit('/', 1)[0] for n in archive.namelist()
                  if n.startswith('maps/') and n.endswith('/misc.mle'))
    for name in maps:
        whole = archive.read(name + '/indexes.mle')
        old_hash, new_hash = hashlib.sha256(), hashlib.sha256()
        surfaces = resets = total = 0
        with archive.open(name + '/indexes.mle') as stream:
            for kind, width in [('trisurfs', 19), ('faces', 29)]:
                descriptors = archive.read(name + '/' + kind + '.mle')
                assert len(descriptors) % width == 0
                for at in range(0, len(descriptors), width):
                    packed = struct.unpack_from('<I', descriptors, at + 11)[0]
                    offset, count = packed >> 12, packed & 4095
                    assert (offset + count) * 2 <= len(whole)
                    assert count * 2 <= 8192
                    resets += offset * 2 < stream.tell()
                    stream.seek(offset * 2)
                    scratch = stream.read(count * 2)
                    original = whole[offset * 2:(offset + count) * 2]
                    assert scratch == original, (name, kind, at)
                    # Production rebases the descriptor offset to zero; parser
                    # must see the same signed shorts at every resulting index.
                    assert struct.unpack('<' + 'h' * count, scratch) == struct.unpack(
                        '<' + 'h' * count, original)
                    old_hash.update(original)
                    new_hash.update(scratch)
                    total += len(scratch)
                    surfaces += 1
        assert old_hash.digest() == new_hash.digest()
        rows.append(dict(map=name, surfaces=surfaces, bytes_compared=total,
                         backward_seeks=resets, index_file_bytes=len(whole),
                         scratch_bytes=8192, parser_input_sha256=new_hash.hexdigest()))
out = root / 'build/research/ps2_re/redrocket_20260913/index_stream_equivalence.json'
out.write_text(json.dumps({'scope': 'Host ZIP input equivalence; does not validate Xbox FS or rendered output',
                           'maps': rows}, indent=2))
print('PASS:', len(rows), 'maps,', sum(r['surfaces'] for r in rows), 'surfaces,',
      sum(r['bytes_compared'] for r in rows), 'index bytes')
