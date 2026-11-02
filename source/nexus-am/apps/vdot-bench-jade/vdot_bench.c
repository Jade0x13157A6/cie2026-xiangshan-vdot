#include <klib.h>
#include <stdint.h>

#define ELEMENTS   8
#define ITERATIONS 1000
#define WARMUP     32
#define ROUNDS     5

static volatile int8_t src_a[ELEMENTS]
    __attribute__((aligned(16))) = {
    1, 2, 3, 4, 5, 6, 7, 8
};

static volatile int8_t src_b[ELEMENTS]
    __attribute__((aligned(16))) = {
    8, 7, 6, 5, 4, 3, 2, 1
};

static volatile int32_t benchmark_sink;

static inline uint64_t read_mcycle(void)
{
    uint64_t value;
    asm volatile("csrr %0, mcycle" : "=r"(value) : : "memory");
    return value;
}

static void init_vector(void)
{
    asm volatile(
        "lui t0, 0x2\n"
        "addiw t0, t0, 512\n"
        "csrs mstatus, t0\n"
        "csrwi vcsr, 0\n"
        "csrwi vstart, 0\n"
        :
        :
        : "t0", "memory"
    );
}

/*
 * Optimized scalar baseline. Auto-vectorization is disabled in the Makefile,
 * so this remains an ordinary scalar int8 dot-product implementation.
 */
static __attribute__((noinline)) int32_t scalar_dot_once(
    const volatile int8_t *a,
    const volatile int8_t *b)
{
    int32_t sum = 0;

    for (int i = 0; i < ELEMENTS; i++) {
        sum += (int32_t)a[i] * (int32_t)b[i];
    }

    return sum;
}

/*
 * Custom VDOT kernel:
 *   - configure e8/m1
 *   - load two 8-byte input vectors
 *   - execute one vdot.vv
 *   - move the 32-bit result to a scalar register
 */
static __attribute__((noinline)) int32_t custom_dot_once(
    const volatile int8_t *a,
    const volatile int8_t *b)
{
    uintptr_t raw;

    asm volatile(
        "vsetivli zero, 8, e8, m1, ta, ma\n"
        "vle8.v v1, (%[a])\n"
        "vle8.v v2, (%[b])\n"
        ".word 0xe61101d7\n"
        "vsetivli zero, 1, e32, m1, ta, ma\n"
        "vmv.x.s %[result], v3\n"
        : [result] "=r"(raw)
        : [a] "r"(a),
          [b] "r"(b)
        : "memory"
    );

    return (int32_t)(uint32_t)raw;
}


/*
 * Standard RVV baseline:
 *   signed INT8 x INT8 -> INT16 products
 *   widening reduction of eight INT16 values -> INT32
 *
 * Register allocation:
 *   v1, v2   : INT8 source vectors
 *   v4-v5    : widened multiplication destination
 *   v8-v9    : widened reduction destination
 *   v10-v11  : zero-initialized reduction seed
 */
static __attribute__((noinline)) int32_t rvv_dot_once(
    const volatile int8_t *a,
    const volatile int8_t *b)
{
    uintptr_t raw;

    asm volatile(
        "vsetivli zero, 1, e32, m1, ta, ma\n"
        "vmv.s.x v10, zero\n"
        "vsetivli zero, 8, e8, m1, ta, ma\n"
        "vle8.v v1, (%[a])\n"
        "vle8.v v2, (%[b])\n"
        "vwmul.vv v4, v1, v2\n"
        "vsetivli zero, 8, e16, m1, ta, ma\n"
        "vwredsum.vs v8, v4, v10\n"
        "vsetivli zero, 1, e32, m1, ta, ma\n"
        "vmv.x.s %[result], v8\n"
        : [result] "=r"(raw)
        : [a] "r"(a), [b] "r"(b)
        : "memory"
    );

    return (int32_t)(uint32_t)raw;
}


static int check_dot_cases(void)
{
    static const int8_t inputs_a[4][ELEMENTS] = {
        { 1, 2, 3, 4, 5, 6, 7, 8 },
        { -1, -2, -3, -4, -5, -6, -7, -8 },
        { -128, -128, -128, -128, -128, -128, -128, -128 },
        { -128, -128, -128, -128, -128, -128, -128, -128 }
    };

    static const int8_t inputs_b[4][ELEMENTS] = {
        { 8, 7, 6, 5, 4, 3, 2, 1 },
        { 8, 7, 6, 5, 4, 3, 2, 1 },
        { -128, -128, -128, -128, -128, -128, -128, -128 },
        { 127, 127, 127, 127, 127, 127, 127, 127 }
    };

    static const int32_t expected[4] = {
        120, -120, 131072, -130048
    };

    static const char *names[4] = {
        "positive", "negative", "maximum", "minimum"
    };

    for (int t = 0; t < 4; t++) {
        for (int i = 0; i < ELEMENTS; i++) {
            src_a[i] = inputs_a[t][i];
            src_b[i] = inputs_b[t][i];
        }

        int32_t scalar = scalar_dot_once(src_a, src_b);
        int32_t rvv = rvv_dot_once(src_a, src_b);
        int32_t vdot = custom_dot_once(src_a, src_b);

        printf(
            "%s: scalar=%d rvv=%d vdot=%d expected=%d\n",
            names[t], (int)scalar, (int)rvv,
            (int)vdot, (int)expected[t]
        );

        if (scalar != expected[t] ||
            rvv != expected[t] ||
            vdot != expected[t]) {
            printf("RVV baseline correctness FAIL\n");
            return 1;
        }
    }

    /* Restore the original benchmark inputs. */
    for (int i = 0; i < ELEMENTS; i++) {
        src_a[i] = inputs_a[0][i];
        src_b[i] = inputs_b[0][i];
    }

    printf("RVV baseline correctness PASS: 4 cases\n");
    return 0;
}

static uint64_t measure_scalar(void)
{
    int32_t checksum = 0;
    uint64_t start = read_mcycle();

    for (int i = 0; i < ITERATIONS; i++) {
        checksum += scalar_dot_once(src_a, src_b);
    }

    uint64_t end = read_mcycle();
    benchmark_sink = checksum;

    return end - start;
}


static uint64_t measure_rvv(void)
{
    int32_t checksum = 0;
    uint64_t start = read_mcycle();

    for (int i = 0; i < ITERATIONS; i++) {
        checksum += rvv_dot_once(src_a, src_b);
    }

    uint64_t end = read_mcycle();
    benchmark_sink = checksum;

    return end - start;
}

static uint64_t measure_custom(void)
{
    int32_t checksum = 0;
    uint64_t start = read_mcycle();

    for (int i = 0; i < ITERATIONS; i++) {
        checksum += custom_dot_once(src_a, src_b);
    }

    uint64_t end = read_mcycle();
    benchmark_sink = checksum;

    return end - start;
}

static void print_per_dot(const char *name, uint64_t cycles)
{
    uint64_t milli_cycles = cycles * 1000 / ITERATIONS;

    printf(
        "%s: total=%lu cycles, per_dot=%lu.%03lu cycles\n",
        name,
        (unsigned long)cycles,
        (unsigned long)(milli_cycles / 1000),
        (unsigned long)(milli_cycles % 1000)
    );
}


int main(void)
{
    uint64_t best_scalar = ~(uint64_t)0;
    uint64_t best_rvv = ~(uint64_t)0;
    uint64_t best_custom = ~(uint64_t)0;

    init_vector();

    if (check_dot_cases() != 0)
        return 1;

    int32_t scalar_result = scalar_dot_once(src_a, src_b);
    int32_t rvv_result = rvv_dot_once(src_a, src_b);
    int32_t custom_result = custom_dot_once(src_a, src_b);

    printf(
        "correctness: scalar=%d rvv=%d vdot=%d expected=120\n",
        (int)scalar_result,
        (int)rvv_result,
        (int)custom_result
    );

    if (scalar_result != 120 ||
        rvv_result != 120 ||
        custom_result != 120) {
        printf("vdot benchmark correctness FAIL\n");
        return 1;
    }

    for (int i = 0; i < WARMUP; i++) {
        benchmark_sink = scalar_dot_once(src_a, src_b);
        benchmark_sink = rvv_dot_once(src_a, src_b);
        benchmark_sink = custom_dot_once(src_a, src_b);
    }

    /*
     * Rotate measurement order across rounds.
     */
    for (int round = 0; round < ROUNDS; round++) {
        uint64_t scalar_cycles;
        uint64_t rvv_cycles;
        uint64_t custom_cycles;

        if (round % 3 == 0) {
            scalar_cycles = measure_scalar();
            rvv_cycles = measure_rvv();
            custom_cycles = measure_custom();
        } else if (round % 3 == 1) {
            rvv_cycles = measure_rvv();
            custom_cycles = measure_custom();
            scalar_cycles = measure_scalar();
        } else {
            custom_cycles = measure_custom();
            scalar_cycles = measure_scalar();
            rvv_cycles = measure_rvv();
        }

        if (scalar_cycles < best_scalar)
            best_scalar = scalar_cycles;
        if (rvv_cycles < best_rvv)
            best_rvv = rvv_cycles;
        if (custom_cycles < best_custom)
            best_custom = custom_cycles;

        printf(
            "round %d: scalar=%lu rvv=%lu custom=%lu cycles\n",
            round,
            (unsigned long)scalar_cycles,
            (unsigned long)rvv_cycles,
            (unsigned long)custom_cycles
        );
    }

    uint64_t scalar_speedup = best_scalar * 1000 / best_custom;
    uint64_t rvv_speedup = best_rvv * 1000 / best_custom;

    printf("=== best result over %d rounds ===\n", ROUNDS);
    print_per_dot("scalar", best_scalar);
    print_per_dot("rvv   ", best_rvv);
    print_per_dot("vdot  ", best_custom);

    printf(
        "scalar -> vdot speedup: %lu.%03lux\n",
        (unsigned long)(scalar_speedup / 1000),
        (unsigned long)(scalar_speedup % 1000)
    );

    printf(
        "rvv -> vdot speedup: %lu.%03lux\n",
        (unsigned long)(rvv_speedup / 1000),
        (unsigned long)(rvv_speedup % 1000)
    );

    printf("vdot benchmark PASS\n");
    return 0;
}
