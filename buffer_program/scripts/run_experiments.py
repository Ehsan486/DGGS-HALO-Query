"""Run actual complete queries; preserve raw reports and never synthesize timings."""
import csv,json,subprocess,statistics,platform,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
exe=root/'build_cuda/Release/atlas_buffer.exe'
cases=[('nose_hill_L12',12,1000,None),('nose_hill_L14',14,1000,None),
       ('nose_hill_L16',16,1000,None),('point_L20',20,10,'51.125,-114.115'),
       ('point_L28',28,.1,'51.125,-114.115'),('dateline_L12',12,1000,'0,179.999'),
       ('polar_L12',12,1000,'89.999,0')]
rows=[]
for name,level,d,point in cases:
    for backend in ['cpu','cuda']:
        for repeat in range(3):
            prefix=root/'results'/f'{name}_{backend}_{repeat}'
            cmd=[str(exe),'--backend',backend,'--level',str(level),'--distance-m',str(d),
                 '--output',str(prefix),'--max-cells','200000','--threads','8']
            cmd+=['--point',point] if point else ['--input',str(root/'data/nose_hill_segments.csv')]
            if repeat==0:cmd+=['--verify']
            run=subprocess.run(cmd,cwd=root,text=True,capture_output=True)
            prefix.with_suffix('.log').write_text(run.stdout+run.stderr,encoding='utf-8')
            if run.returncode:raise RuntimeError(run.stdout+run.stderr)
            r=json.loads(Path(str(prefix)+'_report.json').read_text())
            rows.append(dict(case=name,backend=backend,repeat=repeat,level=level,distance_m=d,
                output_cells=r['output_cells'],unresolved=r['unresolved_retained'],K=r['separating_ring'],
                **r['timing_ms']))
            print(run.stdout.strip(),flush=True)
keys=list(dict.fromkeys(k for row in rows for k in row))
with (root/'results/benchmark_raw.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,keys);w.writeheader();w.writerows(rows)
summary=[]
for name,level,d,point in cases:
    for backend in ['cpu','cuda']:
        group=[r for r in rows if r['case']==name and r['backend']==backend]
        entry={k:group[0][k] for k in ['case','backend','level','distance_m','output_cells','unresolved','K']}
        for k in keys:
            if k.endswith('_ms') and all(k in r for r in group):entry[k]=statistics.median(r[k] for r in group)
        summary.append(entry)
(root/'results/benchmark_medians.json').write_text(json.dumps(summary,indent=2))
manifest={'system':platform.platform(),'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),
    'repetitions':3,'process_lifetime':'fresh process per query; context initialization included separately',
    'source_semantics':'boundary curve, not filled park polygon',
    'timing':'query_total excludes validation and output file; output_file reported separately',
    'gpu':subprocess.check_output(['nvidia-smi','--query-gpu=name,driver_version,memory.total','--format=csv'],text=True)}
(root/'results/experiment_manifest.json').write_text(json.dumps(manifest,indent=2))
