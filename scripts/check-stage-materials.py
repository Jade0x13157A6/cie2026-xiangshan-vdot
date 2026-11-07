#!/usr/bin/env python3
"""Read-only check of recorded stage deliverables; no invented PASS results."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
checks = {
  'Stage 1 | report': 'docs/STAGE1_REPORT.md',
  'Stage 1 | hello source': 'source/nexus-am/apps/hello-jade/hello.c',
  'Stage 1 | hello run log': 'results/stage1/hello-xiangshan.log',
  'Stage 1 | VADD program': 'source/nexus-am/apps/vadd-wave-jade/vadd_wave.c',
  'Stage 1 | VADD wave screenshot': 'results/waveforms/vadd_execution.png',
  'Stage 2 | report': 'docs/STAGE2_REPORT.md',
  'Stage 2 | RTL patch': 'patches/s24/xiangshan.patch',
  'Stage 2 | functional test source': 'source/nexus-am/apps/vdot-jade/vdot.c',
  'Stage 2 | 76-case run log': 'results/validation-difftest.log',
  'Stage 2 | zero test source': 'software/vdot-zero/vdot_zero.c',
  'Stage 2 | zero run log (2 PASS, Difftest)': 'results/stage2/zero-difftest.log',
  'Stage 3 | report': 'docs/STAGE3_REPORT.md',
  'Stage 3 | NEMU patch': 'patches/s24/nemu.patch',
  'Stage 3 | RVV baseline source': 'source/nexus-am/apps/vdot-bench-jade/vdot_bench.c',
  'Stage 3 | baseline timing log': 'results/benchmark-rvv-difftest.log',
  'Stage 3 | mapped area report': 'docs/S24_PPA.md',
  'Stage 3 | PairAcc results': 'results/vdot-s24/real-int8-pairacc/summary.txt',
}
for label, relative in checks.items():
    path = ROOT / relative
    print(f'{"OK" if path.is_file() and path.stat().st_size else "MISSING":7} {label}: {relative}')
print('\nPortal videos and original deployment screen recording are uploaded separately; this script does not claim they exist.')
print('Clean-room reconstruction against public upstream commits is not yet verified.')
