"""M3 regression checks: deterministic bounded geometry and immutable source/assets."""
import json
from pathlib import Path
import unittest
import math

from process_m3 import ROOT, SOURCE, OUTPUT, build, verify_snapshot
from process_public_spaces import digest


def hashes():
    return {p.relative_to(ROOT).as_posix(): digest(p.read_bytes())
            for directory in ('Data/raw', 'Content') for p in sorted((ROOT/directory).rglob('*')) if p.is_file()}


class M3Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.before = hashes()
        cls.first = build()
        cls.manifest = json.loads(cls.first['manifest.json'])

    def test_twice_byte_identical_and_matches_files(self):
        self.assertEqual(self.first, build())
        for name, data in self.first.items():
            self.assertEqual(data, (OUTPUT/name).read_bytes(), name)

    def test_raw_and_assets_unchanged(self):
        self.assertEqual(self.before, hashes())
        verify_snapshot(SOURCE)

    def test_origin_single_source_and_unknown_height(self):
        self.assertNotIn('latitude', self.manifest)
        origin = ROOT/self.manifest['origin_source']
        self.assertEqual(digest(origin.read_bytes()), self.manifest['origin_sha256'])
        self.assertIsNone(json.loads(origin.read_bytes())['altitude'])
        self.assertIsNone(self.manifest['altitude'])

    def test_plaza_area_winding_and_bounds(self):
        plaza = json.loads(self.first['plaza_independencia.json'])['features']
        self.assertEqual(len(plaza), 1)
        feature = plaza[0]
        self.assertGreaterEqual(len(feature['vertices']), 3)
        self.assertAlmostEqual(feature['area_utm_m2'], 15842.005955819597, places=6)
        x0,y0,x1,y1 = feature['expected_local_bbox_cm']
        self.assertTrue(10000 < x1-x0 < 11000)
        self.assertTrue(17500 < y1-y0 < 18500)
        for i in range(0, len(feature['triangles']), 3):
            a,b,c = [feature['vertices'][j] for j in feature['triangles'][i:i+3]]
            self.assertLess((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]), 0)

    def test_authorized_extent_counts_and_no_invented_altitudes(self):
        bounds = self.manifest['area_bbox_m']
        self.assertAlmostEqual(bounds[2]-bounds[0], 700)
        self.assertAlmostEqual(bounds[3]-bounds[1], 700)
        self.assertLess(math.hypot(350,350), self.manifest['max_radius_m'])
        counts = {'polygons':0,'lines':0,'vertices':0,'triangles':0}
        for name in ('plaza_independencia.json','roads.json','blocks.json'):
            for feature in json.loads(self.first[name])['features']:
                counts['polygons' if feature['kind']=='polygon' else 'lines'] += 1
                counts['vertices'] += len(feature['vertices'])
                counts['triangles'] += len(feature['triangles'])//3
                self.assertIsNone(feature['altitude'])
                self.assertIsNone(feature['altitude_reference'])
                for lon,lat in feature['vertices']:
                    self.assertTrue(-180 <= lon <= 180 and -90 <= lat <= 90)
        self.assertEqual(counts, self.manifest['counts'])


if __name__ == '__main__':
    unittest.main()
