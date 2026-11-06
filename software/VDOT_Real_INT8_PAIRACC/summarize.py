import sys,re
s=open(sys.argv[1]).read()
m=re.search(r'S24-PAIRACC MEDIAN samples=(\d+) rounds=(\d+) warmup=(\d+) specialized=(\d+) pairacc=(\d+)',s)
if not m: raise SystemExit('Missing S24-PAIRACC MEDIAN')
n,rounds,warm,base,cand=map(int,m.groups())
assert 'S24-PAIRACC ALL PASS' in s
assert 'HIT GOOD TRAP' in s
print(f'PAIRACC — XiangShan + NEMU, {n} images, {rounds} measured rounds')
print(f'Specialized: {base:,} cycles')
print(f'Pairwise Layer0: {cand:,} cycles')
print(f'Cycle reduction: {100*(base-cand)/base:+.2f}%')
print(f'Winner: {"pairacc" if cand<base else "specialized"}')
