#include <klib.h>
#include <stdint.h>

#define ELEMENTS       8
#define VECTOR_BYTES   16
#define DIRECTED_CASES 4
#define LANE_CASES     8
#define RANDOM_CASES   64

/*
 * Each row occupies 16 bytes so that every test vector is 16-byte aligned.
 * vdot.vv uses signed elements 0 through 7.
 */
static int8_t directed_a[DIRECTED_CASES][VECTOR_BYTES]
    __attribute__((aligned(16))) = {
    { 1, 2, 3, 4, 5, 6, 7, 8 },
    { -1, -1, -1, -1, -1, -1, -1, -1 },
    { -128, -128, -128, -128, -128, -128, -128, -128 },
    { -128, -128, -128, -128, -128, -128, -128, -128 }
};

static int8_t directed_b[DIRECTED_CASES][VECTOR_BYTES]
    __attribute__((aligned(16))) = {
    { 8, 7, 6, 5, 4, 3, 2, 1 },
    { 1, 2, 3, 4, 5, 6, 7, 8 },
    { -128, -128, -128, -128, -128, -128, -128, -128 },
    { 127, 127, 127, 127, 127, 127, 127, 127 }
};

static const char *directed_names[DIRECTED_CASES] = {
    "positive",
    "negative",
    "maximum",
    "minimum"
};

static uint8_t dst[VECTOR_BYTES] __attribute__((aligned(16)));
static uint32_t random_state = 0x013157a6u;

static inline uintptr_t read_mcycle(void)
{
    uintptr_t value;
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

static int32_t reference_dot(const int8_t *a, const int8_t *b)
{
    int32_t sum = 0;

    for (int i = 0; i < ELEMENTS; i++) {
        sum += (int32_t)a[i] * (int32_t)b[i];
    }

    return sum;
}

static int32_t read_result(void)
{
    uint32_t raw =
        ((uint32_t)dst[0])       |
        ((uint32_t)dst[1] << 8)  |
        ((uint32_t)dst[2] << 16) |
        ((uint32_t)dst[3] << 24);

    return (int32_t)raw;
}

static int upper_bytes_are_zero(void)
{
    for (int i = 4; i < VECTOR_BYTES; i++) {
        if (dst[i] != 0) {
            return 0;
        }
    }

    return 1;
}

static uint32_t next_random_u32(void)
{
    uint32_t value = random_state;

    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    random_state = value;

    return value;
}

static int8_t next_random_i8(void)
{
    return (int8_t)(next_random_u32() >> 24);
}

static void print_input(const char *name, const int8_t *data)
{
    printf("%s:", name);

    for (int i = 0; i < ELEMENTS; i++) {
        printf(" %d", (int)data[i]);
    }

    printf("\n");
}

static void run_vdot(const int8_t *a, const int8_t *b)
{
    asm volatile(
        "vsetivli zero, 8, e8, m1, ta, ma\n"
        "vle8.v v1, (%[a])\n"
        "vle8.v v2, (%[b])\n"

        /*
         * Custom instruction:
         *   vdot.vv v3, v1, v2
         *
         * funct6 = 111001
         * vm     = 1
         * vs2    = v1
         * vs1    = v2
         * funct3 = 000
         * vd     = v3
         * opcode = 1010111
         */
        ".word 0xe61101d7\n"

        /*
         * Store all 128 result bits.
         * The hardware must set bytes 4 through 15 to zero.
         */
        "vsetivli zero, 16, e8, m1, ta, ma\n"
        "vse8.v v3, (%[d])\n"
        :
        : [a] "r"(a),
          [b] "r"(b),
          [d] "r"(dst)
        : "memory"
    );
}

static int check_case(
    const char *group,
    int case_index,
    const char *case_name,
    const int8_t *a,
    const int8_t *b,
    int verbose)
{
    for (int i = 0; i < VECTOR_BYTES; i++) {
        dst[i] = 0xa5;
    }

    uintptr_t start_cycle = read_mcycle();
    run_vdot(a, b);
    uintptr_t end_cycle = read_mcycle();

    int32_t expected = reference_dot(a, b);
    int32_t actual = read_result();
    int upper_zero = upper_bytes_are_zero();
    int failed = actual != expected || !upper_zero;

    if (verbose) {
        printf(
            "vdot.vv %-8s: actual=%d expected=%d "
            "upper_zero=%s cycles=%lu\n",
            case_name,
            (int)actual,
            (int)expected,
            upper_zero ? "yes" : "no",
            (unsigned long)(end_cycle - start_cycle)
        );
    }

    if (failed) {
        printf(
            "FAIL %s[%d]: actual=%d expected=%d upper_zero=%s\n",
            group,
            case_index,
            (int)actual,
            (int)expected,
            upper_zero ? "yes" : "no"
        );
        print_input("  src_a", a);
        print_input("  src_b", b);
    }

    return failed;
}

int main(void)
{
    int errors = 0;
    int lane_errors = 0;
    int random_errors = 0;

    int8_t lane_a[VECTOR_BYTES] __attribute__((aligned(16)));
    int8_t lane_b[VECTOR_BYTES] __attribute__((aligned(16)));
    int8_t random_a[VECTOR_BYTES] __attribute__((aligned(16)));
    int8_t random_b[VECTOR_BYTES] __attribute__((aligned(16)));

    init_vector();

    for (int test = 0; test < DIRECTED_CASES; test++) {
        errors += check_case(
            "directed",
            test,
            directed_names[test],
            directed_a[test],
            directed_b[test],
            1
        );
    }

    /*
     * Activate exactly one lane at a time. This detects swapped, omitted,
     * duplicated, or incorrectly sliced byte lanes in the hardware datapath.
     */
    for (int lane = 0; lane < LANE_CASES; lane++) {
        for (int i = 0; i < VECTOR_BYTES; i++) {
            lane_a[i] = 0;
            lane_b[i] = 0;
        }

        lane_a[lane] = (int8_t)(lane * 17 - 60);
        lane_b[lane] =
            (int8_t)((lane & 1) ? -(lane + 1) : (lane + 1));

        lane_errors += check_case(
            "lane",
            lane,
            "lane",
            lane_a,
            lane_b,
            0
        );
    }

    errors += lane_errors;

    if (lane_errors == 0) {
        printf("vdot.vv lane routing PASS: %d cases\n", LANE_CASES);
    }

    /*
     * Fixed-seed pseudo-random tests are deterministic and reproducible.
     * Expected values are calculated independently by reference_dot().
     */
    for (int test = 0; test < RANDOM_CASES; test++) {
        for (int i = 0; i < VECTOR_BYTES; i++) {
            random_a[i] = 0;
            random_b[i] = 0;
        }

        for (int i = 0; i < ELEMENTS; i++) {
            random_a[i] = next_random_i8();
            random_b[i] = next_random_i8();
        }

        random_errors += check_case(
            "random",
            test,
            "random",
            random_a,
            random_b,
            0
        );
    }

    errors += random_errors;

    if (random_errors == 0) {
        printf(
            "vdot.vv deterministic random PASS: %d cases seed=0x013157a6\n",
            RANDOM_CASES
        );
    }

    if (errors == 0) {
        printf(
            "vdot.vv test PASS: %d total cases\n",
            DIRECTED_CASES + LANE_CASES + RANDOM_CASES
        );
        return 0;
    }

    printf("vdot.vv test FAIL: %d cases failed\n", errors);
    return 1;
}
