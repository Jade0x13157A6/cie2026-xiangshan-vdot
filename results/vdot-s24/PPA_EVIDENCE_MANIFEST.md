# S24 PPA evidence manifest

This manifest lists the original artifacts selected from the tested local workspace.
SHA-256 values cover the exact committed file bytes.

| Evidence file | Size (bytes) | SHA-256 |
|---|---:|---|
| [`summary.txt`](ppa-generic/summary.txt) | 600 | `0dce8f268d55a856197c48b3779b2c51a6f0192c98910eec47eb010018d83207` |
| [`summary.json`](ppa-generic/summary.json) | 255 | `fab385a567d6b2241577c4f1ac7b8800df4352170237af767ee7bd03967b1b14` |
| [`ppa-cell-counts.txt`](ppa-generic/ppa-cell-counts.txt) | 381 | `c606c0df9bdcfe63aaf16735c6e07da0a9f03189e2153605b9d1ebaf62c26347` |
| [`tool-and-inputs.txt`](ppa-generic/tool-and-inputs.txt) | 365 | `eaf6eb70cad3f0b68a3b7870d8feac90d733f4fe4d29115ccd983e0cd8734236` |
| [`equivalence.log`](ppa-generic/equivalence.log) | 346 | `624700e4b57a1f1386c88a218e9ee29e1ef4007f5597d4f37abc538a5ad82faa` |
| [`summary.txt`](ppa-fastsel/summary.txt) | 359 | `35235c7f8eee5304890187cb2a9a6ca8f293ba4b78515fcdb84c1d131329a86c` |
| [`tool-and-inputs.txt`](ppa-fastsel/tool-and-inputs.txt) | 414 | `c7fa14dc3c06b5b27d7d6d50328859fbf36d05547a6087b24982819d47f47f8b` |
| [`equivalence.log`](ppa-fastsel/equivalence.log) | 327 | `78c20c4101efbed8ba52b73a8c4d71c08642972c96d41c2e5c5d49ceb5b5d86f` |
| [`summary.txt`](chisel-fastsel-ppa/summary.txt) | 755 | `1988ea864c711f37128c39f5c3b1cae6dd2198f80420ece999b60f15e2ad59c2` |
| [`sparse-overhead-slang.txt`](chisel-fastsel-ppa/sparse-overhead-slang.txt) | 315 | `e4d3f9d5ded02d9e164f7fa2f46a68544c1884ccb59d413a32b54364ae2b0210` |
| [`summary.txt`](chisel-fastsel-ppa/nangate45/summary.txt) | 641 | `97e083ac2517622f5d5b96b38c9ca8ca5836263c0b739aeef0582eba8cfa7a57` |
| [`fastsel.sta.log`](chisel-fastsel-ppa/nangate45/fastsel.sta.log) | 21,060 | `1d864394d9315adaaf3a0787bac4416b322f6091992aa52fb0fafd2a27c41ea8` |
| [`pre-fastsel.sta.log`](chisel-fastsel-ppa/nangate45/pre-fastsel.sta.log) | 20,867 | `ac0d69a1dd4d52759ab7bfd05083ebf91c0b9732b5568e89299c9def49b5dc33` |
| [`sparse-off.sta.log`](chisel-fastsel-ppa/nangate45/sparse-off.sta.log) | 19,158 | `26047092b649cf7c643abaf26291405e5f323a2caa9c262aefe89976d6a0ab40` |
| [`fastsel.vdot-tagged.sta.log`](chisel-fastsel-ppa/nangate45/fastsel.vdot-tagged.sta.log) | 6,193 | `4bccfa02ebd2b1292ac1c42524a785b593d52b01cae6183acbbe2519e8e7ac30` |
| [`pre-fastsel.vdot-tagged.sta.log`](chisel-fastsel-ppa/nangate45/pre-fastsel.vdot-tagged.sta.log) | 6,369 | `cd33af93873d1d0b417e4300ced60f6d27fba93febc7c4cdd644e3275140b3a3` |
| [`pre-fastsel.vdot.sta.log`](chisel-fastsel-ppa/nangate45/pre-fastsel.vdot.sta.log) | 20,968 | `1c72c5ec750a6acf3ac51b701df812ee3b942bd1322326373e1eef30ae624633` |

## Not included

Large or unavailable artifacts remain outside this compact evidence set.
- `results/vdot-s24/chisel-fastsel-ppa/nangate45/fastsel.mapped.json` — large (19,614,199 bytes)
- `results/vdot-s24/chisel-fastsel-ppa/nangate45/pre-fastsel.mapped.json` — large (19,657,028 bytes)
- `results/vdot-s24/chisel-fastsel-ppa/nangate45/sparse-off.mapped.json` — large (19,339,168 bytes)
