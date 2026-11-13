"""Independent point location, without ACM: test WGS84 offset samples against returned sets.
Sampling is additional evidence, not a completeness proof. PROJ and C++ share the
Karney geodesic family; this is not an independent geodesic implementation.
"""
import csv,json,re,math
from pathlib import Path
import numpy as np
from pyproj import Geod
root=Path(__file__).resolve().parents[1]
text=(root/'include/bafe_exp/pentakis_geometry.hpp').read_text()
vertices=np.array([[float(x) for x in s.split(',')] for s in re.findall(r'Vec3\{([^}]+)\}',text.split('kFaces')[0])])
faces=np.array([[int(x) for x in s.split(',')] for s in re.findall(r'std::array<std::uint8_t,3>\{([^}]+)\}',text)])
matrices=np.linalg.inv(vertices[faces].transpose(0,2,1))
def locate(lat,lon,r):
    lat,lon=np.deg2rad([lat,lon]);p=np.array([np.cos(lat)*np.cos(lon),np.cos(lat)*np.sin(lon),np.sin(lat)])
    coeff=matrices@p;sums=coeff.sum(axis=1);b=coeff/np.where(sums>0,sums,1)[:,None]
    margins=b.min(axis=1);margins[sums<=0]=-np.inf;f=int(margins.argmax())
    x,y=np.clip(b[f,1:],0,1)*(1<<r);i,j=int(np.floor(x)),int(np.floor(y));o=int(x-i+y-j>1)
    result=f<<57
    for depth in range(r,0,-1):
        parity=((i&1)<<1)|(j&1)
        digit=([0,2,1,3] if o==0 else [3,1,0,2])[parity]
        result|=digit<<(57-2*depth)
        o=int((i&1) and (j&1)) if o==0 else int((i&1) or (j&1))
        i>>=1;j>>=1
    return result|(1<<(56-2*r))
g=Geod(ellps='WGS84');rng=np.random.default_rng(20260911)
segments=list(csv.DictReader((root/'data/nose_hill_segments.csv').open()))
checks=[]
for name,point in [('nose_hill_L12',None),('nose_hill_L14',None),('nose_hill_L16',None),
    ('point_L20',(51.125,-114.115)),('point_L28',(51.125,-114.115)),
    ('dateline_L12',(0,179.999)),('polar_L12',(89.999,0))]:
    prefix=root/'results'/f'{name}_cuda_0'
    report=json.loads(Path(str(prefix)+'_report.json').read_text())
    output={int(x['public_index']) for x in csv.DictReader(Path(str(prefix)+'_cells.csv').open())}
    misses=[]
    for k in range(3000):
        if point:lat,lon=point
        else:
            s=segments[int(rng.integers(len(segments)))];a,b,c,d=[float(s[x]) for x in ['lat1','lon1','lat2','lon2']]
            azi,_,length=g.inv(b,a,d,c);lon,lat,_=g.fwd(b,a,azi,float(rng.random())*length)
        # Every sample is within d of some source point, including distances near d.
        fraction=1.-1e-7 if k%3==0 else float(rng.random())
        lon,lat,_=g.fwd(lon,lat,float(rng.uniform(-180,180)),fraction*report['distance_m'])
        index=locate(lat,lon,report['level'])
        if index not in output:misses.append({'lat':lat,'lon':lon,'index':str(index)})
    checks.append({'case':name,'samples':3000,'missing':len(misses),'examples':misses[:5]})
Path(root/'results/independent_sample_coverage.json').write_text(json.dumps(checks,indent=2))
print(json.dumps(checks,indent=2));assert not any(x['missing'] for x in checks)
