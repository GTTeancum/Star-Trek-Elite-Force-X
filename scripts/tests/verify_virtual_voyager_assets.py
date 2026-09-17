"""Check nested map selection and safe, ordered retail source extraction."""
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_xbox_patch_pk3 as builder
import extract_virtual_voyager_sources as sources


class VirtualVoyagerAssets(unittest.TestCase):
    def test_nested_campaign_maps(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = ("borg1", "tour/deck01", "tour/deck02", "_holodeck_proton", "hm_borg1")
            for name in names:
                path = root / "maps" / (name + ".bsp")
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"fixture")
            selected = builder.selected_bsp_paths(root, "campaign", "borg1")
            self.assertEqual({p.relative_to(root / "maps").with_suffix("").as_posix()
                              for p in selected}, set(names) - {"hm_borg1"})
            self.assertEqual(builder.selected_bsp_paths(root, "map", "tour/deck02"),
                             [root / "maps/tour/deck02.bsp"])

    def test_source_precedence_and_config_preservation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "stage"
            target.mkdir()
            (target / "default.cfg").write_bytes(b"controller config")
            for number in range(4):
                with zipfile.ZipFile(root / f"PAK{number}.PK3", "w") as pak:
                    pak.writestr("textures/shared.tga", str(number))
                    pak.writestr("default.cfg", b"must not extract")
                    pak.writestr("vm/cgame.qvm", b"must not extract")
                    pak.writestr("real_scripts/tour.ibi", b"must stay authoritative in PAK")
                    if number == 3:
                        pak.writestr("maps/tour/deck02.bsp", b"source BSP")
                        pak.writestr("maps/tour/deck02.nav", b"source NAV")
                        pak.writestr("maps/hm_voy3.bsp", b"unrelated")
            report = sources.extract(target, root)
            self.assertEqual(list(report["maps"]), ["tour/deck02"])
            self.assertEqual((target / "textures/shared.tga").read_bytes(), b"3")
            self.assertEqual((target / "maps/tour/deck02.bsp").read_bytes(), b"source BSP")
            self.assertEqual((target / "default.cfg").read_bytes(), b"controller config")
            for name in ("vm/cgame.qvm", "real_scripts/tour.ibi", "maps/hm_voy3.bsp"):
                self.assertFalse((target / name).exists())

    def test_rejects_archive_traversal(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with zipfile.ZipFile(root / "PAK0.PK3", "w") as pak:
                pak.writestr("../outside.tga", b"bad")
            with self.assertRaises(ValueError):
                sources.extract(root / "stage", root)
            self.assertFalse((root / "outside.tga").exists())


if __name__ == "__main__":
    unittest.main()
