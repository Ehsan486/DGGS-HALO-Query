"""Preserve every supplied boundary segment; explicitly interpret edges as WGS84 geodesics."""
import argparse, csv, json
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('input',type=Path);p.add_argument('output',type=Path)
a=p.parse_args();data=json.loads(a.input.read_text(encoding='utf-8-sig'))
def geometries(x):
    if x['type']=='FeatureCollection':
        for f in x['features']:yield from geometries(f)
    elif x['type']=='Feature':yield from geometries(x['geometry'])
    elif x['type']=='GeometryCollection':
        for g in x['geometries']:yield from geometries(g)
    else:yield x
def rings(g):
    t=g['type'];c=g['coordinates']
    if t=='Polygon':yield from c
    elif t=='MultiPolygon':
        for polygon in c:yield from polygon
    elif t=='LineString':yield c
    elif t=='MultiLineString':yield from c
    elif t=='Point':yield [c,c]
    else:raise ValueError(f'Unsupported geometry {t}')
a.output.parent.mkdir(parents=True,exist_ok=True)
count=0
with a.output.open('w',newline='',encoding='utf-8') as f:
    w=csv.writer(f);w.writerow(['lat1','lon1','lat2','lon2'])
    for g in geometries(data):
        for ring in rings(g):
            for x,y in zip(ring,ring[1:]):
                w.writerow([x[1],x[0],y[1],y[0]]);count+=1
print(f'{count} independent WGS84 geodesic boundary segments; polygon interiors are not source geometry.')
