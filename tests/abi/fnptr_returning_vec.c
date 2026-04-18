/* ABI: function pointers returning/taking vectors. Verifies indirect
   calling-convention path works (not just direct-call optimization). */
#include <mxu2.h>
#include <stdio.h>

typedef v4i32 (*op_t)(v4i32, v4i32);

__attribute__((noinline))
static v4i32 op_add(v4i32 a, v4i32 b) { return __builtin_mxu2_add_w(a, b); }
__attribute__((noinline))
static v4i32 op_sub(v4i32 a, v4i32 b) { return __builtin_mxu2_sub_w(a, b); }
__attribute__((noinline))
static v4i32 op_xor(v4i32 a, v4i32 b) { return (v4i32)__builtin_mxu2_xorv((v16i8)a, (v16i8)b); }

static volatile op_t g_op_table[] = { op_add, op_sub, op_xor };

__attribute__((noinline))
static v4i32 dispatch(int idx, v4i32 a, v4i32 b) {
    return g_op_table[idx](a, b);
}

int main(void) {
    int fails = 0;
    v4i32 a = {10, 20, 30, 40};
    v4i32 b = {1, 2, 3, 4};

    v4i32 r0 = dispatch(0, a, b);  /* add */
    if (r0[0] != 11 || r0[3] != 44) { puts("FAIL dispatch add"); fails++; }
    v4i32 r1 = dispatch(1, a, b);  /* sub */
    if (r1[0] != 9 || r1[3] != 36) { puts("FAIL dispatch sub"); fails++; }
    v4i32 r2 = dispatch(2, a, b);  /* xor */
    if (r2[0] != (10^1) || r2[3] != (40^4)) { puts("FAIL dispatch xor"); fails++; }

    if (fails == 0) puts("ABI fnptr OK");
    return fails;
}
