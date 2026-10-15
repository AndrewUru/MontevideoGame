"""Discover and capture a bounded official building-footprint snapshot; never overwrite raw."""
import argparse
import json
from pathlib import Path
from urllib.parse import urlencode, urlsplit, parse_qs
import xml.etree.ElementTree as ET

from fetch_public_spaces import download, NS
from process_public_spaces import ROOT, DEFAULT_OUTPUT, serialize
from process_m3 import read_shapes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to((ROOT / 'Data/raw').resolve()):
        raise ValueError('Use a NEW snapshot directory under Data/raw')
    origin = json.loads(DEFAULT_OUTPUT.read_bytes())
    e, n = origin['centroid_original']['easting_m'], origin['centroid_original']['northing_m']
    bbox = [e-350, n-350, e+350, n+350]
    previous = json.loads((ROOT/'Data/raw/montevideo/m3/2026-10-14/source.json').read_bytes())
    base = previous['datasets'][0]['download_url'].split('?')[0]
    caps, caps_record = download(base+'?'+urlencode(dict(service='WFS', version='1.1.0', request='GetCapabilities')))
    ns = {'w': 'http://www.opengis.net/wfs'}
    features = ET.fromstring(caps).findall('.//w:FeatureType', ns)
    candidates = [f for f in features if f.findtext('w:Title', namespaces=ns) == 'Construcciones notables']
    if len(candidates) != 1:
        raise ValueError('Expected one verified footprint layer; review capabilities')
    f = candidates[0]
    layer = f.findtext('w:Name', namespaces=ns)
    catalog = f.findtext('w:MetadataURL', namespaces=ns)
    metadata_url = catalog.split('/srv/')[0]+'/srv/api/records/'+catalog.rsplit('/', 1)[-1]+'/formatters/xml'
    metadata, metadata_record = download(metadata_url)
    root = ET.fromstring(metadata)
    legal = ' '.join(' '.join(' '.join(x.itertext()).split()) for x in root.findall('.//gmd:MD_LegalConstraints', NS))
    if '640/10' not in legal or 'libre' not in legal.lower():
        raise ValueError('Unknown license; stop and review')
    if root.findtext('.//gmd:referenceSystemInfo//gmd:code/gco:CharacterString', namespaces=NS) != '32721':
        raise ValueError('Metadata CRS changed')
    links = [x.text for x in root.findall('.//gmd:distributionInfo//gmd:URL', NS) if x.text and 'outputFormat=SHAPE-ZIP' in x.text]
    if len(links) != 1:
        raise ValueError('Ambiguous official distribution')
    parsed = urlsplit(links[0]); params = {k:v[0] for k,v in parse_qs(parsed.query).items()}
    if params['typeName'] != layer:
        raise ValueError('Distribution and advertised layer differ')
    params.update(bbox=','.join(map(str,bbox))+',EPSG:32721', srsName='EPSG:32721', maxFeatures='2000')
    raw, raw_record = download(parsed._replace(query=urlencode(params)).geturl())
    schema, schema_record = download(base+'?'+urlencode(dict(service='WFS',version='1.1.0',request='DescribeFeatureType',typeName=layer)))
    import io
    records = read_shapes(io.BytesIO(raw))  # Checks PRJ, format and truncation before persisting.
    if not records:
        raise ValueError('No buildings in authorized area')
    fields = sorted(records[0][0])
    if fields != ['estado', 'gid', 'nombre']:
        raise ValueError('Attributes changed; inspect height policy before processing: '+str(fields))
    files = {'capabilities.xml':(caps,caps_record), 'metadata.xml':(metadata,metadata_record),
             'schema.xml':(schema,schema_record), 'buildings.zip':(raw,raw_record)}
    source = dict(schema_version=1, pipeline='M3.2', dataset_name='Construcciones notables', layer=layer,
                  organization='Intendencia de Montevideo - Servicio de Geomatica', catalog_url=catalog,
                  metadata_url=metadata_url, download_url=raw_record['url'], downloaded_at_utc=raw_record['downloaded_at_utc'],
                  declared_crs='EPSG:32721', original_format='ESRI Shapefile in ZIP', original_file='buildings.zip',
                  query_bbox_utm_m=bbox, max_features=2000, feature_count=len(records), attributes=fields,
                  license=dict(name='Uso libre - Resolucion IM 640/10 (22/02/2010)', official_statement=legal,url=metadata_url),
                  attribution='Intendencia de Montevideo, Servicio de Geomatica; Construcciones notables.',
                  coverage='Selected notable building silhouettes digitized from the December 2021 orthomosaic; NOT exhaustive building coverage.',
                  height_policy='No official height/floors fields. Synthetic development height only.',
                  transformations=['Server-side 700 x 700 m BBOX filter only. HTTP bodies preserved byte-for-byte.'],
                  files={name:record for name,(_,record) in files.items()})
    output.mkdir(parents=True, exist_ok=False)
    for name,(body,_) in files.items():
        (output/name).write_bytes(body)
    (output/'source.json').write_bytes(serialize(source))
    print(json.dumps(dict(snapshot=str(args.output),layer=layer,count=len(records),attributes=fields)))


if __name__ == '__main__':
    main()
