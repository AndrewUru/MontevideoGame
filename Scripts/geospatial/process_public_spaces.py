"""Reproduce the Plaza Independencia derivative from an immutable official snapshot."""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET
import zipfile

import pyproj
import shapefile
import shapely
from shapely.geometry import mapping, shape
from shapely.ops import transform

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = ROOT / 'Data/raw/montevideo/public_spaces/2026-10-14/source.json'
DEFAULT_OUTPUT = ROOT / 'Data/processed/montevideo/plaza_independencia.json'
NS = {'gmd': 'http://www.isotc211.org/2005/gmd', 'gco': 'http://www.isotc211.org/2005/gco'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def verify_source(source_path):
    source = json.loads(source_path.read_text(encoding='utf-8'))
    if source['license']['id'] != 'odc-uy' or source['declared_crs'] != 'EPSG:32721':
        raise ValueError('Unexpected license or CRS; review the source')
    for name in ('catalog.json', 'metadata.xml', 'public_spaces.zip', 'license.pdf'):
        record = source['files'][name]
        data = (source_path.parent / name).read_bytes()
        if digest(data) != record['sha256'] or len(data) != record['size_bytes']:
            raise ValueError(f'Original hash/size mismatch: {name}')
    if source['original_file'] != 'public_spaces.zip' or source['sha256'] != source['files']['public_spaces.zip']['sha256']:
        raise ValueError('Inconsistent original file manifest')
    metadata = ET.parse(source_path.parent / 'metadata.xml')
    code = metadata.findtext('.//gmd:referenceSystemInfo//gmd:code/gco:CharacterString', namespaces=NS)
    authority = metadata.findtext('.//gmd:referenceSystemInfo//gmd:codeSpace/gco:CharacterString', namespaces=NS)
    if f'{authority}:{code}' != source['declared_crs']:
        raise ValueError('Official metadata CRS disagrees with manifest')
    catalog = json.loads((source_path.parent / 'catalog.json').read_bytes())['result']
    if catalog['license_id'] != source['license']['id'] or catalog['id'] != source['catalog_id']:
        raise ValueError('Catalog license/identity disagrees with manifest')
    return source


def select_feature(records):
    matches = [record for record in records
               if record.record['nombre_esp'].strip().upper() == 'INDEPENDENCIA'
               and record.record['tipo_espac'].strip().upper() == 'PLAZA']
    if len(matches) != 1:
        raise ValueError(f'Expected exactly one Plaza Independencia, found {len(matches)}')
    return matches[0]


def validate_geometry(geometry):
    if geometry.is_empty or geometry.geom_type not in ('Polygon', 'MultiPolygon'):
        raise ValueError('Expected nonempty polygon geometry')
    if not geometry.is_valid or not math.isfinite(geometry.area) or geometry.area <= 0:
        raise ValueError('Invalid polygon; automatic repairs are not allowed')
    if not geometry.contains(geometry.centroid):
        raise ValueError('Geometric centroid is outside polygon')


def build_derivative(source_path=DEFAULT_SOURCE):
    source_path = source_path.resolve()
    source = verify_source(source_path)
    pyproj.network.set_network_enabled(False)
    with zipfile.ZipFile(source_path.parent / source['original_file']) as archive:
        if archive.testzip() is not None:
            raise ValueError('Corrupt ZIP')
        parts = {}
        for extension in ('shp', 'shx', 'dbf', 'prj', 'cst'):
            names = [name for name in archive.namelist() if name.lower().endswith('.' + extension)]
            if len(names) != 1:
                raise ValueError(f'Expected one .{extension} member')
            parts[extension] = names[0]
        crs = pyproj.CRS.from_wkt(archive.read(parts['prj']).decode('utf-8'))
        if crs.to_epsg() != 32721 or not crs.equals(pyproj.CRS(source['declared_crs'])):
            raise ValueError('Shapefile PRJ disagrees with official CRS')
        encoding = archive.read(parts['cst']).decode('ascii').strip()
        if encoding.upper() != 'UTF-8':
            raise ValueError(f'Unexpected DBF charset: {encoding}')
        with shapefile.Reader(**{key: io.BytesIO(archive.read(parts[key])) for key in ('shp', 'shx', 'dbf')}, encoding='utf-8') as reader:
            count = len(reader)
            selected = select_feature(reader.iterShapeRecords())
            attributes = selected.record.as_dict()
            geometry = shape(selected.shape.__geo_interface__)
    validate_geometry(geometry)
    transformer = pyproj.Transformer.from_crs(crs, 'EPSG:4326', always_xy=True, allow_ballpark=False)
    geographic = transform(transformer.transform, geometry)
    centroid = geometry.centroid
    longitude, latitude = transformer.transform(centroid.x, centroid.y, errcheck=True)
    bounds = geographic.bounds
    if not geographic.is_valid or not geographic.contains(transform(transformer.transform, centroid)):
        raise ValueError('Reprojected geometry or centroid containment invalid')
    if not all(math.isfinite(v) for v in (*bounds, longitude, latitude)) or not (-180 <= bounds[0] <= longitude <= bounds[2] <= 180 and -90 <= bounds[1] <= latitude <= bounds[3] <= 90):
        raise ValueError('Invalid WGS84 coordinates')
    return {
        'schema_version': 1, 'name': 'Plaza Independencia',
        'source': source_path.relative_to(ROOT).as_posix(),
        'source_manifest_sha256': digest(source_path.read_bytes()),
        'source_sha256': source['sha256'], 'source_feature_id': str(attributes['gid']),
        'source_feature_id_field': 'gid', 'source_feature_count': count,
        'selection': {'nombre_esp': 'INDEPENDENCIA', 'tipo_espac': 'PLAZA', 'matching_features': 1},
        'attributes_original': attributes, 'crs_original': 'EPSG:32721', 'crs_output': 'EPSG:4326',
        'centroid_original': {'easting_m': centroid.x, 'northing_m': centroid.y},
        'centroid_wgs84': {'latitude': latitude, 'longitude': longitude},
        'altitude': None, 'altitude_reference': None,
        'bbox_original': {'order': ['min_easting', 'min_northing', 'max_easting', 'max_northing'], 'units': 'm', 'values': list(geometry.bounds)},
        'bbox_wgs84': {'order': ['min_longitude', 'min_latitude', 'max_longitude', 'max_latitude'], 'units': 'degrees', 'values': list(bounds)},
        'approximate_area_m2': geometry.area,
        'geometry_original': mapping(geometry), 'geometry_wgs84': mapping(geographic),
        'license': source['license'],
        'processing': {
            'script': 'Scripts/geospatial/process_public_spaces.py', 'method_version': 1,
            'versions': {'pyshp': shapefile.__version__, 'pyproj': pyproj.__version__, 'PROJ': pyproj.proj_version_str, 'shapely': shapely.__version__, 'GEOS': shapely.geos_version_string},
            'axis_order': 'Original: easting/northing metres; output geometry: longitude/latitude degrees (always_xy=True).',
            'method': 'Select by exact normalized name/type; validate without repairs; planar area and area-weighted centroid in EPSG:32721; reproject polygon vertices and metric centroid to EPSG:4326.',
            'modifications': 'Single feature extracted; derived geometry, bounding boxes, centroid and approximate planar area. No simplification, snapping or vertical transformation.',
            'network_enabled': False,
        },
    }


def serialize(result):
    return (json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + '\n').encode('utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=DEFAULT_SOURCE)
    parser.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to((ROOT / 'Data/processed').resolve()):
        raise ValueError('Output must remain under Data/processed')
    data = serialize(build_derivative(args.source))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    print(json.dumps({'output': output.relative_to(ROOT).as_posix(), 'sha256': digest(data)}))


if __name__ == '__main__':
    main()
