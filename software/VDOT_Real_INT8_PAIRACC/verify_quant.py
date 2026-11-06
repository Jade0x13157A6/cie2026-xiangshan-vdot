#!/usr/bin/env python3
# Exhaustive float32 equivalence for the exported model requant coefficient.
# Python float uses double; convert every float op to float32 via struct.
import struct

def f32(x):
    return struct.unpack('f', struct.pack('f',x))[0]

scale=f32(0.012518482010842779)

def reference(v):
    z=f32(f32(float(v)) * scale)
    q=0 if z<=0 else int(f32(z+f32(0.5)))
    return min(q,127)

def candidate(v):
    if v<=0:return 0
    if v>=11000:return 127
    return min(((v*210025 + 8388608)>>24),127)

for v in range(-1000,11001):
    a=reference(v);b=candidate(v)
    if a!=b:raise SystemExit(f'FAIL requant v={v}: expected {a} got {b}')
for v in (11001,13000,100000,2147483647,-2147483648):
    a=reference(v);b=candidate(v)
    if a!=b:raise SystemExit(f'FAIL requant v={v}: expected {a} got {b}')
print('U32QUANT EXHAUSTIVE PASS: v=-1000..11000 and 32-bit saturation boundaries')
