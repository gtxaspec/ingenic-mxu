/* Layer E: aliasing. Vector pointers with restrict + overlapping access.
   Catches compiler bugs where strict aliasing assumptions miscompile.  */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>

static int fail = 0;
#define CHECK(cond, name) do { if(!(cond)){fprintf(stderr,"FAIL: %s\n",name); fail++;} } while(0)

/* --- restrict pointers, no overlap. --- */
__attribute__((noinline))
static void vadd_restrict(v4i32 * __restrict__ out,
                          const v4i32 * __restrict__ a,
                          const v4i32 * __restrict__ b, int n) {
    for (int i = 0; i < n; i++)
        out[i] = __builtin_mxu2_add_w(a[i], b[i]);
}

/* --- aliased pointers (same buffer for src+dst). --- */
__attribute__((noinline))
static void vadd_aliased(v4i32 *out, const v4i32 *a, const v4i32 *b, int n) {
    for (int i = 0; i < n; i++)
        out[i] = __builtin_mxu2_add_w(a[i], b[i]);
}

/* --- mode-pun aliasing: write as v16i8, read as v4i32. --- */
__attribute__((noinline))
static int32_t mode_pun(const v16i8 *p) {
    v4i32 *as_si = (v4i32 *)p;
    return as_si[0][0];
}

int main(void) {
    static v4i32 ina[4] __attribute__((aligned(16)));
    static v4i32 inb[4] __attribute__((aligned(16)));
    static v4i32 out[4] __attribute__((aligned(16)));
    for (int i = 0; i < 4; i++) {
        ina[i] = (v4i32){i*4+0, i*4+1, i*4+2, i*4+3};
        inb[i] = (v4i32){100, 200, 300, 400};
    }

    /* restrict path */
    vadd_restrict(out, ina, inb, 4);
    CHECK(out[0][0] == 100 && out[3][3] == 415, "restrict");

    /* aliased: out == ina */
    for (int i = 0; i < 4; i++) ina[i] = (v4i32){1, 1, 1, 1};
    vadd_aliased(ina, ina, inb, 4);
    CHECK(ina[0][0] == 101, "aliased");

    /* mode pun */
    static v16i8 buf __attribute__((aligned(16))) =
        {0x78, 0x56, 0x34, 0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0};
    CHECK(mode_pun(&buf) == 0x12345678, "mode_pun");

    if (fail == 0) { puts("ALIAS OK"); return 0; }
    printf("ALIAS FAIL %d\n", fail);
    return 1;
}
