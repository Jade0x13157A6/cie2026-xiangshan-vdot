#include <klib.h>
#include <stdint.h>

static volatile int8_t a[8] __attribute__((aligned(16))) =
    {1, 2, 3, 4, 5, 6, 7, 8};

static volatile int8_t b[8] __attribute__((aligned(16))) =
    {8, 7, 6, 5, 4, 3, 2, 1};

static volatile int8_t result[8] __attribute__((aligned(16)));

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

int main(void)
{
    init_vector();

    asm volatile(
        "vsetivli zero, 8, e8, m1, ta, ma\n"
        "vle8.v v1, (%[a])\n"
        "vle8.v v2, (%[b])\n"
        "vadd.vv v3, v1, v2\n"
        "vse8.v v3, (%[result])\n"
        :
        : [a] "r"(a),
          [b] "r"(b),
          [result] "r"(result)
        : "memory"
    );

    for (int i = 0; i < 8; i++) {
        if (result[i] != 9) {
            printf("vadd.vv FAIL: lane=%d actual=%d expected=9\n",
                   i, (int)result[i]);
            return 1;
        }
    }

    printf("vadd.vv PASS: 8 lanes\n");
    return 0;
}
