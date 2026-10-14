"""Failure-mode checks for snapshot integrity, selection and polygon validity."""
import json
from pathlib import Path
import shutil
import tempfile
from types import SimpleNamespace
import unittest

from shapely.geometry import Polygon
from process_public_spaces import DEFAULT_SOURCE, select_feature, validate_geometry, verify_source


class PublicSpacesTests(unittest.TestCase):
    def test_selection_rejects_missing_and_duplicate_names(self):
        plaza = SimpleNamespace(record={'nombre_esp': 'INDEPENDENCIA', 'tipo_espac': 'PLAZA'})
        unrelated = SimpleNamespace(record={'nombre_esp': 'OTHER', 'tipo_espac': 'PLAZA'})
        self.assertIs(select_feature([unrelated, plaza]), plaza)
        for records in ([], [unrelated], [plaza, plaza]):
            with self.assertRaisesRegex(ValueError, 'exactly one'):
                select_feature(records)

    def test_selection_rejects_wrong_type_and_substring(self):
        for name, kind in [('INDEPENDENCIA', 'EXPLANADA'), ('INDEPENDENCIA NORTE', 'PLAZA')]:
            with self.assertRaises(ValueError):
                select_feature([SimpleNamespace(record={'nombre_esp': name, 'tipo_espac': kind})])

    def test_empty_and_self_intersecting_polygons_rejected(self):
        for geometry in (Polygon(), Polygon([(0, 0), (2, 2), (0, 2), (2, 0)])):
            with self.assertRaises(ValueError):
                validate_geometry(geometry)

    def test_centroid_in_hole_is_rejected_not_replaced(self):
        polygon = Polygon([(0, 0), (4, 0), (4, 4), (0, 4)],
                          holes=[[(1, 1), (3, 1), (3, 3), (1, 3)]])
        with self.assertRaisesRegex(ValueError, 'outside'):
            validate_geometry(polygon)

    def test_modified_original_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            snapshot = Path(directory) / 'snapshot'
            shutil.copytree(DEFAULT_SOURCE.parent, snapshot)
            with (snapshot / 'public_spaces.zip').open('ab') as stream:
                stream.write(b'corruption')
            with self.assertRaisesRegex(ValueError, 'hash/size mismatch'):
                verify_source(snapshot / 'source.json')

    def test_unidentified_crs_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / 'source.json'
            source = json.loads(DEFAULT_SOURCE.read_text(encoding='utf-8'))
            source['declared_crs'] = None
            manifest.write_text(json.dumps(source), encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'license or CRS'):
                verify_source(manifest)


if __name__ == '__main__':
    unittest.main()
