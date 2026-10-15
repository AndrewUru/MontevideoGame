"""M3.2 source, geometry, provenance and reproducibility regressions; never rewrite raw/output."""
import json
import unittest
import hashlib
from shapely.geometry import Polygon, box
from process_buildings import SOURCE, OUTPUT, ROOT, build, stable_seed, height_policy
from process_m3 import verify_snapshot, read_shapes


def raw_hashes():
    return {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'Data/raw').rglob('*') if p.is_file()}


class BuildingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.before=raw_hashes()
        cls.output=build()
        cls.manifest=json.loads(cls.output['buildings_manifest.json'])
        cls.features=json.loads(cls.output['buildings.json'])['features']

    def test_01_raw_immutable_and_verified(self):
        verify_snapshot(SOURCE)
        self.assertEqual(self.before,raw_hashes())

    def test_02_crs(self):
        self.assertEqual(self.manifest['area_crs'],'EPSG:32721')
        self.assertEqual(self.manifest['crs'],'EPSG:4326')
        self.assertGreater(len(read_shapes(SOURCE.parent/'buildings.zip')),0) # Also validates actual PRJ.

    def test_03_unique_ids_and_real_identity(self):
        originals={str(r['gid']) for r,g in read_shapes(SOURCE.parent/'buildings.zip')}
        self.assertEqual(len(self.features),len({f['id'] for f in self.features}))
        self.assertEqual(len({f['building_id'] for f in self.features}),self.manifest['counts']['buildings'])
        for f in self.features:
            self.assertIn(f['source_feature_id'],originals)

    def test_04_valid_nonempty_footprints_and_holes(self):
        for f in self.features:
            v=f['vertices']; rings=f['rings']
            p=Polygon([v[i] for i in rings[0]],[[v[i] for i in r] for r in rings[1:]])
            self.assertTrue(p.is_valid and not p.is_empty and p.area>0)
            self.assertGreaterEqual(len(v),3)
            self.assertGreater(len(f['triangles']),0)

    def test_05_bounded_area(self):
        a=box(*self.manifest['area_bbox_m'])
        for f in self.features:
            self.assertTrue(a.covers(box(*f['bbox_utm_m'])))
            self.assertIsNone(f['altitude'])

    def test_06_byte_reproducible(self):
        self.assertEqual(self.output,build())
        for name,body in self.output.items():
            self.assertEqual(body,(OUTPUT/name).read_bytes())

    def test_07_seed_stable_across_parts(self):
        by_building={}
        for f in self.features:
            self.assertEqual(f['seed'],stable_seed(f['building_id']))
            self.assertEqual(f['seed'],by_building.setdefault(f['building_id'],f['seed']))
            self.assertEqual(f['seed'],int(hashlib.sha1(f['building_id'].encode()).hexdigest()[:8],16))

    def test_08_height_provenance_and_configurable_floor(self):
        self.assertEqual(json.loads(SOURCE.read_bytes())['attributes'],['estado','gid','nombre'])
        for f in self.features:
            self.assertEqual(f['height_source'],'synthetic_blockout')
            self.assertEqual(f['floors_source'],'synthetic_blockout')
            self.assertIn(f['floors'],[3,4,5])
            self.assertEqual(f['height_m'],f['floors']*3.0)
            self.assertEqual(height_policy(f['building_id'],3.5)['height_m'],f['floors']*3.5)
        with self.assertRaises(ValueError): height_policy('fixture',float('nan'))

    def test_09_closed_solid_cap_topology(self):
        # Each roof boundary must be exactly its ring edges; extrusion can then close all walls.
        from collections import Counter
        for f in self.features:
            edges=Counter()
            for i in range(0,len(f['triangles']),3):
                t=f['triangles'][i:i+3]
                for a,b in zip(t,t[1:]+t[:1]): edges[tuple(sorted((a,b)))]+=1
            self.assertTrue(all(n in [1,2] for n in edges.values()))
            boundary={e for e,n in edges.items() if n==1}
            rings={tuple(sorted((a,b))) for r in f['rings'] for a,b in zip(r,r[1:]+r[:1])}
            self.assertEqual(boundary,rings)

    def test_10_reject_invalid_geometry_and_winding_independence(self):
        import pyproj
        from process_buildings import encode_polygon
        origin=json.loads((ROOT/'Data/processed/montevideo/plaza_independencia.json').read_bytes())
        c=origin['centroid_wgs84']; utm=origin['centroid_original']; e,n=utm['easting_m'],utm['northing_m']
        wgs=pyproj.Transformer.from_crs(32721,4326,always_xy=True)
        local=pyproj.Transformer.from_pipeline(f'+proj=pipeline +step +proj=cart +ellps=WGS84 +step +proj=topocentric +ellps=WGS84 +lat_0={c["latitude"]} +lon_0={c["longitude"]} +h_0=0')
        shell=[(e,n),(e+20,n),(e+20,n+10),(e+10,n+10),(e+10,n+20),(e,n+20)]
        hole=[(e+2,n+2),(e+2,n+4),(e+4,n+4),(e+4,n+2)]
        a=encode_polygon(Polygon(shell,[hole]),'fixture',wgs,local)
        b=encode_polygon(Polygon(shell[::-1],[hole[::-1]]),'fixture',wgs,local)
        self.assertEqual(a,b)
        self.assertEqual(len(a['rings']),2)
        for p in [Polygon(),Polygon([(e,n),(e+10,n+10),(e,n+10),(e+10,n)])]:
            with self.assertRaises(ValueError): encode_polygon(p,'invalid',wgs,local)


if __name__=='__main__':
    unittest.main()
