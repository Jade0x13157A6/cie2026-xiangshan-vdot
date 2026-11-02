# Final Hybrid verification evidence

- Selected implementation: `VIAluFix_hybrid.scala` in the local analysis workspace.
- Source SHA256: `02dd34723d9d65f488f1081d28d6b33047b556271762c3405722a39706865575`.
- Submitted patch SHA256: `eee2983b0238f4370b51e521aa589eb5066549e08b9fb0e63726c3d2b2b84cc4`.
- Independent generated-RTL testbench: 100,005 PASS (saved in the analysis workspace, not copied because the raw debug log is large).
- Full-core rebuild: generated XiangShan RTL and recompiled the Verilator simulator before Difftest.
- Full-core RTL–NEMU verification: 76/76 PASS, see `validation-difftest.log`.
- Full-core performance: Scalar 24.846, RVV 13.349, Hybrid VDOT 9.015 cycles/dot, see `benchmark-rvv-difftest.log`.
- Five-variant synthesis summary: see `../../docs/RESULTS.md`.
- Both text logs in this directory have line-ending and trailing-whitespace normalization only; numerical results are unchanged.
- No ASIC physical-area, timing signoff, power, or full-application inference result is claimed.
