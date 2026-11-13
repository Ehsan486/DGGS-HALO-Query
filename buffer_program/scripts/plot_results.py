"""Scientific output figure reconstructed from the actual returned public indices."""
import csv,json,re
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection
from pyproj import Transformer
root=Path(__file__).resolve().parents[1]
text=(root/'include/bafe_exp/pentakis_geometry.hpp').read_text()
vertices=np.array([[float(x) for x in s.split(',')] for s in re.findall(r'Vec3\{([^}]+)\}',text.split('kFaces')[0])])
faces=np.array([[int(x) for x in s.split(',')] for s in re.findall(r'std::array<std::uint8_t,3>\{([^}]+)\}',text)])
transform=Transformer.from_crs('EPSG:4326','EPSG:3776',always_xy=True)
origin=np.array(transform.transform(-114.115,51.115))
def coords(index,r):
    i=j=o=0
    for depth in range(1,r+1):
        d=(index>>(57-2*depth))&3;i*=2;j*=2
        if o==0:di,dj,o=[(0,0,0),(1,0,0),(0,1,0),(0,0,1)][d]
        else:di,dj,o=[(1,0,1),(0,1,1),(1,1,1),(1,1,0)][d]
        i+=di;j+=dj
    return (index>>57)&63,i,j,o
def cell(index,r):
    f,i,j,o=coords(index,r);a,b,c=vertices[faces[f]]
    xy=np.array([(i,j),(i+1,j),(i,j+1)] if o==0 else [(i+1,j),(i+1,j+1),(i,j+1)])/(1<<r)
    v=a+xy[:,0,None]*(b-a)+xy[:,1,None]*(c-a)
    # Densification is for drawing curved edges, not used by the distance algorithm.
    dense=np.concatenate([v[k]+np.linspace(0,1,5,endpoint=False)[:,None]*(v[(k+1)%3]-v[k]) for k in range(3)])
    lon=np.rad2deg(np.arctan2(dense[:,1],dense[:,0]));lat=np.rad2deg(np.arctan2(dense[:,2],np.hypot(dense[:,0],dense[:,1])))
    return (np.array(transform.transform(lon,lat)).T-origin)/1000
plt.rcParams.update({'font.family':'serif','font.size':11,'svg.fonttype':'none','pdf.fonttype':42})
fig,axes=plt.subplots(1,2,figsize=(10,5.1),layout='constrained')
segments=list(csv.DictReader((root/'data/nose_hill_segments.csv').open()))
for ax,r,label in zip(axes,[12,14],['(a)','(b)']):
    path=root/'results'/f'nose_hill_L{r}_cuda_0_cells.csv'
    ids=[int(x['public_index']) for x in csv.DictReader(path.open())]
    ax.add_collection(PolyCollection([cell(i,r) for i in ids],facecolor='#b9deed',edgecolor='#40758a',linewidth=.4))
    for s in segments:
        lon=[float(s['lon1']),float(s['lon2'])];lat=[float(s['lat1']),float(s['lat2'])]
        line=(np.array(transform.transform(lon,lat)).T-origin)/1000;ax.plot(line[:,0],line[:,1],color='#1d2733',linewidth=1)
    ax.set_title(f'{label} Level {r}: {len(ids):,} cells',pad=12)
    ax.set_xlim(-5,5);ax.set_ylim(-5,4.5);ax.set_aspect('equal')
    ax.set_xlabel('Local easting (km)');ax.set_ylabel('Local northing (km)')
    ax.spines[['top','right']].set_visible(False)
fig.suptitle('1 km WGS84 buffer of the complete Nose Hill Park boundary',fontsize=13)
out=root/'results/Figure_WGS84_Buffer_Results'
for ext in ['pdf','svg','png']:fig.savefig(str(out)+'.'+ext,dpi=180)
(root/'results/FIGURE_DESCRIPTION.md').write_text('Two panels show actual returned Pentakis cells for the same 1 km WGS84 buffer of all 1,482 Nose Hill Park boundary segments. Panel (a) contains 78 level 12 cells; panel (b) contains 941 level 14 cells. Blue triangles are returned cells and the black curve is the source boundary. This is distance to the boundary on both sides, not distance to a filled park polygon. EPSG:3776 is used only to display the already computed WGS84 result. Curved cell edges are sampled only for rendering. Neither the plotting projection nor the visual samples determine membership. The finer cells follow the requested region more closely. No unsampled cell subset is substituted for the result.\n')
