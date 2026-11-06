# VDOT Pairwise Layer-0 Accumulation

Same 32-image full INT8 MLP and same VDOT ISA. The candidate changes only Layer 0 accumulation order: `v24 += v16` then `v8 += v24`, instead of consecutive `v8 += v24` and `v8 += v16`. All dot outputs and predictions are checked against exported INT8 references outside timing. Runs 3 warm-up plus 5 measured rounds, alternating order, two modes. No RTL/NEMU modifications. Target XiangShan performance not measured by host checks.

```bash
cd ~/work/xiangshan-vdot
unzip -oq ~/下载/VDOT_PAIRACC.zip -d .
cd xs-env
nix develop -c bash ../VDOT_Real_INT8_PAIRACC/run.sh
```
