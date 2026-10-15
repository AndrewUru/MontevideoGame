"""Deterministic, bounded official geometry -> WGS84 blockout contract (no Unreal assets)."""
import argparse
import io
import json
import math
import hashlib
from datetime import datetime, timezone
from pathlib import Path
import zipfile

import pyproj
import shapefile
import shapely
from shapely.geometry import box, shape
from shapely.ops import transform, unary_union

from process_public_spaces import ROOT, DEFAULT_OUTPUT, build_derivative, digest, serialize

SOURCE = ROOT / 'Data/raw/montevideo/m3/2026-10-14/source.json'
OUTPUT = ROOT / 'Data/processed/montevideo/m3'


def verify_snapshot(path):
    source = json.loads(path.read_text(encoding='utf-8'))
    for name, record in source['files'].items():
        data = (path.parent / name).read_bytes()
        if digest(data) != record['sha256'] or len(data) != record['size_bytes']:
            raise ValueError('Raw hash/size mismatch: ' + name)
    if source['declared_crs'] != 'EPSG:32721':
        raise ValueError('Unexpected source CRS')
    return source


def read_shapes(path):
    with zipfile.ZipFile(path) as archive:
        parts = {}
        for extension in ('shp', 'shx', 'dbf', 'prj'):
            matches = [name for name in archive.namelist() if name.endswith('.' + extension)]
            if len(matches) != 1:
                raise ValueError('Ambiguous shapefile')
            parts[extension] = matches[0]
        if pyproj.CRS.from_wkt(archive.read(parts['prj']).decode()).to_epsg() != 32721:
            raise ValueError('PRJ CRS mismatch')
        with shapefile.Reader(**{key: io.BytesIO(archive.read(parts[key])) for key in ('shp', 'shx', 'dbf')}, encoding='utf-8') as reader:
            if len(reader) >= 2000:
                raise ValueError('Possible truncated WFS response; refuse incomplete coverage')
            return [(r.record.as_dict(), shape(r.shape.__geo_interface__)) for r in reader.iterShapeRecords()]


def parts(geometry, kind):
    if geometry.is_empty:
        return []
    if geometry.geom_type == kind:
        return [geometry]
    if hasattr(geometry, 'geoms'):
        return [part for child in geometry.geoms for part in parts(child, kind)]
    return []


def build(source_path=SOURCE, processed_at_utc=None):
    # UTC is an explicit reproducible build parameter, never silently replaced by wall-clock time.
    if processed_at_utc is None:
        existing = OUTPUT / 'manifest.json'
        if not existing.exists():
            raise ValueError('First build requires --processed-at-utc; rebuilds preserve the recorded processing date')
        processed_at_utc = json.loads(existing.read_text(encoding='utf-8'))['processed_at_utc']
    instant = datetime.fromisoformat(processed_at_utc)
    if instant.tzinfo is None or instant.utcoffset().total_seconds() != 0:
        raise ValueError('Processing date must explicitly use UTC')
    processed_at_utc = instant.astimezone(timezone.utc).isoformat()
    pyproj.network.set_network_enabled(False)
    source = verify_snapshot(source_path)
    plaza = build_derivative()
    if serialize(plaza) != DEFAULT_OUTPUT.read_bytes():
        raise ValueError('M2.5 origin/geometry is not reproducible')
    area = box(*source['query_bbox_utm_m'])
    e, n = plaza['centroid_original']['easting_m'], plaza['centroid_original']['northing_m']
    if area.bounds != (e-350, n-350, e+350, n+350):
        raise ValueError('Snapshot extent must be the authorized 700 m square')
    wgs = pyproj.Transformer.from_crs(32721, 4326, always_xy=True)
    c = plaza['centroid_wgs84']
    local = pyproj.Transformer.from_pipeline(f'+proj=pipeline +step +proj=cart +ellps=WGS84 +step +proj=topocentric +ellps=WGS84 +lat_0={c["latitude"]} +lon_0={c["longitude"]} +h_0=0')

    def encode(geometry, role, identity, attributes):
        vertices, lookup, triangles, rings = [], {}, [], []

        def index(point):
            point = tuple(point[:2])
            if point not in lookup:
                lookup[point] = len(vertices)
                vertices.append(point)
            return lookup[point]

        if geometry.geom_type == 'Polygon':
            if not geometry.is_valid or geometry.area <= 0:
                raise ValueError('Invalid polygon: ' + identity)
            geometry = shapely.normalize(geometry)
            for ring in [geometry.exterior, *geometry.interiors]:
                rings.append([index(p) for p in list(ring.coords)[:-1]])
            tessellation = sorted(shapely.constrained_delaunay_triangles(geometry).geoms, key=lambda t: t.wkb_hex)
            if geometry.symmetric_difference(unary_union(tessellation)).area > 1.e-6:
                raise ValueError('Triangulation changes polygon/hole coverage')
            for triangle in tessellation:
                ids = [index(p) for p in list(triangle.exterior.coords)[:3]]
                # Local X=north,Y=east reverses the UTM east/north winding.
                a, b, d = [vertices[i] for i in ids]
                if (b[0]-a[0])*(d[1]-a[1])-(b[1]-a[1])*(d[0]-a[0]) > 0:
                    ids.reverse()
                triangles.extend(ids)
        else:
            for point in geometry.coords:
                vertices.append(tuple(point[:2]))
        coordinates = [list(wgs.transform(*point, errcheck=True)) for point in vertices]
        xyz = [local.transform(lon, lat, 0, errcheck=True) for lon, lat in coordinates]
        positions = [(north*100, east*100) for east, north, _ in xyz]
        if not all(math.hypot(x,y) <= 50000 for x,y in positions):
            raise ValueError('Outside authorized local radius')
        usable_triangles, omitted_count, omitted_area = [], 0, 0.0
        for i in range(0, len(triangles), 3):
            ids = triangles[i:i+3]
            a,b,c = [positions[j] for j in ids]
            twice_area_cm2 = (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
            if abs(twice_area_cm2) <= 0.02:  # At most 1 square millimeter: numerical slivers, not usable faces.
                a,b,c = [vertices[j] for j in ids]
                metric_area = abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))/2
                if metric_area > 1.e-6:
                    raise ValueError('Non-negligible triangle collapsed during reprojection')
                omitted_count += 1
                omitted_area += metric_area
            elif twice_area_cm2 < 0:
                raise ValueError('Non-degenerate inverted local triangle')
            else:
                usable_triangles.extend(ids)
        if triangles and not usable_triangles:
            raise ValueError('Polygon has no usable triangles')
        return {'id': identity, 'role': role, 'kind': 'polygon' if triangles else 'line',
                'attributes': attributes, 'vertices': coordinates, 'triangles': usable_triangles, 'rings': rings,
                'omitted_numerical_triangles': omitted_count, 'omitted_triangle_area_utm_m2': omitted_area,
                'altitude': None, 'altitude_reference': None,
                'area_utm_m2': geometry.area, 'bbox_utm_m': list(geometry.bounds),
                'expected_local_bbox_cm': [min(p[0] for p in positions),min(p[1] for p in positions),max(p[0] for p in positions),max(p[1] for p in positions)]}

    outputs = {}
    features = [encode(shape(plaza['geometry_original']), 'plaza', plaza['source_feature_id'], plaza['attributes_original'])]
    outputs['plaza_independencia.json'] = {'schema_version': 1, 'crs': 'EPSG:4326', 'features': list(features)}
    for role in ('roads', 'blocks'):
        result = []
        for record, geometry in sorted(read_shapes(source_path.parent / (role + '.zip')), key=lambda item: json.dumps(item[0], sort_keys=True)):
            if not geometry.is_valid:
                raise ValueError(f'Invalid official geometry; no silent repairs: {record}')
            clipped = geometry.intersection(area)
            identity = str(record.get('gid', record.get('id', digest(geometry.wkb))))
            for i, part in enumerate(sorted(parts(clipped, 'LineString' if role == 'roads' else 'Polygon'), key=lambda p:p.wkb_hex)):
                result.append(encode(part, role, identity + ':' + str(i), record))
        if not result:
            raise ValueError('Empty layer: ' + role)
        outputs[role + '.json'] = {'schema_version': 1, 'crs': 'EPSG:4326', 'features': result}
        features += result
    files = {name: serialize(data) for name, data in outputs.items()}
    manifest = {
        'schema_version': 1, 'pipeline_version': 'm3-1', 'crs': 'EPSG:4326',
        'processed_at_utc': processed_at_utc,
        'processing_date_policy': 'Explicit UTC build parameter recorded when processing the batch; subsequent rebuilds preserve it. Not refreshed implicitly.',
        'origin_source': DEFAULT_OUTPUT.relative_to(ROOT).as_posix(), 'origin_sha256': digest(DEFAULT_OUTPUT.read_bytes()),
        'origin_sha1': hashlib.sha1(DEFAULT_OUTPUT.read_bytes()).hexdigest(),
        'source': source_path.relative_to(ROOT).as_posix(), 'source_sha256': digest(source_path.read_bytes()),
        'datasets': source['datasets'] + [{'role': 'plaza', 'source': plaza['source'], 'sha256': plaza['source_sha256'], 'license': plaza['license']}],
        'area_crs': 'EPSG:32721', 'area_bbox_m': list(area.bounds), 'max_radius_m': 500,
        'units': 'WGS84 degrees; runtime centimeters', 'axes': 'X north, Y east, Z up',
        'altitude': None, 'altitude_reference': None, 'development_plane_z_cm': 0,
        'method': 'BBOX query; exact square clipping in UTM; constrained triangulation preserving holes; WGS84 vertices; C++ FGeoReference at synthetic h=0 then flatten local Z=0.',
        'mesh_precision_policy': 'Preserve all source/clipped vertices and rings. Exclude only numerical triangles <=1e-6 m2 in both UTM and local projection; counts/area recorded per feature. No polygon repair/simplification.',
        'versions': {'pyproj': pyproj.__version__, 'shapely': shapely.__version__, 'GEOS': shapely.geos_version_string, 'PROJ': pyproj.proj_version_str},
        'files': [{'path': 'Data/processed/montevideo/m3/' + name, 'sha256': digest(data), 'sha1': hashlib.sha1(data).hexdigest()} for name, data in files.items()],
        'counts': {'polygons': sum(f['kind']=='polygon' for f in features), 'lines': sum(f['kind']=='line' for f in features),
                   'vertices': sum(len(f['vertices']) for f in features), 'triangles': sum(len(f['triangles'])//3 for f in features)},
        'expected_local_bbox_cm': [min(f['expected_local_bbox_cm'][i] for f in features) if i<2 else max(f['expected_local_bbox_cm'][i] for f in features) for i in range(4)],
    }
    files['manifest.json'] = serialize(manifest)
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=SOURCE)
    parser.add_argument('--processed-at-utc', help='Explicit processing batch date, UTC ISO8601; preserve existing manifest date if omitted')
    args = parser.parse_args()
    files = build(args.source.resolve(), args.processed_at_utc)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        (OUTPUT / name).write_bytes(data)
    print(json.dumps(json.loads(files['manifest.json'])['counts']))


if __name__ == '__main__':
    main()
