"""Read-only M3.2 geometry audit, provenance and reproducibility report; timings never enter derived data."""
import argparse
import json
import time
from pathlib import Path
from shapely.geometry import box
from process_buildings import build, SOURCE, OUTPUT
from process_public_spaces import digest
from process_m3 import read_shapes


def audit():
    start=time.perf_counter(); timings={}
    files=build(metrics=timings)
    timings['offline_total_ms']=(time.perf_counter()-start)*1000
    if files!=build() or any(body!=(OUTPUT/name).read_bytes() for name,body in files.items()):
        raise ValueError('Reproducibility failure')
    source=json.loads(SOURCE.read_bytes())
    manifest=json.loads(files['buildings_manifest.json'])
    features=json.loads(files['buildings.json'])['features']
    buildings={}; footprints=[]
    for group,f in enumerate(features):
        buildings[f['building_id']]={k:f[k] for k in ['source_feature_id','seed','floors','height_m','height_source']}
        buildings[f['building_id']]['name']=f['attributes']['nombre']
        footprints.append(dict(group=group,id=f['id'],source_feature_id=f['source_feature_id'],vertices=len(f['vertices']),
                               holes=len(f['rings'])-1,roof_triangles=len(f['triangles'])//3,
                               base_triangles=len(f['triangles'])//3,wall_triangles=2*sum(map(len,f['rings'])),
                               area_utm_m2=f['area_utm_m2'],area_local_m2=f['area_local_m2'],roof_area_m2=f['triangulated_area_local_m2'],
                               reprojection_area_relative_difference=f['area_local_m2']/f['area_utm_m2']-1,
                               merged_reprojection_vertices=f['merged_reprojection_vertices'],
                               restored_collinear_boundary_vertices=f['restored_collinear_boundary_vertices']))
    excluded=[]
    for record,geometry in read_shapes(SOURCE.parent/'buildings.zip'):
        if geometry.intersection(box(*manifest['area_bbox_m'])).is_empty:
            excluded.append(dict(source_feature_id=str(record['gid']),reason='No intersection with exact AOI; WFS BBOX matched multipart envelope'))
    return dict(dataset=source['layer'],coverage=source['coverage'],source_feature_count=source['feature_count'],
                building_count=len(buildings),footprint_count=len(features),buildings=list(buildings.values()),footprints=footprints,
                excluded_entities=excluded,invalid_footprints_discarded=0,byte_reproducible=True,
                sha256={name:digest(body) for name,body in files.items()},timings=timings,
                tolerances=dict(roof_vs_local_area_m2=1e-6,local_vs_utm_relative=0.002,reprojection_duplicate_distance_m=1e-6))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args()
    report=audit()
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ['building_count','footprint_count','byte_reproducible','timings']}))
