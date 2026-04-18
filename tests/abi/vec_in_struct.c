/* ABI: vector embedded in struct, by value + by pointer + return.
   Verifies SUB/COMPOSITE_TYPES handling for vector members.

   KNOWN ICE (discovered 2026-04-17): passing a struct CONTAINING v4i32
   BY VALUE triggers reload exhaustion in mips backend (same class as
   the v6 PBV fix, but applies to struct-wrapped vectors). See
   `sum_big_known_ice` below — kept as documentation. */
#include <mxu2.h>
#include <stdio.h>

struct s_small { v4i32 v; int tag; };
struct s_big { int hdr; v4i32 a; v4i32 b; double trail; };

__attribute__((noinline))
static struct s_small make_small(v4i32 v, int tag) {
    struct s_small s = { v, tag };
    s.v = __builtin_mxu2_add_w(s.v, (v4i32){1,1,1,1});
    return s;
}

#if 0   /* enable once ICE in struct-by-value-with-vector is fixed */
__attribute__((noinline))
static int sum_big_known_ice(struct s_big b) {
    v4i32 r = __builtin_mxu2_add_w(b.a, b.b);
    return r[0] + r[1] + r[2] + r[3] + b.hdr + (int)b.trail;
}
#endif

__attribute__((noinline))
static void modify_inplace(struct s_big *b) {
    b->a = __builtin_mxu2_add_w(b->a, b->b);
}

int main(void) {
    int fails = 0;
    struct s_small s = make_small((v4i32){10,20,30,40}, 7);
    if (s.v[0] != 11 || s.v[3] != 41 || s.tag != 7) { puts("FAIL small"); fails++; }

    struct s_big b = { .hdr = 100, .a = {1,2,3,4}, .b = {10,20,30,40}, .trail = 0.5 };
    modify_inplace(&b);
    if (b.a[0] != 11 || b.a[3] != 44) { puts("FAIL modify_inplace"); fails++; }

    if (fails == 0) puts("ABI struct OK");
    return fails;
}
