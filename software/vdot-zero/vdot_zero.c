#include <klib.h>
#include <stdint.h>

#define N 16
static int8_t zeros[N] __attribute__((aligned(16)));
static int8_t nonzeros[N] __attribute__((aligned(16))) =
    {1, 2, 3, 4, 5, 6, 7, 8};
static uint8_t dst[N] __attribute__((aligned(16)));

static void init_vector(void) {
  asm volatile(
      "lui t0, 0x2\n"
      "addiw t0, t0, 512\n"
      "csrs mstatus, t0\n"
      "csrwi vcsr, 0\n"
      "csrwi vstart, 0\n"
      : : : "t0", "memory");
}

static int run_case(const int8_t *a, const int8_t *b, const char *name) {
  for (int i = 0; i < N; ++i) dst[i] = 0xa5;
  asm volatile(
      "vsetivli zero, 8, e8, m1, ta, ma\n"
      "vle8.v v1, (%[a])\n"
      "vle8.v v2, (%[b])\n"
      ".word 0xe61101d7\n"  /* Dense vdot.vv v3, v1, v2 */
      "vsetivli zero, 16, e8, m1, ta, ma\n"
      "vse8.v v3, (%[d])\n"
      : : [a] "r"(a), [b] "r"(b), [d] "r"(dst) : "memory");
  for (int i = 0; i < N; ++i) {
    if (dst[i] != 0) {
      printf("vdot.vv zero FAIL: %s byte=%d val=%u\n", name, i, (unsigned)dst[i]);
      return 1;
    }
  }
  return 0;
}

int main(void) {
  init_vector();
  if (run_case(zeros, nonzeros, "lhs")) return 1;
  if (run_case(nonzeros, zeros, "rhs")) return 1;
  printf("vdot.vv zero PASS: 2 cases\n");
  return 0;
}
