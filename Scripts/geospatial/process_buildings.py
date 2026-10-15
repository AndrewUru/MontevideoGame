"""Bounded notable-building silhouettes -> deterministic M3-compatible footprint contract."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path

import pyproj
import shapely
from shapely.geometry import box, Polygon
from shapely.ops import unary_union
from process_public_spaces import ROOT, DEFAULT_OUTPUT, serialize, digest
from process_m3 import read_shapes, parts, verify_snapshot

SOURCE = ROOT/'Data/raw/montevideo/buildings/2026-10-15/source.json'
OUTPUT = ROOT/'Data/processed/montevideo/m3'


def stable_seed(identity):
    return int.from_bytes(hashlib.sha1(identity.encode('utf-8')).digest()[:4], 'big')


def height_policy(identity, floor_height_m=3.0):
    if not math.isfinite(floor_height_m) or not 2 <= floor_height_m <= 5:
        raise ValueError('Development floor height must be 2..5 m')
    seed = stable_seed(identity)
    floors = 3 + seed % 3
    return dict(seed=seed, floors=floors, floors_source='synthetic_blockout',
                height_m=floors*floor_height_m, height_source='synthetic_blockout')


def encode_polygon(geometry, identity, wgs, local):
    if geometry.is_empty or not geometry.is_valid or geometry.geom_type != 'Polygon' or geometry.area <= 0:
        raise ValueError('Invalid footprint, no automatic repairs: '+identity)
    geometry = shapely.normalize(geometry)
    vertices, lookup, rings = [], {}, []
    def index(p):
        p = tuple(p[:2])
        if p not in lookup:
            lookup[p] = len(vertices); vertices.append(p)
        return lookup[p]
    for ring in [geometry.exterior, *geometry.interiors]:
        rings.append([index(p) for p in list(ring.coords)[:-1]])
    coords = [list(wgs.transform(*p, errcheck=True)) for p in vertices]
    positions = [(n*100,e*100) for e,n,_ in [local.transform(lon,lat,0,errcheck=True) for lon,lat in coords]]
    if any(math.hypot(x,y)>50000 for x,y in positions):
        raise ValueError('Footprint exceeds authorized radius')
    local_polygon = Polygon([positions[i] for i in rings[0]], [[positions[i] for i in r] for r in rings[1:]])
    if not local_polygon.is_valid:
        raise ValueError('Reprojection invalidates footprint: '+identity)
    tessellation = sorted(shapely.constrained_delaunay_triangles(local_polygon).geoms, key=lambda t:t.wkb_hex)
    if local_polygon.symmetric_difference(unary_union(tessellation)).area > 0.01:
        raise ValueError('Triangulation changes footprint or holes')
    position_ids = {p:i for i,p in enumerate(positions)}
    triangles = []
    for t in tessellation:
        ids = [position_ids[tuple(p)] for p in list(t.exterior.coords)[:3]]
        a,b,c = [positions[i] for i in ids]
        cross = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
        if abs(cross) <= 1e-8:
            raise ValueError('Collapsed roof triangle: '+identity)
        if cross < 0:
            ids.reverse()
        triangles.extend(ids)
    centroid = list(wgs.transform(geometry.centroid.x, geometry.centroid.y, errcheck=True))
    return dict(id=identity, role='buildings', kind='polygon', vertices=coords, triangles=triangles, rings=rings,
                centroid=centroid, centroid_source='derived_area_centroid_utm', area_utm_m2=geometry.area,
                bbox_utm_m=list(geometry.bounds), altitude=None, altitude_reference=None,
                expected_local_bbox_cm=[min(p[0] for p in positions),min(p[1] for p in positions),max(p[0] for p in positions),max(p[1] for p in positions)])


def build(source_path=SOURCE, processed_at_utc=None, floor_height_m=3.0):
    source = verify_snapshot(source_path)
    if source['attributes'] != ['estado','gid','nombre'] or '640/10' not in source['license']['official_statement']:
        raise ValueError('Source semantics/license changed; review before processing')
    if processed_at_utc is None:
        processed_at_utc = json.loads((OUTPUT/'buildings_manifest.json').read_bytes())['processed_at_utc']
    instant = datetime.fromisoformat(processed_at_utc)
    if instant.tzinfo is None or instant.utcoffset().total_seconds() != 0:
        raise ValueError('Explicit UTC processing date required')
    origin = json.loads(DEFAULT_OUTPUT.read_bytes())
    c = origin['centroid_wgs84']; center = origin['centroid_original']
    bbox = [center['easting_m']-350,center['northing_m']-350,center['easting_m']+350,center['northing_m']+350]
    if source['query_bbox_utm_m'] != bbox:
        raise ValueError('Snapshot must match approved M3 window')
    pyproj.network.set_network_enabled(False)
    wgs = pyproj.Transformer.from_crs(32721,4326,always_xy=True)
    local = pyproj.Transformer.from_pipeline(f'+proj=pipeline +step +proj=cart +ellps=WGS84 +step +proj=topocentric +ellps=WGS84 +lat_0={c["latitude"]} +lon_0={c["longitude"]} +h_0=0')
    seen, features = set(), []
    for record, geometry in sorted(read_shapes(source_path.parent/'buildings.zip'),key=lambda r:r[0]['gid']):
        sid = str(record['gid']); bid = source['layer']+':'+sid
        if sid in seen or geometry.is_empty or not geometry.is_valid or geometry.geom_type not in ('Polygon','MultiPolygon'):
            raise ValueError('Invalid/duplicate original building '+sid)
        seen.add(sid)
        clipped = geometry.intersection(box(*bbox))
        for i, part in enumerate(sorted(parts(clipped,'Polygon'),key=lambda p:shapely.normalize(p).wkb_hex)):
            f = encode_polygon(part,bid+':'+str(i),wgs,local)
            f.update(building_id=bid,source_feature_id=sid,attributes=record,**height_policy(bid,floor_height_m))
            f['clipped_to_aoi'] = not geometry.equals(clipped)
            features.append(f)
    if not features:
        raise ValueError('No usable footprints')
    payload = serialize(dict(schema_version=1,building_schema_version=1,crs='EPSG:4326',features=features))
    manifest = dict(schema_version=1,pipeline_version='m3.2-1',crs='EPSG:4326',axes='X north, Y east, Z up',
                    processed_at_utc=instant.astimezone(timezone.utc).isoformat(),
                    origin_source=DEFAULT_OUTPUT.relative_to(ROOT).as_posix(),origin_sha1=hashlib.sha1(DEFAULT_OUTPUT.read_bytes()).hexdigest(),
                    origin_sha256=digest(DEFAULT_OUTPUT.read_bytes()),source=source_path.relative_to(ROOT).as_posix(),source_sha256=digest(source_path.read_bytes()),
                    datasets=[source],area_crs='EPSG:32721',area_bbox_m=bbox,max_radius_m=500,
                    altitude=None,altitude_reference=None,development_plane_z_cm=0,
                    height_policy=dict(mode='synthetic_blockout',floor_height_m=floor_height_m,min_floors=3,max_floors=5,
                                       seed='SHA1(UTF8(layer + ":" + gid)), first 4 bytes big-endian unsigned',rule='floors = 3 + seed % 3; height_m = floors * floor_height_m'),
                    method='Exact AOI clipping; no repair/simplification; canonical polygon parts; GEOS constrained Delaunay in the PROJ local plane with holes; unchanged WGS84 vertices; C++ FGeoReference then flat Z=0; synthetic extrusion.',
                    versions=dict(pyproj=pyproj.__version__,PROJ=pyproj.proj_version_str,shapely=shapely.__version__,GEOS=shapely.geos_version_string),
                    files=[dict(path='Data/processed/montevideo/m3/buildings.json',sha256=digest(payload),sha1=hashlib.sha1(payload).hexdigest())],
                    counts=dict(buildings=len({f['building_id'] for f in features}),polygons=len(features),lines=0,vertices=sum(len(f['vertices']) for f in features),triangles=sum(len(f['triangles'])//3 for f in features)),
                    expected_local_bbox_cm=[min(f['expected_local_bbox_cm'][i] for f in features) if i<2 else max(f['expected_local_bbox_cm'][i] for f in features) for i in range(4)])
    return {'buildings.json':payload,'buildings_manifest.json':serialize(manifest)}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=SOURCE)
    parser.add_argument('--processed-at-utc')
    parser.add_argument('--floor-height-m',type=float,default=3.0)
    args=parser.parse_args()
    files=build(args.source.resolve(),args.processed_at_utc,args.floor_height_m)
    for name, data in files.items():
        (OUTPUT/name).write_bytes(data)
    print(json.dumps(json.loads(files['buildings_manifest.json'])['counts']))


if __name__=='__main__':
    main()
