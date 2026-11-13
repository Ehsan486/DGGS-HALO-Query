import numpy as np, itertools
from scipy.spatial import ConvexHull
phi=(1+5**0.5)/2
def unit(v): v=np.asarray(v,float); return v/np.linalg.norm(v,axis=-1,keepdims=True)
# dodecahedron vertices
V=[]
for s in itertools.product([-1,1],repeat=3): V.append(s)
for a in [-1,1]:
    for b in [-1,1]:
        V+= [(0,a/phi,b*phi),(a/phi,b*phi,0),(a*phi,0,b/phi)]
dod=unit(V)
ico=[]
for a in [-1,1]:
    for b in [-1,1]:
        ico+=[(0,a,b*phi),(a,b*phi,0),(a*phi,0,b)]
ico=unit(ico)
octa=unit([(1,0,0),(-1,0,0),(0,1,0),(0,-1,0),(0,0,1),(0,0,-1)])
pent=np.vstack([dod,ico])   # spherical pentakis dodecahedron: dodecahedron + face centres, all on the unit sphere
def stats(P,name):
    h=ConvexHull(P); F=h.simplices
    val=np.bincount(F.ravel())
    out=[]
    for f in F:
        A,B,C=P[f]; n=np.cross(B-A,C-A); n/=np.linalg.norm(n)
        hf=abs(n@A)                      # plane distance from centre
        g=(A+B+C)/3; mf=max(np.linalg.norm(X-g) for X in (A,B,C))
        thc=np.degrees(np.arccos(hf))    # angular circumradius = max angle from projection centre
        out.append((mf/hf,thc,1/hf**3))
    out=np.array(out)
    E=len(set(tuple(sorted(e)) for f in F for e in ((f[0],f[1]),(f[1],f[2]),(f[0],f[2]))))
    print(f"{name:10s} V={len(P)} E={E} F={len(F)} valences={sorted(set(val))}  max m_f/h_f={out[:,0].max():.4f}  max angle from chart centre={out[:,1].max():.2f} deg  area ratio sec^3={out[:,2].max():.3f}  directed records={3*len(F)}")
stats(octa,'octahedron'); stats(ico,'icosahedron'); stats(pent,'pentakis')
