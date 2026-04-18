/* ABI: tail calls with vector args.
   Verifies that tail-call optimization handles vector argument
   passing correctly (esp. when the callee receives same-shape args). */
#include <mxu2.h>
#include <stdio.h>

__attribute__((noinline))
static v4i32 tail_target(v4i32 a, v4i32 b) {
    return __builtin_mxu2_add_w(a, b);
}

__attribute__((noinline))
static v4i32 tail_caller(v4i32 a, v4i32 b) {
    /* Compiler may turn this into a tail call. */
    return tail_target(a, __builtin_mxu2_sub_w(b, (v4i32){1,1,1,1}));
}

__attribute__((noinline))
static v4i32 tail_chain_3(v4i32 a, v4i32 b) {
    return tail_caller(__builtin_mxu2_add_w(a, (v4i32){1,1,1,1}), b);
}

int main(void) {
    v4i32 a = {10, 20, 30, 40};
    v4i32 b = {1, 2, 3, 4};
    v4i32 r = tail_chain_3(a, b);
    /* a' = {11,21,31,41}; b' = {0,1,2,3}; r = a' + b' = {11,22,33,44} */
    int ok = (r[0] == 11 && r[1] == 22 && r[2] == 33 && r[3] == 44);
    if (ok) puts("ABI tail OK");
    else printf("FAIL tail got {%d,%d,%d,%d} want {11,22,33,44}\n", r[0],r[1],r[2],r[3]);
    return !ok;
}
