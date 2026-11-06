#include <klib.h>
#include <stdint.h>
#include "real_weights.h"

/*
 * Multi-shape, multiple-mask stress and performance benchmark for XiangShan.
 * Exercises 2:4 structured sparse INT8 GEMV, including K>8 tiled accumulation.
 * ISA extension: dense .word 0xe61101d7 / sparse .word 0xea2081d7.
 * Uses the same logical weights and activations for scalar, RVV, dense, sparse; adds optimized dense comparator.
 * VLEN=128; each sparse instruction computes two independent dot8 outputs.
 * Measured cycles include vector setup, loads, extracts/stores and K accumulation.
 */
#define MAX_ROWS 64
#define MAX_COLS 128
#define MAX_TILES (MAX_COLS / 8)
#define MAX_PAIRS (MAX_ROWS / 2)
#define ROUND_COUNT 5

static const uint8_t mask_codes[6] = {3,5,6,9,10,12};
static int8_t act[MAX_COLS] __attribute__((aligned(16)));
static int8_t dense_weights[MAX_ROWS][MAX_COLS] __attribute__((aligned(16)));
static uint8_t compressed_weights[MAX_PAIRS][MAX_TILES][10] __attribute__((aligned(16)));
static int32_t expected[MAX_ROWS];
static int32_t scalar_out[MAX_ROWS] __attribute__((aligned(16)));
static int32_t rvv_out[MAX_ROWS] __attribute__((aligned(16)));
static int32_t dense_out[MAX_ROWS] __attribute__((aligned(16)));
static int32_t dense_acc_out[MAX_ROWS] __attribute__((aligned(16)));
static int32_t sparse_out[MAX_ROWS] __attribute__((aligned(16)));
static int32_t sparse_acc_out[MAX_ROWS] __attribute__((aligned(16)));
static int32_t sparse_block2_out[MAX_ROWS] __attribute__((aligned(16)));
static int32_t sparse_block4_out[MAX_ROWS] __attribute__((aligned(16)));
static int32_t dense_block2_out[MAX_ROWS] __attribute__((aligned(16)));
static int32_t dense_block4_out[MAX_ROWS] __attribute__((aligned(16)));
static volatile int32_t checksum_sink;

struct test_case { int rows, cols, repeats, seed; };
/* Keep simulation work bounded; longer K cases remain representative of tiled GEMV. */
static const struct test_case shapes[] = {
  {32, 64, 1, 0}, /* real trained INT8 digits MLP layer 0, 64 -> 32 */
  {16, 32, 1, 0}, /* real trained INT8 digits MLP layer 1, 32 -> 10 (+6 pad) */
};

#ifndef HOST_TEST
static inline uint64_t cycle_now(void) {
  uint64_t v;
  asm volatile("csrr %0, mcycle" : "=r"(v) :: "memory");
  return v;
}
static void vector_enable(void) {
  asm volatile(
      "lui t0, 0x2\n"
      "addiw t0, t0, 512\n"
      "csrs mstatus, t0\n"
      "csrwi vcsr, 0\n"
      "csrwi vstart, 0\n"
      ::: "t0", "memory");
}
#else
/* Host mode tests packing/reference/shape loops only, NOT XiangShan cycles. */
static volatile uint64_t host_ticks;
static inline uint64_t cycle_now(void) { return ++host_ticks; }
static void vector_enable(void) {}
#endif

static int current_rows, current_cols;

static int popcount4(uint8_t v) {
  int n = 0;
  for (int i=0; i<4; ++i) n += (v >> i) & 1U;
  return n;
}
static int active_layer = 0;
static int active_sample = 0;
static int8_t real_hidden_runtime[32] __attribute__((aligned(16)));

static void init_case(const struct test_case *c) {
  current_rows = c->rows;
  current_cols = c->cols;
  const int8_t *a = active_layer == 0 ? real_input[active_sample] : real_hidden_runtime;
  for (int j=0; j<c->cols; ++j) act[j] = a[j];
  for (int j=c->cols; j<MAX_COLS; ++j) act[j] = 0;
  for (int r=0;r<c->rows;++r) {
    scalar_out[r]=0; rvv_out[r]=0; dense_out[r]=0; dense_acc_out[r]=0;
    sparse_out[r]=0; sparse_acc_out[r]=0; sparse_block2_out[r]=0; sparse_block4_out[r]=0;
    dense_block2_out[r]=0; dense_block4_out[r]=0;
    expected[r]=0;
    for (int j=0;j<c->cols;++j) {
      int8_t w = active_layer == 0 ? real_w0[r][j] : real_w1[r][j];
      dense_weights[r][j] = w;
      expected[r]+=(int32_t)act[j]*w;
    }
  }
  for (int p=0;p<c->rows/2;++p) {
    for (int t=0;t<c->cols/8;++t) {
      for (int k=0;k<10;++k) {
        compressed_weights[p][t][k] = active_layer == 0
           ? real_packed0[p][t][k] : real_packed1[p][t][k];
      }
    }
  }
}

/* Floating-point step is between integer MLP layers, excluded from GEMV timing.
 * It converts sparse int32 dot + quantized bias to ReLU int8 using exported
 * host TRAIN-only activation scale. Nearest integer rounding, saturate at 127. */
static int derive_hidden(void) {
  for (int i=0;i<32;++i) {
    int32_t d=sparse_block4_out[i];
    if (d!=real_dot0[active_sample][i]) {
      printf("S24-REAL DOT0 FAIL sample=%d row=%d got=%d expected=%d\n",
             active_sample,i,d,real_dot0[active_sample][i]);
      return 1;
    }
    int32_t v=d+real_bias0[i];
    float z=(float)v * REAL_HIDDEN_REQUANT;
    int32_t q=z<=0 ? 0 : (int32_t)(z+0.5f);
    if(q>127) q=127;
    real_hidden_runtime[i]=(int8_t)q;
    if(real_hidden_runtime[i]!=real_hidden[active_sample][i]) {
      printf("S24-REAL HIDDEN FAIL sample=%d row=%d got=%d expected=%d\n",
             active_sample,i,(int)real_hidden_runtime[i],(int)real_hidden[active_sample][i]);
      return 1;
    }
  }
  return 0;
}

static __attribute__((noinline)) void sparse_block4(int first_pair);

/* On-device END-TO-END sparse INT8 inference for held-out handwritten digits.
 * The measured layer kernels exclude ReLU/quantization, as labeled in output. */
static __attribute__((unused)) int test_real_inference(void) {
  int correct=0;
  for (active_sample=0; active_sample<REAL_TEST_COUNT; ++active_sample) {
    active_layer=0;
    init_case(&shapes[0]);
    for (int p=0;p<shapes[0].rows/2;p+=4) sparse_block4(p);
    if(derive_hidden())return 1;
    active_layer=1;
    init_case(&shapes[1]);
    for (int p=0;p<shapes[1].rows/2;p+=4) sparse_block4(p);
    int best=0;
    int32_t best_logit=sparse_block4_out[0]+real_bias1[0];
    for (int i=0;i<16;++i) {
      if(sparse_block4_out[i]!=real_dot1[active_sample][i]) {
        printf("S24-REAL DOT1 FAIL sample=%d row=%d got=%d expected=%d\n",
               active_sample,i,sparse_block4_out[i],real_dot1[active_sample][i]);
        return 1;
      }
      if(i>0 && i<10) {
        int32_t z=sparse_block4_out[i]+real_bias1[i];
        if(z>best_logit){best_logit=z;best=i;}
      }
    }
    if(best!=real_pred[active_sample]){
      printf("S24-REAL PRED FAIL sample=%d got=%d expected=%d\n",
             active_sample,best,real_pred[active_sample]);
      return 1;
    }
    correct += best==real_label[active_sample];
  }
  printf("S24-REAL INFERENCE PASS samples=%d correct=%d reference_predictions_match=yes\n",
         REAL_TEST_COUNT,correct);
  return 0;
}

/* Independent decoder for weight packing: validates all 2:4 bytes and masks. */
static __attribute__((unused)) int validate_packing(const struct test_case *c) {
  int observed[6] = {0,0,0,0,0,0};
  for (int row=0; row<c->rows; ++row) {
    for (int tile=0; tile<c->cols/8; ++tile) {
      const uint8_t *packed=compressed_weights[row/2][tile];
      int within=row%2;
      for (int g=0; g<2; ++g) {
        int nibble=within*2+g;
        uint8_t mask=(packed[8+nibble/2]>>(4*(nibble%2)))&15;
        if (popcount4(mask)!=2) return 1;
        for (int k=0; k<6; ++k) if(mask==mask_codes[k]) ++observed[k];
        int used=0;
        for (int j=0; j<4; ++j) {
          int col=tile*8+g*4+j;
          int8_t decoded = (mask & (1U<<j))
              ? (int8_t)packed[within*4+g*2+used++] : 0;
          if (decoded != dense_weights[row][col]) {
            printf("S24-SWEEP packing mismatch row=%d tile=%d col=%d\n",row,tile,col);
            return 1;
          }
        }
      }
    }
  }
  for (int k=0; k<6; ++k) {
    if (!observed[k]) {
      printf("S24-SWEEP missing mask=%d\n",mask_codes[k]);
      return 1;
    }
  }
  return 0;
}

#ifndef HOST_TEST
static __attribute__((noinline)) void dense_pair(int pair) {
  int32_t acc0=0, acc1=0;
  for (int tile=0; tile<current_cols/8; ++tile) {
    uintptr_t raw0,raw1;
    const int8_t *a = &act[tile*8];
    const int8_t *w0=&dense_weights[2*pair][tile*8];
    const int8_t *w1=&dense_weights[2*pair+1][tile*8];
    asm volatile(
        "vsetivli zero, 8, e8, m1, ta, ma\n"
        "vle8.v v1, (%[a])\n"
        "vle8.v v2, (%[w0])\n"
        ".word 0xe61101d7\n"
        "vsetivli zero, 1, e32, m1, ta, ma\n"
        "vmv.x.s %[r0], v3\n"
        "vsetivli zero, 8, e8, m1, ta, ma\n"
        "vle8.v v2, (%[w1])\n"
        ".word 0xe61101d7\n"
        "vsetivli zero, 1, e32, m1, ta, ma\n"
        "vmv.x.s %[r1], v3\n"
        : [r0] "=&r"(raw0), [r1] "=&r"(raw1)
        : [a] "r"(a), [w0] "r"(w0), [w1] "r"(w1)
        : "memory");
    acc0 += (int32_t)(uint32_t)raw0;
    acc1 += (int32_t)(uint32_t)raw1;
  }
  dense_out[2*pair]=acc0;
  dense_out[2*pair+1]=acc1;
}
static __attribute__((noinline)) void sparse_pair(int pair) {
  int32_t acc0=0,acc1=0;
  int32_t tmp[2] __attribute__((aligned(16)));
  for (int tile=0; tile<current_cols/8; ++tile) {
    const uint8_t *w=compressed_weights[pair][tile];
    const int8_t *a=&act[tile*8];
    asm volatile(
        "vsetivli zero, 10, e8, m1, ta, ma\n"
        "vle8.v v2, (%[w])\n"
        "vsetivli zero, 8, e8, m1, ta, ma\n"
        "vle8.v v1, (%[a])\n"
        ".word 0xea2081d7\n"
        "vsetivli zero, 8, e8, m1, ta, ma\n"
        "vse8.v v3, (%[d])\n"
        :
        : [w] "r"(w), [a] "r"(a), [d] "r"(tmp)
        : "memory");
    acc0 += tmp[0];
    acc1 += tmp[1];
  }
  sparse_out[2*pair]=acc0;
  sparse_out[2*pair+1]=acc1;
}
/*
 * Optimized sparse microkernel. 2x INT32 results from vdot.s24.vv are
 * accumulated in v8 (two e32 lanes) across K/8 chunks; one final store.
 * A SINGLE asm block prevents the compiler from touching the live v8 state.
 * This is a software kernel optimization, NOT another custom ISA instruction.
 */
static __attribute__((noinline)) void sparse_acc_pair(int pair) {
  const uint8_t *packed = &compressed_weights[pair][0][0];
  int32_t *dest = &sparse_acc_out[2 * pair];
  int chunks = current_cols/8;
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "mv t2, %[chunks]\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "1:\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t0, t0, 8\n"
      "addi t1, t1, 10\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "vse32.v v8, (%[dest])\n"
      :
      : [a] "r"(act), [w] "r"(packed), [chunks] "r"(chunks), [dest] "r"(dest)
      : "t0", "t1", "t2", "memory");
}
/* Dense comparator with the SAME across-tile vector accumulation strategy as
 * sparse_acc_pair: no intermediate extraction/store per tile. Two dense VDOTs
 * and two separate e32 accumulator registers are required per row pair.
 * This is an ordinary RVV software microkernel, not a new ISA instruction. */
static __attribute__((noinline)) void dense_acc_pair(int pair) {
  const int8_t *w0 = &dense_weights[2*pair][0];
  const int8_t *w1 = &dense_weights[2*pair+1][0];
  int32_t *d0 = &dense_acc_out[2*pair];
  int32_t *d1 = &dense_acc_out[2*pair+1];
  int chunks = current_cols / 8;
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "mv t3, %[chunks]\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t2)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t0, t0, 8\n"
      "addi t1, t1, 8\n"
      "addi t2, t2, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "vse32.v v8, (%[d0])\n"
      "vse32.v v9, (%[d1])\n"
      :
      : [a] "r"(act), [w0] "r"(w0), [w1] "r"(w1),
        [chunks] "r"(chunks), [d0] "r"(d0), [d1] "r"(d1)
      : "t0", "t1", "t2", "t3", "memory");
}

/* Row-blocked sparse VDOT: compute 2 row pairs per tile, reusing v1.
 * Each output pair maintains its own e32 accumulator in v8..v9.
 * The original s24 instruction is unchanged.
 */
static __attribute__((noinline)) void sparse_block2(int first_pair) {
  const uint8_t *w0 = &compressed_weights[first_pair+0][0][0];
  const uint8_t *w1 = &compressed_weights[first_pair+1][0][0];
  int32_t *dest = &sparse_block2_out[first_pair*2];
  int chunks = current_cols / 8;
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "mv t5, %[chunks]\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t2)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t2, t2, 10\n"
      "addi t0, t0, 8\n"
      "addi t5, t5, -1\n"
      "bnez t5, 1b\n"
      "mv t6, %[dest]\n"
      "vse32.v v8, (t6)\n"
      "addi t6, t6, 8\n"
      "vse32.v v9, (t6)\n"
      :
      : [a] "r"(act), [chunks] "r"(chunks), [dest] "r"(dest),
        [w0] "r"(w0), [w1] "r"(w1)
      : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");
}

/* Row-blocked sparse VDOT: compute 4 row pairs per tile, reusing v1.
 * Each output pair maintains its own e32 accumulator in v8..v11.
 * The original s24 instruction is unchanged.
 */
static __attribute__((noinline)) void sparse_block4(int first_pair) {
  const uint8_t *w0 = &compressed_weights[first_pair+0][0][0];
  const uint8_t *w1 = &compressed_weights[first_pair+1][0][0];
  const uint8_t *w2 = &compressed_weights[first_pair+2][0][0];
  const uint8_t *w3 = &compressed_weights[first_pair+3][0][0];
  int32_t *dest = &sparse_block4_out[first_pair*2];
  int chunks = current_cols / 8;
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "mv t3, %[w2]\n"
      "mv t4, %[w3]\n"
      "mv t5, %[chunks]\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t2)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t2, t2, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t3)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v10, v10, v3\n"
      "addi t3, t3, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t4)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v11, v11, v3\n"
      "addi t4, t4, 10\n"
      "addi t0, t0, 8\n"
      "addi t5, t5, -1\n"
      "bnez t5, 1b\n"
      "mv t6, %[dest]\n"
      "vse32.v v8, (t6)\n"
      "addi t6, t6, 8\n"
      "vse32.v v9, (t6)\n"
      "addi t6, t6, 8\n"
      "vse32.v v10, (t6)\n"
      "addi t6, t6, 8\n"
      "vse32.v v11, (t6)\n"
      :
      : [a] "r"(act), [chunks] "r"(chunks), [dest] "r"(dest),
        [w0] "r"(w0), [w1] "r"(w1), [w2] "r"(w2), [w3] "r"(w3)
      : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");
}

/* Dense fair row blocking: 2 row pairs, shared activation load.
 * Weight rows remain 128-byte stride (MAX_COLS), exactly as allocated.
 * Each dense dot8 is one VDOT; 4 independent e32 vector accumulators.
 * Same output rows and K-tile loop as Sparse Block2.
 */
static __attribute__((noinline)) void dense_block2(int first_pair) {
  const int8_t *weight = &dense_weights[first_pair*2][0];
  int32_t *dest = &dense_block2_out[first_pair*2];
  const int chunks = current_cols / 8;
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "mv t2, %[chunks]\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v10, v10, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v11, v11, v3\n"
      "addi t1, t1, 128\n"
      "addi t1, t1, -504\n"
      "addi t0, t0, 8\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "mv t3, %[dest]\n"
      "vse32.v v8, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v9, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v10, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v11, (t3)\n"
      :
      : [a] "r"(act), [w] "r"(weight), [chunks] "r"(chunks), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "memory");
}

/* Dense fair row blocking: 4 row pairs, shared activation load.
 * Weight rows remain 128-byte stride (MAX_COLS), exactly as allocated.
 * Each dense dot8 is one VDOT; 8 independent e32 vector accumulators.
 * Same output rows and K-tile loop as Sparse Block4.
 */
static __attribute__((noinline)) void dense_block4(int first_pair) {
  const int8_t *weight = &dense_weights[first_pair*2][0];
  int32_t *dest = &dense_block4_out[first_pair*2];
  const int chunks = current_cols / 8;
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "mv t2, %[chunks]\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v10, v10, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v11, v11, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v12, v12, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v13, v13, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v14, v14, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v15, v15, v3\n"
      "addi t1, t1, 128\n"
      "addi t1, t1, -1016\n"
      "addi t0, t0, 8\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "mv t3, %[dest]\n"
      "vse32.v v8, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v9, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v10, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v11, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v12, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v13, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v14, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v15, (t3)\n"
      :
      : [a] "r"(act), [w] "r"(weight), [chunks] "r"(chunks), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "memory");
}

static __attribute__((noinline)) int32_t rvv_dot(const int8_t *a, const int8_t *w) {
  uintptr_t raw;
  asm volatile(
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vmv.s.x v10, zero\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (%[a])\n"
      "vle8.v v2, (%[w])\n"
      "vwmul.vv v4, v1, v2\n"
      "vsetivli zero, 8, e16, m1, ta, ma\n"
      "vwredsum.vs v8, v4, v10\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vmv.x.s %[res], v8\n"
      : [res] "=r"(raw)
      : [a] "r"(a), [w] "r"(w)
      : "memory");
  return (int32_t)(uint32_t)raw;
}
static __attribute__((noinline)) void rvv_pair(int pair) {
  int32_t acc0=0,acc1=0;
  for (int tile=0; tile<current_cols/8; ++tile) {
    const int8_t *a=&act[tile*8];
    acc0 += rvv_dot(a,&dense_weights[2*pair][tile*8]);
    acc1 += rvv_dot(a,&dense_weights[2*pair+1][tile*8]);
  }
  rvv_out[2*pair]=acc0;
  rvv_out[2*pair+1]=acc1;
}
#else
static __attribute__((noinline)) void dense_pair(int pair) {
  for (int r=0;r<2;++r) {
    int32_t sum=0;
    for (int j=0;j<current_cols;++j) sum+=(int32_t)act[j]*dense_weights[2*pair+r][j];
    dense_out[2*pair+r]=sum;
  }
}
static __attribute__((noinline)) void dense_acc_pair(int pair) {
  for (int r=0; r<2; ++r) {
    int32_t sum=0;
    for (int j=0; j<current_cols; ++j)
      sum+=(int32_t)act[j]*dense_weights[2*pair+r][j];
    dense_acc_out[2*pair+r]=sum;
  }
}

static __attribute__((noinline)) void rvv_pair(int pair) {
  for (int r=0;r<2;++r) {
    int32_t sum=0;
    for (int j=0;j<current_cols;++j) sum+=(int32_t)act[j]*dense_weights[2*pair+r][j];
    rvv_out[2*pair+r]=sum;
  }
}
static __attribute__((noinline)) void sparse_pair(int pair) {
  for (int r=0;r<2;++r) {
    int32_t sum=0;
    for (int tile=0;tile<current_cols/8;++tile) {
      const uint8_t *w=compressed_weights[pair][tile];
      int within=r;
      for (int g=0;g<2;++g) {
        int n=within*2+g;
        uint8_t mask=(w[8+n/2] >> (4*(n%2))) & 15;
        int used=0;
        for (int j=0;j<4;++j) if(mask & (1U<<j)) {
          sum+=(int32_t)act[tile*8+g*4+j]*(int8_t)w[within*4+g*2+used++];
        }
      }
    }
    sparse_out[2*pair+r]=sum;
  }
}
static __attribute__((noinline)) void sparse_acc_pair(int pair) {
  // Independent host-only emulation of repeated VDOT+VADD on 2 INT32 lanes.
  int32_t sum[2]={0,0};
  for (int tile=0;tile<current_cols/8;++tile) {
    const uint8_t *w=compressed_weights[pair][tile];
    for (int r=0;r<2;++r) {
      for (int g=0;g<2;++g) {
        int nibble=r*2+g;
        uint8_t mask=(w[8+nibble/2]>>(4*(nibble%2)))&15;
        int selected=0;
        for (int j=0;j<4;++j) if (mask & (1U<<j)) {
          int8_t weight=(int8_t)w[r*4+g*2+selected++];
          sum[r]+=(int32_t)act[tile*8+g*4+j]*weight;
        }
      }
    }
  }
  sparse_acc_out[2*pair]=sum[0];
  sparse_acc_out[2*pair+1]=sum[1];
}

static __attribute__((noinline)) void sparse_block2(int first_pair) {
  for (int i=0;i<2;++i) {
    sparse_acc_pair(first_pair+i);
    sparse_block2_out[2*(first_pair+i)] = sparse_acc_out[2*(first_pair+i)];
    sparse_block2_out[2*(first_pair+i)+1] = sparse_acc_out[2*(first_pair+i)+1];
  }
}
static __attribute__((noinline)) void sparse_block4(int first_pair) {
  for (int i=0;i<4;++i) {
    sparse_acc_pair(first_pair+i);
    sparse_block4_out[2*(first_pair+i)] = sparse_acc_out[2*(first_pair+i)];
    sparse_block4_out[2*(first_pair+i)+1] = sparse_acc_out[2*(first_pair+i)+1];
  }
}

static __attribute__((noinline)) void dense_block2(int first_pair) {
  for (int i=0; i<2; ++i) {
    dense_acc_pair(first_pair+i);
    for (int r=0;r<2;++r) dense_block2_out[2*(first_pair+i)+r]=dense_acc_out[2*(first_pair+i)+r];
  }
}
static __attribute__((noinline)) void dense_block4(int first_pair) {
  for (int i=0; i<4; ++i) {
    dense_acc_pair(first_pair+i);
    for (int r=0;r<2;++r) dense_block4_out[2*(first_pair+i)+r]=dense_acc_out[2*(first_pair+i)+r];
  }
}
#endif

static __attribute__((noinline)) void scalar_pair(int pair) {
  for (int r=0;r<2;++r) {
    int32_t sum=0;
    const volatile int8_t *a=act;
    const volatile int8_t *w=dense_weights[2*pair+r];
    for (int j=0;j<current_cols;++j) sum+=(int32_t)a[j]*(int32_t)w[j];
    scalar_out[2*pair+r]=sum;
  }
}

static __attribute__((unused)) int check_all(const struct test_case *c) {
  for (int p=0;p<c->rows/2;++p) {
    scalar_pair(p);rvv_pair(p);dense_pair(p);dense_acc_pair(p);sparse_pair(p);sparse_acc_pair(p);
  }
  for (int p=0;p<c->rows/2;p+=2) { sparse_block2(p); dense_block2(p); }
  for (int p=0;p<c->rows/2;p+=4) { sparse_block4(p); dense_block4(p); }
  for (int r=0;r<c->rows;++r) {
    if (dense_block2_out[r]!=expected[r] || dense_block4_out[r]!=expected[r] ||
        sparse_block2_out[r]!=expected[r] || sparse_block4_out[r]!=expected[r]) {
      printf("S24-BLOCK FAIL rows=%d cols=%d row=%d expected=%d block2=%d block4=%d\n",
          c->rows,c->cols,r,expected[r],sparse_block2_out[r],sparse_block4_out[r]);
      return 1;
    }
    if (scalar_out[r]!=expected[r] || rvv_out[r]!=expected[r] ||
        dense_out[r]!=expected[r] || dense_acc_out[r]!=expected[r] || sparse_out[r]!=expected[r] || sparse_acc_out[r]!=expected[r]) {
      printf("S24-SWEEP FAIL rows=%d cols=%d seed=%d row=%d expected=%d scalar=%d rvv=%d dense=%d dense_acc=%d sparse=%d sparse_acc=%d\n",
          c->rows,c->cols,c->seed,r,expected[r],scalar_out[r],rvv_out[r],
          dense_out[r],dense_acc_out[r],sparse_out[r],sparse_acc_out[r]);
      return 1;
    }
  }
  printf("S24-SWEEP correctness PASS rows=%d cols=%d seed=%d masks=6 six_kernels=yes\n",
       c->rows,c->cols,c->seed);
  return 0;
}

static __attribute__((unused)) uint64_t time_kernel(int which,const struct test_case *c) {
  uint64_t start=cycle_now();
  for(int rep=0; rep<c->repeats; ++rep) {
    if (which == 6) {
      for(int p=0; p<c->rows/2; p+=2) sparse_block2(p);
    } else if (which == 7) {
      for(int p=0; p<c->rows/2; p+=4) sparse_block4(p);
    } else if (which == 8) {
      for(int p=0; p<c->rows/2; p+=2) dense_block2(p);
    } else if (which == 9) {
      for(int p=0; p<c->rows/2; p+=4) dense_block4(p);
    } else {
    for(int p=0; p<c->rows/2; ++p) {
      switch(which) {
        case 0: scalar_pair(p); break;
        case 1: rvv_pair(p); break;
        case 2: dense_pair(p); break;
        case 3: sparse_pair(p); break;
        case 4: sparse_acc_pair(p); break;
        default: dense_acc_pair(p); break;
      }
    }
    }
  }
  uint64_t stop=cycle_now();
  int32_t sum=0;
  for(int r=0;r<c->rows;++r) {
    switch(which) {
      case 0: sum+=scalar_out[r]; break;
      case 1: sum+=rvv_out[r]; break;
      case 2: sum+=dense_out[r]; break;
      case 3: sum+=sparse_out[r]; break;
      case 4: sum+=sparse_acc_out[r]; break;
      case 6: sum+=sparse_block2_out[r]; break;
      case 7: sum+=sparse_block4_out[r]; break;
      case 8: sum+=dense_block2_out[r]; break;
      case 9: sum+=dense_block4_out[r]; break;
      default: sum+=dense_acc_out[r]; break;
    }
  }
  checksum_sink=sum;
  return stop-start;
}


/* -------------------------------------------------------------------------
 * Measured complete inference: 64->32 ReLU INT8 ->10 (16 padded) logits.
 * Both modes use identical pruned INT8 weights, input samples, bias and
 * requantization. Weights are copied into two static layer buffers ONCE
 * BEFORE timing, preserving the dense 128-byte row stride and sparse
 * 160-byte pair stride required by the original Block4 assembly kernels.
 * Timing includes per-image input copy, two GEMVs, bias, ReLU, requant,
 * argmax and loop overhead, EXCLUDES model initialization and verification.
 * All comparison kernels are the ORIGINAL Block4 instruction sequences.
 * ------------------------------------------------------------------------- */
#define E2E_ROUNDS 5
static int8_t e2e_dense_weights[2][MAX_ROWS][MAX_COLS] __attribute__((aligned(16)));
static uint8_t e2e_packed_weights[2][MAX_PAIRS][MAX_TILES][10] __attribute__((aligned(16)));
static int32_t rvv_block4_out[MAX_ROWS] __attribute__((aligned(16)));
static int e2e_layer;
/* Tile-major sparse kernels: no padding in compressed weight storage.
 * Layer0 32 rows = 2 blocks x 8 tiles x 8 pairs x 10 bytes.
 * Layer1 10 rows = 4 tiles x 5 pairs x 10 bytes.
 */
/* Whole-register loads always read VLEN=128/8 = 16 bytes.  The packed
 * records still occupy exactly 10 bytes each.  Only the END of each matrix
 * gets a 16-byte guard so the final vl1re8.v cannot read outside an object.
 * Both arrays and guards are in the same struct; no per-tile padding. */
static struct {
  uint8_t tiles[2][8][8][10];
  uint8_t guard[16];
} e2e_tile_w0_storage __attribute__((aligned(16)));
static struct {
  uint8_t tiles[4][5][10];
  uint8_t guard[16];
} e2e_tile_w1_storage __attribute__((aligned(16)));
#define e2e_tile_w0 e2e_tile_w0_storage.tiles
#define e2e_tile_w1 e2e_tile_w1_storage.tiles
/* RACE3: 16-byte-aligned sparse tile records. Payload remains 10 bytes,
 * extra 6 bytes per record are zero. Offline preparation is not timed.
 * This trades +60% sparse weight storage for aligned whole-register loads.
 */
static uint8_t e2e_tile_w0_aligned[2][8][8][16] __attribute__((aligned(16)));
static uint8_t e2e_tile_w1_aligned[4][5][16] __attribute__((aligned(16)));
static int32_t sparse_tile_out[MAX_ROWS] __attribute__((aligned(16)));

static void e2e_prepare_weights(void) {
  for (int i=0; i<2; ++i) {
    int rows = i==0 ? 32 : 16;
    int cols = i==0 ? 64 : 32;
    for(int r=0; r<rows; ++r) {
      for(int c=0; c<cols; ++c) {
        e2e_dense_weights[i][r][c] = i==0 ? real_w0[r][c] : real_w1[r][c];
      }
    }
    for(int p=0; p<rows/2; ++p) {
      for(int tile=0; tile<cols/8; ++tile) {
        for(int b=0; b<10; ++b) {
          e2e_packed_weights[i][p][tile][b] = i==0
            ? real_packed0[p][tile][b] : real_packed1[p][tile][b];
        }
      }
    }
  }
  /* Transform only the physical ordering of the 10-byte sparse tiles;
   * copy exactly once before any timed run, no repacking in inference.
   * A 10-byte tile still carries eight signed weights and four 2:4 masks.
   */
  for(int block=0;block<2;++block)
    for(int tile=0;tile<8;++tile)
      for(int pair=0;pair<8;++pair)
        for(int b=0;b<10;++b)
          e2e_tile_w0[block][tile][pair][b]=real_packed0[block*8+pair][tile][b];
  for(int tile=0;tile<4;++tile)
    for(int pair=0;pair<5;++pair)
      for(int b=0;b<10;++b)
        e2e_tile_w1[tile][pair][b]=real_packed1[pair][tile][b];
  for(int block=0;block<2;++block)
    for(int tile=0;tile<8;++tile)
      for(int pair=0;pair<8;++pair)
        for(int b=0;b<16;++b)
          e2e_tile_w0_aligned[block][tile][pair][b] =
            b < 10 ? e2e_tile_w0[block][tile][pair][b] : 0;
  for(int tile=0;tile<4;++tile)
    for(int pair=0;pair<5;++pair)
      for(int b=0;b<16;++b)
        e2e_tile_w1_aligned[tile][pair][b] =
          b < 10 ? e2e_tile_w1[tile][pair][b] : 0;
}

/* Standard RVV baseline with the same 8-row (Block4) tiling as Dense VDOT.
 * VLEN=128, 8 signed INT8 lanes; widen products to INT16, widen reduction
 * to INT32. Maintain eight independent INT32 accumulators in v8..v15.
 * One activation vector load per tile, reused across eight weight rows.
 * No custom VDOT instructions. Weight layout and input are IDENTICAL to Dense.
 */
#ifndef HOST_TEST
static __attribute__((noinline)) void rvv_block4_e2e(int first_pair) {
  const int8_t *weight = &e2e_dense_weights[e2e_layer][first_pair*2][0];
  int32_t *dest = &rvv_block4_out[first_pair*2];
  const int chunks = current_cols / 8;
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "mv t2, %[chunks]\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vmv.v.i v0, 0\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vle8.v v2, (t1)\n"
      "vwmul.vv v4, v1, v2\n"
      "vsetivli zero, 8, e16, m1, ta, ma\n"
      "vwredsum.vs v6, v4, v0\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v6\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vwmul.vv v4, v1, v2\n"
      "vsetivli zero, 8, e16, m1, ta, ma\n"
      "vwredsum.vs v6, v4, v0\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v6\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vwmul.vv v4, v1, v2\n"
      "vsetivli zero, 8, e16, m1, ta, ma\n"
      "vwredsum.vs v6, v4, v0\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v10, v10, v6\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vwmul.vv v4, v1, v2\n"
      "vsetivli zero, 8, e16, m1, ta, ma\n"
      "vwredsum.vs v6, v4, v0\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v11, v11, v6\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vwmul.vv v4, v1, v2\n"
      "vsetivli zero, 8, e16, m1, ta, ma\n"
      "vwredsum.vs v6, v4, v0\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v12, v12, v6\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vwmul.vv v4, v1, v2\n"
      "vsetivli zero, 8, e16, m1, ta, ma\n"
      "vwredsum.vs v6, v4, v0\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v13, v13, v6\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vwmul.vv v4, v1, v2\n"
      "vsetivli zero, 8, e16, m1, ta, ma\n"
      "vwredsum.vs v6, v4, v0\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v14, v14, v6\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vwmul.vv v4, v1, v2\n"
      "vsetivli zero, 8, e16, m1, ta, ma\n"
      "vwredsum.vs v6, v4, v0\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v15, v15, v6\n"
      "addi t1, t1, 128\n"
      "addi t1, t1, -1016\n"
      "addi t0, t0, 8\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "mv t3, %[dest]\n"
      "vse32.v v8, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v9, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v10, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v11, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v12, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v13, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v14, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v15, (t3)\n"
      :
      : [a] "r"(act), [w] "r"(weight), [chunks] "r"(chunks), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "memory");
}
#else
static __attribute__((noinline)) void rvv_block4_e2e(int first_pair) {
  for(int row=first_pair*2; row<first_pair*2+8; ++row) {
    int32_t sum=0;
    for(int c=0;c<current_cols;++c)
      sum+=(int32_t)act[c]*e2e_dense_weights[e2e_layer][row][c];
    rvv_block4_out[row]=sum;
  }
}
#endif

#ifndef HOST_TEST
static __attribute__((noinline)) void dense_block4_e2e(int first_pair) {
  const int8_t *weight = &e2e_dense_weights[e2e_layer][first_pair*2][0];
  int32_t *dest = &dense_block4_out[first_pair*2];
  const int chunks = current_cols / 8;
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "mv t2, %[chunks]\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v10, v10, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v11, v11, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v12, v12, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v13, v13, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v14, v14, v3\n"
      "addi t1, t1, 128\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      ".word 0xe61101d7\n"
      "vsetivli zero, 1, e32, m1, ta, ma\n"
      "vadd.vv v15, v15, v3\n"
      "addi t1, t1, 128\n"
      "addi t1, t1, -1016\n"
      "addi t0, t0, 8\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "mv t3, %[dest]\n"
      "vse32.v v8, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v9, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v10, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v11, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v12, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v13, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v14, (t3)\n"
      "addi t3, t3, 4\n"
      "vse32.v v15, (t3)\n"
      :
      : [a] "r"(act), [w] "r"(weight), [chunks] "r"(chunks), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "memory");
}

static __attribute__((noinline)) void sparse_block4_e2e(int first_pair) {
  const uint8_t *w0 = &e2e_packed_weights[e2e_layer][first_pair+0][0][0];
  const uint8_t *w1 = &e2e_packed_weights[e2e_layer][first_pair+1][0][0];
  const uint8_t *w2 = &e2e_packed_weights[e2e_layer][first_pair+2][0][0];
  const uint8_t *w3 = &e2e_packed_weights[e2e_layer][first_pair+3][0][0];
  int32_t *dest = &sparse_block4_out[first_pair*2];
  int chunks = current_cols / 8;
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "mv t3, %[w2]\n"
      "mv t4, %[w3]\n"
      "mv t5, %[chunks]\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t2)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t2, t2, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t3)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v10, v10, v3\n"
      "addi t3, t3, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t4)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v11, v11, v3\n"
      "addi t4, t4, 10\n"
      "addi t0, t0, 8\n"
      "addi t5, t5, -1\n"
      "bnez t5, 1b\n"
      "mv t6, %[dest]\n"
      "vse32.v v8, (t6)\n"
      "addi t6, t6, 8\n"
      "vse32.v v9, (t6)\n"
      "addi t6, t6, 8\n"
      "vse32.v v10, (t6)\n"
      "addi t6, t6, 8\n"
      "vse32.v v11, (t6)\n"
      :
      : [a] "r"(act), [chunks] "r"(chunks), [dest] "r"(dest),
        [w0] "r"(w0), [w1] "r"(w1), [w2] "r"(w2), [w3] "r"(w3)
      : "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");
}
#else
/* Host implementation is for functional testing only; no cycle claim. */
static __attribute__((noinline)) void dense_block4_e2e(int first_pair) {
  for (int row=first_pair*2;row<first_pair*2+8;++row) {
    int32_t sum=0;
    for(int c=0;c<current_cols;++c)
      sum+=(int32_t)act[c]*e2e_dense_weights[e2e_layer][row][c];
    dense_block4_out[row]=sum;
  }
}
static __attribute__((noinline)) void sparse_block4_e2e(int first_pair) {
  for (int row=first_pair*2;row<first_pair*2+8;++row) {
    int32_t sum=0;
    const int within=row%2;
    for(int tile=0;tile<current_cols/8;++tile) {
      const uint8_t *w=e2e_packed_weights[e2e_layer][row/2][tile];
      for(int g=0;g<2;++g) {
        const int nibble=within*2+g;
        const int mask=(w[8+nibble/2] >> ((nibble%2)*4)) & 15;
        int selected=0;
        for(int j=0;j<4;++j) if(mask & (1<<j)) {
          const int8_t weight=(int8_t)w[within*4+g*2+selected++];
          sum+=(int32_t)act[tile*8+g*4+j]*weight;
        }
      }
    }
    sparse_block4_out[row]=sum;
  }
}
#endif


/* Optimized packed-weight sparse VDOT: tile-major, Block8 on layer 0,
 * exact 5-pair tail on layer 1. Same 10-byte tile, same custom instruction. */
#ifndef HOST_TEST
static __attribute__((noinline)) void sparse_tiled_layer0(int block) {
  const uint8_t *weights = &e2e_tile_w0[block][0][0][0];
  int32_t *dest = &sparse_tile_out[block*16];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t2, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v10, v10, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v11, v11, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v12, v12, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v13, v13, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v14, v14, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v15, v15, v3\n"
      "addi t1, t1, 10\n"
      "addi t0, t0, 8\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "mv t3, %[dest]\n"
      "vse32.v v8, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v9, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v10, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v11, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v12, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v13, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v14, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v15, (t3)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "memory");
}
static __attribute__((noinline)) void sparse_tiled_layer1(void) {
  const uint8_t *weights = &e2e_tile_w1[0][0][0];
  int32_t *dest = &sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t2, 4\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v10, v10, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v11, v11, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 10, e8, m1, ta, ma\n"
      "vle8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v12, v12, v3\n"
      "addi t1, t1, 10\n"
      "addi t0, t0, 8\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "mv t3, %[dest]\n"
      "vse32.v v8, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v9, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v10, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v11, (t3)\n"
      "addi t3, t3, 8\n"
      "vse32.v v12, (t3)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "memory");
}
#else
/* Independent host scalar decode of the new tile-major layout. */
static int32_t sparse_tile_dot8(const uint8_t *w, const int8_t *a, int row_in_pair) {
  int32_t sum=0;
  for(int g=0;g<2;++g) {
    int nibble=row_in_pair*2+g;
    unsigned mask=(w[8+nibble/2] >> ((nibble%2)*4)) & 15;
    int selected=0;
    for(int j=0;j<4;++j) if(mask&(1U<<j)) {
      int8_t weight=(int8_t)w[row_in_pair*4+g*2+selected++];
      sum+=(int32_t)a[g*4+j]*weight;
    }
  }
  return sum;
}
static __attribute__((noinline)) void sparse_tiled_layer0(int block) {
  for(int pair=0;pair<8;++pair) {
    int32_t sum0=0,sum1=0;
    for(int tile=0;tile<8;++tile) {
      const uint8_t *w=e2e_tile_w0[block][tile][pair];
      sum0+=sparse_tile_dot8(w,act+tile*8,0);
      sum1+=sparse_tile_dot8(w,act+tile*8,1);
    }
    sparse_tile_out[block*16+pair*2]=sum0;
    sparse_tile_out[block*16+pair*2+1]=sum1;
  }
}
static __attribute__((noinline)) void sparse_tiled_layer1(void) {
  for(int pair=0;pair<5;++pair) {
    int32_t sum0=0,sum1=0;
    for(int tile=0;tile<4;++tile) {
      const uint8_t *w=e2e_tile_w1[tile][pair];
      sum0+=sparse_tile_dot8(w,act+tile*8,0);
      sum1+=sparse_tile_dot8(w,act+tile*8,1);
    }
    sparse_tile_out[pair*2]=sum0;
    sparse_tile_out[pair*2+1]=sum1;
  }
}
#endif

/* RACE candidates: one-register whole loads + VDOT are supported by
 * standard RVV. The 10-byte packed weights are unchanged, with 16-byte
 * tail guards only at each array end. The first candidate isolates the
 * load/VL-switch change, the second batches dots before e32 adds, and
 * the third also fuses both layer-0 16-row blocks. All predictions must
 * pass the independent reference before timing. */
#ifndef HOST_TEST
static __attribute__((noinline)) void sparse_tiled_layer0_whole(int block) {
  const uint8_t *weights = &e2e_tile_w0[block][0][0][0];
  int32_t *dest = &sparse_tile_out[block * 16];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t2, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v10, v10, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v11, v11, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v12, v12, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v13, v13, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v14, v14, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v15, v15, v3\n"
      "addi t1, t1, 10\n"
      "addi t0, t0, 8\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v13, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v14, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v15, (t4)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t2", "t4", "memory");
}

static __attribute__((noinline)) void sparse_tiled_layer1_whole(void) {
  const uint8_t *weights = &e2e_tile_w1[0][0][0];
  int32_t *dest = &sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t2, 4\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v9, v9, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v10, v10, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v11, v11, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      ".word 0xea2081d7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v12, v12, v3\n"
      "addi t1, t1, 10\n"
      "addi t0, t0, 8\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t2", "t4", "memory");
}

static __attribute__((noinline)) void sparse_tiled_layer0_batch(int block) {
  const uint8_t *weights = &e2e_tile_w0[block][0][0][0];
  int32_t *dest = &sparse_tile_out[block * 16];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t2, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v24, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v25, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v26, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v27, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v28, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v29, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v30, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v31, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "vadd.vv v13, v13, v29\n"
      "vadd.vv v14, v14, v30\n"
      "vadd.vv v15, v15, v31\n"
      "addi t0, t0, 8\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v13, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v14, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v15, (t4)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t2", "t4", "memory");
}

static __attribute__((noinline)) void sparse_tiled_layer1_batch(void) {
  const uint8_t *weights = &e2e_tile_w1[0][0][0];
  int32_t *dest = &sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t2, 4\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v24, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v25, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v26, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v27, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v28, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "addi t0, t0, 8\n"
      "addi t2, t2, -1\n"
      "bnez t2, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t2", "t4", "memory");
}

static __attribute__((noinline)) void sparse_tiled_layer0_fused(void) {
  const uint8_t *w0 = &e2e_tile_w0[0][0][0][0];
  const uint8_t *w1 = &e2e_tile_w0[1][0][0][0];
  int32_t *dest = &sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "li t3, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "vmv.v.i v16, 0\n"
      "vmv.v.i v17, 0\n"
      "vmv.v.i v18, 0\n"
      "vmv.v.i v19, 0\n"
      "vmv.v.i v20, 0\n"
      "vmv.v.i v21, 0\n"
      "vmv.v.i v22, 0\n"
      "vmv.v.i v23, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v24, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v25, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v26, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v27, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v28, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v29, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v30, v3\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v2, (t1)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v31, v3\n"
      "addi t1, t1, 10\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "vadd.vv v13, v13, v29\n"
      "vadd.vv v14, v14, v30\n"
      "vadd.vv v15, v15, v31\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t2)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v24, v3\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v2, (t2)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v25, v3\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v2, (t2)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v26, v3\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v2, (t2)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v27, v3\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v2, (t2)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v28, v3\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v2, (t2)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v29, v3\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v2, (t2)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v30, v3\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v2, (t2)\n"
      ".word 0xea2081d7\n"
      "vmv1r.v v31, v3\n"
      "addi t2, t2, 10\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v16, v16, v24\n"
      "vadd.vv v17, v17, v25\n"
      "vadd.vv v18, v18, v26\n"
      "vadd.vv v19, v19, v27\n"
      "vadd.vv v20, v20, v28\n"
      "vadd.vv v21, v21, v29\n"
      "vadd.vv v22, v22, v30\n"
      "vadd.vv v23, v23, v31\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v13, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v14, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v15, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v16, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v17, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v18, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v19, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v20, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v21, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v22, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v23, (t4)\n"
      :
      : [a] "r"(act), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "t4", "memory");
}

/* RACE2: direct VDOT destination and optional weight-register prefetch.
 * Each .word changes ONLY vd and (when prefetching) vs2 of existing sparse VDOT.
 * No changes to semantic encoding, sparse masks, quantization, weights or NEMU.
 */
static __attribute__((noinline)) void sparse_tiled_layer0_direct(void) {
  const uint8_t *w0=&e2e_tile_w0[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "li t3, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "vmv.v.i v16, 0\n"
      "vmv.v.i v17, 0\n"
      "vmv.v.i v18, 0\n"
      "vmv.v.i v19, 0\n"
      "vmv.v.i v20, 0\n"
      "vmv.v.i v21, 0\n"
      "vmv.v.i v22, 0\n"
      "vmv.v.i v23, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208cd7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208dd7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208e57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208ed7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208f57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "vadd.vv v13, v13, v29\n"
      "vadd.vv v14, v14, v30\n"
      "vadd.vv v15, v15, v31\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208cd7\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208dd7\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208e57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208ed7\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208f57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v16, v16, v24\n"
      "vadd.vv v17, v17, v25\n"
      "vadd.vv v18, v18, v26\n"
      "vadd.vv v19, v19, v27\n"
      "vadd.vv v20, v20, v28\n"
      "vadd.vv v21, v21, v29\n"
      "vadd.vv v22, v22, v30\n"
      "vadd.vv v23, v23, v31\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v13, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v14, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v15, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v16, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v17, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v18, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v19, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v20, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v21, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v22, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v23, (t4)\n"
      :
      : [a] "r"(act), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "t4", "memory");
}
static __attribute__((noinline)) void sparse_tiled_layer1_direct(void) {
  const uint8_t *weights=&e2e_tile_w1[0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t3, 4\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208cd7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208dd7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208e57\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t3", "t4", "memory");
}
static __attribute__((noinline)) void sparse_tiled_layer0_prefetch2(void) {
  const uint8_t *w0=&e2e_tile_w0[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "li t3, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "vmv.v.i v16, 0\n"
      "vmv.v.i v17, 0\n"
      "vmv.v.i v18, 0\n"
      "vmv.v.i v19, 0\n"
      "vmv.v.i v20, 0\n"
      "vmv.v.i v21, 0\n"
      "vmv.v.i v22, 0\n"
      "vmv.v.i v23, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea408cd7\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea408dd7\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208e57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea408ed7\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208f57\n"
      ".word 0xea408fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "vadd.vv v13, v13, v29\n"
      "vadd.vv v14, v14, v30\n"
      "vadd.vv v15, v15, v31\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea408cd7\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea408dd7\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208e57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea408ed7\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208f57\n"
      ".word 0xea408fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v16, v16, v24\n"
      "vadd.vv v17, v17, v25\n"
      "vadd.vv v18, v18, v26\n"
      "vadd.vv v19, v19, v27\n"
      "vadd.vv v20, v20, v28\n"
      "vadd.vv v21, v21, v29\n"
      "vadd.vv v22, v22, v30\n"
      "vadd.vv v23, v23, v31\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v13, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v14, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v15, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v16, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v17, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v18, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v19, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v20, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v21, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v22, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v23, (t4)\n"
      :
      : [a] "r"(act), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "t4", "memory");
}
static __attribute__((noinline)) void sparse_tiled_layer1_prefetch2(void) {
  const uint8_t *weights=&e2e_tile_w1[0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t3, 4\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea408cd7\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea408dd7\n"
      ".word 0xea208e57\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t3", "t4", "memory");
}
static __attribute__((noinline)) void sparse_tiled_layer0_prefetch4(void) {
  const uint8_t *w0=&e2e_tile_w0[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "li t3, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "vmv.v.i v16, 0\n"
      "vmv.v.i v17, 0\n"
      "vmv.v.i v18, 0\n"
      "vmv.v.i v19, 0\n"
      "vmv.v.i v20, 0\n"
      "vmv.v.i v21, 0\n"
      "vmv.v.i v22, 0\n"
      "vmv.v.i v23, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v5, (t1)\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v6, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea408cd7\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea508d57\n"
      "vl1re8.v v5, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea608dd7\n"
      "vl1re8.v v6, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208e57\n"
      ".word 0xea408ed7\n"
      ".word 0xea508f57\n"
      ".word 0xea608fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "vadd.vv v13, v13, v29\n"
      "vadd.vv v14, v14, v30\n"
      "vadd.vv v15, v15, v31\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v5, (t2)\n"
      "addi t2, t2, 10\n"
      "vl1re8.v v6, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea408cd7\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea508d57\n"
      "vl1re8.v v5, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea608dd7\n"
      "vl1re8.v v6, (t2)\n"
      "addi t2, t2, 10\n"
      ".word 0xea208e57\n"
      ".word 0xea408ed7\n"
      ".word 0xea508f57\n"
      ".word 0xea608fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v16, v16, v24\n"
      "vadd.vv v17, v17, v25\n"
      "vadd.vv v18, v18, v26\n"
      "vadd.vv v19, v19, v27\n"
      "vadd.vv v20, v20, v28\n"
      "vadd.vv v21, v21, v29\n"
      "vadd.vv v22, v22, v30\n"
      "vadd.vv v23, v23, v31\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v13, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v14, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v15, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v16, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v17, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v18, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v19, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v20, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v21, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v22, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v23, (t4)\n"
      :
      : [a] "r"(act), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "t4", "memory");
}
static __attribute__((noinline)) void sparse_tiled_layer1_prefetch4(void) {
  const uint8_t *weights=&e2e_tile_w1[0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t3, 4\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v5, (t1)\n"
      "addi t1, t1, 10\n"
      "vl1re8.v v6, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 10\n"
      ".word 0xea408cd7\n"
      ".word 0xea508d57\n"
      ".word 0xea608dd7\n"
      ".word 0xea208e57\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t3", "t4", "memory");
}

/* RACE3: aligned-record variants of direct/prefetch2/prefetch4. */
static __attribute__((noinline)) void sparse_tiled_layer0_align16_direct(void) {
  const uint8_t *w0=&e2e_tile_w0_aligned[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0_aligned[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "li t3, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "vmv.v.i v16, 0\n"
      "vmv.v.i v17, 0\n"
      "vmv.v.i v18, 0\n"
      "vmv.v.i v19, 0\n"
      "vmv.v.i v20, 0\n"
      "vmv.v.i v21, 0\n"
      "vmv.v.i v22, 0\n"
      "vmv.v.i v23, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208cd7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208dd7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208e57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208ed7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208f57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "vadd.vv v13, v13, v29\n"
      "vadd.vv v14, v14, v30\n"
      "vadd.vv v15, v15, v31\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208cd7\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208dd7\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208e57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208ed7\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208f57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v16, v16, v24\n"
      "vadd.vv v17, v17, v25\n"
      "vadd.vv v18, v18, v26\n"
      "vadd.vv v19, v19, v27\n"
      "vadd.vv v20, v20, v28\n"
      "vadd.vv v21, v21, v29\n"
      "vadd.vv v22, v22, v30\n"
      "vadd.vv v23, v23, v31\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v13, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v14, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v15, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v16, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v17, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v18, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v19, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v20, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v21, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v22, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v23, (t4)\n"
      :
      : [a] "r"(act), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "t4", "memory");
}

static __attribute__((noinline)) void sparse_tiled_layer0_align16_direct_input(const int8_t *input) {
  const uint8_t *w0=&e2e_tile_w0_aligned[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0_aligned[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "li t3, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "vmv.v.i v16, 0\n"
      "vmv.v.i v17, 0\n"
      "vmv.v.i v18, 0\n"
      "vmv.v.i v19, 0\n"
      "vmv.v.i v20, 0\n"
      "vmv.v.i v21, 0\n"
      "vmv.v.i v22, 0\n"
      "vmv.v.i v23, 0\n"
      "1:\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (t0)\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208cd7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208dd7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208e57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208ed7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208f57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "vadd.vv v13, v13, v29\n"
      "vadd.vv v14, v14, v30\n"
      "vadd.vv v15, v15, v31\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208cd7\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208dd7\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208e57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208ed7\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208f57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v16, v16, v24\n"
      "vadd.vv v17, v17, v25\n"
      "vadd.vv v18, v18, v26\n"
      "vadd.vv v19, v19, v27\n"
      "vadd.vv v20, v20, v28\n"
      "vadd.vv v21, v21, v29\n"
      "vadd.vv v22, v22, v30\n"
      "vadd.vv v23, v23, v31\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v13, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v14, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v15, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v16, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v17, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v18, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v19, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v20, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v21, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v22, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v23, (t4)\n"
      :
      : [a] "r"(input), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "t4", "memory");
}


static __attribute__((noinline)) void sparse_tiled_layer1_align16_direct(void) {
  const uint8_t *weights=&e2e_tile_w1_aligned[0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t3, 4\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208cd7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208dd7\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208e57\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t3", "t4", "memory");
}

static __attribute__((noinline)) void sparse_tiled_layer0_align16_prefetch2(void) {
  const uint8_t *w0=&e2e_tile_w0_aligned[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0_aligned[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "li t3, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "vmv.v.i v16, 0\n"
      "vmv.v.i v17, 0\n"
      "vmv.v.i v18, 0\n"
      "vmv.v.i v19, 0\n"
      "vmv.v.i v20, 0\n"
      "vmv.v.i v21, 0\n"
      "vmv.v.i v22, 0\n"
      "vmv.v.i v23, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea408cd7\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea408dd7\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208e57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea408ed7\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208f57\n"
      ".word 0xea408fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "vadd.vv v13, v13, v29\n"
      "vadd.vv v14, v14, v30\n"
      "vadd.vv v15, v15, v31\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea408cd7\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea408dd7\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208e57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea408ed7\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208f57\n"
      ".word 0xea408fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v16, v16, v24\n"
      "vadd.vv v17, v17, v25\n"
      "vadd.vv v18, v18, v26\n"
      "vadd.vv v19, v19, v27\n"
      "vadd.vv v20, v20, v28\n"
      "vadd.vv v21, v21, v29\n"
      "vadd.vv v22, v22, v30\n"
      "vadd.vv v23, v23, v31\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v13, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v14, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v15, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v16, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v17, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v18, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v19, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v20, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v21, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v22, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v23, (t4)\n"
      :
      : [a] "r"(act), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "t4", "memory");
}

static __attribute__((noinline)) void sparse_tiled_layer1_align16_prefetch2(void) {
  const uint8_t *weights=&e2e_tile_w1_aligned[0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t3, 4\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea408cd7\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208d57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea408dd7\n"
      ".word 0xea208e57\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t3", "t4", "memory");
}

static __attribute__((noinline)) void sparse_tiled_layer0_align16_prefetch4(void) {
  const uint8_t *w0=&e2e_tile_w0_aligned[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0_aligned[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w0]\n"
      "mv t2, %[w1]\n"
      "li t3, 8\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "vmv.v.i v13, 0\n"
      "vmv.v.i v14, 0\n"
      "vmv.v.i v15, 0\n"
      "vmv.v.i v16, 0\n"
      "vmv.v.i v17, 0\n"
      "vmv.v.i v18, 0\n"
      "vmv.v.i v19, 0\n"
      "vmv.v.i v20, 0\n"
      "vmv.v.i v21, 0\n"
      "vmv.v.i v22, 0\n"
      "vmv.v.i v23, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 16\n"
      "vl1re8.v v5, (t1)\n"
      "addi t1, t1, 16\n"
      "vl1re8.v v6, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea408cd7\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea508d57\n"
      "vl1re8.v v5, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea608dd7\n"
      "vl1re8.v v6, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208e57\n"
      ".word 0xea408ed7\n"
      ".word 0xea508f57\n"
      ".word 0xea608fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "vadd.vv v13, v13, v29\n"
      "vadd.vv v14, v14, v30\n"
      "vadd.vv v15, v15, v31\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 16\n"
      "vl1re8.v v5, (t2)\n"
      "addi t2, t2, 16\n"
      "vl1re8.v v6, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea408cd7\n"
      "vl1re8.v v4, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea508d57\n"
      "vl1re8.v v5, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea608dd7\n"
      "vl1re8.v v6, (t2)\n"
      "addi t2, t2, 16\n"
      ".word 0xea208e57\n"
      ".word 0xea408ed7\n"
      ".word 0xea508f57\n"
      ".word 0xea608fd7\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v16, v16, v24\n"
      "vadd.vv v17, v17, v25\n"
      "vadd.vv v18, v18, v26\n"
      "vadd.vv v19, v19, v27\n"
      "vadd.vv v20, v20, v28\n"
      "vadd.vv v21, v21, v29\n"
      "vadd.vv v22, v22, v30\n"
      "vadd.vv v23, v23, v31\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v13, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v14, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v15, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v16, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v17, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v18, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v19, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v20, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v21, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v22, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v23, (t4)\n"
      :
      : [a] "r"(act), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
      : "t0", "t1", "t2", "t3", "t4", "memory");
}

static __attribute__((noinline)) void sparse_tiled_layer1_align16_prefetch4(void) {
  const uint8_t *weights=&e2e_tile_w1_aligned[0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
      "mv t0, %[a]\n"
      "mv t1, %[w]\n"
      "li t3, 4\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vmv.v.i v8, 0\n"
      "vmv.v.i v9, 0\n"
      "vmv.v.i v10, 0\n"
      "vmv.v.i v11, 0\n"
      "vmv.v.i v12, 0\n"
      "1:\n"
      "vl1re8.v v1, (t0)\n"
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      "vl1re8.v v4, (t1)\n"
      "addi t1, t1, 16\n"
      "vl1re8.v v5, (t1)\n"
      "addi t1, t1, 16\n"
      "vl1re8.v v6, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea208c57\n"
      "vl1re8.v v2, (t1)\n"
      "addi t1, t1, 16\n"
      ".word 0xea408cd7\n"
      ".word 0xea508d57\n"
      ".word 0xea608dd7\n"
      ".word 0xea208e57\n"
      "vsetivli zero, 2, e32, m1, ta, ma\n"
      "vadd.vv v8, v8, v24\n"
      "vadd.vv v9, v9, v25\n"
      "vadd.vv v10, v10, v26\n"
      "vadd.vv v11, v11, v27\n"
      "vadd.vv v12, v12, v28\n"
      "addi t0, t0, 8\n"
      "addi t3, t3, -1\n"
      "bnez t3, 1b\n"
      "mv t4, %[dest]\n"
      "vse32.v v8, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v9, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v10, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v11, (t4)\n"
      "addi t4, t4, 8\n"
      "vse32.v v12, (t4)\n"
      :
      : [a] "r"(act), [w] "r"(weights), [dest] "r"(dest)
      : "t0", "t1", "t3", "t4", "memory");
}

/* RACE6 candidate: paired 16-byte weight records through vl2re8.v. */
static __attribute__((noinline)) void sparse_tiled_layer0_align16_pairload_input(const int8_t *input) {
  const uint8_t *w0=&e2e_tile_w0_aligned[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0_aligned[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
    "mv t0, %[a]\n"
    "mv t1, %[w0]\n"
    "mv t2, %[w1]\n"
    "li t3, 8\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vmv.v.i v8, 0\n"
    "vmv.v.i v9, 0\n"
    "vmv.v.i v10, 0\n"
    "vmv.v.i v11, 0\n"
    "vmv.v.i v12, 0\n"
    "vmv.v.i v13, 0\n"
    "vmv.v.i v14, 0\n"
    "vmv.v.i v15, 0\n"
    "vmv.v.i v16, 0\n"
    "vmv.v.i v17, 0\n"
    "vmv.v.i v18, 0\n"
    "vmv.v.i v19, 0\n"
    "vmv.v.i v20, 0\n"
    "vmv.v.i v21, 0\n"
    "vmv.v.i v22, 0\n"
    "vmv.v.i v23, 0\n"
    "1:\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vle8.v v1, (t0)\n"
    "vl2re8.v v2, (t1)\n"
    "addi t1, t1, 32\n"
    ".word 0xea208c57\n"
    ".word 0xea308cd7\n"
    "vl2re8.v v2, (t1)\n"
    "addi t1, t1, 32\n"
    ".word 0xea208d57\n"
    ".word 0xea308dd7\n"
    "vl2re8.v v2, (t1)\n"
    "addi t1, t1, 32\n"
    ".word 0xea208e57\n"
    ".word 0xea308ed7\n"
    "vl2re8.v v2, (t1)\n"
    "addi t1, t1, 32\n"
    ".word 0xea208f57\n"
    ".word 0xea308fd7\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "vadd.vv v13, v13, v29\n"
    "vadd.vv v14, v14, v30\n"
    "vadd.vv v15, v15, v31\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vl2re8.v v2, (t2)\n"
    "addi t2, t2, 32\n"
    ".word 0xea208c57\n"
    ".word 0xea308cd7\n"
    "vl2re8.v v2, (t2)\n"
    "addi t2, t2, 32\n"
    ".word 0xea208d57\n"
    ".word 0xea308dd7\n"
    "vl2re8.v v2, (t2)\n"
    "addi t2, t2, 32\n"
    ".word 0xea208e57\n"
    ".word 0xea308ed7\n"
    "vl2re8.v v2, (t2)\n"
    "addi t2, t2, 32\n"
    ".word 0xea208f57\n"
    ".word 0xea308fd7\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v16, v16, v24\n"
    "vadd.vv v17, v17, v25\n"
    "vadd.vv v18, v18, v26\n"
    "vadd.vv v19, v19, v27\n"
    "vadd.vv v20, v20, v28\n"
    "vadd.vv v21, v21, v29\n"
    "vadd.vv v22, v22, v30\n"
    "vadd.vv v23, v23, v31\n"
    "addi t0, t0, 8\n"
    "addi t3, t3, -1\n"
    "bnez t3, 1b\n"
    "mv t4, %[dest]\n"
    "vse32.v v8, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v9, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v10, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v11, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v12, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v13, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v14, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v15, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v16, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v17, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v18, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v19, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v20, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v21, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v22, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v23, (t4)\n"
    :
    : [a] "r"(input), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
    : "t0", "t1", "t2", "t3", "t4", "memory");
}

/* RACE6 candidate: paired 16-byte weight records through vl2re8.v. */
static __attribute__((noinline)) void sparse_tiled_layer1_align16_pairload(void) {
  const uint8_t *w0=&e2e_tile_w1_aligned[0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
    "mv t0, %[a]\n"
    "mv t1, %[w0]\n"
    "li t3, 4\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vmv.v.i v8, 0\n"
    "vmv.v.i v9, 0\n"
    "vmv.v.i v10, 0\n"
    "vmv.v.i v11, 0\n"
    "vmv.v.i v12, 0\n"
    "1:\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vl1re8.v v1, (t0)\n"
    "vl2re8.v v2, (t1)\n"
    "addi t1, t1, 32\n"
    ".word 0xea208c57\n"
    ".word 0xea308cd7\n"
    "vl2re8.v v2, (t1)\n"
    "addi t1, t1, 32\n"
    ".word 0xea208d57\n"
    ".word 0xea308dd7\n"
    "vl1re8.v v2, (t1)\n"
    "addi t1, t1, 16\n"
    ".word 0xea208e57\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "addi t0, t0, 8\n"
    "addi t3, t3, -1\n"
    "bnez t3, 1b\n"
    "mv t4, %[dest]\n"
    "vse32.v v8, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v9, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v10, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v11, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v12, (t4)\n"
    :
    : [a] "r"(act), [w0] "r"(w0), [dest] "r"(dest)
    : "t0", "t1", "t3", "t4", "memory");
}

/* QuadLoad: one vl4re8 loads four independently addressed 16B sparse records. */
static __attribute__((noinline)) void sparse_tiled_layer0_align16_quadload_input(const int8_t *input) {
  const uint8_t *w0=&e2e_tile_w0_aligned[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0_aligned[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
    "mv t0, %[a]\n"
    "mv t1, %[w0]\n"
    "mv t2, %[w1]\n"
    "li t3, 8\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vmv.v.i v8, 0\n"
    "vmv.v.i v9, 0\n"
    "vmv.v.i v10, 0\n"
    "vmv.v.i v11, 0\n"
    "vmv.v.i v12, 0\n"
    "vmv.v.i v13, 0\n"
    "vmv.v.i v14, 0\n"
    "vmv.v.i v15, 0\n"
    "vmv.v.i v16, 0\n"
    "vmv.v.i v17, 0\n"
    "vmv.v.i v18, 0\n"
    "vmv.v.i v19, 0\n"
    "vmv.v.i v20, 0\n"
    "vmv.v.i v21, 0\n"
    "vmv.v.i v22, 0\n"
    "vmv.v.i v23, 0\n"
    "1:\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408e57\n"
    ".word 0xea508ed7\n"
    ".word 0xea608f57\n"
    ".word 0xea708fd7\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "vadd.vv v13, v13, v29\n"
    "vadd.vv v14, v14, v30\n"
    "vadd.vv v15, v15, v31\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vl4re8.v v4, (t2)\n"
    "addi t2, t2, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl4re8.v v4, (t2)\n"
    "addi t2, t2, 64\n"
    ".word 0xea408e57\n"
    ".word 0xea508ed7\n"
    ".word 0xea608f57\n"
    ".word 0xea708fd7\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v16, v16, v24\n"
    "vadd.vv v17, v17, v25\n"
    "vadd.vv v18, v18, v26\n"
    "vadd.vv v19, v19, v27\n"
    "vadd.vv v20, v20, v28\n"
    "vadd.vv v21, v21, v29\n"
    "vadd.vv v22, v22, v30\n"
    "vadd.vv v23, v23, v31\n"
    "addi t0, t0, 8\n"
    "addi t3, t3, -1\n"
    "bnez t3, 1b\n"
    "mv t4, %[dest]\n"
    "vse32.v v8, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v9, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v10, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v11, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v12, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v13, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v14, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v15, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v16, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v17, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v18, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v19, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v20, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v21, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v22, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v23, (t4)\n"
    :
    : [a] "r"(input), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
    : "t0", "t1", "t2", "t3", "t4", "memory");
}

/* QuadLoad: one vl4re8 loads four independently addressed 16B sparse records. */
static __attribute__((noinline)) void sparse_tiled_layer1_align16_quadload(void) {
  const uint8_t *w0=&e2e_tile_w1_aligned[0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
    "mv t0, %[a]\n"
    "mv t1, %[w0]\n"
    "li t3, 4\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vmv.v.i v8, 0\n"
    "vmv.v.i v9, 0\n"
    "vmv.v.i v10, 0\n"
    "vmv.v.i v11, 0\n"
    "vmv.v.i v12, 0\n"
    "1:\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl1re8.v v4, (t1)\n"
    "addi t1, t1, 16\n"
    ".word 0xea408e57\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "addi t0, t0, 8\n"
    "addi t3, t3, -1\n"
    "bnez t3, 1b\n"
    "mv t4, %[dest]\n"
    "vse32.v v8, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v9, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v10, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v11, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v12, (t4)\n"
    :
    : [a] "r"(act), [w0] "r"(w0), [dest] "r"(dest)
    : "t0", "t1", "t3", "t4", "memory");
}




/* Unroll two input tiles per loop iteration. No change to data layout/ISA. */
static __attribute__((noinline)) void sparse_tiled_layer0_align16_unroll2_input(const int8_t *input) {
  const uint8_t *w0=&e2e_tile_w0_aligned[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0_aligned[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
    "mv t0, %[a]\n"
    "mv t1, %[w0]\n"
    "mv t2, %[w1]\n"
    "li t3, 4\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vmv.v.i v8, 0\n"
    "vmv.v.i v9, 0\n"
    "vmv.v.i v10, 0\n"
    "vmv.v.i v11, 0\n"
    "vmv.v.i v12, 0\n"
    "vmv.v.i v13, 0\n"
    "vmv.v.i v14, 0\n"
    "vmv.v.i v15, 0\n"
    "vmv.v.i v16, 0\n"
    "vmv.v.i v17, 0\n"
    "vmv.v.i v18, 0\n"
    "vmv.v.i v19, 0\n"
    "vmv.v.i v20, 0\n"
    "vmv.v.i v21, 0\n"
    "vmv.v.i v22, 0\n"
    "vmv.v.i v23, 0\n"
    "1:\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408e57\n"
    ".word 0xea508ed7\n"
    ".word 0xea608f57\n"
    ".word 0xea708fd7\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "vadd.vv v13, v13, v29\n"
    "vadd.vv v14, v14, v30\n"
    "vadd.vv v15, v15, v31\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vl4re8.v v4, (t2)\n"
    "addi t2, t2, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl4re8.v v4, (t2)\n"
    "addi t2, t2, 64\n"
    ".word 0xea408e57\n"
    ".word 0xea508ed7\n"
    ".word 0xea608f57\n"
    ".word 0xea708fd7\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v16, v16, v24\n"
    "vadd.vv v17, v17, v25\n"
    "vadd.vv v18, v18, v26\n"
    "vadd.vv v19, v19, v27\n"
    "vadd.vv v20, v20, v28\n"
    "vadd.vv v21, v21, v29\n"
    "vadd.vv v22, v22, v30\n"
    "vadd.vv v23, v23, v31\n"
    "addi t0, t0, 8\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408e57\n"
    ".word 0xea508ed7\n"
    ".word 0xea608f57\n"
    ".word 0xea708fd7\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "vadd.vv v13, v13, v29\n"
    "vadd.vv v14, v14, v30\n"
    "vadd.vv v15, v15, v31\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vl4re8.v v4, (t2)\n"
    "addi t2, t2, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl4re8.v v4, (t2)\n"
    "addi t2, t2, 64\n"
    ".word 0xea408e57\n"
    ".word 0xea508ed7\n"
    ".word 0xea608f57\n"
    ".word 0xea708fd7\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v16, v16, v24\n"
    "vadd.vv v17, v17, v25\n"
    "vadd.vv v18, v18, v26\n"
    "vadd.vv v19, v19, v27\n"
    "vadd.vv v20, v20, v28\n"
    "vadd.vv v21, v21, v29\n"
    "vadd.vv v22, v22, v30\n"
    "vadd.vv v23, v23, v31\n"
    "addi t0, t0, 8\n"
    "addi t3, t3, -1\n"
    "bnez t3, 1b\n"
    "mv t4, %[dest]\n"
    "vse32.v v8, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v9, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v10, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v11, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v12, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v13, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v14, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v15, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v16, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v17, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v18, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v19, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v20, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v21, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v22, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v23, (t4)\n"
    :
    : [a] "r"(input), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
    : "t0", "t1", "t2", "t3", "t4", "memory");
}


/* Unroll two input tiles per loop iteration. No change to data layout/ISA. */
static __attribute__((noinline)) void sparse_tiled_layer1_align16_unroll2(void) {
  const uint8_t *w0=&e2e_tile_w1_aligned[0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
    "mv t0, %[a]\n"
    "mv t1, %[w0]\n"
    "li t3, 2\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vmv.v.i v8, 0\n"
    "vmv.v.i v9, 0\n"
    "vmv.v.i v10, 0\n"
    "vmv.v.i v11, 0\n"
    "vmv.v.i v12, 0\n"
    "1:\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl1re8.v v4, (t1)\n"
    "addi t1, t1, 16\n"
    ".word 0xea408e57\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "addi t0, t0, 8\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl1re8.v v4, (t1)\n"
    "addi t1, t1, 16\n"
    ".word 0xea408e57\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "addi t0, t0, 8\n"
    "addi t3, t3, -1\n"
    "bnez t3, 1b\n"
    "mv t4, %[dest]\n"
    "vse32.v v8, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v9, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v10, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v11, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v12, (t4)\n"
    :
    : [a] "r"(act), [w0] "r"(w0), [dest] "r"(dest)
    : "t0", "t1", "t3", "t4", "memory");
}

/* Reduce vsetivli changes by accumulating two input tiles together.
 * v24..v31 = first tile, v16..v23 = second tile,
 * v8..v15 = persistent pair accumulators.
 * No intermediate stores; only two vtype switches for two input tiles.
 */
static __attribute__((noinline)) void sparse_tiled_layer0_align16_vsetcut_input(const int8_t *input) {
  const uint8_t *w0=&e2e_tile_w0_aligned[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0_aligned[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
    "mv t0, %[a]\n"
    "mv t1, %[w0]\n"
    "mv t2, %[w1]\n"
    "li t5, 2\n"
    "mv t4, %[dest]\n"
    "2:\n"
    "li t3, 4\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vmv.v.i v8, 0\n"
    "vmv.v.i v9, 0\n"
    "vmv.v.i v10, 0\n"
    "vmv.v.i v11, 0\n"
    "vmv.v.i v12, 0\n"
    "vmv.v.i v13, 0\n"
    "vmv.v.i v14, 0\n"
    "vmv.v.i v15, 0\n"
    "1:\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408e57\n"
    ".word 0xea508ed7\n"
    ".word 0xea608f57\n"
    ".word 0xea708fd7\n"
    "addi t0, t0, 8\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408857\n"
    ".word 0xea5088d7\n"
    ".word 0xea608957\n"
    ".word 0xea7089d7\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408a57\n"
    ".word 0xea508ad7\n"
    ".word 0xea608b57\n"
    ".word 0xea708bd7\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "vadd.vv v13, v13, v29\n"
    "vadd.vv v14, v14, v30\n"
    "vadd.vv v15, v15, v31\n"
    "vadd.vv v8, v8, v16\n"
    "vadd.vv v9, v9, v17\n"
    "vadd.vv v10, v10, v18\n"
    "vadd.vv v11, v11, v19\n"
    "vadd.vv v12, v12, v20\n"
    "vadd.vv v13, v13, v21\n"
    "vadd.vv v14, v14, v22\n"
    "vadd.vv v15, v15, v23\n"
    "addi t0, t0, 8\n"
    "addi t3, t3, -1\n"
    "bnez t3, 1b\n"
    "vse32.v v8, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v9, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v10, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v11, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v12, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v13, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v14, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v15, (t4)\n"
    "addi t4, t4, 8\n"
    "mv t0, %[a]\n"
    "mv t1, t2\n"
    "addi t5, t5, -1\n"
    "bnez t5, 2b\n"
    :
    : [a] "r"(input), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
    : "t0", "t1", "t2", "t3", "t4", "t5", "memory");
}


/* Reduce vsetivli changes by accumulating two input tiles together.
 * v24..v28 = first tile, v16..v20 = second tile,
 * v8..v12 = persistent pair accumulators.
 * No intermediate stores; only two vtype switches for two input tiles.
 */
/* Layer 0 pairwise reduction: VDOT(tile0)+VDOT(tile1) first, then
 * update long-lived v8..v15 accumulator once per two input tiles.
 * Same vector instructions and dot products as VSetCut, different dependencies.
 */
static __attribute__((noinline)) void sparse_tiled_layer0_align16_pairacc_input(const int8_t *input) {
  const uint8_t *w0=&e2e_tile_w0_aligned[0][0][0][0];
  const uint8_t *w1=&e2e_tile_w0_aligned[1][0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
    "mv t0, %[a]\n"
    "mv t1, %[w0]\n"
    "mv t2, %[w1]\n"
    "li t5, 2\n"
    "mv t4, %[dest]\n"
    "2:\n"
    "li t3, 4\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vmv.v.i v8, 0\n"
    "vmv.v.i v9, 0\n"
    "vmv.v.i v10, 0\n"
    "vmv.v.i v11, 0\n"
    "vmv.v.i v12, 0\n"
    "vmv.v.i v13, 0\n"
    "vmv.v.i v14, 0\n"
    "vmv.v.i v15, 0\n"
    "1:\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408e57\n"
    ".word 0xea508ed7\n"
    ".word 0xea608f57\n"
    ".word 0xea708fd7\n"
    "addi t0, t0, 8\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408857\n"
    ".word 0xea5088d7\n"
    ".word 0xea608957\n"
    ".word 0xea7089d7\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408a57\n"
    ".word 0xea508ad7\n"
    ".word 0xea608b57\n"
    ".word 0xea708bd7\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v24, v24, v16\n"
    "vadd.vv v25, v25, v17\n"
    "vadd.vv v26, v26, v18\n"
    "vadd.vv v27, v27, v19\n"
    "vadd.vv v28, v28, v20\n"
    "vadd.vv v29, v29, v21\n"
    "vadd.vv v30, v30, v22\n"
    "vadd.vv v31, v31, v23\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "vadd.vv v13, v13, v29\n"
    "vadd.vv v14, v14, v30\n"
    "vadd.vv v15, v15, v31\n"
    "addi t0, t0, 8\n"
    "addi t3, t3, -1\n"
    "bnez t3, 1b\n"
    "vse32.v v8, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v9, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v10, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v11, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v12, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v13, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v14, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v15, (t4)\n"
    "addi t4, t4, 8\n"
    "mv t0, %[a]\n"
    "mv t1, t2\n"
    "addi t5, t5, -1\n"
    "bnez t5, 2b\n"
    :
    : [a] "r"(input), [w0] "r"(w0), [w1] "r"(w1), [dest] "r"(dest)
    : "t0", "t1", "t2", "t3", "t4", "t5", "memory");
}


/* Reduce vsetivli changes by accumulating two input tiles together.
 * v24..v28 = first tile, v16..v20 = second tile,
 * v8..v12 = persistent pair accumulators.
 * No intermediate stores; only two vtype switches for two input tiles.
 */

static __attribute__((noinline)) void sparse_tiled_layer1_align16_vsetcut(void) {
  const uint8_t *w0=&e2e_tile_w1_aligned[0][0][0];
  int32_t *dest=&sparse_tile_out[0];
  asm volatile(
    "mv t0, %[a]\n"
    "mv t1, %[w0]\n"
    "li t3, 2\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vmv.v.i v8, 0\n"
    "vmv.v.i v9, 0\n"
    "vmv.v.i v10, 0\n"
    "vmv.v.i v11, 0\n"
    "vmv.v.i v12, 0\n"
    "1:\n"
    "vsetivli zero, 8, e8, m1, ta, ma\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408c57\n"
    ".word 0xea508cd7\n"
    ".word 0xea608d57\n"
    ".word 0xea708dd7\n"
    "vl1re8.v v4, (t1)\n"
    "addi t1, t1, 16\n"
    ".word 0xea408e57\n"
    "addi t0, t0, 8\n"
    "vle8.v v1, (t0)\n"
    "vl4re8.v v4, (t1)\n"
    "addi t1, t1, 64\n"
    ".word 0xea408857\n"
    ".word 0xea5088d7\n"
    ".word 0xea608957\n"
    ".word 0xea7089d7\n"
    "vl1re8.v v4, (t1)\n"
    "addi t1, t1, 16\n"
    ".word 0xea408a57\n"
    "vsetivli zero, 2, e32, m1, ta, ma\n"
    "vadd.vv v8, v8, v24\n"
    "vadd.vv v9, v9, v25\n"
    "vadd.vv v10, v10, v26\n"
    "vadd.vv v11, v11, v27\n"
    "vadd.vv v12, v12, v28\n"
    "vadd.vv v8, v8, v16\n"
    "vadd.vv v9, v9, v17\n"
    "vadd.vv v10, v10, v18\n"
    "vadd.vv v11, v11, v19\n"
    "vadd.vv v12, v12, v20\n"
    "addi t0, t0, 8\n"
    "addi t3, t3, -1\n"
    "bnez t3, 1b\n"
    "mv t4, %[dest]\n"
    "vse32.v v8, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v9, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v10, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v11, (t4)\n"
    "addi t4, t4, 8\n"
    "vse32.v v12, (t4)\n"
    "addi t4, t4, 8\n"
    :
    : [a] "r"(act), [w0] "r"(w0), [dest] "r"(dest)
    : "t0", "t1", "t3", "t4", "memory");
}


#else
static __attribute__((noinline)) void sparse_tiled_layer0_whole(int block) { sparse_tiled_layer0(block); }
static __attribute__((noinline)) void sparse_tiled_layer1_whole(void) { sparse_tiled_layer1(); }
static __attribute__((noinline)) void sparse_tiled_layer0_batch(int block) { sparse_tiled_layer0(block); }
static __attribute__((noinline)) void sparse_tiled_layer1_batch(void) { sparse_tiled_layer1(); }
static __attribute__((noinline)) void sparse_tiled_layer0_fused(void) {
  sparse_tiled_layer0(0); sparse_tiled_layer0(1);
}
static __attribute__((noinline)) void sparse_tiled_layer0_direct(void) { sparse_tiled_layer0_fused(); }
static __attribute__((noinline)) void sparse_tiled_layer1_direct(void) { sparse_tiled_layer1(); }

static __attribute__((noinline)) void sparse_tiled_layer0_prefetch2(void) { sparse_tiled_layer0_fused(); }
static __attribute__((noinline)) void sparse_tiled_layer1_prefetch2(void) { sparse_tiled_layer1(); }

static __attribute__((noinline)) void sparse_tiled_layer0_prefetch4(void) { sparse_tiled_layer0_fused(); }
static __attribute__((noinline)) void sparse_tiled_layer1_prefetch4(void) { sparse_tiled_layer1(); }
static __attribute__((noinline)) void sparse_tiled_layer0_align16_direct(void) { sparse_tiled_layer0_direct(); }
static __attribute__((noinline)) void sparse_tiled_layer1_align16_direct(void) { sparse_tiled_layer1_direct(); }
static __attribute__((noinline)) void sparse_tiled_layer0_align16_prefetch2(void) { sparse_tiled_layer0_prefetch2(); }
static __attribute__((noinline)) void sparse_tiled_layer1_align16_prefetch2(void) { sparse_tiled_layer1_prefetch2(); }
static __attribute__((noinline)) void sparse_tiled_layer0_align16_prefetch4(void) { sparse_tiled_layer0_prefetch4(); }
static __attribute__((noinline)) void sparse_tiled_layer1_align16_prefetch4(void) { sparse_tiled_layer1_prefetch4(); }
static __attribute__((noinline)) void sparse_tiled_layer0_align16_pairload_input(const int8_t *input) {
  for(int j=0;j<64;++j)act[j]=input[j];
  sparse_tiled_layer0_align16_direct();
}
static __attribute__((noinline)) void sparse_tiled_layer1_align16_pairload(void) { sparse_tiled_layer1_align16_direct(); }
static __attribute__((noinline)) void sparse_tiled_layer0_align16_quadload_input(const int8_t *input) {
  for(int j=0;j<64;++j)act[j]=input[j];
  sparse_tiled_layer0_align16_direct();
}
static __attribute__((noinline)) void sparse_tiled_layer1_align16_quadload(void) { sparse_tiled_layer1_align16_direct(); }
static __attribute__((noinline)) void sparse_tiled_layer0_align16_unroll2_input(const int8_t *input) {
  sparse_tiled_layer0_align16_quadload_input(input);
}
static __attribute__((noinline)) void sparse_tiled_layer1_align16_unroll2(void) {
  sparse_tiled_layer1_align16_quadload();
}
static __attribute__((noinline)) void sparse_tiled_layer0_align16_vsetcut_input(const int8_t *input) {
  sparse_tiled_layer0_align16_quadload_input(input);
}
static __attribute__((noinline)) void sparse_tiled_layer0_align16_pairacc_input(const int8_t *input) {
  sparse_tiled_layer0_align16_vsetcut_input(input);
}
static __attribute__((noinline)) void sparse_tiled_layer1_align16_vsetcut(void) {
  sparse_tiled_layer1_align16_quadload();
}

#endif

#ifdef HOST_TEST
static __attribute__((noinline)) void sparse_tiled_layer0_align16_direct_input(const int8_t *input) {
  for (int j=0;j<64;++j) act[j]=input[j];
  sparse_tiled_layer0_align16_direct();
}
#endif

/* Fast exact-to-reference requantization for the exported model.
 * REAL_HIDDEN_REQUANT is exactly 13441618/2^30 as IEEE-754 binary32.
 * For 0<v<11000 use exact 64-bit fixed-point arithmetic, with a conservative
 * +/- 1/1024 guard band about half-integers that reverts to the original
 * float32 computation. Above 11000 the output is unconditionally saturated.
 * This avoids reliance on an approximate fixed-point result at tie boundaries.
 */
static inline int32_t requant_fast_guarded(int32_t v) {
  if (v <= 0) return 0;
  if (v >= 11000) return 127;
  const uint64_t p=(uint64_t)(uint32_t)v * 13441618ULL;
  const uint64_t frac=p & ((1ULL<<30)-1);
  const uint64_t half=1ULL<<29;
  if (frac > half-(1ULL<<20) && frac < half+(1ULL<<20)) {
    float z=(float)v * REAL_HIDDEN_REQUANT;
    int32_t q=z<=0 ? 0 : (int32_t)(z+0.5f);
    return q>127 ? 127 : q;
  }
  int32_t q=(int32_t)((p+half)>>30);
  return q>127 ? 127 : q;
}

/* Exact-to-reference integer requantization for this exported model.
 * Original float32 rounding + ReLU + saturation was exhaustively compared
 * against the equation below for every integer v in [0, 11000].
 * The nonpositive and >=11000 regions saturate to 0 and 127 respectively.
 * No 64-bit multiplication, floating point, or fallback path.
 * 210025 / 2^24 approximates the exported constant sufficiently accurately
 * after integer output rounding and saturation; not a generic Q-format rule.
 * Upper bound: (10999*210025+2^23) < 2^32.
 */
static inline int32_t requant_u32_exact(int32_t v) {
  if(v <= 0) return 0;
  if(v >= 11000) return 127;
  uint32_t q=((uint32_t)v * 210025U + 8388608U) >> 24;
  return q > 127 ? 127 : (int32_t)q;
}

/* Dedicated inference path for the current fastest kernel.
 * Removes the per-sample mode dispatch and the per-output verification
 * branches from the measured path. The same VSetCut GEMVs and U32 quant
 * arithmetic are used as in mode 19.
 * Reference checking is performed separately before timed rounds.
 */
static __attribute__((noinline)) int e2e_u32_specialized_one_sample(int sample) {
  sparse_tiled_layer0_align16_vsetcut_input(real_input[sample]);
  for (int i = 0; i < 32; ++i) {
    act[i] = (int8_t)requant_u32_exact(sparse_tile_out[i] + real_bias0[i]);
  }
  sparse_tiled_layer1_align16_vsetcut();
  int best = 0;
  int32_t best_logit = sparse_tile_out[0] + real_bias1[0];
  for (int i = 1; i < 10; ++i) {
    int32_t logit = sparse_tile_out[i] + real_bias1[i];
    if (logit > best_logit) {
      best = i;
      best_logit = logit;
    }
  }
  return best;
}

/* Same exact model and second-layer GEMV, only layer-0 vector accumulation changes. */
static __attribute__((noinline)) int e2e_pairacc_one_sample(int sample) {
  sparse_tiled_layer0_align16_pairacc_input(real_input[sample]);
  for (int i=0;i<32;++i)
    act[i]=(int8_t)requant_u32_exact(sparse_tile_out[i]+real_bias0[i]);
  sparse_tiled_layer1_align16_vsetcut();
  int best=0;
  int32_t best_logit=sparse_tile_out[0]+real_bias1[0];
  for (int i=1;i<10;++i) {
    const int32_t logit=sparse_tile_out[i]+real_bias1[i];
    if (logit>best_logit) {best=i;best_logit=logit;}
  }
  return best;
}

/* mode=0: standard RVV Block4; mode=1: Dense VDOT Block4;
 * mode=2: Sparse VDOT Block4. -1 indicates reference mismatch. */
#define RACE_MODES 20
static const char *const e2e_mode_names[RACE_MODES] = {
  "rvv", "dense", "sparse", "sparse-tile8", "sparse-whole", "sparse-batch", "sparse-fused",
  "sparse-direct", "sparse-prefetch2", "sparse-prefetch4",
  "sparse-align16-direct", "sparse-align16-prefetch2", "sparse-align16-prefetch4",
  "sparse-nocopy", "sparse-nocopy-intquant", "sparse-pairload-intquant", "sparse-quadload-intquant", "sparse-unroll2-intquant", "sparse-vsetcut-intquant", "sparse-vsetcut-u32quant"
};
static __attribute__((noinline)) int e2e_one_sample(int mode, int sample, int verify) {
  if (mode < 13) for(int j=0;j<64;++j) act[j]=real_input[sample][j];
  current_cols=64;
  e2e_layer=0;
  if(mode>=3) {
    if(mode==3) { sparse_tiled_layer0(0); sparse_tiled_layer0(1); }
    else if(mode==4) { sparse_tiled_layer0_whole(0); sparse_tiled_layer0_whole(1); }
    else if(mode==5) { sparse_tiled_layer0_batch(0); sparse_tiled_layer0_batch(1); }
    else if(mode==6) { sparse_tiled_layer0_fused(); }
    else if(mode==7) { sparse_tiled_layer0_direct(); }
    else if(mode==8) { sparse_tiled_layer0_prefetch2(); }
    else if(mode==9) { sparse_tiled_layer0_prefetch4(); }
    else if(mode==10) { sparse_tiled_layer0_align16_direct(); }
    else if(mode==11) { sparse_tiled_layer0_align16_prefetch2(); }
    else if(mode==12) { sparse_tiled_layer0_align16_prefetch4(); }
    else if (mode==15) { sparse_tiled_layer0_align16_pairload_input(real_input[sample]); }
    else if (mode==16) { sparse_tiled_layer0_align16_quadload_input(real_input[sample]); }
    else if (mode==17) { sparse_tiled_layer0_align16_unroll2_input(real_input[sample]); }
    else if (mode==18) { sparse_tiled_layer0_align16_vsetcut_input(real_input[sample]); }
    else if (mode==19) { sparse_tiled_layer0_align16_vsetcut_input(real_input[sample]); }
    else { sparse_tiled_layer0_align16_direct_input(real_input[sample]); }
  } else {
    for(int p=0;p<16;p+=4) {
      if(mode==0) rvv_block4_e2e(p);
      else if(mode==1) dense_block4_e2e(p);
      else sparse_block4_e2e(p);
    }
  }
  const int32_t *dot0 = mode==0 ? rvv_block4_out :
                        mode==1 ? dense_block4_out :
                        mode==2 ? sparse_block4_out : sparse_tile_out;
  for(int i=0;i<32;++i) {
    int32_t sum=dot0[i];
    if(verify && sum!=real_dot0[sample][i]) {
      printf("S24-RACE-E2E FAIL mode=%s sample=%d layer=0 row=%d got=%d expected=%d\n",
             e2e_mode_names[mode],sample,i,sum,real_dot0[sample][i]);
      return -1;
    }
    int32_t q;
    if (mode==19) q=requant_u32_exact(sum+real_bias0[i]);
    else if (mode>=14) q=requant_fast_guarded(sum+real_bias0[i]);
    else {
      float z=(float)(sum+real_bias0[i])*REAL_HIDDEN_REQUANT;
      q=z<=0 ? 0 : (int32_t)(z+0.5f);
      if(q>127) q=127;
    }
    if(verify && q!=real_hidden[sample][i]) {
      printf("S24-RACE-E2E FAIL mode=%s sample=%d hidden=%d got=%d expected=%d\n",
             e2e_mode_names[mode],sample,i,q,(int)real_hidden[sample][i]);
      return -1;
    }
    act[i]=(int8_t)q;
  }
  current_cols=32;
  e2e_layer=1;
  if(mode>=3) {
    if(mode==3) sparse_tiled_layer1();
    else if(mode==4) sparse_tiled_layer1_whole();
    else if(mode<=6) sparse_tiled_layer1_batch();
    else if(mode==7) sparse_tiled_layer1_direct();
    else if(mode==8) sparse_tiled_layer1_prefetch2();
    else if(mode==9) sparse_tiled_layer1_prefetch4();
    else if(mode==10) sparse_tiled_layer1_align16_direct();
    else if(mode==11) sparse_tiled_layer1_align16_prefetch2();
    else if(mode==12) sparse_tiled_layer1_align16_prefetch4();
    else if(mode==15) sparse_tiled_layer1_align16_pairload();
    else if(mode==16) sparse_tiled_layer1_align16_quadload();
    else if(mode==17) sparse_tiled_layer1_align16_unroll2();
    else if(mode==18) sparse_tiled_layer1_align16_vsetcut();
    else if(mode==19) sparse_tiled_layer1_align16_vsetcut();
    else sparse_tiled_layer1_align16_direct();
  } else {
    for(int p=0;p<8;p+=4) {
      if(mode==0) rvv_block4_e2e(p);
      else if(mode==1) dense_block4_e2e(p);
      else sparse_block4_e2e(p);
    }
  }
  const int32_t *dot1 = mode==0 ? rvv_block4_out :
                        mode==1 ? dense_block4_out :
                        mode==2 ? sparse_block4_out : sparse_tile_out;
  int best=0;
  int32_t best_logit=dot1[0]+real_bias1[0];
  for(int i=0;i<16;++i) {
    /* Tail kernel omits 6 padded logits that never feed argmax. */
    if(verify && (mode<3 || i<10) && dot1[i]!=real_dot1[sample][i]) {
      printf("S24-RACE-E2E FAIL mode=%s sample=%d layer=1 row=%d got=%d expected=%d\n",
             e2e_mode_names[mode],sample,i,dot1[i],real_dot1[sample][i]);
      return -1;
    }
    if(i>0 && i<10) {
      int32_t logit=dot1[i]+real_bias1[i];
      if(logit>best_logit){best_logit=logit;best=i;}
    }
  }
  if(verify && best!=real_pred[sample]) {
    printf("S24-RACE-E2E FAIL mode=%s sample=%d pred=%d expected=%d\n",
           e2e_mode_names[mode],sample,best,(int)real_pred[sample]);
    return -1;
  }
  return best;
}

static __attribute__((unused)) uint64_t e2e_time_batch(int mode, int *pred_checksum) {
  int checksum=0;
  uint64_t before=cycle_now();
  for(int sample=0;sample<REAL_TEST_COUNT;++sample)
    checksum+=e2e_one_sample(mode,sample,0);
  uint64_t after=cycle_now();
  *pred_checksum=checksum;
  checksum_sink=checksum;
  return after-before;
}

#define E2E_RVV_ROUNDS 10
static __attribute__((unused)) uint64_t e2e_median(uint64_t input[E2E_RVV_ROUNDS]) {
  uint64_t temp[E2E_RVV_ROUNDS];
  for(int i=0;i<E2E_RVV_ROUNDS;++i) temp[i]=input[i];
  for(int i=0;i<E2E_RVV_ROUNDS;++i)
    for(int j=i+1;j<E2E_RVV_ROUNDS;++j)
      if(temp[j]<temp[i]) {uint64_t x=temp[i];temp[i]=temp[j];temp[j]=x;}
  return (temp[E2E_RVV_ROUNDS/2-1]+temp[E2E_RVV_ROUNDS/2])/2;
}
/* Correctness checks deliberately outside timed rounds. */
static uint64_t pairacc_time_batch(int mode,int *pred_checksum){
  int checksum=0;
  uint64_t start=cycle_now();
  for(int sample=0;sample<REAL_TEST_COUNT;++sample)
    checksum+=mode ? e2e_pairacc_one_sample(sample) : e2e_u32_specialized_one_sample(sample);
  uint64_t cycles=cycle_now()-start;
  *pred_checksum=checksum;
  checksum_sink=checksum;
  return cycles;
}
static int e2e_benchmark(void){
  e2e_prepare_weights();
  int checksums[2]={0,0},correct[2]={0,0};
  for(int sample=0;sample<REAL_TEST_COUNT;++sample){
    /* Validate source dot-products against the exported full-int8 reference. */
    if(e2e_one_sample(19,sample,1)<0)return 1;
    for(int mode=0;mode<2;++mode){
      if(mode) sparse_tiled_layer0_align16_pairacc_input(real_input[sample]);
      else sparse_tiled_layer0_align16_vsetcut_input(real_input[sample]);
      for(int i=0;i<32;++i)if(sparse_tile_out[i]!=real_dot0[sample][i]){
        printf("S24-PAIRACC FAIL layer0 mode=%d sample=%d row=%d got=%d expected=%d\n",
          mode,sample,i,sparse_tile_out[i],real_dot0[sample][i]);return 1;
      }
      int pred=mode ? e2e_pairacc_one_sample(sample):e2e_u32_specialized_one_sample(sample);
      if(pred!=real_pred[sample]){
        printf("S24-PAIRACC FAIL pred mode=%d sample=%d got=%d want=%d\n",
          mode,sample,pred,(int)real_pred[sample]);return 1;
      }
      for(int i=0;i<32;++i)if(act[i]!=real_hidden[sample][i]){
        printf("S24-PAIRACC FAIL hidden mode=%d sample=%d row=%d\n",mode,sample,i);return 1;
      }
      for(int i=0;i<10;++i)if(sparse_tile_out[i]!=real_dot1[sample][i]){
        printf("S24-PAIRACC FAIL layer1 mode=%d sample=%d row=%d\n",mode,sample,i);return 1;
      }
      checksums[mode]+=pred;
      correct[mode]+=(pred==real_label[sample]);
    }
  }
  for(int mode=0;mode<2;++mode)
    printf("S24-PAIRACC VERIFY PASS mode=%s samples=%d correct=%d reference=python-int8\n",
      mode?"pairwise-layer0":"specialized",REAL_TEST_COUNT,correct[mode]);
  enum{WARMUP=3,MEASURE=5,ROUNDS=WARMUP+MEASURE};
  uint64_t cycles[2][ROUNDS];
  for(int round=0;round<ROUNDS;++round){
    for(int slot=0;slot<2;++slot){
      int mode=(round+slot)&1, checksum;
      cycles[mode][round]=pairacc_time_batch(mode,&checksum);
      if(checksum!=checksums[mode]){
        printf("S24-PAIRACC FAIL checksum mode=%d round=%d\n",mode,round);return 1;
      }
    }
    printf("S24-PAIRACC ROUND round=%d %s specialized=%llu pairacc=%llu\n",
      round,round<WARMUP?"warmup":"measured",
      (unsigned long long)cycles[0][round],(unsigned long long)cycles[1][round]);
  }
  uint64_t median[2];
  for(int mode=0;mode<2;++mode){
    uint64_t sorted[MEASURE];
    for(int i=0;i<MEASURE;++i)sorted[i]=cycles[mode][WARMUP+i];
    for(int i=0;i<MEASURE;++i)for(int j=i+1;j<MEASURE;++j)
      if(sorted[j]<sorted[i]){uint64_t t=sorted[i];sorted[i]=sorted[j];sorted[j]=t;}
    median[mode]=sorted[MEASURE/2];
  }
  printf("S24-PAIRACC MEDIAN samples=%d rounds=%d warmup=%d specialized=%llu pairacc=%llu\n",
    REAL_TEST_COUNT,MEASURE,WARMUP,(unsigned long long)median[0],(unsigned long long)median[1]);
  printf("S24-PAIRACC ALL PASS reference_predictions_match=yes layer0_dots_match=yes hidden_match=yes layer1_dots_match=yes\n");
  return 0;
}
int main(void){vector_enable();return e2e_benchmark();}
