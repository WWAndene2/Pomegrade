#!/usr/bin/env python3
# The same measurements for every Punica Perfetto trace, so a before and an after are measured alike
# (docs/Eden_Optimisation_Xiaomi.md 10.1). Usage: trace_metrics.py <trace>.perfetto-trace (from the full analysis
# zip). Needs the perfetto Python package (pip install perfetto), which downloads trace_processor_shell once.
# The game's frames are Eden's presents (queueBuffer on its VulkanPresent thread); SurfaceFlinger's compositions
# also count Punica's interface.
import sys
from perfetto.trace_processor import TraceProcessor
tp = TraceProcessor(trace=sys.argv[1]); q = lambda s: list(tp.query(s))
t0, t1 = [(r.a, r.b) for r in q("select min(ts) a, max(ts) b from sched")][0]
dur = (t1 - t0) / 1e9
print(f"trace length {dur:.1f} s")
# frames the game presented: Punica's surface queueBuffer calls, else SurfaceFlinger compositions
qb = q("""select count(*) n from slice s join thread_track tt on s.track_id=tt.id join thread t using(utid) join process p using(upid)
 where p.name like '%pomegrade%' and t.name='VulkanPresent' and s.name='queueBuffer' and s.depth=1""")[0].n
comp = q("select count(*) n from slice where name='Composition'")[0].n
print(f"game frames presented (VulkanPresent queueBuffer) {qb} = {qb/dur:.1f} FPS ; SurfaceFlinger compositions (game + UI) {comp/dur:.1f}/s")
# frame-to-frame intervals from Punica's queueBuffer
iv = [r.d/1e6 for r in q("""select ts - lag(ts) over (order by ts) d from slice s join thread_track tt on s.track_id=tt.id join thread t using(utid) join process p using(upid)
 where p.name like '%pomegrade%' and t.name='VulkanPresent' and s.name='queueBuffer' and s.depth=1""") if r.d]
if iv:
    iv.sort(); n=len(iv)
    print(f"frame interval ms: median {iv[n//2]:.1f}, p90 {iv[int(n*.9)]:.1f}, p99 {iv[int(n*.99)]:.1f}  -> median fps {1000/iv[n//2]:.1f}")
mx = q("select max(value) m from counter join counter_track t on track_id=t.id where t.name='gpufreq'")[0].m
g = q(f"""with c as (select ts, value, lead(ts) over (order by ts) nts from counter join counter_track t on track_id=t.id where t.name='gpufreq')
 select sum(case when value>={mx}*0.97 then nts-ts else 0 end)*1.0/sum(nts-ts) top, sum(value*(nts-ts))/sum(nts-ts)/1000 avg from c where nts is not null""")[0]
print(f"GPU clock: avg {g.avg:.0f} MHz, max seen {mx/1000:.0f} MHz, at max {g.top*100:.0f}% of the time")
for r in q("""select t.name tname, sum(s.dur)/1e9 cpu from sched s join thread t using(utid) join process p using(upid)
 where p.name like '%pomegrade%' group by t.utid order by cpu desc limit 5"""):
    print(f"  CPU {r.cpu/dur*100:5.1f}% {r.tname}")
