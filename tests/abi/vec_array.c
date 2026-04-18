/* ABI: arrays of vector types as args, locals, returns. */
#include <mxu2.h>
#include <stdio.h>

__attribute__((noinline))
static void sum_array(v4i32 *arr, int n, v4i32 *result) {
    v4i32 acc = {0,0,0,0};
    for (int i = 0; i < n; i++)
        acc = __builtin_mxu2_add_w(acc, arr[i]);
    *result = acc;
}

__attribute__((noinline))
static int test_local_array(void) {
    v4i32 local[8];
    for (int i = 0; i < 8; i++)
        local[i] = (v4i32){i, i+1, i+2, i+3};
    v4i32 r;
    sum_array(local, 8, &r);
    /* sum of i across 8 = 0+1+...+7 = 28; per-lane = {28, 36, 44, 52} */
    return r[0] == 28 && r[1] == 36 && r[2] == 44 && r[3] == 52;
}

int main(void) {
    int fails = 0;
    if (!test_local_array()) { puts("FAIL local_array"); fails++; }

    /* Static array of vectors */
    static v4i32 static_arr[4] __attribute__((aligned(16))) = {
        {1,2,3,4}, {5,6,7,8}, {9,10,11,12}, {13,14,15,16}};
    v4i32 r;
    sum_array(static_arr, 4, &r);
    /* {1+5+9+13, 2+6+10+14, 3+7+11+15, 4+8+12+16} = {28,32,36,40} */
    if (r[0] != 28 || r[3] != 40) { puts("FAIL static_array"); fails++; }

    if (fails == 0) puts("ABI array OK");
    return fails;
}
