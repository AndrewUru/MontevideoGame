"""Discover official WFS layers and acquire only the M3 bounding-box subset."""
import argparse
import json
from pathlib import Path
from urllib.parse import urlencode, urlsplit, parse_qs
import xml.etree.ElementTree as ET
import zipfile
import io

from fetch_public_spaces import download, NS
from process_public_spaces import ROOT, DEFAULT_SOURCE, DEFAULT_OUTPUT, verify_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to(ROOT / 'Data/raw'):
        raise ValueError('Use a new directory under Data/raw; snapshots are immutable')
    verify_source(DEFAULT_SOURCE)
    origin = json.loads(DEFAULT_OUTPUT.read_text(encoding='utf-8'))
    e, n = origin['centroid_original']['easting_m'], origin['centroid_original']['northing_m']
    bbox = [e - 350, n - 350, e + 350, n + 350]
    previous = json.loads(DEFAULT_SOURCE.read_text(encoding='utf-8'))
    base = previous['download_url'].split('?')[0]
    caps_url = base + '?' + urlencode({'service': 'WFS', 'version': '1.1.0', 'request': 'GetCapabilities'})
    caps, caps_record = download(caps_url)
    root = ET.fromstring(caps)
    wns = {'w': 'http://www.opengis.net/wfs'}
    files = {'capabilities.xml': (caps, caps_record)}
    datasets = []
    for role, title in [('roads', 'Ejes de Calles'), ('blocks', 'Manzanas materializadas')]:
        matches = [f for f in root.findall('.//w:FeatureType', wns) if f.findtext('w:Title', namespaces=wns) == title]
        if len(matches) != 1:
            raise ValueError('Expected one advertised layer: ' + title)
        feature = matches[0]
        layer = feature.findtext('w:Name', namespaces=wns)
        crs = feature.findtext('w:DefaultSRS', namespaces=wns)
        if not crs.endswith(':32721'):
            raise ValueError('Unexpected CRS: ' + crs)
        catalog_url = feature.findtext('w:MetadataURL', namespaces=wns)
        metadata_id = catalog_url.rsplit('/', 1)[-1]
        metadata_url = catalog_url.split('/srv/')[0] + '/srv/api/records/' + metadata_id + '/formatters/xml'
        metadata_bytes, metadata_record = download(metadata_url)
        metadata = ET.fromstring(metadata_bytes)
        legal = ' '.join(' '.join(' '.join(node.itertext()).split()) for node in metadata.findall('.//gmd:MD_LegalConstraints', NS))
        if '640/10' not in legal or 'libre' not in legal.lower():
            raise ValueError('Review changed license before downloading')
        if metadata.findtext('.//gmd:referenceSystemInfo//gmd:code/gco:CharacterString', namespaces=NS) != '32721':
            raise ValueError('Metadata CRS mismatch')
        links = [node.text for node in metadata.findall('.//gmd:distributionInfo//gmd:URL', NS)
                 if node.text and 'outputFormat=SHAPE-ZIP' in node.text]
        if len(links) != 1:
            raise ValueError('Missing unambiguous official distribution')
        parsed = urlsplit(links[0]); params = {k: v[0] for k, v in parse_qs(parsed.query).items()}
        if params['typeName'] != layer:
            raise ValueError('Metadata layer and capabilities disagree')
        params.update({'bbox': ','.join(map(str, bbox)) + ',EPSG:32721', 'srsName': 'EPSG:32721', 'maxFeatures': '2000'})
        data_url = parsed._replace(query=urlencode(params)).geturl()
        data, record = download(data_url)
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            if archive.testzip() is not None:
                raise ValueError('Invalid ZIP')
        files[role + '-metadata.xml'] = (metadata_bytes, metadata_record)
        files[role + '.zip'] = (data, record)
        datasets.append({'role': role, 'dataset_name': title, 'layer': layer,
                         'organization': 'Intendencia de Montevideo - Servicio de Geomatica',
                         'catalog_url': catalog_url, 'metadata_url': metadata_url,
                         'original_file': role + '.zip', 'download_url': data_url,
                         'downloaded_at_utc': record['downloaded_at_utc'], 'sha256': record['sha256'],
                         'declared_crs': 'EPSG:32721', 'original_format': 'ESRI Shapefile in ZIP',
                         'license': {'name': 'Uso libre - Resolucion IM 640/10 (22/02/2010)', 'official_statement': legal,
                                     'evidence': role + '-metadata.xml', 'url': metadata_url},
                         'attribution': 'Intendencia de Montevideo, Servicio de Geomatica; ' + title,
                         'transformation_notes': ['Server-side BBOX subset only; original HTTP bytes unchanged.', 'Clipping/triangulation are separate processed derivatives.']})
    source = {'schema_version': 1, 'pipeline': 'M3', 'declared_crs': 'EPSG:32721', 'query_bbox_utm_m': bbox,
              'origin_source': DEFAULT_OUTPUT.relative_to(ROOT).as_posix(), 'max_features_per_layer': 2000,
              'datasets': datasets, 'files': {key: value[1] for key, value in files.items()}}
    output.mkdir(parents=True, exist_ok=False)
    for name, (data, _) in files.items():
        (output / name).write_bytes(data)
    (output / 'source.json').write_text(json.dumps(source, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'snapshot': str(args.output), 'datasets': datasets}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
