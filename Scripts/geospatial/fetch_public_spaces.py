"""Download an immutable official snapshot. Never replace an existing snapshot."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import urllib.request
import xml.etree.ElementTree as ET
import zipfile
import io

CATALOG = 'https://catalogodatos.gub.uy/api/3/action/package_show?id=espacios-publicos-de-montevideo'
CATALOG_PAGE = 'https://catalogodatos.gub.uy/dataset/espacios-publicos-de-montevideo'
METADATA = 'https://geoweb.montevideo.gub.uy/geonetwork/srv/api/records/55a1f334-aada-48df-811a-ddde43ab739a/formatters/xml'
NS = {'gmd': 'http://www.isotc211.org/2005/gmd', 'gco': 'http://www.isotc211.org/2005/gco'}


def download(url):
    request = urllib.request.Request(url, headers={'User-Agent': 'MontevideoGame-Provenance/1.0'})
    with urllib.request.urlopen(request, timeout=120) as response:
        data = response.read(50 * 1024 * 1024 + 1)
        if len(data) > 50 * 1024 * 1024:
            raise ValueError('Response exceeds the 50 MiB snapshot limit')
        return data, {
            'url': url, 'resolved_url': response.geturl(),
            'downloaded_at_utc': datetime.now(timezone.utc).isoformat(),
            'sha256': hashlib.sha256(data).hexdigest(), 'size_bytes': len(data),
            'content_type': response.headers.get('Content-Type'),
            'last_modified': response.headers.get('Last-Modified'),
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='New directory under Data/raw')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    if not output.is_relative_to((root / 'Data/raw').resolve()) or output.exists():
        raise ValueError('Use a NEW snapshot directory under Data/raw; existing snapshots are immutable')
    catalog_bytes, catalog_record = download(CATALOG)
    catalog = json.loads(catalog_bytes)['result']
    if catalog['license_id'] != 'odc-uy':
        raise ValueError('License changed: review before acquiring data')
    metadata_bytes, metadata_record = download(METADATA)
    metadata = ET.fromstring(metadata_bytes)
    code = metadata.findtext('.//gmd:referenceSystemInfo//gmd:code/gco:CharacterString', namespaces=NS)
    authority = metadata.findtext('.//gmd:referenceSystemInfo//gmd:codeSpace/gco:CharacterString', namespaces=NS)
    if (authority, code) != ('EPSG', '32721'):
        raise ValueError('Unexpected official CRS; review metadata before processing')
    resource = next(r for r in catalog['resources'] if r['id'] == '344a4ade-1c9a-48f2-becd-6a252e874794')
    links = [node.text for node in metadata.findall('.//gmd:distributionInfo//gmd:URL', NS)
             if node.text and 'outputFormat=SHAPE-ZIP' in node.text]
    if len(links) != 1 or not links[0].startswith('https://montevideo.gub.uy/app/geoserver/ows?'):
        raise ValueError('Expected one official Shapefile distribution URL in metadata')
    original, original_record = download(links[0])
    with zipfile.ZipFile(io.BytesIO(original)) as archive:
        if archive.testzip() is not None or not any(n.lower().endswith('.prj') for n in archive.namelist()):
            raise ValueError('Invalid Shapefile ZIP / missing PRJ')
    license_bytes, license_record = download(catalog['license_url'])
    if not license_bytes.startswith(b'%PDF'):
        raise ValueError('Expected official license PDF')
    files = {'catalog.json': (catalog_bytes, catalog_record), 'metadata.xml': (metadata_bytes, metadata_record),
             'public_spaces.zip': (original, original_record), 'license.pdf': (license_bytes, license_record)}
    source = {
        'schema_version': 1, 'dataset_name': catalog['title'],
        'organization': 'Intendencia de Montevideo - Servicio de Geomatica',
        'catalog_url': CATALOG_PAGE, 'catalog_id': catalog['id'], 'resource_id': resource['id'],
        'catalog_resource_url': resource['url'], 'metadata_url': METADATA,
        'download_url': original_record['url'], 'resolved_download_url': original_record['resolved_url'],
        'downloaded_at_utc': original_record['downloaded_at_utc'],
        'declared_crs': f'{authority}:{code}', 'crs_evidence': 'metadata.xml / referenceSystemInfo and original ZIP .prj',
        'original_format': 'ESRI Shapefile in ZIP', 'original_file': 'public_spaces.zip',
        'sha256': original_record['sha256'],
        'license': {'id': catalog['license_id'], 'name': catalog['license_title'],
                    'version': '0.1', 'url': catalog['license_url'],
                    'attribution': 'Intendencia de Montevideo, Servicio de Geomatica; Espacios publicos de Montevideo; Licencia de Datos Abiertos - Uruguay.'},
        'catalog_modified_at': catalog['metadata_modified'],
        'transformation_notes': ['Original HTTP bodies preserved byte-for-byte.', 'Download uses the SHAPE-ZIP distribution advertised by official GeoNetwork metadata; the legacy catalog resource endpoint returned a non-ZIP response during discovery.', 'No transformations in Data/raw; derived files belong in Data/processed.'],
        'files': {name: record for name, (_, record) in files.items()},
    }
    output.mkdir(parents=True, exist_ok=False)
    for name, (data, _) in files.items():
        with (output / name).open('xb') as f:
            f.write(data)
    with (output / 'source.json').open('x', encoding='utf-8', newline='\n') as f:
        json.dump(source, f, ensure_ascii=False, indent=2)
        f.write('\n')
    print(json.dumps({'snapshot': str(args.output), 'sha256': source['sha256'], 'bytes': len(original)}))


if __name__ == '__main__':
    main()
