"""Reject stale or truncated ISO contents even when release hashes are valid."""
import hashlib
import io
from pathlib import Path
import struct
import sys
import unittest
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify_beta_iso_components import verify, media_patch_offset


class ComponentProof(unittest.TestCase):
    def test_media_patch(self):
        self.assertEqual(media_patch_offset(b'a\x7db', b'a\xebb'), 1)
        for data in (b'a\xebc', b'a\x7cb', b'a\x7db', b'a\xeb'):
            with self.assertRaises(ValueError):
                media_patch_offset(b'a\x7db', data)

    def test_packaged_xbe_records_actual_hash(self):
        data, _ = self.fixture()
        entry = struct.pack('<HHIIBB', 0, 0, 35, 3, 0, 11) + b'default.xbe'
        struct.pack_into('<II', data, 32 * 2048 + 20, 33, len(entry))
        data[33 * 2048:33 * 2048 + len(entry)] = entry
        data[35 * 2048:35 * 2048 + 3] = b'a\xebb'
        source_hash = hashlib.sha256(b'a\x7db').hexdigest()
        manifest = {'default.xbe': {'bytes': 3, 'sha256': source_hash}}
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'default.xbe').write_bytes(b'a\x7db')
            proof = verify(io.BytesIO(data), manifest, root)['default.xbe']
            self.assertEqual(proof['sourceSha256'], source_hash)
            self.assertEqual(proof['sha256'], hashlib.sha256(b'a\xebb').hexdigest())
            self.assertEqual(proof['mediaEnablePatchOffset'], 1)
            (root / 'default.xbe').write_bytes(b'old')
            with self.assertRaisesRegex(ValueError, 'changed during packaging'):
                verify(io.BytesIO(data), manifest, root)

    def fixture(self):
        data = bytearray(36 * 2048)
        data[32 * 2048:32 * 2048 + 28] = b'MICROSOFT*XBOX*MEDIA' + struct.pack('<II', 33, 20)
        root = struct.pack('<HHIIBB', 0, 0, 34, 23, 0x10, 6) + b'BaseEF'
        data[33 * 2048:33 * 2048 + len(root)] = root
        child = struct.pack('<HHIIBB', 0, 0, 35, 4, 0, 9) + b'xbox1.pk3'
        data[34 * 2048:34 * 2048 + len(child)] = child
        data[35 * 2048:35 * 2048 + 4] = b'good'
        manifest = {'BaseEF/xbox1.pk3': {'bytes': 4, 'sha256': hashlib.sha256(b'good').hexdigest()}}
        return data, manifest

    def test_nested_payload(self):
        data, manifest = self.fixture()
        self.assertEqual(verify(io.BytesIO(data), manifest), manifest)

    def test_stale_iso(self):
        data, manifest = self.fixture()
        data[35 * 2048:35 * 2048 + 4] = b'old!'
        with self.assertRaisesRegex(ValueError, 'mismatch'):
            verify(io.BytesIO(data), manifest)

    def test_truncated(self):
        data, manifest = self.fixture()
        with self.assertRaisesRegex(ValueError, 'Truncated payload'):
            verify(io.BytesIO(data[:35 * 2048 + 2]), manifest)

    def test_missing(self):
        data, manifest = self.fixture()
        manifest['BaseEF/absent.pk3'] = manifest.pop('BaseEF/xbox1.pk3')
        with self.assertRaisesRegex(ValueError, 'Missing'):
            verify(io.BytesIO(data), manifest)


if __name__ == '__main__':
    unittest.main()
